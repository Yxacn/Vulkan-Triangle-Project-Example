// BufferManager.hpp
#pragma once

#include <array>
#include <cstddef>
#include <span>
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

    // std140 布局：mat4 对齐 16 字节，三个矩阵依次位于 0/64/128 偏移，
    // 与 shader.vert 中的 UniformBufferObject 一一对应。
    struct alignas(16) UniformBufferObject
    {
        glm::mat4 model{ 1.0f };
        glm::mat4 view{ 1.0f };
        glm::mat4 proj{ 1.0f };
    };

    static_assert(sizeof(UniformBufferObject) == 3 * sizeof(glm::mat4));

    class BufferManager
    {
    public:
        BufferManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                      CommandManager& cmdManager);
        ~BufferManager();

        BufferManager(const BufferManager&) = delete;
        BufferManager& operator=(const BufferManager&) = delete;

        void updateUniformBuffer(uint32_t currentImage, const UniformBufferObject& ubo);

        void bindBuffers(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, uint32_t currentImage) const;

        [[nodiscard]] uint32_t getIndexCount() const { return static_cast<uint32_t>(m_indices.size()); }

    private:
        void createVertexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createIndexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createDeviceLocalBuffer(VulkanContext& context, CommandManager& cmdManager,
                                     std::span<const std::byte> data, VkBufferUsageFlags usage, VkBuffer& buffer,
                                     VkDeviceMemory& bufferMemory);
        void createUniformBuffers(VulkanContext& context);
        void createDescriptorPool(VulkanContext& context);
        void createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline);

        [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        void createBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VulkanContext& context, CommandManager& cmdManager, VkBuffer srcBuffer, VkBuffer dstBuffer,
                        VkDeviceSize size);
        void destroyResources() noexcept;

        VulkanContext* m_context{ nullptr };
        SwapChain* m_swapChain{ nullptr };
        VkPhysicalDeviceMemoryProperties m_memoryProperties{};
        VkBuffer m_vertexBuffer{ VK_NULL_HANDLE };
        VkDeviceMemory m_vertexBufferMemory{ VK_NULL_HANDLE };
        VkBuffer m_indexBuffer{ VK_NULL_HANDLE };
        VkDeviceMemory m_indexBufferMemory{ VK_NULL_HANDLE };
        std::vector<VkBuffer> m_uniformBuffers;
        std::vector<VkDeviceMemory> m_uniformBuffersMemory;
        std::vector<void*> m_uniformBuffersMapped;

        VkDescriptorPool m_descriptorPool{ VK_NULL_HANDLE };
        std::vector<VkDescriptorSet> m_descriptorSets;

        const std::array<Vertex, 3> m_vertices = { { { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                                                     { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
                                                     { { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } } } };
        const std::array<uint16_t, 3> m_indices = { 0, 1, 2 };
    };

} // namespace vkp
