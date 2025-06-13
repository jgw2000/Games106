#include "triangle.h"

VulkanTriangle::VulkanTriangle() : VulkanExampleBase()
{
    title = "Vulkan Example - Basic indexed triangle";

    // Setup a default look-at camera
    camera.type = Camera::CameraType::lookat;
    camera.setPosition(glm::vec3(0.0f, 0.0f, -2.5f));
    camera.setRotation(glm::vec3(0.0f));
    camera.setPerspective(60.0f, (float)width / (float)height, 1.0f, 256.0f);
}

VulkanTriangle::~VulkanTriangle()
{
    // Clean up used vulkan resources
    // Note: Inherited destructor cleans up resources stored in base class
    if (device) {
        device.destroyPipeline(pipeline);
        device.destroyPipelineLayout(pipelineLayout);
        device.destroyDescriptorPool(descriptorPool);
        device.destroyDescriptorSetLayout(descriptorSetLayout);
        device.destroyBuffer(vertexBuffer.handle);
        device.freeMemory(vertexBuffer.memory);
        device.destroyBuffer(indexBuffer.handle);
        device.freeMemory(indexBuffer.memory);

        for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
            device.destroyBuffer(uniformBuffers[i].handle);
            device.freeMemory(uniformBuffers[i].memory);
        }
    }
}

void VulkanTriangle::prepare()
{
    VulkanExampleBase::prepare();
    createVertexBuffer();
    createUniformBuffers();
    createDescriptors();
    createPipeline();
    prepared = true;
}

void VulkanTriangle::render()
{
    // Use a fence to wait until the command buffer has finished execution before using it again
    VK_CHECK_RESULT(device.waitForFences(1, &waitFences[currentFrame], vk::True, UINT64_MAX));
    VK_CHECK_RESULT(device.resetFences(1, &waitFences[currentFrame]));

    // Get the next swap chain image from the implementation
    // Note that the implementation is free to return the images in any order, so we must use the acquire function and can't just cycle through the images/imageIndex on our own
    uint32_t imageIndex;
    vk::Result result = device.acquireNextImageKHR(swapchain.swapchain, UINT64_MAX, presentCompleteSemaphores[currentFrame], nullptr, &imageIndex);
    if (result == vk::Result::eErrorOutOfDateKHR) {
        windowResize();
        return;
    }
    else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw "Could not acquire the next swap chain iamge!";
    }

    // Update the uniform buffer for the next frame
    ShaderData shaderData{};
    shaderData.modelMatrix = glm::mat4(1.0f);
    shaderData.viewMatrix = camera.matrices.view;
    shaderData.projectionMatrix = camera.matrices.perspective;
    
    // Copy the current matrices to the current frame's uniform buffer. As we requested a host coherent memory type for the uniform buffer, the write is instantly visible to the GPU
    memcpy(uniformBuffers[currentFrame].mapped, &shaderData, sizeof(ShaderData));

    // Build the command buffer for the next frame to render
    commandBuffers[currentFrame].reset();
    vk::CommandBufferBeginInfo cmdBufInfo = {};
    const vk::CommandBuffer commandBuffer = commandBuffers[currentFrame];
    VK_CHECK_RESULT(commandBuffer.begin(&cmdBufInfo));

    // Set clear values for all framebuffer attachments with loadOp set to clear
    // We use two attachments (color and depth) that are cleared at the start of the subpass and as such we need to set clear values for both
    vk::ClearValue clearValues[2]{};
    clearValues[0].color = { 0.0f, 0.0f, 0.2f, 1.0f };
    clearValues[1].depthStencil = { {1.0f, 0} };

    vk::RenderPassBeginInfo renderPassBeginInfo = {};
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = width;
    renderPassBeginInfo.renderArea.extent.height = height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.framebuffer = frameBuffers[imageIndex];

    // Start the first sub pass specified in our default render pass setup b the base class
    // This will clear the color and depth attachment
    commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);

    // Update dynamic viewport state
    vk::Viewport viewport = {};
    viewport.width = (float)width;
    viewport.height = (float)height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    commandBuffer.setViewport(0, 1, &viewport);

    // Update dynamic scissor state
    vk::Rect2D scissor = {};
    scissor.extent.width = width;
    scissor.extent.height = height;
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    commandBuffer.setScissor(0, 1, &scissor);

    // Bind descriptor set for the current frame's uniform buffer, so the shader uses the data from that buffer for this draw
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, 1, &uniformBuffers[currentFrame].descriptorSet, 0, nullptr);

    // Bind the rendering pipeline
    // The pipeline (state object) contains all states of the rendering pipeline, binding it will set all the states specified at pipeline creation time
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);

    // Bind triangle vertex buffer (contains position and colors)
    vk::DeviceSize offsets[1]{ 0 };
    commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer.handle, offsets);

    // Bind triangle index buffer
    commandBuffer.bindIndexBuffer(indexBuffer.handle, 0, vk::IndexType::eUint32);

    // Draw indexed triangle
    commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);

    commandBuffer.endRenderPass();

    // Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
    // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system
    commandBuffer.end();

    // Submit the command buffer to the graphics queue

    // Pipeline stage at which the queue submission will wait
    vk::PipelineStageFlags waitStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    // The submit info structure specifies a command buffer queue submission batch
    vk::SubmitInfo submitInfo = {};
    submitInfo.pWaitDstStageMask = &waitStageMask;      // Pointer to the list of pipeline stages that the semaphore waits will occur at
    submitInfo.pCommandBuffers = &commandBuffer;        // Command buffer(s) to execute in this batch (submission)
    submitInfo.commandBufferCount = 1;

    // Semaphore to wait upon before the submitted command buffer starts executing
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &presentCompleteSemaphores[currentFrame];
    
    // Semaphore to be signaled when command buffers have completed
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderCompleteSemaphores[currentFrame];

    // Submit to the graphics queue passing a wait fence
    VK_CHECK_RESULT(queue.submit(1, &submitInfo, waitFences[currentFrame]));

    // Present the current frame buffer to the swap chain
    // Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
    // This ensures that the image is not presented to the windowing system until all commands have been submitted
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderCompleteSemaphores[currentFrame];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain.swapchain;
    presentInfo.pImageIndices = &imageIndex;
    result = queue.presentKHR(presentInfo);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
        windowResize();
    }
    else if (result != vk::Result::eSuccess) {
        throw "Could not present the image to the swap chain!";
    }

    // Select the next frame to render to, based on the max. no. of concurrent frames
    currentFrame = (currentFrame + 1) % MAX_CONCURRENT_FRAMES;
}

void VulkanTriangle::buildCommandBuffers()
{

}

void VulkanTriangle::getEnabledFeatures()
{
    // Vulkan 1.3 device support is required for this example
    if (deviceProperties.apiVersion < VK_API_VERSION_1_3) {
        vks::tools::exitFatal("Selected GPU does not support Vulkan 1.3", vk::Result::eErrorIncompatibleDriver);
    }
}

// Prepare vertex and index buffers for an indexed triangle
// Also uploads them to device local memory using staging and initializes vertex input and attribute binding to match the vertex shader
void VulkanTriangle::createVertexBuffer()
{
    // A note on memory management in Vulkan in general:
    // This is a complex topic and while it's fine for an example application to small individual memory allocations that is not
    // what should be done in a real-world application, where you should allocate large chunks of memory at once instead

    // Setup vertices
    const std::vector<Vertex> vertices{
        { {  1.0f,  1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
        { { -1.0f,  1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
        { {  0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } }
    };
    uint32_t vertexBufferSize = static_cast<uint32_t>(vertices.size()) * sizeof(Vertex);

    // Setup indices
    // We do this for demonstration purpose, a triangle doesn't require indices to be rendered, but more complex shapes usually make use of indices
    std::vector<uint32_t> indices{ 0,1,2 };
    indexCount = static_cast<uint32_t>(indices.size());
    uint32_t indexBufferSize = indexCount * sizeof(uint32_t);

    // Static data like vertex and index buffer should be stored on the device memory for optimal (and fastest) access by the GPU
    //
    // To achieve this we use so-called "staging buffers" :
    // - Create a buffer that's visible to the host (and can be mapped)
    // - Copy the data to this buffer
    // - Create another buffer that's local on the device (VRAM) with the same size
    // - Copy the data from the host to the device using a command buffer
    // - Delete the host visible (staging) buffer
    // - Use the device local buffers for rendering
    //
    // Note: On unified memory architectures where host (CPU) and GPU share the same memory, staging is not necessary
    // To keep this sample easy to follow, there is no check for that in place

    // Create the host visible staging buffer that we copy vertices and indices too, and from which we copy to the device
    VulkanBuffer stagingBuffer;
    vk::BufferCreateInfo stagingBufferCI = {};
    stagingBufferCI.size = vertexBufferSize + indexBufferSize;
    stagingBufferCI.usage = vk::BufferUsageFlagBits::eTransferSrc;
    // Create a host-visible buffer to copy the vertex data to (staging buffer)
    VK_CHECK_RESULT(device.createBuffer(&stagingBufferCI, nullptr, &stagingBuffer.handle));

    // Allocate memory
    vk::MemoryAllocateInfo memAlloc = {};
    vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(stagingBuffer.handle);
    memAlloc.allocationSize = memReqs.size;
    // Request a host visible memory tpe that can be used to copy our data to
    // Also request it to be coherent, so that writes are visible to the GPU right after unmapping the buffer
    memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
    VK_CHECK_RESULT(device.allocateMemory(&memAlloc, nullptr, &stagingBuffer.memory));
    device.bindBufferMemory(stagingBuffer.handle, stagingBuffer.memory, 0);

    // Map the buffer and copy vertices and indices into it, this way we can use a single buffer as the source for both vertex and index GPU buffers
    uint8_t* data{ nullptr };
    VK_CHECK_RESULT(device.mapMemory(stagingBuffer.memory, 0, memAlloc.allocationSize, {}, (void**)&data));
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(((char*)data) + vertexBufferSize, indices.data(), indexBufferSize);

    // Create a device local buffer to which the (host local) vertex data will be copied and which will be used for rendering
    vk::BufferCreateInfo vertexBufferCI = {};
    vertexBufferCI.size = vertexBufferSize;
    vertexBufferCI.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    VK_CHECK_RESULT(device.createBuffer(&vertexBufferCI, nullptr, &vertexBuffer.handle));
    memReqs = device.getBufferMemoryRequirements(vertexBuffer.handle);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    VK_CHECK_RESULT(device.allocateMemory(&memAlloc, nullptr, &vertexBuffer.memory));
    device.bindBufferMemory(vertexBuffer.handle, vertexBuffer.memory, 0);

    // Create a device local buffer to which the (host local) index data will be copied and which will be used for rendering
    vk::BufferCreateInfo indexBufferCI = {};
    indexBufferCI.size = indexBufferSize;
    indexBufferCI.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst;
    VK_CHECK_RESULT(device.createBuffer(&indexBufferCI, nullptr, &indexBuffer.handle));
    memReqs = device.getBufferMemoryRequirements(indexBuffer.handle);
    memAlloc.allocationSize = memReqs.size;
    memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eDeviceLocal);
    VK_CHECK_RESULT(device.allocateMemory(&memAlloc, nullptr, &indexBuffer.memory));
    device.bindBufferMemory(indexBuffer.handle, indexBuffer.memory, 0);

    // Buffer copies have to be submitted to a queue, so we need a command buffer for them
    vk::CommandBuffer copyCmd;

    vk::CommandBufferAllocateInfo cmdBufAllocateInfo = {};
    cmdBufAllocateInfo.commandPool        = vulkanDevice->commandPool;
    cmdBufAllocateInfo.level              = vk::CommandBufferLevel::ePrimary;
    cmdBufAllocateInfo.commandBufferCount = 1;
    VK_CHECK_RESULT(device.allocateCommandBuffers(&cmdBufAllocateInfo, &copyCmd));

    vk::CommandBufferBeginInfo cmdBufInfo = {};
    VK_CHECK_RESULT(copyCmd.begin(&cmdBufInfo));

    // Copy vertex and index buffers to the device
    vk::BufferCopy copyRegion = {};
    copyRegion.size = vertexBufferSize;
    copyCmd.copyBuffer(stagingBuffer.handle, vertexBuffer.handle, 1, &copyRegion);
    copyRegion.size = indexBufferSize;
    copyRegion.srcOffset = vertexBufferSize;
    copyCmd.copyBuffer(stagingBuffer.handle, indexBuffer.handle, 1, &copyRegion);
    copyCmd.end();

    // Submit the command buffer to the queue to finish the copy
    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &copyCmd;

    // Create fence to ensure that the command buffer has finished executing
    vk::FenceCreateInfo fenceCI = {};
    vk::Fence fence = device.createFence(fenceCI);

    // Submit copies to the queue
    VK_CHECK_RESULT(queue.submit(1, &submitInfo, fence));
    // Wait for the fence to signal that command buffer has finished executing
    VK_CHECK_RESULT(device.waitForFences(1, &fence, vk::True, DEFAULT_FENCE_TIMEOUT));
    
    device.destroyFence(fence);
    device.freeCommandBuffers(vulkanDevice->commandPool, 1, &copyCmd);

    // The fence made sure copies are finished, so we can safely delete the staging buffer
    device.destroyBuffer(stagingBuffer.handle);
    device.freeMemory(stagingBuffer.memory);
}

void VulkanTriangle::createUniformBuffers()
{
    // Prepare and initialize the per-frame uniform buffer blocks containing shader uniforms
    // Single uniforms like in OpenGL are no longer present in Vulkan. All shader uniforms are passed via uniform buffer blocks
    vk::BufferCreateInfo bufferInfo = {};
    bufferInfo.size = sizeof(ShaderData);
    bufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer;

    // Create the buffers
    for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
        VK_CHECK_RESULT(device.createBuffer(&bufferInfo, nullptr, &uniformBuffers[i].handle));

        // Get memory requirements including size, alignment and memory type based on the buffer type we request
        vk::MemoryRequirements memReqs = device.getBufferMemoryRequirements(uniformBuffers[i].handle);
        vk::MemoryAllocateInfo memAlloc = {};
        memAlloc.allocationSize = memReqs.size;
        memAlloc.memoryTypeIndex = getMemoryTypeIndex(memReqs.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        VK_CHECK_RESULT(device.allocateMemory(&memAlloc, nullptr, &uniformBuffers[i].memory));
        device.bindBufferMemory(uniformBuffers[i].handle, uniformBuffers[i].memory, 0);
        
        // We map the buffer once, so we can update it without having to map it again
        VK_CHECK_RESULT(device.mapMemory(uniformBuffers[i].memory, 0, sizeof(ShaderData), {}, (void**)(&uniformBuffers[i].mapped)));
    }
}

// Descriptors are used to pass data to shaders, for our sample we use a descriptor to pass parameters like matrices to the shader
void VulkanTriangle::createDescriptors()
{
    // Descriptors are allocated from a pool, that tells the implementation how many and what types of descriptors we are going to use (at maximum)
    vk::DescriptorPoolSize descriptorTypeCounts[1]{};
    // This example only one descriptor type (uniform buffer)
    descriptorTypeCounts[0].type = vk::DescriptorType::eUniformBuffer;
    // We have one buffer (and as such descriotpr) per frame
    descriptorTypeCounts[0].descriptorCount = MAX_CONCURRENT_FRAMES;
    // For additional types you need to add new entries in the type count list
    // E.g. for two combined image samplers :
    // typeCounts[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    // typeCounts[1].descriptorCount = 2;

    // Create the global descriptor pool
    // All descriptors used in this example are allocated from this pool
    vk::DescriptorPoolCreateInfo descriptorPoolCI = {};
    descriptorPoolCI.poolSizeCount = 1;
    descriptorPoolCI.pPoolSizes = descriptorTypeCounts;
    // Set the max. number of descriptor sets that can be requested from this pool (requesting beyond this limit will result in an error)
    // Our sample will create one set per uniform buffer perframe
    descriptorPoolCI.maxSets = MAX_CONCURRENT_FRAMES;
    VK_CHECK_RESULT(device.createDescriptorPool(&descriptorPoolCI, nullptr, &descriptorPool));

    // Descriptor set layouts define the interface between our application and the shader
    // Basically connects the different shader stages to descriptors for binding uniform buffers, image samplers, etc.
    // So every shader binding should map to one descriptor set layout binding
    // Binding 0: Uniform buffer (Vertex shader)
    vk::DescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
    layoutBinding.descriptorCount = 1;
    layoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;

    vk::DescriptorSetLayoutCreateInfo descriptorLayoutCI = {};
    descriptorLayoutCI.bindingCount = 1;
    descriptorLayoutCI.pBindings = &layoutBinding;
    VK_CHECK_RESULT(device.createDescriptorSetLayout(&descriptorLayoutCI, nullptr, &descriptorSetLayout));

    // Where the descriptor set layout is the interface, the descriptor set points to actual data
    // Descriptors that are changed per frame need to be multiplied, so we can update descriptor n+1 while n is still used by the GPUs, so we create one per max frame in flights
    for (uint32_t i = 0; i < MAX_CONCURRENT_FRAMES; ++i) {
        vk::DescriptorSetAllocateInfo allocInfo = {};
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        VK_CHECK_RESULT(device.allocateDescriptorSets(&allocInfo, &uniformBuffers[i].descriptorSet));

        // Update the descriptor set determining the shader binding points
        // For every binding point used in a shader there needs to be one
        // descriptor set matching that binding point
        vk::WriteDescriptorSet writeDescriptorSet = {};

        // The buffer's information is passed using a descriptor info structure
        vk::DescriptorBufferInfo bufferInfo = {};
        bufferInfo.buffer = uniformBuffers[i].handle;
        bufferInfo.range = sizeof(ShaderData);

        // Binding 0 : Uniform buffer
        writeDescriptorSet.dstSet          = uniformBuffers[i].descriptorSet;
        writeDescriptorSet.dstBinding      = 0;
        writeDescriptorSet.descriptorCount = 1;
        writeDescriptorSet.descriptorType  = vk::DescriptorType::eUniformBuffer;
        writeDescriptorSet.pBufferInfo     = &bufferInfo;
        device.updateDescriptorSets(1, &writeDescriptorSet, 0, nullptr);
    }
}

void VulkanTriangle::createPipeline()
{
    // The pipeline layout is the interface telling the pipeline what type of dscriptors will later be bound
    vk::PipelineLayoutCreateInfo pipelineLayoutCI = {};
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout;
    VK_CHECK_RESULT(device.createPipelineLayout(&pipelineLayoutCI, nullptr, &pipelineLayout));

    // Create the graphics pipeline used in this example
    // Vulkan uses the concept of rendering pipelines to encapsulate fixed states, replacing OpenGL's complex state machine
    // A pipeline is then stored and hashed on the GPU making pipeline changes very fast
    
    vk::GraphicsPipelineCreateInfo pipelineCI = {};
    // The layout used for this pipeline (can be shared among multiple pipelines using the same layout)
    pipelineCI.layout = pipelineLayout;
    // Renderpass this pipeline is attached to
    pipelineCI.renderPass = renderPass;

    // Construct the different states making up the pipeline

    // Input assembly state describes how primitives are assembled
    // This pipeline will assemble vertex data as a triangle lists (though we only use one triange)
    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyStateCI = {};
    inputAssemblyStateCI.topology = vk::PrimitiveTopology::eTriangleList;

    // Rasterization state
    vk::PipelineRasterizationStateCreateInfo rasterizationStateCI = {};
    rasterizationStateCI.polygonMode             = vk::PolygonMode::eFill;
    rasterizationStateCI.cullMode                = vk::CullModeFlagBits::eNone;
    rasterizationStateCI.frontFace               = vk::FrontFace::eCounterClockwise;
    rasterizationStateCI.depthClampEnable        = vk::False;
    rasterizationStateCI.rasterizerDiscardEnable = vk::False;
    rasterizationStateCI.depthBiasEnable         = vk::False;
    rasterizationStateCI.lineWidth               = 1.0f;

    // Color blend state describes how blend factors are calculated (if used)
    // We need one blend attachment state per color attachment (even if blending is not used)
    vk::PipelineColorBlendAttachmentState blendAttachmentState = {};
    blendAttachmentState.blendEnable             = vk::False;
    blendAttachmentState.colorWriteMask          = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    
    vk::PipelineColorBlendStateCreateInfo colorBlendStateCI = {};
    colorBlendStateCI.attachmentCount            = 1;
    colorBlendStateCI.pAttachments               = &blendAttachmentState;

    // Viewport state sets the number of viewports and scissor used in this pipeline
    // Note: This is actually overrideen by the dynamic states (see below)
    vk::PipelineViewportStateCreateInfo viewportStateCI = {};
    viewportStateCI.viewportCount                = 1;
    viewportStateCI.scissorCount                 = 1;

    // Enable dynamic states
    // Most states are baked into the pipeline, but there is some state that can be dynamiclly changed within the command buffer to make a things easier
    // To be able to change these we need to specify which dynamic states will be changed using this pipeline. Their actual states are set later in in the comman buffer
    std::vector<vk::DynamicState> dynamicStateEnables = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    vk::PipelineDynamicStateCreateInfo dynamicStateCI = {};
    dynamicStateCI.dynamicStateCount             = static_cast<uint32_t>(dynamicStateEnables.size());
    dynamicStateCI.pDynamicStates                = dynamicStateEnables.data();

    // Depth and stencil state containing depth and stencil compare and test operations
    // We only use depth tests and want depth tests and writes to be enabled and compare with less or equal
    vk::PipelineDepthStencilStateCreateInfo depthStencilStateCI = {};
    depthStencilStateCI.depthTestEnable          = vk::True;
    depthStencilStateCI.depthWriteEnable         = vk::True;
    depthStencilStateCI.depthCompareOp           = vk::CompareOp::eLessOrEqual;
    depthStencilStateCI.depthBoundsTestEnable    = vk::False;
    depthStencilStateCI.back.failOp              = vk::StencilOp::eKeep;
    depthStencilStateCI.back.passOp              = vk::StencilOp::eKeep;
    depthStencilStateCI.back.compareOp           = vk::CompareOp::eAlways;
    depthStencilStateCI.stencilTestEnable        = vk::False;
    depthStencilStateCI.front                    = depthStencilStateCI.back;

    // This example does not make use of multi sampling (for anti-aliasing), the state must still be set and passed to the pipeline
    vk::PipelineMultisampleStateCreateInfo multisampleStateCI = {};
    multisampleStateCI.rasterizationSamples      = vk::SampleCountFlagBits::e1;

    // Vertex input descriptions
    // Specifies the vertex input parameters for a pipeline

    // Vertex input binding
    // This example uses a single vertex input binding point 0
    vk::VertexInputBindingDescription vertexInputBinding = {};
    vertexInputBinding.binding = 0;
    vertexInputBinding.stride = sizeof(Vertex);
    vertexInputBinding.inputRate = vk::VertexInputRate::eVertex;

    // Input attribute bindings describe shader attribute locations and memory layouts
    std::array<vk::VertexInputAttributeDescription, 2> vertexInputAttributes{};
    // These match the following shader layout
    // layout (location = 0) in vec3 inPos;
    // layout (location = 1) in vec3 inColor;
    // Attribute location 0: Position
    vertexInputAttributes[0].binding = 0;
    vertexInputAttributes[0].location = 0;
    // Position attribute is three 32 bit signed (SFLOAT) floats (R32 G32 B32)
    vertexInputAttributes[0].format = vk::Format::eR32G32B32Sfloat;
    vertexInputAttributes[0].offset = offsetof(Vertex, position);
    // Attribute location 1: Color
    vertexInputAttributes[1].binding = 0;
    vertexInputAttributes[1].location = 1;
    // Color attribute is three 32 bit signed (SFLOAT) float3 (R32 G32 B32)
    vertexInputAttributes[1].format = vk::Format::eR32G32B32Sfloat;
    vertexInputAttributes[1].offset = offsetof(Vertex, color);

    // Vertex input state used for pipeline creation
    vk::PipelineVertexInputStateCreateInfo vertexInputStateCI = {};
    vertexInputStateCI.vertexBindingDescriptionCount = 1;
    vertexInputStateCI.pVertexBindingDescriptions = &vertexInputBinding;
    vertexInputStateCI.vertexAttributeDescriptionCount = 2;
    vertexInputStateCI.pVertexAttributeDescriptions = vertexInputAttributes.data();

    // Shaders
    std::array<vk::PipelineShaderStageCreateInfo, 2> shaderStages = {};

    // Vertex shader
    shaderStages[0].stage = vk::ShaderStageFlagBits::eVertex;
    shaderStages[0].module = loadSpirvShader("shaders/glsl/triangle.vert.spv");
    shaderStages[0].pName = "main";
    assert(shaderStages[0].module != nullptr);

    // Fragment shader
    shaderStages[1].stage = vk::ShaderStageFlagBits::eFragment;
    shaderStages[1].module = loadSpirvShader("shaders/glsl/triangle.frag.spv");
    shaderStages[1].pName = "main";
    assert(shaderStages[1].module != nullptr);

    // Set pipeline shader stage info
    pipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCI.pStages = shaderStages.data();

    // Assign the pipeline states to the pipeline creation info structure
    pipelineCI.pVertexInputState   = &vertexInputStateCI;
    pipelineCI.pInputAssemblyState = &inputAssemblyStateCI;
    pipelineCI.pRasterizationState = &rasterizationStateCI;
    pipelineCI.pColorBlendState    = &colorBlendStateCI;
    pipelineCI.pMultisampleState   = &multisampleStateCI;
    pipelineCI.pViewportState      = &viewportStateCI;
    pipelineCI.pDepthStencilState  = &depthStencilStateCI;
    pipelineCI.pDynamicState       = &dynamicStateCI;

    // Create rendering pipeline using the specified states
    auto r = device.createGraphicsPipeline(pipelineCache, pipelineCI);
    VK_CHECK_RESULT(r.result);
    pipeline = r.value;

    // Shader modules can safely be destroyed when the pipeline has been created
    device.destroyShaderModule(shaderStages[0].module);
    device.destroyShaderModule(shaderStages[1].module);
}

uint32_t VulkanTriangle::getMemoryTypeIndex(uint32_t typeBits, vk::MemoryPropertyFlags properties)
{
    // Iterate over all memory types available for the device used in this example
    for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; ++i) {
        if ((typeBits & 1) == 1) {
            if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }
        typeBits >>= 1;
    }
    throw "Could not find a suitable memory type!";
}

vk::ShaderModule VulkanTriangle::loadSpirvShader(const std::string& filename)
{
    size_t shaderSize;
    char* shaderCode{ nullptr };

    std::ifstream is(filename, std::ios::binary | std::ios::in | std::ios::ate);
    if (is.is_open()) {
        shaderSize = is.tellg();
        is.seekg(0, std::ios::beg);
        // Copy file contents into a buffer
        shaderCode = new char[shaderSize];
        is.read(shaderCode, shaderSize);
        is.close();
        assert(shaderSize > 0);
    }

    if (shaderCode) {
        // Create a new shader module that will be used for pipeline creation
        vk::ShaderModuleCreateInfo shaderModuleCI = {};
        shaderModuleCI.codeSize = shaderSize;
        shaderModuleCI.pCode = (uint32_t*)shaderCode;

        vk::ShaderModule shaderModule;
        VK_CHECK_RESULT(device.createShaderModule(&shaderModuleCI, nullptr, &shaderModule));

        delete[] shaderCode;

        return shaderModule;
    }
    else {
        std::cerr << "Error: Could not open shader file \"" << filename << "\"" << std::endl;
        return nullptr;
    }
}