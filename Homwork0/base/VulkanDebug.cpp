/*
 * Vulkan examples debug wrapper
 *
 * Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#include "VulkanDebug.h"
#include <iostream>

namespace vks::debug
{
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
    vk::DebugUtilsMessengerEXT debugUtilsMessenger;

    VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessageCallback(
        vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        vk::DebugUtilsMessageTypeFlagsEXT messageType,
        const vk::DebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData)
    {
        // Select prefix depending on flags passed to the callback
        std::string prefix;

        if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose) {
            prefix = "VERBOSE: ";
#if defined(_WIN32)
            prefix = "\033[32m" + prefix + "\033[0m";
#endif
        }
        else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo) {
            prefix = "INFO: ";
#if defined(_WIN32)
            prefix = "\033[36m" + prefix + "\033[0m";
#endif
        }
        else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) {
            prefix = "WARNING: ";
#if defined(_WIN32)
            prefix = "\033[33m" + prefix + "\033[0m";
#endif
        }
        else if (messageSeverity & vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
            prefix = "ERROR: ";
#if defined(_WIN32)
            prefix = "\033[31m" + prefix + "\033[0m";
#endif
        }


        // Display message to default output (console/logcat)
        std::stringstream debugMessage;
        if (pCallbackData->pMessageIdName) {
            debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "][" << pCallbackData->pMessageIdName << "] : " << pCallbackData->pMessage;
        }
        else {
            debugMessage << prefix << "[" << pCallbackData->messageIdNumber << "] : " << pCallbackData->pMessage;
        }

#if defined(__ANDROID__)
        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
            LOGE("%s", debugMessage.str().c_str());
        }
        else {
            LOGD("%s", debugMessage.str().c_str());
        }
#else
        if (messageSeverity >= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError) {
            std::cerr << debugMessage.str() << "\n\n";
        }
        else {
            std::cout << debugMessage.str() << "\n\n";
        }

        fflush(stdout);
#endif


        // The return value of this callback controls whether the Vulkan call that caused the validation message will be aborted or not
        // We return VK_FALSE as we DON'T want Vulkan calls that cause a validation message to abort
        // If you instead want to have calls abort, pass in VK_TRUE and the function will return VK_ERROR_VALIDATION_FAILED_EXT
        return VK_FALSE;
    }

    void setupDebugingMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& debugUtilsMessengerCI)
    {
        debugUtilsMessengerCI.messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
        debugUtilsMessengerCI.messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
        debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessageCallback;
    }

    void setupDebugging(vk::Instance instance)
    {
        vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(instance.getProcAddr("vkCreateDebugUtilsMessengerEXT"));
        vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(instance.getProcAddr("vkDestroyDebugUtilsMessengerEXT"));

        vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{};
        setupDebugingMessengerCreateInfo(debugUtilsMessengerCI);
        vk::Result result = instance.createDebugUtilsMessengerEXT(&debugUtilsMessengerCI, nullptr, &debugUtilsMessenger);
        assert(result == vk::Result::eSuccess);
    }

    void freeDebugCallback(vk::Instance instance)
    {
        if (debugUtilsMessenger != VK_NULL_HANDLE)
        {
            instance.destroyDebugUtilsMessengerEXT(debugUtilsMessenger, nullptr);
        }
    }
}

namespace vks::debugutils
{
    PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT{ nullptr };
    PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT{ nullptr };
    PFN_vkCmdInsertDebugUtilsLabelEXT vkCmdInsertDebugUtilsLabelEXT{ nullptr };

    void setup(vk::Instance instance)
    {
        vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(instance.getProcAddr("vkCmdBeginDebugUtilsLabelEXT"));
        vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(instance.getProcAddr("vkCmdEndDebugUtilsLabelEXT"));
        vkCmdInsertDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdInsertDebugUtilsLabelEXT>(instance.getProcAddr("vkCmdInsertDebugUtilsLabelEXT"));
    }

    void cmdBeginLabel(vk::CommandBuffer cmdbuffer, std::string caption, glm::vec4 color)
    {
        if (!vkCmdBeginDebugUtilsLabelEXT) {
            return;
        }
        VkDebugUtilsLabelEXT labelInfo{};
        labelInfo.pLabelName = caption.c_str();
        memcpy(labelInfo.color, &color[0], sizeof(float) * 4);
        vkCmdBeginDebugUtilsLabelEXT(cmdbuffer, &labelInfo);
    }

    void cmdEndLabel(vk::CommandBuffer cmdbuffer)
    {
        if (!vkCmdEndDebugUtilsLabelEXT) {
            return;
        }
        vkCmdEndDebugUtilsLabelEXT(cmdbuffer);
    }

};