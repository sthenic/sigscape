#pragma once

#include <GLFW/glfw3.h>
#include <string>

namespace Screenshot
{
bool Screenshot(GLFWwindow *window, const std::string &filename);
};
