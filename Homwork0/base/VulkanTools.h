/*
 * Assorted Vulkan helper functions
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
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
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

 // Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

// Macro to check and display Vulkan return results
#if defined(__ANDROID__)
#define VK_CHECK_RESULT(f)                                                                               \
{                                                                                                        \
    VkResult res = (f);                                                                                  \
    if (res != VK_SUCCESS)                                                                               \
    {                                                                                                    \
        LOGE("Fatal : VkResult is \" %s \" in %s at line %d", vks::tools::errorString(res).c_str(), __FILE__, __LINE__); \
        assert(res == VK_SUCCESS);                                                                       \
    }                                                                                                    \
}
#else
#define VK_CHECK_RESULT(f)                                                                               \
{                                                                                                        \
    vk::Result res = (f);                                                                                \
    if (res != vk::Result::eSuccess)                                                                     \
    {                                                                                                    \
        std::cout << "Fatal : vk::Result is \"" << vks::tools::errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << "\n"; \
        assert(res == vk::Result::eSuccess);                                                             \
    }                                                                                                    \
}
#endif

namespace vks::tools
{
    /** @brief Disable message boxed on fatal errors */
    extern bool errorModeSilent;

    /** @brief Returns an error code as a string */
    std::string errorString(vk::Result errorCode);

    // Display error message and exit on fatal error
    void exitFatal(const std::string& message, int32_t exitCode);
    void exitFatal(const std::string& message, vk::Result resultCode);
}