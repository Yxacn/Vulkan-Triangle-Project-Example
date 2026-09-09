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
        , m_swapChain(&swapChain)
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
        uint32_t imageCount = m_swapChain->getImageCount();
        VkDeviceSize bufferSize = sizeof(glm::mat4) * 3; // model, view, proj

        m_uniformBuffers.resize(imageCount);
        m_uniformBuffersMemory.resize(imageCount);
        m_uniformBuffersMapped.resize(imageCount);

        for (uint32_t i = 0; i < imageCount; i++)
        {
            createBuffer(context, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         m_uniformBuffers[i], m_uniformBuffersMemory[i]);

            vkMapMemory(context.getDevice(), m_uniformBuffersMemory[i], 0, bufferSize, 0, &m_uniformBuffersMapped[i]);
        }
    }

    void BufferManager::createDescriptorPool(VulkanContext& context)
    {
        uint32_t imageCount = m_swapChain->getImageCount();

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = imageCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = imageCount;

        if (vkCreateDescriptorPool(context.getDevice(), &poolInfo, nullptr, &m_descriptorPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor pool!");
        }
    }

    void BufferManager::createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline)
    {
        uint32_t imageCount = m_swapChain->getImageCount();
        std::vector<VkDescriptorSetLayout> layouts(imageCount, pipeline.getDescriptorSetLayout());

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_descriptorPool;
        allocInfo.descriptorSetCount = imageCount;
        allocInfo.pSetLayouts = layouts.data();

        m_descriptorSets.resize(imageCount);
        if (vkAllocateDescriptorSets(context.getDevice(), &allocInfo, m_descriptorSets.data()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate descriptor sets!");
        }

        for (uint32_t i = 0; i < imageCount; i++)
        {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_uniformBuffers[i];
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(glm::mat4) * 3;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(context.getDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void BufferManager::updateUniformBuffer(uint32_t currentImage, const glm::mat4& model, const glm::mat4& view,
                                            const glm::mat4& proj)
    {
        // 将三个矩阵连续写入映射内存
        void* data = m_uniformBuffersMapped[currentImage];
        memcpy(data, &model, sizeof(glm::mat4));
        memcpy(static_cast<char*>(data) + sizeof(glm::mat4), &view, sizeof(glm::mat4));
        memcpy(static_cast<char*>(data) + 2 * sizeof(glm::mat4), &proj, sizeof(glm::mat4));
    }

    void BufferManager::bindBuffers(VkCommandBuffer commandBuffer, VkPipelineLayout pipelineLayout,
                                    uint32_t currentImage) const
    {
        VkBuffer vertexBuffers[] = { m_vertexBuffer };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
                                &m_descriptorSets[currentImage], 0, nullptr);
    }

} // namespace vkp