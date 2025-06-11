/*
 * Vulkan device class
 *
 * Encapsulates a physical Vulkan device and its logical representation
 *
 * Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
 *
 * This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
 */

#if (defined(VK_USE_PLATFORM_IOS_MVK) || defined(VK_USE_PLATFORM_MACOS_MVK) || defined(VK_USE_PLATFORM_METAL_EXT))
 // SRS - Enable beta extensions and make VK_KHR_portability_subset visible
#define VK_ENABLE_BETA_EXTENSIONS
#endif
#include "VulkanDevice.h"
#include <unordered_set>

namespace vks
{
    /**
    * Default constructor
    *
    * @param physicalDevice Physical device that is to be used
    */
    VulkanDevice::VulkanDevice(vk::PhysicalDevice physicalDevice)
    {
        assert(physicalDevice);
        this->physicalDevice = physicalDevice;

        // Store Properties features, limits and properties of the physical device for later use
        // Device properties also contain limits and sparse properties
        properties = physicalDevice.getProperties();

        // Features should be checked by the examples before using them
        features = physicalDevice.getFeatures();

        // Memory properties are used regularly for creating all kinds of buffers
        memoryProperties = physicalDevice.getMemoryProperties();

        // Queue family properties, used for setting up requested queues upon device creation
        queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // Get list of supported extensions
        auto extensions = physicalDevice.enumerateDeviceExtensionProperties();
        for (auto& ext : extensions)
        {
            supportedExtensions.push_back(ext.extensionName);
        }
    }

    /**
    * Default destructor
    *
    * @note Frees the logical device
    */
    VulkanDevice::~VulkanDevice()
    {
        if (commandPool)
        {
            logicalDevice.destroyCommandPool(commandPool);
        }

        if (logicalDevice)
        {
            logicalDevice.destroy();
        }
    }

    /**
    * Get the index of a memory type that has all the requested property bits set
    *
    * @param typeBits Bit mask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
    * @param properties Bit mask of properties for the memory type to request
    * @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
    *
    * @return Index of the requested memory type
    *
    * @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
    */
    uint32_t VulkanDevice::getMemoryType(uint32_t typeBits, vk::MemoryPropertyFlags properties, vk::Bool32* memTypeFound) const
    {
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
        {
            if ((typeBits & 1) == 1)
            {
                if ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
                {
                    if (memTypeFound)
                    {
                        *memTypeFound = true;
                    }
                    return i;
                }
            }
            typeBits >>= 1;
        }

        if (memTypeFound)
        {
            *memTypeFound = false;
            return 0;
        }
        else
        {
            throw std::runtime_error("Could not find a matching memory type");
        }
    }

    /**
    * Get the index of a queue family that supports the requested queue flags
    * SRS - support VkQueueFlags parameter for requesting multiple flags vs. VkQueueFlagBits for a single flag only
    *
    * @param queueFlags Queue flags to find a queue family index for
    *
    * @return Index of the queue family index that matches the flags
    *
    * @throw Throws an exception if no queue family index could be found that supports the requested flags
    */
    uint32_t VulkanDevice::getQueueFamilyIndex(vk::QueueFlags queueFlags) const
    {
        // Dedicated queue for compute
        // Try to find a queue family index that supports compute but not graphics
        if ((queueFlags & vk::QueueFlagBits::eCompute) == queueFlags)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i)
            {
                if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute) && !(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics))
                {
                    return i;
                }
            }
        }

        // Dedicated queue for transfer
        // Try to find a queue family index that supports transfer but not graphics and compute
        if ((queueFlags & vk::QueueFlagBits::eTransfer) == queueFlags)
        {
            for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i)
            {
                if ((queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eTransfer) && !(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) && !(queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute))
                {
                    return i;
                }
            }
        }

        // For other queue types or if no separate compute queue is present, return the first one to support the requested flags
        for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); ++i)
        {
            if ((queueFamilyProperties[i].queueFlags & queueFlags) == queueFlags)
            {
                return i;
            }
        }

        throw std::runtime_error("Could not find a matching queue family index");
    }

    /**
    * Create the logical device based on the assigned physical device, also gets default queue family indices
    *
    * @param enabledFeatures Can be used to enable certain features upon device creation
    * @param pNextChain Optional chain of pointer to extension structures
    * @param useSwapChain Set to false for headless rendering to omit the swapchain device extensions
    * @param requestedQueueTypes Bit flags specifying the queue types to be requested from the device
    *
    * @return VkResult of the device creation call
    */
    vk::Result VulkanDevice::createLogicalDevice(vk::PhysicalDeviceFeatures enabledFeatures,
                                                 std::vector<const char*>   enabledExtensions,
                                                 void*                      pNextChain,
                                                 bool                       useSwapChain,
                                                 vk::QueueFlags             requestedQueueTypes)
    {
        // Desired queues need to be requested upon logical device creation
        // Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
        // requests different queue types
        std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos{};

        // Get queue family indices for the requested queue family types
        // Note that the indices may overlap depending on the implementation
        const float defaultQueuePriority(0.0f);

        // Graphis queue
        if (requestedQueueTypes & vk::QueueFlagBits::eGraphics)
        {
            queueFamilyIndices.graphics = getQueueFamilyIndex(vk::QueueFlagBits::eGraphics);

            vk::DeviceQueueCreateInfo queueInfo{};
            queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
            queueInfo.queueCount       = 1;
            queueInfo.pQueuePriorities = &defaultQueuePriority;
            queueCreateInfos.push_back(queueInfo);
        }
        else
        {
            queueFamilyIndices.graphics = 0;
        }

        // Dedicated compute queue
        if (requestedQueueTypes & vk::QueueFlagBits::eCompute)
        {
            queueFamilyIndices.compute = getQueueFamilyIndex(vk::QueueFlagBits::eCompute);
            if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
            {
                // If compute family index differs, we need an additional queue create info for the compute queue
                vk::DeviceQueueCreateInfo queueInfo{};
                queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
                queueInfo.queueCount       = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;
                queueCreateInfos.push_back(queueInfo);
            }
        }
        else
        {
            // Else we use the same queue
            queueFamilyIndices.compute = queueFamilyIndices.graphics;
        }

        // Dedicated transfer queue
        if (requestedQueueTypes & vk::QueueFlagBits::eTransfer)
        {
            queueFamilyIndices.transfer = getQueueFamilyIndex(vk::QueueFlagBits::eTransfer);
            if (queueFamilyIndices.transfer != queueFamilyIndices.graphics && queueFamilyIndices.transfer != queueFamilyIndices.compute)
            {
                // If transfer family index differs, we need an additional queue create info for the transfer queue
                vk::DeviceQueueCreateInfo queueInfo{};
                queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
                queueInfo.queueCount       = 1;
                queueInfo.pQueuePriorities = &defaultQueuePriority;
                queueCreateInfos.push_back(queueInfo);
            }
        }
        else
        {
            // Else we use the same queue
            queueFamilyIndices.transfer = queueFamilyIndices.graphics;
        }

        // Create the logical device representation
        std::vector<const char*> deviceExtensions(enabledExtensions);
        if (useSwapChain)
        {
            // If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
            deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        }

        vk::DeviceCreateInfo deviceCreateInfo = {};
        deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        deviceCreateInfo.pQueueCreateInfos    = queueCreateInfos.data();
        deviceCreateInfo.pEnabledFeatures     = &enabledFeatures;

        // If a pNext(Chain) has been passed, we need to add it to the device create info
        vk::PhysicalDeviceFeatures2 physicalDeviceFeatures2{};
        if (pNextChain)
        {
            physicalDeviceFeatures2.features  = enabledFeatures;
            physicalDeviceFeatures2.pNext     = pNextChain;
            deviceCreateInfo.pEnabledFeatures = nullptr;
            deviceCreateInfo.pNext            = &physicalDeviceFeatures2;
        }

        if (deviceExtensions.size() > 0)
        {
            for (const char* enabledExtension : deviceExtensions)
            {
                if (!extensionSupported(enabledExtension)) {
                    std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
                }
            }

            deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
        }

        this->enabledFeatures = enabledFeatures;

        vk::Result result = physicalDevice.createDevice(&deviceCreateInfo, nullptr, &logicalDevice);
        if (result != vk::Result::eSuccess)
        {
            return result;
        }

        // Create a default command pool for graphics command buffers
        commandPool = createCommandPool(queueFamilyIndices.graphics);

        return result;
    }

    /**
    * Create a command pool for allocation command buffers from
    *
    * @param queueFamilyIndex Family index of the queue to create the command pool for
    * @param createFlags (Optional) Command pool creation flags (Defaults to VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
    *
    * @note Command buffers allocated from the created pool can only be submitted to a queue with the same family index
    *
    * @return A handle to the created command buffer
    */
    vk::CommandPool VulkanDevice::createCommandPool(uint32_t queueFamilyIndex, vk::CommandPoolCreateFlags createFlags)
    {
        vk::CommandPoolCreateInfo cmdPoolInfo = {};
        cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
        cmdPoolInfo.flags            = createFlags;

        vk::CommandPool cmdPool;
        VK_CHECK_RESULT(logicalDevice.createCommandPool(&cmdPoolInfo, nullptr, &cmdPool));
        return cmdPool;
    }

    /**
    * Check if an extension is supported by the (physical device)
    *
    * @param extension Name of the extension to check
    *
    * @return True if the extension is supported (present in the list read at device creation time)
    */
    bool VulkanDevice::extensionSupported(std::string extension)
    {
        return std::find(supportedExtensions.begin(), supportedExtensions.end(), extension) != supportedExtensions.end();
    }
}