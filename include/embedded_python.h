#pragma once

#include "error.h"

#include "Python.h"
#include <string>

namespace EmbeddedPython
{
int AddToPath(const std::string &directory);
int CallMain(const std::string &module);
};

