#include "embedded_python.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include "Python.h"
#endif

/* Run-time dynamic linking helpers. */
#if defined(_WIN32)
/* Use anonymous structs to emulate the library behavior: only pointer use. */
struct PyObject;
struct PyThreadState;

/* Typedefs for functions we're going to load from the library. */
typedef void (__cdecl *Py_Initialize_t)();
typedef int (__cdecl *Py_FinalizeEx_t)();
typedef void (__cdecl *Py_DecRef_t)(PyObject *);
typedef PyThreadState *(__cdecl *PyEval_SaveThread_t)();
typedef void (__cdecl *PyEval_RestoreThread_t)(PyThreadState *);
typedef int (__cdecl *PyGILState_Ensure_t)();
typedef PyObject *(__cdecl *PyImport_ImportModule_t)(const char *);
typedef PyObject *(__cdecl *PyObject_GetAttrString_t)(PyObject *, const char *);
typedef PyObject *(__cdecl *PyUnicode_DecodeFSDefault_t)(const char *);
typedef int (__cdecl *PyList_Append_t)(PyObject *, PyObject *);
typedef int (__cdecl *PyList_Insert_t)(PyObject *, SSIZE_T, PyObject *);
typedef void (__cdecl *PyErr_Print_t)();
typedef void (__cdecl *PyGILState_Release_t)(int);
typedef PyObject *(__cdecl *PyImport_ReloadModule_t)(PyObject *);
typedef int (__cdecl *PyCallable_Check_t)(PyObject *);
typedef PyObject *(__cdecl *PyTuple_New_t)(SSIZE_T);
typedef int (__cdecl *PyTuple_SetItem_t)(PyObject *, SSIZE_T, PyObject *);
typedef PyObject *(__cdecl *PyObject_CallObject_t)(PyObject *, PyObject *);
typedef PyObject *(__cdecl *PyLong_FromSize_t_t)(SIZE_T);
typedef PyObject *(__cdecl *PyLong_FromLong_t)(long);
typedef PyObject *(__cdecl *PyObject_CallFunctionObjArgs_t)(PyObject *, ...);
typedef int (__cdecl *PySys_SetObject_t)(const char *, PyObject *);
typedef PyObject *(__cdecl *PySys_GetObject_t)(const char *);
typedef PyObject *(__cdecl *PyObject_CallMethod_t)(PyObject *, const char *, const char*, ...);
typedef PyObject *(__cdecl *PyUnicode_AsEncodedString_t)(PyObject *, const char *, const char *);
typedef char *(__cdecl *PyBytes_AsString_t)(PyObject *);

/* Static function pointers. We'll populate these once we load the library. */
static Py_Initialize_t Py_Initialize = NULL;
static Py_FinalizeEx_t Py_FinalizeEx = NULL;
static Py_DecRef_t Py_DecRef = NULL;
static PyEval_SaveThread_t PyEval_SaveThread = NULL;
static PyEval_RestoreThread_t PyEval_RestoreThread = NULL;
static PyGILState_Ensure_t PyGILState_Ensure = NULL;
static PyImport_ImportModule_t PyImport_ImportModule = NULL;
static PyObject_GetAttrString_t PyObject_GetAttrString = NULL;
static PyUnicode_DecodeFSDefault_t PyUnicode_DecodeFSDefault = NULL;
static PyList_Append_t PyList_Append = NULL;
static PyList_Insert_t PyList_Insert = NULL;
static PyErr_Print_t PyErr_Print = NULL;
static PyGILState_Release_t PyGILState_Release = NULL;
static PyImport_ReloadModule_t PyImport_ReloadModule = NULL;
static PyCallable_Check_t PyCallable_Check = NULL;
static PyTuple_New_t PyTuple_New = NULL;
static PyTuple_SetItem_t PyTuple_SetItem = NULL;
static PyObject_CallObject_t PyObject_CallObject = NULL;
static PyLong_FromSize_t_t PyLong_FromSize_t = NULL;
static PyLong_FromLong_t PyLong_FromLong = NULL;
static PyObject_CallFunctionObjArgs_t PyObject_CallFunctionObjArgs = NULL;
static PySys_SetObject_t PySys_SetObject = NULL;
static PySys_GetObject_t PySys_GetObject = NULL;
static PyObject_CallMethod_t PyObject_CallMethod = NULL;
static PyUnicode_AsEncodedString_t PyUnicode_AsEncodedString = NULL;
static PyBytes_AsString_t PyBytes_AsString = NULL;
#endif

/* This is a wrapper around a `PyObject *` that mimics the behavior of a
   `std::unique_ptr` with a custom deleter. We'll use this type to simplify the
   freeing of resources. */
class UniquePyObject
{
public:
    UniquePyObject(PyObject *o)
        : m_object(o)
    {}

    ~UniquePyObject()
    {
        if (m_object != NULL)
            Py_DecRef(m_object);
    }

    /* Disallow copy */
    UniquePyObject(const UniquePyObject &other) = delete;
    UniquePyObject &operator=(const UniquePyObject &other) = delete;

    /* Allow moving */
    UniquePyObject &operator=(UniquePyObject &&other)
    {
        if (this != &other)
        {
            /* If the objects are managing different addresses, we're about
                to let go of our current object. Thus, we need to decrement
                its refcount. */
            if (m_object != other.m_object)
                Py_DecRef(m_object);
            m_object = other.m_object;
            other.m_object = NULL;
        }

        return *this;
    }

    PyObject *get() const
    {
        return m_object;
    }

    bool operator==(const PyObject *other) const
    {
        return m_object == other;
    }

    bool operator!=(const PyObject *other) const
    {
        return m_object != other;
    }

private:
    PyObject *m_object;
};

/* A scoped lock that leverages RAII to reduce the complexity in the code below. */
class PyLock
{
public:
    PyLock()
    {
        m_state = PyGILState_Ensure();
    }

    ~PyLock()
    {
        PyGILState_Release(m_state);
    }

private:
    PyGILState_STATE m_state;
};

static bool RedirectStderr()
{
    /* This code redirects Python's stderr to a io.StringIO() object. The code
       below is equivalent to:

         import sys
         from io import StringIO
         sys.stderr = StringIO() */

    try
    {
        UniquePyObject io{PyImport_ImportModule("io")};
        if (io == NULL)
            throw std::runtime_error("");

        UniquePyObject stringio{PyObject_GetAttrString(io.get(), "StringIO")};
        if (stringio == NULL)
            throw std::runtime_error("");

        UniquePyObject instance{PyObject_CallFunctionObjArgs(stringio.get(), NULL)};
        if (instance == NULL)
            throw std::runtime_error("");

        if (PySys_SetObject("stderr", instance.get()) == -1)
            throw std::runtime_error("");

        return true;
    }
    catch (const std::runtime_error &)
    {
        fprintf(stderr, "Failed to redirect stderr from Python.\n");
        return false;
    }
}

static std::string GetErrorString()
{
    /* This code reads from the (presumed) string object associated with
       Python's stderr. The code is equivalent to:

         import sys
         value = sys.stderr.getvalue()
         encoded = value.encode() */

    /* TODO: We can't throw EmbeddedPythonException in here since this function
             is called in the exception constructor. */
    try
    {
        /* It's crucial that we call PyErr_Print() before we proceed. Otherwise
           stderr will be empty. */
        PyErr_Print();

        /* Borrowed reference from PySys_GetObject so no smart object. */
        auto py_stderr = PySys_GetObject("stderr");
        if (py_stderr == NULL)
            throw std::runtime_error("");

        UniquePyObject value{PyObject_CallMethod(py_stderr, "getvalue", NULL)};
        if (value == NULL)
            throw std::runtime_error("");

        UniquePyObject encoded{PyUnicode_AsEncodedString(value.get(), "utf-8", "strict")};
        if (encoded == NULL)
            throw std::runtime_error("");

        const char *str = PyBytes_AsString(encoded.get());
        if (str == NULL)
            throw std::runtime_error("");

        /* Before we leave, set up a clean buffer to receive new error messages. */
        RedirectStderr();

        return std::string{str};
    }
    catch (const std::runtime_error &)
    {
        fprintf(stderr, "Failed to get the last error message.\n");
        return "Failed to construct the error message.";
    }
}

EmbeddedPythonException::EmbeddedPythonException()
    : std::runtime_error(GetErrorString())
{}

/* A base class to use for the Python session on Windows where we rely on
   run-time dynamic linking. */
#if defined(_WIN32)
class PythonSessionRunTimeLinking
{
public:
    PythonSessionRunTimeLinking()
        : m_dll(NULL)
    {
        printf("Constructing PythonSessionRunTimeLinking\n");
        if (!InitializeRunTimeLinking() && m_dll != NULL)
        {
            if (FreeLibrary(m_dll) == 0)
                fprintf(stderr, "Failed to free the run-time linked Python library handle.\n");
            m_dll = NULL;
        }
    }

    ~PythonSessionRunTimeLinking()
    {
        printf("Destroying PythonSessionRunTimeLinking\n");
        if (m_dll != NULL)
        {
            if (FreeLibrary(m_dll) == 0)
                fprintf(stderr, "Failed to free the run-time linked Python library handle.\n");
            m_dll = NULL;
        }
    }

protected:
    bool IsInitialized()
    {
        return m_dll != NULL;
    }

private:
    HMODULE m_dll;

    bool InitializeRunTimeLinking()
    {
        /* FIXME: Search for multiple versions. */
        m_dll = LoadLibraryA("python3.dll");
        if (m_dll == NULL)
            return false;

        Py_Initialize = (Py_Initialize_t)GetProcAddress(m_dll, "Py_Initialize");
        if (Py_Initialize == NULL)
            return false;

        Py_FinalizeEx = (Py_FinalizeEx_t)GetProcAddress(m_dll, "Py_FinalizeEx");
        if (Py_FinalizeEx == NULL)
            return false;

        Py_DecRef = (Py_DecRef_t)GetProcAddress(m_dll, "Py_DecRef");
        if (Py_DecRef == NULL)
            return false;

        PyEval_SaveThread = (PyEval_SaveThread_t)GetProcAddress(m_dll, "PyEval_SaveThread");
        if (PyEval_SaveThread == NULL)
            return false;

        PyEval_RestoreThread = (PyEval_RestoreThread_t)GetProcAddress(m_dll, "PyEval_RestoreThread");
        if (PyEval_RestoreThread == NULL)
            return false;

        PyGILState_Ensure = (PyGILState_Ensure_t)GetProcAddress(m_dll, "PyGILState_Ensure");
        if (PyGILState_Ensure == NULL)
            return false;

        PyImport_ImportModule = (PyImport_ImportModule_t)GetProcAddress(m_dll, "PyImport_ImportModule");
        if (PyImport_ImportModule == NULL)
            return false;

        PyObject_GetAttrString = (PyObject_GetAttrString_t)GetProcAddress(m_dll, "PyObject_GetAttrString");
        if (PyObject_GetAttrString == NULL)
            return false;

        PyUnicode_DecodeFSDefault = (PyUnicode_DecodeFSDefault_t)GetProcAddress(m_dll, "PyUnicode_DecodeFSDefault");
        if (PyUnicode_DecodeFSDefault == NULL)
            return false;

        PyList_Append = (PyList_Append_t)GetProcAddress(m_dll, "PyList_Append");
        if (PyList_Append == NULL)
            return false;

        PyList_Insert = (PyList_Insert_t)GetProcAddress(m_dll, "PyList_Insert");
        if (PyList_Insert == NULL)
            return false;

        PyErr_Print = (PyErr_Print_t)GetProcAddress(m_dll, "PyErr_Print");
        if (PyErr_Print == NULL)
            return false;

        PyGILState_Release = (PyGILState_Release_t)GetProcAddress(m_dll, "PyGILState_Release");
        if (PyGILState_Release == NULL)
            return false;

        PyImport_ReloadModule = (PyImport_ReloadModule_t)GetProcAddress(m_dll, "PyImport_ReloadModule");
        if (PyImport_ReloadModule == NULL)
            return false;

        PyCallable_Check = (PyCallable_Check_t)GetProcAddress(m_dll, "PyCallable_Check");
        if (PyCallable_Check == NULL)
            return false;

        PyTuple_New = (PyTuple_New_t)GetProcAddress(m_dll, "PyTuple_New");
        if (PyTuple_New == NULL)
            return false;

        PyTuple_SetItem = (PyTuple_SetItem_t)GetProcAddress(m_dll, "PyTuple_SetItem");
        if (PyTuple_SetItem == NULL)
            return false;

        PyObject_CallObject = (PyObject_CallObject_t)GetProcAddress(m_dll, "PyObject_CallObject");
        if (PyObject_CallObject == NULL)
            return false;

        PyLong_FromSize_t = (PyLong_FromSize_t_t)GetProcAddress(m_dll, "PyLong_FromSize_t");
        if (PyLong_FromSize_t == NULL)
            return false;

        PyLong_FromLong = (PyLong_FromLong_t)GetProcAddress(m_dll, "PyLong_FromLong");
        if (PyLong_FromLong == NULL)
            return false;

        PyObject_CallFunctionObjArgs = (PyObject_CallFunctionObjArgs_t)GetProcAddress(m_dll, "PyObject_CallFunctionObjArgs");
        if (PyObject_CallFunctionObjArgs == NULL)
            return false;

        PySys_SetObject = (PySys_SetObject_t)GetProcAddress(m_dll, "PySys_SetObject");
        if (PySys_SetObject == NULL)
            return false;

        PySys_GetObject = (PySys_GetObject_t)GetProcAddress(m_dll, "PySys_GetObject");
        if (PySys_GetObject == NULL)
            return false;

        PyObject_CallMethod = (PyObject_CallMethod_t)GetProcAddress(m_dll, "PyObject_CallMethod");
        if (PyObject_CallMethod == NULL)
            return false;

        PyUnicode_AsEncodedString = (PyUnicode_AsEncodedString_t)GetProcAddress(m_dll, "PyUnicode_AsEncodedString");
        if (PyUnicode_AsEncodedString == NULL)
            return false;

        PyBytes_AsString = (PyBytes_AsString_t)GetProcAddress(m_dll, "PyBytes_AsString");
        if (PyBytes_AsString == NULL)
            return false;

        /* FIXME: Remove */
        printf("Successfully completed run-time loading of the Python DLL.\n");
        return true;
    }
};
#endif

/* The Python session object, handling construction and destruction of the
   Python runtime. If we're compiling for Windows, we inherit from a class that
   sets up the run-time dynamic linking for us, though we have to take care not
   to call any undefined function pointers in case the load fails. */
class PythonSession
#if defined(_WIN32)
: public PythonSessionRunTimeLinking
#endif
{
public:
    PythonSession()
    {
        printf("Constructing PythonSession\n");
        /* Initialize the Python session and release the global interpreter lock
           (GIL). We need to remember the state until we run the destructor, at
           which point it's _very_ important that we restore it when acquiring
           the GIL for the last time. */
        if (IsInitialized())
        {
            Py_Initialize();
            RedirectStderr();
            m_state = PyEval_SaveThread();
        }
    }

    ~PythonSession()
    {
        printf("Destroying PythonSession\n");
        if (IsInitialized())
        {
            PyEval_RestoreThread(m_state);
            if (Py_FinalizeEx() != 0)
                fprintf(stderr, "Failed to destroy the Python session.\n");
        }
    }

    inline bool IsInitialized()
    {
#if defined(_WIN32)
        return PythonSessionRunTimeLinking::IsInitialized();
#else
        return true;
#endif
    }

private:
    PyThreadState *m_state;
    bool m_is_initialized;
};

/* The global Python session object. */
static PythonSession PYTHON_SESSION;

bool EmbeddedPython::IsInitialized()
{
    return PYTHON_SESSION.IsInitialized();
}

int EmbeddedPython::AddToPath(const std::string &directory)
{
    if (!IsInitialized())
        return SCAPE_ENOTREADY;

    const PyLock lock{};
    int result = SCAPE_EOK;
    try
    {
        UniquePyObject sys{PyImport_ImportModule("sys")};
        if (sys == NULL)
            throw EmbeddedPythonException();

        UniquePyObject sys_path{PyObject_GetAttrString(sys.get(), "path")};
        if (sys_path == NULL)
            throw EmbeddedPythonException();

        UniquePyObject path{PyUnicode_DecodeFSDefault(directory.c_str())};
        if (path == NULL)
            throw EmbeddedPythonException();

        /* Prepend to the path so that any local modules get found first. This
           is required for the test suite (custom `pyadq`), but could be a bad
           idea in general? */
        if (PyList_Insert(sys_path.get(), 0, path.get()) != 0)
            throw EmbeddedPythonException();
    }
    catch (const EmbeddedPythonException &e)
    {
        printf("Got an error: '%s'\n", e.what());
        result = SCAPE_EEXTERNAL;
    }

    return result;
}

bool EmbeddedPython::HasMain(const std::filesystem::path &path)
{
    if (!IsInitialized())
        return false;

    const PyLock lock{};
    bool result;
    try
    {
        UniquePyObject module{PyImport_ImportModule(path.stem().string().c_str())};
        if (module == NULL)
            throw EmbeddedPythonException();
        module = UniquePyObject{PyImport_ReloadModule(module.get())};

        UniquePyObject function{PyObject_GetAttrString(module.get(), "main")};
        result = function != NULL && PyCallable_Check(function.get()) > 0;
    }
    catch (const EmbeddedPythonException &e)
    {
        printf("Got an error: '%s'\n", e.what());
        result = false;
    }

    return result;
}

static PyObject *LibAdqAsCtypesDll()
{
    UniquePyObject ctypes{PyImport_ImportModule("ctypes")};
    if (ctypes == NULL)
        throw EmbeddedPythonException();

    UniquePyObject cdll{PyObject_GetAttrString(ctypes.get(), "cdll")};
    if (cdll == NULL)
        throw EmbeddedPythonException();

    UniquePyObject constructor{PyObject_GetAttrString(cdll.get(), "LoadLibrary")};
    if (constructor == NULL)
        throw EmbeddedPythonException();

    /* Reference to `value` is stolen by `PyTuple_SetItem`. */
    UniquePyObject args{PyTuple_New(1)};
    if (args == NULL)
        throw EmbeddedPythonException();

    auto value = PyUnicode_DecodeFSDefault(
#if defined(MOCK_ADQAPI_PATH)
        MOCK_ADQAPI_PATH
#elif defined(_WIN32)
        "ADQAPI.dll"
#else
        "libadq.so"
#endif
    );

    if (PyTuple_SetItem(args.get(), 0, value) != 0)
        throw EmbeddedPythonException();

    auto result = PyObject_CallObject(constructor.get(), args.get());
    if (result == NULL)
        throw EmbeddedPythonException();

    return result;
}

static PyObject *AsCtypesPointer(void *handle)
{
    UniquePyObject ctypes{PyImport_ImportModule("ctypes")};
    if (ctypes == NULL)
        throw EmbeddedPythonException();

    UniquePyObject constructor{PyObject_GetAttrString(ctypes.get(), "c_void_p")};
    if (constructor == NULL)
        throw EmbeddedPythonException();

    /* Reference to `value` is stolen by `PyTuple_SetItem`. */
    UniquePyObject args{PyTuple_New(1)};
    if (args == NULL)
        throw EmbeddedPythonException();

    auto value = PyLong_FromSize_t(reinterpret_cast<size_t>(handle));
    if (value == NULL)
        throw EmbeddedPythonException();

    if (PyTuple_SetItem(args.get(), 0, value) != 0)
        throw EmbeddedPythonException();

    auto result = PyObject_CallObject(constructor.get(), args.get());
    if (result == NULL)
        throw EmbeddedPythonException();

    return result;
}

static PyObject *AsPyLong(int index)
{
    auto i = PyLong_FromLong(index);
    if (i == NULL)
        throw EmbeddedPythonException();
    return i;
}

static PyObject *AsPyAdqDevice(void *handle, int index)
{
    UniquePyObject pyadq{PyImport_ImportModule("pyadq")};
    if (pyadq == NULL)
        throw EmbeddedPythonException();

    UniquePyObject constructor{PyObject_GetAttrString(pyadq.get(), "ADQ")};
    if (constructor == NULL)
        throw EmbeddedPythonException();

    /* References are stolen by `PyTuple_SetItem`. */
    UniquePyObject args{PyTuple_New(3)};
    if (args == NULL)
        throw EmbeddedPythonException();

    if (PyTuple_SetItem(args.get(), 0, LibAdqAsCtypesDll()) != 0)
        throw EmbeddedPythonException();

    if (PyTuple_SetItem(args.get(), 1, AsCtypesPointer(handle)) != 0)
        throw EmbeddedPythonException();

    if (PyTuple_SetItem(args.get(), 2, AsPyLong(index)) != 0)
        throw EmbeddedPythonException();

    auto result = PyObject_CallObject(constructor.get(), args.get());
    if (result == NULL)
        throw EmbeddedPythonException();

    return result;
}

int EmbeddedPython::CallMain(const std::string &module_str, void *handle, int index)
{
    if (!IsInitialized())
        return SCAPE_ENOTREADY;

    /* It's important that the lock is released after we've left the scope of
       the try block since the `UniquePyObject` destructors will interact w/ the
       Python session to decrement the reference counts. */
    const PyLock lock{};

    int result = SCAPE_EOK;
    try
    {
        UniquePyObject module{PyImport_ImportModule(module_str.c_str())};
        if (module == NULL)
            throw EmbeddedPythonException();

        /* `PyImport_ImportModule` makes use of any cached version of the
            module, so we always reload the module because its contents could
            have changed between calls. */
        module = UniquePyObject{PyImport_ReloadModule(module.get())};

        UniquePyObject function{PyObject_GetAttrString(module.get(), "main")};
        if (function != NULL && PyCallable_Check(function.get()))
        {
            /* The PyObject reference will be stolen by `PyTuple_SetItem` so we
               should not track these objects as a `UniquePyObject`. */
            UniquePyObject args{PyTuple_New(1)};
            if (args == NULL)
                throw EmbeddedPythonException();

            if (PyTuple_SetItem(args.get(), 0, AsPyAdqDevice(handle, index)) != 0)
                throw EmbeddedPythonException();

            UniquePyObject call_result{PyObject_CallObject(function.get(), args.get())};
            if (call_result == NULL)
                throw EmbeddedPythonException();
        }
        else
        {
            throw EmbeddedPythonException();
        }
    }
    catch (const EmbeddedPythonException &e)
    {
        printf("Got an error: '%s'\n", e.what());
        result = SCAPE_EEXTERNAL;
    }

    return result;
}
