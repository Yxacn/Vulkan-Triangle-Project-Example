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
        // 构造时只创建与交换链无关的几何缓冲（顶点/索引）；
        // 依赖交换链图像数量的 UBO/描述符由 createUniformResources 单独创建，
        // 这样交换链重建时无需重新上传几何数据
        BufferManager(VulkanContext& context, CommandManager& cmdManager);
        ~BufferManager();

        BufferManager(const BufferManager&) = delete;
        BufferManager& operator=(const BufferManager&) = delete;

        void createUniformResources(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline);
        void destroyUniformResources() noexcept;

        void updateUniformBuffer(uint32_t currentImage, const UniformBufferObject& ubo);

        void bindBuffers(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout, uint32_t currentImage) const;

        [[nodiscard]] uint32_t getIndexCount() const { return static_cast<uint32_t>(m_indices.size()); }

    private:
        void createVertexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createIndexBuffer(VulkanContext& context, CommandManager& cmdManager);
        void createDeviceLocalBuffer(VulkanContext& context, CommandManager& cmdManager,
                                     std::span<const std::byte> data, VkBufferUsageFlags usage, VkBuffer& buffer,
                                     VkDeviceMemory& bufferMemory);
        void createUniformBuffers(VulkanContext& context, SwapChain& swapChain);
        void createDescriptorPool(VulkanContext& context, SwapChain& swapChain);
        void createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline, SwapChain& swapChain);

        [[nodiscard]] uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        void createBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
        void copyBuffer(VulkanContext& context, CommandManager& cmdManager, VkBuffer srcBuffer, VkBuffer dstBuffer,
                        VkDeviceSize size);
        void destroyResources() noexcept;

        VulkanContext* m_context{ nullptr };
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

        // 几何数据为编译期常量：所有实例共享同一份只读数据，避免每个实例拷贝一份顶点/索引
        static constexpr std::array<Vertex, 3> m_vertices = { { { { -0.5f, -0.5f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
                                                                 { { 0.5f, -0.5f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
                                                                 { { 0.0f, 0.5f, 0.0f }, { 0.0f, 0.0f, 1.0f } } } };
        static constexpr std::array<uint16_t, 3> m_indices = { 0, 1, 2 };
    };

} // namespace vkp
