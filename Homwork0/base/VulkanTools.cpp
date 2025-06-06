/*
 * Assorted commonly used Vulkan helper functions
 *
 * Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanTools.h"

namespace vks::tools
{
    bool errorModeSilent = false;

    std::string errorString(vk::Result errorCode)
    {
        switch (errorCode)
        {
#define STR(r) case vk::Result::e ##r: return #r
            STR(NotReady);
            STR(Timeout);
            STR(EventSet);
            STR(EventReset);
            STR(Incomplete);
            STR(ErrorOutOfHostMemory);
            STR(ErrorOutOfDeviceMemory);
            STR(ErrorInitializationFailed);
            STR(ErrorDeviceLost);
            STR(ErrorMemoryMapFailed);
            STR(ErrorLayerNotPresent);
            STR(ErrorExtensionNotPresent);
            STR(ErrorFeatureNotPresent);
            STR(ErrorIncompatibleDriver);
            STR(ErrorTooManyObjects);
            STR(ErrorFormatNotSupported);
            STR(ErrorSurfaceLostKHR);
            STR(ErrorNativeWindowInUseKHR);
            STR(SuboptimalKHR);
            STR(ErrorOutOfDateKHR);
            STR(ErrorIncompatibleDisplayKHR);
            STR(ErrorValidationFailedEXT);
            STR(ErrorInvalidShaderNV);
            STR(ErrorIncompatibleShaderBinaryEXT);
#undef STR
        default:
            return "UNKNOWN_ERROR";
        }
    }

    void exitFatal(const std::string& message, int32_t exitCode)
    {
#if defined(_WIN32)
        if (!errorModeSilent) {
            MessageBox(NULL, message.c_str(), NULL, MB_OK | MB_ICONERROR);
        }
#endif
        std::cerr << message << "\n";
#if !defined(__ANDROID__)
        exit(exitCode);
#endif
    }

    void exitFatal(const std::string& message, vk::Result resultCode)
    {
        exitFatal(message, (int32_t)resultCode);
    }
}