#include "embedded_python.h"

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

    bool operator==(const UniquePyObject &other) const
    {
        return m_object == other.m_object;
    }

    bool operator!=(const UniquePyObject &other) const
    {
        return m_object != other.m_object;
    }

private:
    PyObject *m_object;
};

/* The global Python session object. */
static struct PythonSession
{
    PythonSession()
    {
        Py_Initialize();
    }

    ~PythonSession()
    {
        if (Py_FinalizeEx() != 0)
            fprintf(stderr, "Failed to destroy the Python session.\n");
    }
} python_session;

/* A private exception type that we use for simplified control flow. */
class EmbeddedPythonException : public std::runtime_error
{
public:
    EmbeddedPythonException() : std::runtime_error("") {};
};

int EmbeddedPython::AddToPath(const std::string &directory)
{
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

        PyList_Append(sys_path.get(), path.get());
        return SCAPE_EOK;
    }
    catch (const EmbeddedPythonException &)
    {
        PyErr_Print();
        return SCAPE_EEXTERNAL;
    }
}

int EmbeddedPython::CallMain(const std::string &target_module)
{
    auto state = PyGILState_Ensure();
    try
    {
        UniquePyObject name{PyUnicode_DecodeFSDefault(target_module.c_str())};
        if (name == NULL)
            throw EmbeddedPythonException();

        UniquePyObject module{PyImport_Import(name.get())};
        if (module == NULL)
            throw EmbeddedPythonException();

        /* `PyImport_Import` makes use of any cached version of the module, so
            we always reload the module because its contents could have changed
            between calls. */
        module = UniquePyObject{PyImport_ReloadModule(module.get())};

        UniquePyObject function{PyObject_GetAttrString(module.get(), "main")};
        if (function != NULL && PyCallable_Check(function.get()))
        {
            UniquePyObject result{PyObject_CallObject(function.get(), NULL)};
            if (result == NULL)
                throw EmbeddedPythonException();
        }
        else
        {
            throw EmbeddedPythonException();
        }

        PyRun_SimpleString("from datetime import datetime\n"
                           "print(datetime.now().strftime('%Y-%m-%d %H:%M:%S'))\n");

        PyGILState_Release(state);
        return SCAPE_EOK;
    }
    catch (const EmbeddedPythonException &)
    {
        PyErr_Print();
        PyGILState_Release(state);
        return SCAPE_EEXTERNAL;
    }
}
