/*
 * Vulkan examples debug wrapper
 *
 * Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include <vulkan/vulkan.hpp>
#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace vks::debug
{
    // Load debug function pointers and set debug callback
    void setupDebugging(vk::Instance instance);

    // Clear debug callback
    void freeDebugCallback(vk::Instance instance);

    // Used to populate a vk::DebugUtilMessengerCreateInfoEXT with our example messenger function and desired flags
    void setupDebugingMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCI);
}

// Wrapper for the VK_EXT_debug_utils extension
// These can be used to name Vulkan objects for debugging tools like RenderDoc
namespace vks::debugutils
{
    void setup(vk::Instance instance);
    void cmdBeginLabel(vk::CommandBuffer cmdbuffer, std::string caption, glm::vec4 color);
    void cmdEndLabel(vk::CommandBuffer cmdbuffer);
}