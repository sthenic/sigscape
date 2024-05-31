#include "embedded_python.h"
#include "log.h"

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
typedef int PyGILState_STATE;

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
typedef PyObject *(__cdecl *PyLong_FromSize_t_t)(SIZE_T);
typedef PyObject *(__cdecl *PyLong_FromLong_t)(long);
typedef long (__cdecl *PyLong_AsLong_t)(PyObject *);
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
static PyLong_FromSize_t_t PyLong_FromSize_t = NULL;
static PyLong_FromLong_t PyLong_FromLong = NULL;
static PyLong_AsLong_t PyLong_AsLong = NULL;
static PySys_SetObject_t PySys_SetObject = NULL;
static PySys_GetObject_t PySys_GetObject = NULL;
static PyObject_CallMethod_t PyObject_CallMethod = NULL;
static PyUnicode_AsEncodedString_t PyUnicode_AsEncodedString = NULL;
static PyBytes_AsString_t PyBytes_AsString = NULL;

#define INITIALIZE_PROC_ADDRESS(name)                                                              \
    do                                                                                             \
    {                                                                                              \
        name = (name##_t)GetProcAddress(m_dll, #name);                                             \
        if (name == NULL)                                                                          \
            return false;                                                                          \
    } while (0)
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

static bool RedirectStream(const std::string &stream)
{
    /* This code redirects the target stream to a io.StringIO() object.
       The code below is equivalent to (for stderr):

         import sys
         from io import StringIO
         sys.stderr = StringIO() */

    try
    {
        UniquePyObject io{PyImport_ImportModule("io")};
        if (io == NULL)
            throw std::runtime_error("");

        UniquePyObject instance{PyObject_CallMethod(io.get(), "StringIO", NULL)};
        if (instance == NULL)
            throw std::runtime_error("");

        if (PySys_SetObject(stream.c_str(), instance.get()) == -1)
            throw std::runtime_error("");

        return true;
    }
    catch (const std::runtime_error &)
    {
        return false;
    }
}

static std::string GetStringFromStream(const std::string &stream)
{
    /* This code reads from the (presumed) string object associated with
       the target stream. The code is equivalent to (for stderr):

         import sys
         value = sys.stderr.getvalue()
         encoded = value.encode() */

    /* TODO: We can't throw EmbeddedPythonException in here since this function
             is called in that same exception's constructor. */
    try
    {
        /* It's crucial that if we're targeting stderr, we call PyErr_Print()
           before we proceed. Otherwise the string object will be empty. */
        if (stream == "stderr")
            PyErr_Print();

        /* Borrowed reference from PySys_GetObject so no smart object. */
        auto stringio = PySys_GetObject(stream.c_str());
        if (stringio == NULL)
            throw std::runtime_error("");

        UniquePyObject value{PyObject_CallMethod(stringio, "getvalue", NULL)};
        if (value == NULL)
            throw std::runtime_error("");

        UniquePyObject encoded{PyUnicode_AsEncodedString(value.get(), "utf-8", "strict")};
        if (encoded == NULL)
            throw std::runtime_error("");

        const char *str = PyBytes_AsString(encoded.get());
        if (str == NULL)
            throw std::runtime_error("");

        /* Before we leave, set up a clean buffer for the target stream. */
        RedirectStream(stream);

        return std::string{str};
    }
    catch (const std::runtime_error &)
    {
        return "Failed to construct the error message.";
    }
}

EmbeddedPythonException::EmbeddedPythonException()
    : std::runtime_error(GetStringFromStream("stderr"))
{}

EmbeddedPythonException::EmbeddedPythonException(const std::string &str)
    : std::runtime_error(str)
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
        if (!InitializeRunTimeLinking() && m_dll != NULL)
        {
            FreeLibrary(m_dll);
            m_dll = NULL;
        }
    }

    ~PythonSessionRunTimeLinking()
    {
        if (m_dll != NULL)
        {
            FreeLibrary(m_dll);
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
        m_dll = LoadLibraryA("python3.dll");
        if (m_dll == NULL)
            return false;

        INITIALIZE_PROC_ADDRESS(Py_Initialize);
        INITIALIZE_PROC_ADDRESS(Py_FinalizeEx);
        INITIALIZE_PROC_ADDRESS(Py_DecRef);
        INITIALIZE_PROC_ADDRESS(PyEval_SaveThread);
        INITIALIZE_PROC_ADDRESS(PyEval_RestoreThread);
        INITIALIZE_PROC_ADDRESS(PyGILState_Ensure);
        INITIALIZE_PROC_ADDRESS(PyImport_ImportModule);
        INITIALIZE_PROC_ADDRESS(PyObject_GetAttrString);
        INITIALIZE_PROC_ADDRESS(PyUnicode_DecodeFSDefault);
        INITIALIZE_PROC_ADDRESS(PyList_Append);
        INITIALIZE_PROC_ADDRESS(PyList_Insert);
        INITIALIZE_PROC_ADDRESS(PyErr_Print);
        INITIALIZE_PROC_ADDRESS(PyGILState_Release);
        INITIALIZE_PROC_ADDRESS(PyImport_ReloadModule);
        INITIALIZE_PROC_ADDRESS(PyCallable_Check);
        INITIALIZE_PROC_ADDRESS(PyLong_FromSize_t);
        INITIALIZE_PROC_ADDRESS(PyLong_FromLong);
        INITIALIZE_PROC_ADDRESS(PyLong_AsLong);
        INITIALIZE_PROC_ADDRESS(PySys_SetObject);
        INITIALIZE_PROC_ADDRESS(PySys_GetObject);
        INITIALIZE_PROC_ADDRESS(PyObject_CallMethod);
        INITIALIZE_PROC_ADDRESS(PyUnicode_AsEncodedString);
        INITIALIZE_PROC_ADDRESS(PyBytes_AsString);

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
        /* Initialize the Python session and release the global interpreter lock
           (GIL). We need to remember the state until we run the destructor, at
           which point it's _very_ important that we restore it when acquiring
           the GIL for the last time. */
        if (IsInitialized())
        {
            Py_Initialize();
            RedirectStream("stderr");
            m_state = PyEval_SaveThread();
        }
    }

    ~PythonSession()
    {
        if (IsInitialized())
        {
            PyEval_RestoreThread(m_state);
            Py_FinalizeEx();
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

void EmbeddedPython::AddToPath(const std::string &directory)
{
    if (!IsInitialized())
        throw EmbeddedPythonException("Not initialized");

    const PyLock lock{};

    /* Borrowed reference from `PySys_GetObject`. */
    auto sys_path = PySys_GetObject("path");
    if (sys_path == NULL)
        throw EmbeddedPythonException();

    UniquePyObject path{PyUnicode_DecodeFSDefault(directory.c_str())};
    if (path == NULL)
        throw EmbeddedPythonException();

    /* Prepend to the path so that any local modules get found first. */
    /* TODO: This is required for the test suite (custom `pyadq`), but could be
       a bad idea in general? */
    if (PyList_Insert(sys_path, 0, path.get()) != 0)
        throw EmbeddedPythonException();
}

bool EmbeddedPython::HasMain(const std::filesystem::path &path)
{
    if (!IsInitialized())
        return false;

    const PyLock lock{};
    try
    {
        UniquePyObject module{PyImport_ImportModule(path.stem().string().c_str())};
        if (module == NULL)
            throw EmbeddedPythonException();

        module = UniquePyObject{PyImport_ReloadModule(module.get())};
        if (module == NULL)
            throw EmbeddedPythonException();

        UniquePyObject function{PyObject_GetAttrString(module.get(), "main")};
        return function != NULL && PyCallable_Check(function.get()) > 0;
    }
    catch (const EmbeddedPythonException &)
    {
        return false;
    }
}

static PyObject *LibAdqAsCtypesDll()
{
    UniquePyObject ctypes{PyImport_ImportModule("ctypes")};
    if (ctypes == NULL)
        throw EmbeddedPythonException();

    UniquePyObject cdll{PyObject_GetAttrString(ctypes.get(), "cdll")};
    if (cdll == NULL)
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

    auto result = PyObject_CallMethod(cdll.get(), "LoadLibrary", "(N)", value);
    if (result == NULL)
        throw EmbeddedPythonException();

    return result;
}

static PyObject *AsCtypesPointer(void *handle)
{
    UniquePyObject ctypes{PyImport_ImportModule("ctypes")};
    if (ctypes == NULL)
        throw EmbeddedPythonException();

    auto result = PyObject_CallMethod(ctypes.get(), "c_void_p", "(N)",
                                      PyLong_FromSize_t(reinterpret_cast<size_t>(handle)));
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

    /* Call the `pyadq.ADQ` constructor to create an unmanaged PyObject. The `N`
       format specifier steals references so we have to make sure that and all
       the function calls return unmanaged PyObjects. */
    auto result = PyObject_CallMethod(pyadq.get(), "ADQ", "(N,N,N)", LibAdqAsCtypesDll(),
                                      AsCtypesPointer(handle), AsPyLong(index));
    if (result == NULL)
        throw EmbeddedPythonException();

    return result;
}

void EmbeddedPython::CallMain(const std::string &module, void *handle, int index, std::string &out)
{
    if (!IsInitialized())
        throw EmbeddedPythonException("Not initialized");

    /* It's important that the lock is released after we've left the scope of
       the try block since the `UniquePyObject` destructors will interact w/ the
       Python session to decrement the reference counts. */
    const PyLock lock{};

    /* Redirect stdout to a fresh string object. */
    RedirectStream("stdout");

    UniquePyObject mod{PyImport_ImportModule(module.c_str())};
    if (mod == NULL)
        throw EmbeddedPythonException();

    /* `PyImport_ImportModule` makes use of any cached version of the
        module, so we always reload the module because its contents could
        have changed between calls. */
    mod = UniquePyObject{PyImport_ReloadModule(mod.get())};
    if (mod == NULL)
        throw EmbeddedPythonException();

    /* The `N` format specifier steals a reference which is why we pass an unmanaged PyObject. */
    UniquePyObject result{PyObject_CallMethod(mod.get(), "main", "(N)", AsPyAdqDevice(handle, index))};
    if (result == NULL)
        throw EmbeddedPythonException();

    /* Right now, you only get stdout if the script completes successfully. */
    out = GetStringFromStream("stdout");
}

bool EmbeddedPython::IsPyadqCompatible()
{
    if (!IsInitialized())
        return false;

    const PyLock lock{};

    UniquePyObject pyadq{PyImport_ImportModule("pyadq")};
    if (pyadq == NULL)
        throw EmbeddedPythonException();

    UniquePyObject libadq{LibAdqAsCtypesDll()};
    if (libadq == NULL)
        throw EmbeddedPythonException();

    /* Load libadq with ctypes and call ADQAPI_ValidateVersion with the version
       numbers defined by the pyadq library. The `N` format specifier steals a
       reference which is why we pass unmanaged PyObjects. */
    UniquePyObject result{PyObject_CallMethod(
        libadq.get(), "ADQAPI_ValidateVersion", "(N,N)",
        PyObject_GetAttrString(pyadq.get(), "ADQAPI_VERSION_MAJOR"),
        PyObject_GetAttrString(pyadq.get(), "ADQAPI_VERSION_MINOR"))};

    if (result == NULL)
        throw EmbeddedPythonException();

    /* PyLong_AsLong returns -1 if the PyObject cannot be converted. We're only
       looking for a successful conversion and the value zero so we don't care
       to disambiguate. */
    return PyLong_AsLong(result.get()) == 0;
}
