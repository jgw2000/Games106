/*
* Class wrapping access to the swap chain
*
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanSwapchain.h"

#if defined(VK_USE_PLATFORM_WIN32_KHR)
void VulkanSwapchain::initSurface(void* platformHandle, void* platformWindow)
{
    // Create the os-specific surface
    vk::Win32SurfaceCreateInfoKHR createInfo{};
    createInfo.hinstance = (HINSTANCE)platformHandle;
    createInfo.hwnd = (HWND)platformWindow;
    vk::Result result = instance.createWin32SurfaceKHR(&createInfo, nullptr, &surface);

    if (result != vk::Result::eSuccess) {
        vks::tools::exitFatal("Could not create surface!", result);
    }

    // Get available queue family properties
    auto queueProps = physicalDevice.getQueueFamilyProperties();

    // Iterate over each queue to learn whether it supports presenting:
    // Find a queue with present support
    // Will be used to present the swap chain images to the windowing system
    uint32_t queueCount = static_cast<uint32_t>(queueProps.size());
    std::vector<vk::Bool32> supportsPresent(queueCount);
    for (uint32_t i = 0; i < queueCount; ++i)
    {
        physicalDevice.getSurfaceSupportKHR(i, surface, &supportsPresent[i]);
    }

    // Search for a graphics and a present queue in the array of queue
    // families, try to find one that supports both
    uint32_t graphicsQueueNodeIndex = UINT32_MAX;
    uint32_t presentQueueNodeIndex = UINT32_MAX;
    for (uint32_t i = 0; i < queueCount; ++i)
    {
        if (queueProps[i].queueFlags & vk::QueueFlagBits::eGraphics)
        {
            if (graphicsQueueNodeIndex == UINT32_MAX)
            {
                graphicsQueueNodeIndex = i;
            }

            if (supportsPresent[i] == vk::True)
            {
                graphicsQueueNodeIndex = i;
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    if (presentQueueNodeIndex == UINT32_MAX)
    {
        // If there's no queue that supports both present and graphics
        // try to find a separate present queue
        for (uint32_t i = 0; i < queueCount; ++i)
        {
            if (supportsPresent[i] == vk::True)
            {
                presentQueueNodeIndex = i;
                break;
            }
        }
    }

    // Exit if either a graphics or a presenting queue hasn't been found
    if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
    {
        vks::tools::exitFatal("Could not find a graphics and/or presenting queue!", -1);
    }

    if (graphicsQueueNodeIndex != presentQueueNodeIndex)
    {
        vks::tools::exitFatal("Separate graphics and presenting queues are not supported yet!", -1);
    }

    queueNodeIndex = graphicsQueueNodeIndex;

    // Get list of supported surface formats
    auto surfaceFormats = physicalDevice.getSurfaceFormatsKHR(surface);

    // We want to get a format that best suits our needs, so we try to get one from a set of preferred formats
    // Initialize the format to the first one returned by the implementation in case we can't find one of the preffered formats
    vk::SurfaceFormatKHR selectedFormat = surfaceFormats[0];
    std::vector<vk::Format> preferredImageFormats = {
        vk::Format::eB8G8R8A8Unorm,
        vk::Format::eR8G8B8A8Unorm,
        vk::Format::eA8B8G8R8UnormPack32
    };

    for (auto& availableFormat : surfaceFormats) {
        if (std::find(preferredImageFormats.begin(), preferredImageFormats.end(), availableFormat.format) != preferredImageFormats.end()) {
            selectedFormat = availableFormat;
            break;
        }
    }

    colorFormat = selectedFormat.format;
    colorSpace = selectedFormat.colorSpace;
}
#endif

void VulkanSwapchain::setContext(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device)
{
    this->instance = instance;
    this->physicalDevice = physicalDevice;
    this->device = device;
}

void VulkanSwapchain::create(uint32_t& width, uint32_t& height, bool vsync, bool fullscreen)
{
    assert(physicalDevice);
    assert(device);
    assert(instance);

    // Store the current swap chain handle so we can use it later on to ease up recreation
    vk::SwapchainKHR oldSwapchain = swapchain;

    // Get physical device surface properties and formats
    vk::SurfaceCapabilitiesKHR surfCaps;
    VK_CHECK_RESULT(physicalDevice.getSurfaceCapabilitiesKHR(surface, &surfCaps));

    vk::Extent2D swapchainExtent = {};
    // If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
    if (surfCaps.currentExtent.width == (uint32_t)-1)
    {
        // If the surface size is undefined, the size is set to the size of the images requested
        swapchainExtent.width = width;
        swapchainExtent.height = height;
    }
    else
    {
        // If the surface size is defined, the swap chain size must match
        swapchainExtent = surfCaps.currentExtent;
        width = surfCaps.currentExtent.width;
        height = surfCaps.currentExtent.height;
    }

    // Select a present mode for the swapchain
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    // The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
    // This mode waits for the vertical blank ("v-sync")
    vk::PresentModeKHR swapchainPresentMode = vk::PresentModeKHR::eFifo;

    // If v-sync is not requested, try to find a mailbox mode
    // It's the lowest latency non-tearing present mode available
    if (!vsync)
    {
        for (size_t i = 0; i < presentModes.size(); ++i)
        {
            if (presentModes[i] == vk::PresentModeKHR::eMailbox)
            {
                swapchainPresentMode = vk::PresentModeKHR::eMailbox;
                break;
            }
            if (presentModes[i] == vk::PresentModeKHR::eImmediate)
            {
                swapchainPresentMode = vk::PresentModeKHR::eImmediate;
            }
        }
    }

    // Determine the number of images
    uint32_t desiredNumberOfSwapchainImages = surfCaps.minImageCount + 1;
    if (surfCaps.maxImageCount > 0 && desiredNumberOfSwapchainImages > surfCaps.maxImageCount)
    {
        desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
    }

    // Find the transformation of the surface
    vk::SurfaceTransformFlagBitsKHR preTransform;
    if (surfCaps.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity)
    {
        // We prefer a non-rotated transform
        preTransform = vk::SurfaceTransformFlagBitsKHR::eIdentity;
    }
    else
    {
        preTransform = surfCaps.currentTransform;
    }

    // Find a supported composite alpha format (not all devices support alpha opaque)
    vk::CompositeAlphaFlagBitsKHR compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;

    // Simply select the first composite alpha format available
    std::vector<vk::CompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
        vk::CompositeAlphaFlagBitsKHR::eOpaque,
        vk::CompositeAlphaFlagBitsKHR::ePreMultiplied,
        vk::CompositeAlphaFlagBitsKHR::ePostMultiplied,
        vk::CompositeAlphaFlagBitsKHR::eInherit
    };

    for (auto& compositeAlphaFlag : compositeAlphaFlags) {
        if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
            compositeAlpha = compositeAlphaFlag;
            break;
        }
    }

    vk::SwapchainCreateInfoKHR swapchainCI = {};
    swapchainCI.surface               = surface;
    swapchainCI.minImageCount         = desiredNumberOfSwapchainImages;
    swapchainCI.imageFormat           = colorFormat;
    swapchainCI.imageColorSpace       = colorSpace;
    swapchainCI.imageExtent           = swapchainExtent;
    swapchainCI.imageUsage            = vk::ImageUsageFlagBits::eColorAttachment;
    swapchainCI.preTransform          = preTransform;
    swapchainCI.imageArrayLayers      = 1;
    swapchainCI.imageSharingMode      = vk::SharingMode::eExclusive;
    swapchainCI.queueFamilyIndexCount = 0;
    swapchainCI.presentMode = swapchainPresentMode;
    // Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
    swapchainCI.oldSwapchain          = oldSwapchain;
    // Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
    swapchainCI.clipped               = vk::True;
    swapchainCI.compositeAlpha        = compositeAlpha;

    // Enable transfer source on swap chain images if supported
    if (surfCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferSrc) {
        swapchainCI.imageUsage |= vk::ImageUsageFlagBits::eTransferSrc;
    }

    // Enable transfer destination on swap chain images if supported
    if (surfCaps.supportedUsageFlags & vk::ImageUsageFlagBits::eTransferDst) {
        swapchainCI.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
    }

    VK_CHECK_RESULT(device.createSwapchainKHR(&swapchainCI, nullptr, &swapchain));

    // If an existing swap chain is re-created, destroy the old swap chain and the ressources owned by the application (image views, images are owned by the swap chain)
    if (oldSwapchain != nullptr) {
        for (auto i = 0; i < images.size(); ++i) {
            device.destroyImageView(imageViews[i]);
        }
        device.destroySwapchainKHR(oldSwapchain);
    }

    // Get the swap chain images
    images = device.getSwapchainImagesKHR(swapchain);

    // Get the swap chain buffers containing the image and imageview
    uint32_t imageCount = static_cast<uint32_t>(images.size());
    imageViews.resize(imageCount);
    for (auto i = 0; i < imageCount; ++i)
    {
        vk::ImageViewCreateInfo colorAttachmentView = {};
        colorAttachmentView.format = colorFormat;
        colorAttachmentView.components = {
            vk::ComponentSwizzle::eR,
            vk::ComponentSwizzle::eG,
            vk::ComponentSwizzle::eB,
            vk::ComponentSwizzle::eA
        };
        colorAttachmentView.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        colorAttachmentView.subresourceRange.baseMipLevel = 0;
        colorAttachmentView.subresourceRange.levelCount = 1;
        colorAttachmentView.subresourceRange.baseArrayLayer = 0;
        colorAttachmentView.subresourceRange.layerCount = 1;
        colorAttachmentView.viewType = vk::ImageViewType::e2D;
        colorAttachmentView.image = images[i];
        VK_CHECK_RESULT(device.createImageView(&colorAttachmentView, nullptr, &imageViews[i]));
    }
}

void VulkanSwapchain::cleanup()
{
    if (swapchain != nullptr) {
        for (auto i = 0; i < images.size(); ++i) {
            device.destroyImageView(imageViews[i]);
        }
        device.destroySwapchainKHR(swapchain);
    }

    if (surface != nullptr) {
        instance.destroySurfaceKHR(surface);
    }

    surface = nullptr;
    swapchain = nullptr;
}