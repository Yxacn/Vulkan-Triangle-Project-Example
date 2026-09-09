#pragma once

#include <array>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

namespace vkp
{
    class VulkanContext;
    class SwapChain;
    class RenderPassPipeline;
    class CommandManager;

    struct Vertex
    {
        glm::vec3 pos;
        glm::vec3 color;
    };

    // 缓冲和描述符集
    class BufferManager
    {
    public:
        BufferManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                      CommandManager& cmdManager);
        ~BufferManager();

        BufferManager(const BufferManager&) = delete;
        BufferManager& operator=(const BufferManager&) = delete;

        void updateUniformBuffer(uint32_t currentImage, const glm::mat4& model, const glm::mat4& view,
                                 const glm::mat4& proj);

        void bindBuffers(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, uint32_t currentImage) const;

    private:
        void createVertexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createIndexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createUniformBuffers(VulkanContext& context);
        void createDescriptorPool(VulkanContext& context);
        void createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline);

        uint32_t findMemoryType(VulkanContext& context, uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void createBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VulkanContext& context, CommandManager& cmdManager, VkBuffer srcBuffer, VkBuffer dstBuffer,
                        VkDeviceSize size);
        void destroyResources() noexcept;

        VulkanContext* m_context{ nullptr };
        SwapChain* m_swapChain{ nullptr };
        VkBuffer m_vertexBuffer{ VK_NULL_HANDLE };
        VkDeviceMemory m_vertexBufferMemory{ VK_NULL_HANDLE };
        VkBuffer m_indexBuffer{ VK_NULL_HANDLE };
        VkDeviceMemory m_indexBufferMemory{ VK_NULL_HANDLE };
        std::vector<VkBuffer> m_uniformBuffers;
        std::vector<VkDeviceMemory> m_uniformBuffersMemory;
        std::vector<void*> m_uniformBuffersMapped;

        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_descriptorSets;

        const std::array<Vertex, 3> m_vertices = {{ { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                                                     { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
                                                     { { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } } }};
        const std::array<uint16_t, 3> m_indices = { 0, 1, 2 };
    };

} // namespace vkp