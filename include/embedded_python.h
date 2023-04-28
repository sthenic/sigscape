#pragma once

#include "error.h"
#include <string>

namespace EmbeddedPython
{
int AddToPath(const std::string &directory);
int CallMain(const std::string &module, int i);
};

