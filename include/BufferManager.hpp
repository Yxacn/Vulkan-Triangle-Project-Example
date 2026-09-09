// BufferManager.hpp
#pragma once

#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vkp
{

    class VulkanContext;
    class SwapChain;
    class RenderPassPipeline;
    class CommandManager; // 前向声明

    // 顶点结构
    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
    };

    // 管理顶点缓冲、索引缓冲、Uniform 缓冲及描述符集
    class BufferManager
    {
    public:
        BufferManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                      CommandManager& cmdManager);
        ~BufferManager();

        BufferManager(const BufferManager&) = delete;
        BufferManager& operator=(const BufferManager&) = delete;

        // 更新 Uniform 缓冲
        void updateUniformBuffer(uint32_t currentImage, const glm::mat4& model, const glm::mat4& view,
                                 const glm::mat4& proj);

        // 绑定缓冲和描述符集（用于命令录制）
        void bindBuffers(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, uint32_t currentImage) const;

    private:
        void createVertexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createIndexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createUniformBuffers(VulkanContext& context);
        void createDescriptorPool(VulkanContext& context);
        void createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline);

        // 辅助函数
        uint32_t findMemoryType(VulkanContext& context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VulkanContext& context, CommandManager& cmdManager, VkBuffer srcBuffer, VkBuffer dstBuffer,
                        VkDeviceSize size);

        VulkanContext* m_context;
        SwapChain* m_swapChain;
        VkBuffer m_vertexBuffer;
        VkDeviceMemory m_vertexBufferMemory;
        VkBuffer m_indexBuffer;
        VkDeviceMemory m_indexBufferMemory;
        std::vector<VkBuffer> m_uniformBuffers;
        std::vector<VkDeviceMemory> m_uniformBuffersMemory;
        std::vector<void*> m_uniformBuffersMapped;

        VkDescriptorPool m_descriptorPool;
        std::vector<VkDescriptorSet> m_descriptorSets;

        // 示例数据
        const std::vector<Vertex> m_vertices = { { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                                                 { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
                                                 { { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } } };
        const std::vector<uint16_t> m_indices = { 0, 1, 2 };
    };

} // namespace vkp