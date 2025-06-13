/*
* Vulkan Example base class
*
* Copyright (C) 2016-2025 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

std::vector<const char*> VulkanExampleBase::args;

VulkanExampleBase::VulkanExampleBase()
{
    // Command line arguments
    commandLineParser.add("help", { "--help" }, 0, "Show help");
    commandLineParser.add("validation", { "-v", "--validation" }, 0, "Enable validation layers");
    commandLineParser.add("validationlog", { "-vl", "--validationlog" }, 0, "Log validation messages to a textfile (validation.txt)");
    commandLineParser.add("vsync", { "-vs", "--vsync" }, 0, "Enable V-Sync");
    commandLineParser.add("fullscreen", { "-f", "--fullscreen" }, 0, "Start in fullscreen mode");
    commandLineParser.add("width", { "-w", "--width" }, 1, "Set window width");
    commandLineParser.add("height", { "-h", "--height" }, 1, "Set window height");
    commandLineParser.add("shaders", { "-s", "--shaders" }, 1, "Select shader type to use (gls, hlsl or slang)");
    commandLineParser.add("gpuselection", { "-g", "--gpu" }, 1, "Select GPU to run on");
    commandLineParser.add("gpulist", { "-gl", "--listgpus" }, 0, "Display a list of available Vulkan devices");
    commandLineParser.add("benchmark", { "-b", "--benchmark" }, 0, "Run example in benchmark mode");
    commandLineParser.add("benchmarkwarmup", { "-bw", "--benchwarmup" }, 1, "Set warmup time for benchmark mode in seconds");
    commandLineParser.add("benchmarkruntime", { "-br", "--benchruntime" }, 1, "Set duration time for benchmark mode in seconds");
    commandLineParser.add("benchmarkresultfile", { "-bf", "--benchfilename" }, 1, "Set file name for benchmark results");
    commandLineParser.add("benchmarkresultframes", { "-bt", "--benchframetimes" }, 0, "Save frame times to benchmark results file");
    commandLineParser.add("benchmarkframes", { "-bfs", "--benchmarkframes" }, 1, "Only render the given number of frames");

    commandLineParser.parse(args);
    if (commandLineParser.isSet("help")) {
#if defined(_WIN32)
        setupConsole("Vulkan example");
#endif
        commandLineParser.printHelp();
        std::cin.get();
        exit(0);
    }

    if (commandLineParser.isSet("validation")) {
        settings.validation = true;
    }

    if (commandLineParser.isSet("vsync")) {
        settings.vsync = true;
    }

    if (commandLineParser.isSet("width")) {
        width = commandLineParser.getValueAsInt("width", width);
    }

    if (commandLineParser.isSet("height")) {
        height = commandLineParser.getValueAsInt("height", height);
    }

    if (commandLineParser.isSet("fullscreen")) {
        settings.fullscreen = true;
    }

#if defined(_WIN32)
    // Enable console if validation is active, debug message callback will output to it
    if (this->settings.validation) {
        setupConsole("Vulkan Example");
    }
    setupDPIAwareness();
#endif
}

VulkanExampleBase::~VulkanExampleBase()
{
    // Clean up Vulkan resources
    swapchain.cleanup();

    if (renderPass != nullptr) {
        device.destroyRenderPass(renderPass);
    }

    for (auto& framebuffer : frameBuffers) {
        device.destroyFramebuffer(framebuffer);
    }

    device.destroyImageView(depthStencil.view);
    device.destroyImage(depthStencil.image);
    device.freeMemory(depthStencil.memory);

    // synchronization objects
    for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
        device.destroySemaphore(presentCompleteSemaphores[i]);
        device.destroySemaphore(renderCompleteSemaphores[i]);
        device.destroyFence(waitFences[i]);
    }

    delete vulkanDevice;

    instance.destroy();
}

bool VulkanExampleBase::initVulkan()
{
    // Create the instance
    vk::Result result = createInstance();
    if (result != vk::Result::eSuccess) {
        vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(result), result);
        return false;
    }

    // Physical device
    auto physicalDevices = instance.enumeratePhysicalDevices();
    if (physicalDevices.empty()) {
        vks::tools::exitFatal("No device with Vulkan support found", -1);
        return false;
    }

    // GPU selection

    // Select physical device to be used for the Vulkan example
    // Defualts to the first device unless specified by command line
    physicalDevice = physicalDevices[0];

    // Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
    deviceProperties = physicalDevice.getProperties();
    deviceFeatures = physicalDevice.getFeatures();
    deviceMemoryProperties = physicalDevice.getMemoryProperties();

    // Drived examples can override this to set actual features to enable for logical device creation
    getEnabledFeatures();

    // Vulkan device creation
    // This is handled by a separate class that gets a logical device representation
    // and encapsulates functions related to a device
    vulkanDevice = new vks::VulkanDevice(physicalDevice);

    // Derived examples can enable extensions based on the list of supported extensions read from the physical device
    getEnabledExtensions();

    result = vulkanDevice->createLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
    if (result != vk::Result::eSuccess) {
        vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(result), result);
        return false;
    }
    device = vulkanDevice->logicalDevice;

    // Get a graphics queue from the device
    queue = device.getQueue(vulkanDevice->queueFamilyIndices.graphics, 0);

    // Find a suitable depth and/or stencil format
    vk::Bool32 validFormat{ false };

    // Samples that make use of stencil will require a depth + stencil format, so we select from a different list
    if (requiresStencil) {
        validFormat = vks::tools::getSupportedDepthStencilFormat(physicalDevice, &depthFormat);
    }
    else {
        validFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
    }
    assert(validFormat);

    swapchain.setContext(instance, physicalDevice, device);
 
    return true;
}

void VulkanExampleBase::prepare()
{
    createSurface();
    createSwapchain();
    createCommandBuffers();
    createSynchronizationPrimitives();
    createPipelineCache();
    setupDepthStencil();
    setupRenderPass();
    setupFrameBuffer();
}

void VulkanExampleBase::renderLoop()
{
    timer.onRender();

    destWidth = width;
    destHeight = height;

#if defined(_WIN32)
    MSG msg;
    bool quitMessageReceived = false;
    while (!quitMessageReceived) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                quitMessageReceived = true;
                break;
            }
        }
        if (prepared && !IsIconic(window)) {
            nextFrame();
        }
    }
#endif

    // Flush device to make sure all resources can be freed
    if (device != nullptr) {
        device.waitIdle();
    }
}

vk::Result VulkanExampleBase::createInstance()
{
    std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

    // Enable surface extensions depending on os
#if defined(_WIN32)
    instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

    // Get extensions supported by the instance and store for later use
    auto extensions = vk::enumerateInstanceExtensionProperties();
    for (auto& extension : extensions)
    {
        supportedInstanceExtensions.push_back(extension.extensionName);
    }

    // Enabled requested instance extensions
    if (!enabledInstanceExtensions.empty())
    {
        for (const char* enabledExtension : enabledInstanceExtensions)
        {
            // Output message if requested extension is not available
            if (std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), enabledExtension) == supportedInstanceExtensions.end())
            {
                std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
            }
            instanceExtensions.push_back(enabledExtension);
        }
    }

    // Shaders generated by Slang require a certain SPIR-V environment that can't be satisfied by Vulkan 1.0, so we need to expliclity up that to at least 1.1 and enable some required extensions
    if (shaderDir == "slang") {
        if (apiVersion < VK_API_VERSION_1_1) {
            apiVersion = VK_API_VERSION_1_1;
        }
        enabledDeviceExtensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
        enabledDeviceExtensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    }

    vk::ApplicationInfo appInfo{ name.c_str(), {}, name.c_str(), {}, apiVersion };
    vk::InstanceCreateInfo instanceCreateInfo{};
    instanceCreateInfo.pApplicationInfo = &appInfo;

    // Enable the debug utils extension if available (e.g. when debugging tools are present)
    if (settings.validation || std::find(supportedInstanceExtensions.begin(), supportedInstanceExtensions.end(), VK_EXT_DEBUG_UTILS_EXTENSION_NAME) != supportedInstanceExtensions.end())
    {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    if (!instanceExtensions.empty()) {
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }

    // The VK_LAYER_KHRONOS_validation layer contains all current validation functionality
    // Note that on Android this layer requires at least NDK r20
    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    if (settings.validation)
    {
        // Check if this layer is available at instance level
        auto instanceLayerProperties = vk::enumerateInstanceLayerProperties();
        bool validationLayerPresent = false;
        for (auto& layer : instanceLayerProperties) {
            if (strcmp(layer.layerName, validationLayerName) == 0) {
                validationLayerPresent = true;
                break;
            }
        }

        if (validationLayerPresent) {
            instanceCreateInfo.enabledLayerCount = 1;
            instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
        }
        else {
            std::cerr << "Validation layer VK_LAYER_KHRONOS_validation not present, validation is disabled";
        }
    }

    // If layer settings are defined, then activate the sample's required layer settings during instance creation.
    // Layer settings are typically used to activate specific features of a layer, such as the Validation Layer's
    // printf feature, or to configure specific capabilities of drivers such as MoltenVK on macOS and/or iOS.
    vk::LayerSettingsCreateInfoEXT layerSettingsCreateInfo{};
    if (enabledLayerSettings.size() > 0) {
        layerSettingsCreateInfo.settingCount = static_cast<uint32_t>(enabledLayerSettings.size());
        layerSettingsCreateInfo.pSettings = enabledLayerSettings.data();
        layerSettingsCreateInfo.pNext = instanceCreateInfo.pNext;
        instanceCreateInfo.pNext = &layerSettingsCreateInfo;
    }

    vk::Result result = vk::createInstance(&instanceCreateInfo, nullptr, &instance);
    return result;
}

void VulkanExampleBase::setupDepthStencil()
{
    vk::ImageCreateInfo imageCI{};
    imageCI.imageType   = vk::ImageType::e2D;
    imageCI.format      = depthFormat;
    imageCI.extent      = vk::Extent3D(width, height, 1);
    imageCI.mipLevels   = 1;
    imageCI.arrayLayers = 1;
    imageCI.samples     = vk::SampleCountFlagBits::e1;
    imageCI.tiling      = vk::ImageTiling::eOptimal;
    imageCI.usage       = vk::ImageUsageFlagBits::eDepthStencilAttachment;
    VK_CHECK_RESULT(device.createImage(&imageCI, nullptr, &depthStencil.image));

    auto memReqs = device.getImageMemoryRequirements(depthStencil.image);
    vk::MemoryAllocateInfo memAlloc{};
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    VK_CHECK_RESULT(device.allocateMemory(&memAlloc, nullptr, &depthStencil.memory));
    device.bindImageMemory(depthStencil.image, depthStencil.memory, 0);

    vk::ImageViewCreateInfo imageViewCI{};
    imageViewCI.viewType                        = vk::ImageViewType::e2D;
    imageViewCI.image                           = depthStencil.image;
    imageViewCI.format                          = depthFormat;
    imageViewCI.subresourceRange.baseMipLevel   = 0;
    imageViewCI.subresourceRange.levelCount     = 1;
    imageViewCI.subresourceRange.baseArrayLayer = 0;
    imageViewCI.subresourceRange.layerCount     = 1;
    imageViewCI.subresourceRange.aspectMask     = vk::ImageAspectFlagBits::eDepth;
    // Stencil aspect should only be set on depth + stencil formats
    if (requiresStencil) {
        imageViewCI.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
    }
    VK_CHECK_RESULT(device.createImageView(&imageViewCI, nullptr, &depthStencil.view));
}

void VulkanExampleBase::setupFrameBuffer()
{
    // Create frame buffers for every swap chain image
    frameBuffers.resize(swapchain.images.size());
    for (uint32_t i = 0; i < frameBuffers.size(); ++i)
    {
        const vk::ImageView attachments[2] = {
            swapchain.imageViews[i],
            // Depth/Stencil attachment is the same for all frame buffers
            depthStencil.view
        };

        vk::FramebufferCreateInfo frameBufferCreateInfo{};
        frameBufferCreateInfo.renderPass      = renderPass;
        frameBufferCreateInfo.attachmentCount = 2;
        frameBufferCreateInfo.pAttachments    = attachments;
        frameBufferCreateInfo.width           = width;
        frameBufferCreateInfo.height          = height;
        frameBufferCreateInfo.layers          = 1;
        VK_CHECK_RESULT(device.createFramebuffer(&frameBufferCreateInfo, nullptr, &frameBuffers[i]));
    }
}

void VulkanExampleBase::setupRenderPass()
{
    std::array<vk::AttachmentDescription, 2> attachments = {};

    // Color attachment
    attachments[0].format         = swapchain.colorFormat;
    attachments[0].samples        = vk::SampleCountFlagBits::e1;
    attachments[0].loadOp         = vk::AttachmentLoadOp::eClear;
    attachments[0].storeOp        = vk::AttachmentStoreOp::eStore;
    attachments[0].stencilLoadOp  = vk::AttachmentLoadOp::eDontCare;
    attachments[0].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[0].initialLayout  = vk::ImageLayout::eUndefined;
    attachments[0].finalLayout    = vk::ImageLayout::ePresentSrcKHR;

    // Depth attachment
    attachments[1].format         = depthFormat;
    attachments[1].samples        = vk::SampleCountFlagBits::e1;
    attachments[1].loadOp         = vk::AttachmentLoadOp::eClear;
    attachments[1].storeOp        = vk::AttachmentStoreOp::eStore;
    attachments[1].stencilLoadOp  = vk::AttachmentLoadOp::eClear;
    attachments[1].stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    attachments[1].initialLayout  = vk::ImageLayout::eUndefined;
    attachments[1].finalLayout    = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    // Color attachment reference
    vk::AttachmentReference colorReference = {};
    colorReference.attachment     = 0;
    colorReference.layout         = vk::ImageLayout::eColorAttachmentOptimal;

    // Depth attachment reference
    vk::AttachmentReference depthReference = {};
    depthReference.attachment     = 1;
    depthReference.layout         = vk::ImageLayout::eDepthStencilAttachmentOptimal;

    // Subpass
    vk::SubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint       = vk::PipelineBindPoint::eGraphics;
    subpassDescription.colorAttachmentCount    = 1;
    subpassDescription.pColorAttachments       = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;
    subpassDescription.inputAttachmentCount    = 0;
    subpassDescription.pInputAttachments       = nullptr;
    subpassDescription.preserveAttachmentCount = 0;
    subpassDescription.pPreserveAttachments    = nullptr;
    subpassDescription.pResolveAttachments     = nullptr;

    // Subpass dependencies for layout transitions
    std::array<vk::SubpassDependency, 2> dependencies = {};

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
    dependencies[0].dstStageMask    = vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
    dependencies[0].srcAccessMask   = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
    dependencies[0].dstAccessMask   = vk::AccessFlagBits::eDepthStencilAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentRead;
    dependencies[0].dependencyFlags = {};

    dependencies[1].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].dstSubpass      = 0;
    dependencies[1].srcStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].dstStageMask    = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependencies[1].srcAccessMask   = {};
    dependencies[1].dstAccessMask   = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eColorAttachmentRead;
    dependencies[1].dependencyFlags = {};

    // Render Pass
    vk::RenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    VK_CHECK_RESULT(device.createRenderPass(&renderPassInfo, nullptr, &renderPass));
}

void VulkanExampleBase::windowResize()
{
    if (!prepared)
    {
        return;
    }

    prepared = false;
    resized = true;

    // Ensure all operations on the device have been finished before destroying resources
    device.waitIdle();

    // Recreate swap chain
    width = destWidth;
    height = destHeight;
    createSwapchain();

    // Recreate the frame buffers
    device.destroyImageView(depthStencil.view);
    device.destroyImage(depthStencil.image);
    device.freeMemory(depthStencil.memory);
    setupDepthStencil();

    for (auto& frameBuffer : frameBuffers) {
        device.destroyFramebuffer(frameBuffer);
    }
    setupFrameBuffer();

    // TODO

    // Command buffers need to be recreated as they may store
    // references to the recreated frame buffer
    destroyCommandBuffers();
    createCommandBuffers();
    buildCommandBuffers();

    // SRS - Recreate fences in case number of swapchain images has changed on resize
    for (auto& fence : waitFences) {
        device.destroyFence(fence);
    }
    createSynchronizationPrimitives();

    device.waitIdle();

    if (width > 0.0f && height > 0.0f) {
        camera.updateAspectRatio((float)width / height);
    }

    windowResized();

    prepared = true;
}

#if defined(_WIN32)
void VulkanExampleBase::setupConsole(std::string title)
{
    AllocConsole();
    AttachConsole(GetCurrentProcessId());
    FILE* stream;
    freopen_s(&stream, "CONIN$", "r", stdin);
    freopen_s(&stream, "CONOUT$", "w+", stdout);
    freopen_s(&stream, "CONOUT$", "w+", stderr);
    // Enable flags so we can color the output
    HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(consoleHandle, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(consoleHandle, dwMode);
    SetConsoleTitle(TEXT(title.c_str()));
}

void VulkanExampleBase::setupDPIAwareness()
{
    typedef HRESULT* (__stdcall* SetProcessDpiAwarenessFunc)(PROCESS_DPI_AWARENESS);

    HMODULE shCore = LoadLibraryA("Shcore.dll");
    if (shCore)
    {
        SetProcessDpiAwarenessFunc setProcessDpiAwareness =
            (SetProcessDpiAwarenessFunc)GetProcAddress(shCore, "SetProcessDpiAwareness");

        if (setProcessDpiAwareness != nullptr)
        {
            setProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        }

        FreeLibrary(shCore);
    }
}

HWND VulkanExampleBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
    this->windowInstance = hinstance;

    WNDCLASSEX wndClass{};

    wndClass.cbSize = sizeof(WNDCLASSEX);
    wndClass.style = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc = wndproc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hinstance;
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = name.c_str();
    wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

    if (!RegisterClassEx(&wndClass))
    {
        std::cout << "Could not register window class!\n";
        fflush(stdout);
        exit(1);
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    if (settings.fullscreen)
    {
        if ((width != (uint32_t)screenWidth) && (height != (uint32_t)screenHeight))
        {
            DEVMODE dmScreenSettings;
            memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
            dmScreenSettings.dmSize       = sizeof(dmScreenSettings);
            dmScreenSettings.dmPelsWidth  = width;
            dmScreenSettings.dmPelsHeight = height;
            dmScreenSettings.dmBitsPerPel = 32;
            dmScreenSettings.dmFields     = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
            if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
            {
                if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
                {
                    settings.fullscreen = false;
                }
                else
                {
                    return nullptr;
                }
            }
            screenWidth = width;
            screenHeight = height;
        }
    }

    DWORD dwExStyle;
    DWORD dwStyle;

    if (settings.fullscreen)
    {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }
    else
    {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
    }

    RECT windowRect = {
        0L,
        0L,
        settings.fullscreen ? (long)screenWidth : (long)width,
        settings.fullscreen ? (long)screenHeight : (long)height
    };

    AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

    std::string windowTitle = getWindowTitle();
    window = CreateWindowEx(
        0,
        name.c_str(),
        windowTitle.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0,
        0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        NULL,
        NULL,
        hinstance,
        NULL
    );

    if (!window)
    {
        std::cerr << "Could not create window!\n";
        fflush(stdout);
        return nullptr;
    }

    if (!settings.fullscreen)
    {
        // Center on screen
        uint32_t x = (GetSystemMetrics(SM_CXSCREEN) - windowRect.right) / 2;
        uint32_t y = (GetSystemMetrics(SM_CYSCREEN) - windowRect.bottom) / 2;
        SetWindowPos(window, 0, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
    }

    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    SetFocus(window);

    return window;
}

void VulkanExampleBase::handleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
        prepared = false;
        DestroyWindow(hWnd);
        PostQuitMessage(0);
        break;
    case WM_PAINT:
        ValidateRect(window, NULL);
        break;
    case WM_KEYDOWN:
        switch (wParam)
        {
        case KEY_P:
            timer.onKeyP();
            break;
        case KEY_F1:
            // TODO
            break;
        case KEY_F2:
            if (camera.type == Camera::CameraType::lookat) {
                camera.type = Camera::CameraType::firstperson;
            }
            else {
                camera.type = Camera::CameraType::lookat;
            }
            break;
        case KEY_ESCAPE:
            PostQuitMessage(0);
            break;
        }

        if (camera.type == Camera::CameraType::firstperson)
        {
            switch (wParam)
            {
            case KEY_W:
                camera.keys.up = true;
                break;
            case KEY_S:
                camera.keys.down = true;
                break;
            case KEY_A:
                camera.keys.left = true;
                break;
            case KEY_D:
                camera.keys.right = true;
                break;
            }
        }

        keyPressed((uint32_t)wParam);
        break;
    case WM_KEYUP:
        if (camera.type == Camera::CameraType::firstperson)
        {
            switch (wParam)
            {
            case KEY_W:
                camera.keys.up = false;
                break;
            case KEY_S:
                camera.keys.down = false;
                break;
            case KEY_A:
                camera.keys.left = false;
                break;
            case KEY_D:
                camera.keys.right = false;
                break;
            }
        }
        break;
    case WM_LBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.left = true;
        break;
    case WM_RBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.right = true;
        break;
    case WM_MBUTTONDOWN:
        mouseState.position = glm::vec2((float)LOWORD(lParam), (float)HIWORD(lParam));
        mouseState.buttons.middle = true;
        break;
    case WM_LBUTTONUP:
        mouseState.buttons.left = false;
        break;
    case WM_RBUTTONUP:
        mouseState.buttons.right = false;
        break;
    case WM_MBUTTONUP:
        mouseState.buttons.middle = false;
        break;
    case WM_MOUSEWHEEL:
    {
        short wheelDelta = GET_WHEEL_DELTA_WPARAM(wParam);
        camera.translate(glm::vec3(0.0f, 0.0f, (float)wheelDelta * 0.005f));
        viewUpdated = true;
    }
        break;
    case WM_MOUSEMOVE:
        handleMouseMove(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_SIZE:
        if (prepared && (wParam != SIZE_MINIMIZED))
        {
            if (resizing || (wParam == SIZE_MAXIMIZED) || (wParam == SIZE_RESTORED))
            {
                destWidth = LOWORD(lParam);
                destHeight = HIWORD(lParam);
                windowResize();
            }
        }
        break;
    case WM_GETMINMAXINFO:
    {
        LPMINMAXINFO minMaxInfo = (LPMINMAXINFO)lParam;
        minMaxInfo->ptMinTrackSize.x = 64;
        minMaxInfo->ptMinTrackSize.y = 64;
        break;
    }
    case WM_ENTERSIZEMOVE:
        resizing = true;
        break;
    case WM_EXITSIZEMOVE:
        resizing = false;
        break;
    }

    OnHandleMessage(hWnd, uMsg, wParam, lParam);
}
#endif

void VulkanExampleBase::createSurface()
{
#if defined(_WIN32)
    swapchain.initSurface(windowInstance, window);
#endif
}

void VulkanExampleBase::createSwapchain()
{
    swapchain.create(width, height, settings.vsync, settings.fullscreen);
}

void VulkanExampleBase::createCommandBuffers()
{
    vk::CommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.commandPool = vulkanDevice->commandPool;
    cmdBufAllocateInfo.level = vk::CommandBufferLevel::ePrimary;
    cmdBufAllocateInfo.commandBufferCount = MAX_CONCURRENT_FRAMES;
    VK_CHECK_RESULT(device.allocateCommandBuffers(&cmdBufAllocateInfo, commandBuffers.data()));
}

void VulkanExampleBase::destroyCommandBuffers()
{
    device.freeCommandBuffers(vulkanDevice->commandPool, commandBuffers);
}

void VulkanExampleBase::createSynchronizationPrimitives()
{
    for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
        // Semaphores are used for correct command ordering within a queue
        vk::SemaphoreCreateInfo semaphoreCI = {};
        // Semaphore used to ensure that image presentation is complete before starting to submit again
        VK_CHECK_RESULT(device.createSemaphore(&semaphoreCI, nullptr, &presentCompleteSemaphores[i]));
        // Semaphore used to ensure that all commands submitted have been finished before submitting the image to the queue
        VK_CHECK_RESULT(device.createSemaphore(&semaphoreCI, nullptr, &renderCompleteSemaphores[i]));
        // Fence used to ensure that command buffer has completed execution before using it again
        vk::FenceCreateInfo fenceCI = {};
        // Create the fences in signaled state (so we don't wait on first render of each command buffer)
        fenceCI.flags = vk::FenceCreateFlagBits::eSignaled;
        VK_CHECK_RESULT(device.createFence(&fenceCI, nullptr, &waitFences[i]));
    }
}

void VulkanExampleBase::createPipelineCache()
{
    vk::PipelineCacheCreateInfo pipelineCacheCreateInfo = {};
    VK_CHECK_RESULT(device.createPipelineCache(&pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

std::string VulkanExampleBase::getWindowTitle() const
{
    return title;
}

void VulkanExampleBase::handleMouseMove(int32_t x, int32_t y)
{
    int32_t dx = (int32_t)mouseState.position.x - x;
    int32_t dy = (int32_t)mouseState.position.y - y;

    bool handled = false;

    // TODO

    mouseMoved((float)x, (float)y, handled);

    if (handled) {
        mouseState.position = glm::vec2((float)x, (float)y);
        return;
    }

    if (mouseState.buttons.left) {
        camera.rotate(glm::vec3(dy * camera.rotationSpeed, -dx * camera.rotationSpeed, 0.0f));
        viewUpdated = true;
    }
    if (mouseState.buttons.right) {
        camera.translate(glm::vec3(0.0f, 0.0f, dy * 0.005f));
        viewUpdated = true;
    }
    if (mouseState.buttons.middle) {
        camera.translate(glm::vec3(-dx * 0.005f, -dy * 0.005f, 0.0f));
        viewUpdated = true;
    }
    mouseState.position = glm::vec2((float)x, (float)y);
}

void VulkanExampleBase::nextFrame()
{
    timer.onFrameStart();

    if (viewUpdated)
    {
        viewUpdated = false;
    }

    render();

    timer.onFrameStop();

    camera.update(timer.getFrameTime());
    if (camera.moving())
    {
        viewUpdated = true;
    }
}