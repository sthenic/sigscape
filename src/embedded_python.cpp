#include "embedded_python.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include "Python.h"
/* FIXME: Remove */
typedef size_t SIZE_T;
typedef ssize_t SSIZE_T;
#endif

#include <stdexcept>


/* Run-time dynamic linking helpers. */
#if defined(_WIN32)
/* FIXME: As void instead of opaque struct definition? */
struct PyObject;
struct PyThreadState;

/* Static function pointers. We'll populate these once we load the library. */
static void (*Py_Initialize)();
static int (*Py_FinalizeEx)();
static void (*Py_DecRef)(PyObject *);
static PyThreadState *(*PyEval_SaveThread)();
static void (*PyEval_RestoreThread)(PyThreadState *);
static int (*PyGILState_Ensure)(); /* FIXME: Actually returns enum, problem? */
static PyObject *(*PyImport_ImportModule)(const char *);
static PyObject *(*PyObject_GetAttrString)(PyObject *, const char *);
static PyObject *(*PyUnicode_DecodeFSDefault)(const char *);
static int (*PyList_Append)(PyObject *, PyObject *);
static void (*PyErr_Print)();
static void (*PyGILState_Release)(int); /* FIXME: Actually enum, problem? */
static PyObject *(*PyImport_ReloadModule)(PyObject *);
static int (*PyCallable_Check)(PyObject *);
static PyObject *(*PyTuple_New)(SSIZE_T); /* FIXME: Actually Py_ssize_t */
static int (*PyTuple_SetItem)(PyObject *, SSIZE_T, PyObject *);
static PyObject *(*PyObject_CallObject)(PyObject *, PyObject *);
static PyObject *(*PyLong_FromSize_t)(SIZE_T);
static PyObject *(*PyLong_FromLong)(long);
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

/* The global Python session object. */
/* FIXME: Two separate versions of this instead? */
static struct PythonSession
{
    PythonSession()
    {
#if defined(_WIN32)
        m_is_initialized = RunTimeLoad();
#else
        m_is_initialized = true;
#endif

        /* Initialize the Python session and release the global interpreter lock
           (GIL). We need to remember the state until we run the destructor, at
           which point it's _very_ important that we restore it when acquiring
           the GIL for the last time. */
        Py_Initialize();
        m_state = PyEval_SaveThread();
    }

    ~PythonSession()
    {
#if defined(_WIN32)
        if (m_dll != NULL)
        {
#endif
            PyEval_RestoreThread(m_state);
            if (Py_FinalizeEx() != 0)
                fprintf(stderr, "Failed to destroy the Python session.\n");

#if defined(_WIN32)
            if (FreeLibrary(m_dll) == 0)
                fprintf(stderr, "Failed to free the run-time linked Python library handle.\n");
        }
#endif
    }

    bool IsInitialized()
    {
        return m_is_initialized;
    }


private:
    bool m_is_initialized;
    PyThreadState *m_state;

#if defined(_WIN32)
    HMODULE m_dll;

    bool RunTimeLoad()
    {
        /* FIXME: Search for multiple versions. */
        m_dll = LoadLibraryA("python3.dll");
        if (m_dll == NULL)
            return false;

        Py_Initialize = GetProcAddress(m_dll, "Py_Initialize");
        if (Py_Initialize == NULL)
            return false;

        Py_FinalizeEx = GetProcAddress(m_dll, "Py_FinalizeEx");
        if (Py_FinalizeEx == NULL)
            return false;

        Py_DecRef = GetProcAddress(m_dll, "Py_DecRef");
        if (Py_DecRef == NULL)
            return false;

        PyEval_SaveThread = GetProcAddress(m_dll, "PyEval_SaveThread");
        if (PyEval_SaveThread == NULL)
            return false;

        PyEval_RestoreThread = GetProcAddress(m_dll, "PyEval_RestoreThread");
        if (PyEval_RestoreThread == NULL)
            return false;

        PyGILState_Ensure = GetProcAddress(m_dll, "PyGILState_Ensure");
        if (PyGILState_Ensure == NULL)
            return false;

        PyImport_ImportModule = GetProcAddress(m_dll, "PyImport_ImportModule");
        if (PyImport_ImportModule == NULL)
            return false;

        PyObject_GetAttrString = GetProcAddress(m_dll, "PyObject_GetAttrString");
        if (PyObject_GetAttrString == NULL)
            return false;

        PyUnicode_DecodeFSDefault = GetProcAddress(m_dll, "PyUnicode_DecodeFSDefault");
        if (PyUnicode_DecodeFSDefault == NULL)
            return false;

        PyList_Append = GetProcAddress(m_dll, "PyList_Append");
        if (PyList_Append == NULL)
            return false;

        PyErr_Print = GetProcAddress(m_dll, "PyErr_Print");
        if (PyErr_Print == NULL)
            return false;

        PyGILState_Release = GetProcAddress(m_dll, "PyGILState_Release");
        if (PyGILState_Release == NULL)
            return false;

        PyImport_ReloadModule = GetProcAddress(m_dll, "PyImport_ReloadModule");
        if (PyImport_ReloadModule == NULL)
            return false;

        PyCallable_Check = GetProcAddress(m_dll, "PyCallable_Check");
        if (PyCallable_Check == NULL)
            return false;

        PyTuple_New = GetProcAddress(m_dll, "PyTuple_New");
        if (PyTuple_New == NULL)
            return false;

        PyTuple_SetItem = GetProcAddress(m_dll, "PyTuple_SetItem");
        if (PyTuple_SetItem == NULL)
            return false;

        PyObject_CallObject = GetProcAddress(m_dll, "PyObject_CallObject");
        if (PyObject_CallObject == NULL)
            return false;

        PyLong_FromSize_t = GetProcAddress(m_dll, "PyLong_FromSize_t");
        if (PyLong_FromSize_t == NULL)
            return false;

        PyLong_FromLong = GetProcAddress(m_dll, "PyLong_FromLong");
        if (PyLong_FromLong == NULL)
            return false;

        /* FIXME: Remove */
        printf("Successfully completed run-time loading of the Python DLL.\n");
        return true;
    }
#endif
} python_session;

/* A private exception type that we use for simplified control flow. */
class EmbeddedPythonException : public std::runtime_error
{
public:
    EmbeddedPythonException() : std::runtime_error("") {};
};

bool EmbeddedPython::IsInitialized()
{
    return python_session.IsInitialized();
}

int EmbeddedPython::AddToPath(const std::string &directory)
{
    auto state = PyGILState_Ensure();
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

        if (PyList_Append(sys_path.get(), path.get()) != 0)
            throw EmbeddedPythonException();
    }
    catch (const EmbeddedPythonException &)
    {
        PyErr_Print();
        result = SCAPE_EEXTERNAL;
    }

    PyGILState_Release(state);
    return result;
}

bool EmbeddedPython::HasMain(const std::filesystem::path &path)
{
    auto state = PyGILState_Ensure();
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
    catch (const EmbeddedPythonException &)
    {
        PyErr_Print();
        result = false;
    }

    PyGILState_Release(state);
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
    auto state = PyGILState_Ensure();
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
    catch (const EmbeddedPythonException &)
    {
        PyErr_Print();
        result = SCAPE_EEXTERNAL;
    }

    /* It's important that the lock is released after we've left the scope of
       the try block since the `UniquePyObject` destructors will interact w/ the
       Python session to decrement the reference counts. */
    PyGILState_Release(state);
    return result;
}
