/*
 * Vulkan device class
 *
 * Encapsulates a physical Vulkan device and its logical representation
 *
 * Copyright (C) 2016-2023 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "VulkanTools.h"

#include <vulkan/vulkan.hpp>
#include <algorithm>
#include <cassert>
#include <exception>

namespace vks
{
    struct VulkanDevice
    {
        /** @brief Physical device representation */
        vk::PhysicalDevice physicalDevice;

        /** @brief Logical device representation (application's view of the device) */
        vk::Device logicalDevice;

        /** @brief Properties of the physical device including limits that the application can check against */
        vk::PhysicalDeviceProperties properties;

        /** @brief Features of the physical device that an application can use to check if a feature is supported */
        vk::PhysicalDeviceFeatures features;

        /** @brief Features that have been enabled for use on the physical device */
        vk::PhysicalDeviceFeatures enabledFeatures;

        /** @brief Memory types and heaps of the physical device */
        vk::PhysicalDeviceMemoryProperties memoryProperties;

        /** @brief Queue family properties of the physical device */
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties;

        /** @brief List of extensions supported by the device */
        std::vector<std::string> supportedExtensions;

        /** @brief Default command pool for the graphics queue family index */
        vk::CommandPool commandPool = VK_NULL_HANDLE;

        /** @brief Contains queue family indices */
        struct
        {
            uint32_t graphics;
            uint32_t compute;
            uint32_t transfer;
        } queueFamilyIndices;

        operator VkDevice() const
        {
            return logicalDevice;
        };

        explicit VulkanDevice(vk::PhysicalDevice physicalDevice);
        ~VulkanDevice();

        uint32_t getMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties, vk::Bool32* memTypeFound = nullptr) const;
        uint32_t getQueueFamilyIndex(vk::QueueFlags queueFlags) const;

        vk::Result createLogicalDevice(vk::PhysicalDeviceFeatures enabledFeatures,
                                       std::vector<const char*>   enabledExtensions,
                                       void*                      pNextChain,
                                       bool                       useSwapChain        = true,
                                       vk::QueueFlags             requestedQueueTypes = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute);

        vk::CommandPool createCommandPool(uint32_t                   queueFamilyIndex,
                                          vk::CommandPoolCreateFlags createFlags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
       
        bool extensionSupported(std::string extension);
    };
}