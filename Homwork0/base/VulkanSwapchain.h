/*
* Class wrapping access to the swap chain
*
* A swap chain is a collection of framebuffers used for rendering and presentation to the windowing system
*
* Copyright (C) 2016-2024 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include <cstdlib>
#include <string>
#include <cassert>
#include <cstdio>
#include <vector>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "VulkanTools.h"

#ifdef __ANDROID__
#include "VulkanAndroid.h"
#endif

#ifdef __APPLE__
#include <sys/utsname.h>
#endif

class VulkanSwapchain
{
private:
    vk::Instance       instance       { nullptr };
    vk::PhysicalDevice physicalDevice { nullptr };
    vk::Device         device         { nullptr };
    vk::SurfaceKHR     surface        { nullptr };

public:
    vk::Format                 colorFormat    {};
    vk::ColorSpaceKHR          colorSpace     {};
    vk::SwapchainKHR           swapchain      { nullptr };
    std::vector<vk::Image>     images         {};
    std::vector<vk::ImageView> imageViews     {};
    uint32_t                   queueNodeIndex { UINT32_MAX };

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    void initSurface(void* platformHandle, void* platformWindow);
#endif

    /* Set the Vulkan objects required for swapchain creation and management, must be called before swapchain creation */
    void setContext(vk::Instance instance, vk::PhysicalDevice physicalDevice, vk::Device device);

    /**
    * Create the swapchain and get its images with given width and height
    *
    * @param width Pointer to the width of the swapchain (may be adjusted to fit the requirements of the swapchain)
    * @param height Pointer to the height of the swapchain (may be adjusted to fit the requirements of the swapchain)
    * @param vsync (Optional, default = false) Can be used to force vsync-ed rendering (by using VK_PRESENT_MODE_FIFO_KHR as presentation mode)
    */
    void create(uint32_t& width, uint32_t& height, bool vsync = false, bool fullscreen = false);

    /**
    * Acquires the next image in the swap chain
    *
    * @param presentCompleteSemaphore (Optional) Semaphore that is signaled when the image is ready for use
    * @param imageIndex Pointer to the image index that will be increased if the next image could be acquired
    *
    * @note The function will always wait until the next image has been acquired by setting timeout to UINT64_MAX
    *
    * @return VkResult of the image acquisition
    */
    vk::Result acquireNextImage(vk::Semaphore presentCompleteSemaphore, uint32_t& imageIndex);

    /**
    * Queue an image for presentation
    *
    * @param queue Presentation queue for presenting the image
    * @param imageIndex Index of the swapchain image to queue for presentation
    * @param waitSemaphore (Optional) Semaphore that is waited on before the image is presented (only used if != VK_NULL_HANDLE)
    *
    * @return VkResult of the queue presentation
    */
    vk::Result queuePresent(vk::Queue queue, uint32_t imageIndex, vk::Semaphore waitSemaphore = nullptr);

    /* Free all Vulkan resources acquired by the swapchain */
    void cleanup();
};