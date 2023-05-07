#pragma once

#include "error.h"
#include <string>
#include <filesystem>
#include <stdexcept>

class EmbeddedPythonException : public std::runtime_error
{
public:
    EmbeddedPythonException();
};

namespace EmbeddedPython
{
bool IsInitialized();
int AddToPath(const std::string &directory);
bool HasMain(const std::filesystem::path &path);
int CallMain(const std::string &module_str, void *handle, int index);
};

