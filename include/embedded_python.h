#pragma once

#include "error.h"
#include <string>
#include <filesystem>
#include <stdexcept>

class EmbeddedPythonException : public std::runtime_error
{
public:
    EmbeddedPythonException();
    EmbeddedPythonException(const std::string &str)
        : std::runtime_error(str)
    {}
};

namespace EmbeddedPython
{
/* Test if the embedded Python session has been loaded correctly. This function
   needs to return `true` for any of the other other functions in this namespace
   to perform their intended tasks. Otherwise, they will immediately
   return/throw an error. */
bool IsInitialized();

/* Prepend the `directory` to the `sys.path` object. `SCAPE_EOK` is returned on
   success and an `EmbeddedPythonException` is raised on any error. */
void AddToPath(const std::filesystem::path &directory);

/* Test if the module (read as the _stem_) at `path` has a `main()` function.
   Returns false on any errors. */
bool HasMain(const std::filesystem::path &path);

/* Call the `main()` function of the `module` and pass a `pyadq.ADQ` object
   constructed from the `handle` and `index` as the sole argument. An
   `EmbeddedPythonException` is raised on any error. */
void CallMain(const std::string &module, void *handle, int index, std::string &out);

/* Check if the `pyadq` library is present and compatible. */
bool IsPyadqCompatible();
};

