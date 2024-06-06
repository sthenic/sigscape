#pragma once

#include <GLFW/glfw3.h>
#include <filesystem>

namespace Screenshot
{
bool Screenshot(GLFWwindow *window, const std::filesystem::path &filename);
};
