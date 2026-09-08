#include "BufferManager.hpp"

#include <cstring>
#include <stdexcept>

#include "CommandManager.hpp"
#include "RenderPassPipeline.hpp"
#include "SwapChain.hpp"
#include "VulkanContext.hpp"

namespace vkp
{

    BufferManager::BufferManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                                 CommandManager& cmdManager)
        : m_context(&context)
    {
        createVertexBuffer(context, cmdManager);
        createIndexBuffer(context, cmdManager);
        createUniformBuffers(context);
        createDescriptorPool(context);
        createDescriptorSets(context, pipeline);
    }

    BufferManager::~BufferManager()
    {
        VkDevice device = m_context->getDevice();
        for (size_t i = 0; i < m_uniformBuffers.size(); i++)
        {
            vkUnmapMemory(device, m_uniformBuffersMemory[i]);
            vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
            vkFreeMemory(device, m_uniformBuffersMemory[i], nullptr);
        }
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        vkDestroyBuffer(device, m_indexBuffer, nullptr);
        vkFreeMemory(device, m_indexBufferMemory, nullptr);
        vkDestroyBuffer(device, m_vertexBuffer, nullptr);
        vkFreeMemory(device, m_vertexBufferMemory, nullptr);
    }

    uint32_t BufferManager::findMemoryType(VulkanContext& context, uint32_t typeFilter,
                                           VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        throw std::runtime_error("Failed to find suitable memory type!");
    }

    void BufferManager::createBuffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage,
                                     VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(context.getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(context.getDevice(), buffer, &memRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(context, memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(context.getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(context.getDevice(), buffer, bufferMemory, 0);
    }

    void BufferManager::copyBuffer(VulkanContext& context, CommandManager& cmdManager, VkBuffer srcBuffer,
                                   VkBuffer dstBuffer, VkDeviceSize size)
    {
        VkCommandPool pool = cmdManager.getCommandPool();
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        if (vkAllocateCommandBuffers(context.getDevice(), &allocInfo, &commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate command buffer for copy!");
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(context.getGraphicsQueue());

        vkFreeCommandBuffers(context.getDevice(), pool, 1, &commandBuffer);
    }

    void BufferManager::createVertexBuffer(VulkanContext& context, CommandManager& cmdManager)
    {
        VkDeviceSize bufferSize = sizeof(m_vertices[0]) * m_vertices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingBufferMemory);

        void* data;
        vkMapMemory(context.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, m_vertices.data(), (size_t)bufferSize);
        vkUnmapMemory(context.getDevice(), stagingBufferMemory);

        createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_vertexBuffer, m_vertexBufferMemory);

        copyBuffer(context, cmdManager, stagingBuffer, m_vertexBuffer, bufferSize);

        vkDestroyBuffer(context.getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(context.getDevice(), stagingBufferMemory, nullptr);
    }

    void BufferManager::createIndexBuffer(VulkanContext& context, CommandManager& cmdManager)
    {
        VkDeviceSize bufferSize = sizeof(m_indices[0]) * m_indices.size();

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                     stagingBufferMemory);

        void* data;
        vkMapMemory(context.getDevice(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, m_indices.data(), (size_t)bufferSize);
        vkUnmapMemory(context.getDevice(), stagingBufferMemory);

        createBuffer(context, bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_indexBuffer, m_indexBufferMemory);

        copyBuffer(context, cmdManager, stagingBuffer, m_indexBuffer, bufferSize);

        vkDestroyBuffer(context.getDevice(), stagingBuffer, nullptr);
        vkFreeMemory(context.getDevice(), stagingBufferMemory, nullptr);
    }

    void BufferManager::createUniformBuffers(VulkanContext& context)
    {
        // 假设有 MAX_FRAMES_IN_FLIGHT 个缓冲，这里简化为交换链图像数量
        size_t imageCount = 1; // 占位，实际由 VKEngine 传递，但我们在构造函数中无法得知，所以稍后在 VKEngine 中更新
        // 我们将在 VKEngine 中调用一个 resize 方法，但为了简单，我们在这里硬编码为 2
        // 更好的方式：让 VKEngine 在创建 BufferManager 后调用 setImageCount
        // 此处先设为 2，后续在 VKEngine 构造函数中通过重新创建或 resize 修正。
        // 为简便，我们在 VKEngine 中重新设计，但这里我们假设外部会调用 resize。
        // 临时：在构造函数中先创建空，外部再初始化。
        // 我们将在 VKEngine 中处理。
    }

    void BufferManager::createDescriptorPool(VulkanContext& context)
    {
        // 类似，需要图像数量，暂不实现
    }

    void BufferManager::createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline)
    {
        // 暂不实现
    }

    void BufferManager::updateUniformBuffer(uint32_t currentImage, const glm::mat4& model, const glm::mat4& view,
                                            const glm::mat4& proj)
    {
        // 实现
    }

    void BufferManager::bindBuffers(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                                    uint32_t currentImage) const
    {
        // 实现
    }

} // namespace vkp