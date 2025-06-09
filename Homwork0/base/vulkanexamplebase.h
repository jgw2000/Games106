/*
* Vulkan Example base class
*
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <ShellScalingAPI.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <vector>
#include <array>
#include <unordered_map>
#include <numeric>
#include <ctime>
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <numeric>
#include <array>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>

#include "keycodes.h"
#include "CommandLineParser.h"
#include "VulkanTools.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"

class VulkanExampleBase
{
public:
    /** @brief Default base class constructor */
    VulkanExampleBase();
    virtual ~VulkanExampleBase();

    /** @brief Setup the vulkan instance, enable required extensions and connect to the physical device (GPU) */
    bool initVulkan();

#if defined(_WIN32)
    void setupConsole(std::string title);
    void setupDPIAwareness();
    HWND setupWindow(HINSTANCE hinstance, WNDPROC wndproc);
    void handleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif

    /** @brief (Virtual) Creates the application wide Vulkan instance */
    virtual vk::Result createInstance();

    /** @brief (Pure virtual) Render function to be implemented by the sample application */
    virtual void render() {};

    /** @brief (Virtual) Called after a key was pressed, can be used to do custom key handling */
    virtual void keyPressed(uint32_t) {}

    /** @brief (Virtual) Called after the mouse cursor moved and before internal events (like camera rotation) is handled */
    virtual void mouseMoved(double x, double y, bool& handled) {}

    /** @brief (Virtual) Called when the window has been resized, can be used by the sample application to recreate resources */
    virtual void windowResized() {}

    /** @brief (Virtual) Called when resources have been recreated that require a rebuild of the command buffers (e.g. frame buffer), to be implemented by the sample application */
    virtual void buildCommandBuffers() {}

    /** @brief (Virtual) Setup default depth and stencil views */
    virtual void setupDepthStencil();

    /** @brief (Virtual) Setup default framebuffers for all requested swapchain images */
    virtual void setupFrameBuffer() {}

    /** @brief (Virtual) Setup a default renderpass */
    virtual void setupRenderPass() {}

    /** @brief (Virtual) Called after the physical device features have been read, can be used to set features to enable on the device */
    virtual void getEnabledFeatures() {}

    /** @brief (Virtual) Called after the physical device extensions have been read, can be used to enable extensions based on the supported extension listing*/
    virtual void getEnabledExtensions() {}

    /** @brief Prepares all Vulkan resources and functions required to run the sample */
    virtual void prepare();

    void windowResize();

    /** @brief Entry point for the main render loop */
    void renderLoop();

    /** Prepare the next frame for workload submission by acquiring the next swap chain image */
    void prepareFrame() {}

    /** @brief Presents the current image to the swap chain */
    void submitFrame() {}

    /** @brief (Virtual) Default image acquire + submission and command buffer submission function */
    virtual void renderFrame() {}

#if defined(_WIN32)
    virtual void OnHandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {}
#endif

public:
    static std::vector<const char*> args;

    CommandLineParser commandLineParser;

    /** @brief Encapsulated physical and logical vulkan device */
    vks::VulkanDevice* vulkanDevice{};

    /** @brief Example settings that can be changed e.g. by command line arguments */
    struct Settings {
        /** @brief Activates validation layers (and message output) when set to true */
        bool validation = false;
        /** @brief Set to true if fullscreen mode has been requested via command line */
        bool fullscreen = false;
        /** @brief Set to true if v-sync will be forced for the swapchain */
        bool vsync = false;
        /** @brief Enable UI overlay */
        bool overlay = true;
    } settings;

    /** @brief State of mouse/touch input */
    struct {
        struct {
            bool left = false;
            bool right = false;
            bool middle = false;
        } buttons;
        glm::vec2 position;
    } mouseState;

    bool prepared = false;
    bool resized = false;
    bool viewUpdated = false;
    uint32_t width = 1280;
    uint32_t height = 720;

    /** @brief Last frame time measured using a high performance timer (if available) */
    float frameTimer = 1.0f;

    // Defines a frame rate independent timer value clamped from -1.0...1.0
    // For use in animations, rotations, etc.
    float timer = 0.0f;

    // Multiplier for speeding up (or slowing down) the global timer
    float timerSpeed = 0.25f;

    bool paused = false;

    /** @brief Default depth stencil attachment used by the default render pass */
    struct {
        vk::Image image;
        vk::DeviceMemory memory;
        vk::ImageView view;
    } depthStencil{};

    // OS specific
#if defined(_WIN32)
    HWND window;
    HINSTANCE windowInstance;
#endif

    std::string title = "Vulkan Example";
    std::string name = "vulkanExample";
    uint32_t apiVersion = VK_API_VERSION_1_0;

protected:
    // Frame counter to display fps
    uint32_t frameCounter = 0;
    uint32_t lastFPS = 0;
    std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp, tPrevEnd;

    // Vulkan instance, stores all per-application states
    vk::Instance instance{ nullptr };
    std::vector<std::string> supportedInstanceExtensions;

    // Physical device (GPU) that Vulkan will use
    vk::PhysicalDevice physicalDevice{ nullptr };

    /** @brief Logical device, application's view of the physical device (GPU) */
    vk::Device device{ nullptr };

    // Handle to the device graphics queue that command buffers are submitted to
    vk::Queue queue{ nullptr };

    // Depth buffer format (selected during Vulkan initialization)
    vk::Format depthFormat{ vk::Format::eUndefined };

    // Stores physical device properties (for e.g. checking device limits)
    vk::PhysicalDeviceProperties deviceProperties{};

    // Stores the features available on the selected physical device (for e.g. checking if a feature is available)
    vk::PhysicalDeviceFeatures deviceFeatures{};

    // Stores all available memory (type) properties for the physical device
    vk::PhysicalDeviceMemoryProperties deviceMemoryProperties{};

    /** @brief Set of instance extensions to be enabled for this example (must be set in the derived constructor) */
    std::vector<const char*> enabledInstanceExtensions;

    /** @brief Set of device extensions to be enabled for this example (must be set in the derived constructor) */
    std::vector<const char*> enabledDeviceExtensions;

    /** @brief Set of physical device features to be enabled for this example (must be set in the derived constructor) */
    vk::PhysicalDeviceFeatures enabledFeatures{};

    /** @brief Set of layer settings to be enabled for this example (must be set in the derived constructor) */
    std::vector<vk::LayerSettingEXT> enabledLayerSettings;

    // Wraps the swap chain to present images (framebuffers) to the windowing system
    VulkanSwapchain swapchain;

    // Command buffer pool
    vk::CommandPool cmdPool{ nullptr };

    // Command buffers used for rendering
    std::vector<vk::CommandBuffer> drawCmdBuffers;

    // Synchronization semaphores
    struct {
        // Swap chain image presentation
        vk::Semaphore presentComplete;
        // Command buffer submission and execution
        vk::Semaphore renderComplete;
    } semaphores{};
    std::vector<vk::Fence> waitFences;

    /** @brief Optional pNext structure for passing extension structures to device creation */
    void* deviceCreatepNextChain = nullptr;

    bool requiresStencil{ false };

private:
    void createSurface();
    void createSwapchain();
    void createCommandPool();
    void createCommandBuffers();
    void createSynchronizationPrimitives();

    std::string getWindowTitle() const;
    void handleMouseMove(int32_t x, int32_t y);
    void nextFrame();

    bool resizing = false;
    uint32_t destWidth{};
    uint32_t destHeight{};

    std::string shaderDir = "glsl";
};