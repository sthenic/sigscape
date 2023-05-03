#include "embedded_python.h"

#include "Python.h"
#include <stdexcept>

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
            Py_DECREF(m_object);
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
                Py_DECREF(m_object);
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
static struct PythonSession
{
    PythonSession()
    {
        /* Initialize the Python session and release the global interpreter lock
           (GIL). We need to remember the state until we run the destructor, at
           which point it's _very_ important that we restore it when acquiring
           the GIL for the last time. */
        Py_Initialize();
        m_state = PyEval_SaveThread();
    }

    ~PythonSession()
    {
        PyEval_RestoreThread(m_state);
        if (Py_FinalizeEx() != 0)
            fprintf(stderr, "Failed to destroy the Python session.\n");
    }

private:
    PyThreadState *m_state;
} python_session;

/* A private exception type that we use for simplified control flow. */
class EmbeddedPythonException : public std::runtime_error
{
public:
    EmbeddedPythonException() : std::runtime_error("") {};
};

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
