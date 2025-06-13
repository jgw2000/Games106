#pragma once

#include "base/vulkanexamplebase.h"

class VulkanTriangle : public VulkanExampleBase
{
public:
    // Vertex layout used in this example
    struct Vertex {
        glm::vec3 position;
        glm::vec3 color;
    };

    struct VulkanBuffer {
        vk::DeviceMemory memory{ nullptr };
        vk::Buffer handle{ nullptr };
    };

    // Uniform buffer block object
    struct UniformBuffer : VulkanBuffer {
        // The descriptor set stores the resources bound to the binding points in a shader
        // It connects the binding points of the differenct shaders with the buffers and images used for those bindings
        vk::DescriptorSet descriptorSet{ nullptr };
        // We keep a pointer to the mapped buffer, so we can easily update its contents via a memcpy
        uint8_t* mapped{ nullptr };
    };

    // For simplicity we use the same uniform block layout as in the shader
    // This way we can just memcpy the data to the ubo
    // Note: You should use data types that align with the GPU in order to avoid manual padding (vec4, mat)
    struct ShaderData {
        glm::mat4 modelMatrix;
        glm::mat4 viewMatrix;
        glm::mat4 projectionMatrix;
    };

public:
    VulkanTriangle();
    virtual ~VulkanTriangle() override;

    virtual void prepare() override;
    virtual void render() override;
    virtual void buildCommandBuffers() override;

    virtual void getEnabledFeatures() override;

private:
    void createVertexBuffer();
    void createUniformBuffers();
    void createDescriptors();
    void createPipeline();

    // This function is used to request a device memory type that supports all the property flags we request (e.g. device local, host visible)
    // Upon success it will return the index of the memory type that fits our requested memory properties
    // This is necessary as implementations can offer an arbitrary number of memory types with different memory properties
    // You can check https://vulkan.gpuinfo.org/ for details on different memory configurations
    uint32_t getMemoryTypeIndex(uint32_t typeBits, vk::MemoryPropertyFlags properties);

    // Vulkan loads its shaders from an immediate binary representation called SPIR-V
    // Shaders are compiled offline from e.g. GLSL using the reference glslang compiler
    // This function loads such a shader from a binary file and returns a shader module structure
    vk::ShaderModule loadSpirvShader(const std::string& filename);

    VulkanBuffer vertexBuffer;
    VulkanBuffer indexBuffer;
    uint32_t indexCount{ 0 };

    // We use on UBO per frame, so we can have a frame overlap and make sure that uniforms aren't updated while still in use
    std::array<UniformBuffer, MAX_CONCURRENT_FRAMES> uniformBuffers;

    // Descriptor set pool
    vk::DescriptorPool descriptorPool{ nullptr };

    // The descriptor set layout describes the shader binding layout (without actually referencing descriptor)
    // Like the pipeline layout it's pretty much a blueprint and can be used with different discriptor sets as long as their layout matches
    vk::DescriptorSetLayout descriptorSetLayout{ nullptr };

    // The pipeline layout is used by a pipeline to access the descriptor sets
    // It defines interface (without binding any actual data) between the shader stages used by the pipeline and the shader resources
    // A pipeline layout can be shared among multiple pipelines as long as their interfaces match
    vk::PipelineLayout pipelineLayout{ nullptr };

    // Pipelines (often called "pipeline state objects") are used to bake all states that affect a pipeline
    // While in OpenGL every state can be changed at (almost) any time, Vulkan requires to layout the graphics (and compute) pipeline states upfront
    // So for each combination of non-dynamic pipeline states you need a new pipeline (there are a few exceptions to this not discussed here)
    // Even though this adds a new dimension of planning ahead, it's a great opportunity for performance optimizations by the driver
    vk::Pipeline pipeline{ nullptr };
};