// BufferManager.cpp
#include "BufferManager.hpp"

#include <cstring>
#include <stdexcept>

#include "CommandManager.hpp"
#include "RenderPassPipeline.hpp"
#include "SwapChain.hpp"
#include "VulkanContext.hpp"

namespace
{
    class ScopedCommandBuffer
    {
    public:
        ScopedCommandBuffer(VkDevice device, VkCommandPool pool)
            : m_device(device)
            , m_pool(pool)
        {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.commandPool = m_pool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;

            if (vkAllocateCommandBuffers(m_device, &allocInfo, &m_buffer) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to allocate command buffer for copy!");
            }
        }

        ~ScopedCommandBuffer()
        {
            if (m_buffer)
            {
                vkFreeCommandBuffers(m_device, m_pool, 1, &m_buffer);
            }
        }

        ScopedCommandBuffer(const ScopedCommandBuffer&) = delete;
        ScopedCommandBuffer& operator=(const ScopedCommandBuffer&) = delete;

        [[nodiscard]] VkCommandBuffer get() const { return m_buffer; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkCommandPool m_pool{ VK_NULL_HANDLE };
        VkCommandBuffer m_buffer{ VK_NULL_HANDLE };
    };
} // namespace

namespace vkp
{
    BufferManager::BufferManager(VulkanContext& context, CommandManager& cmdManager)
        : m_context(&context)
    {
        vkGetPhysicalDeviceMemoryProperties(context.getPhysicalDevice(), &m_memoryProperties);
        try
        {
            createVertexBuffer(context, cmdManager);
            createIndexBuffer(context, cmdManager);
        }
        catch (...)
        {
            destroyResources();
            throw;
        }
    }

    BufferManager::~BufferManager()
    {
        destroyResources();
    }

    // 交换链重建时调用：只重建与交换链图像数量相关的 UBO/描述符池/描述符集，
    // 顶点/索引等几何缓冲保持不变，避免重复上传设备本地内存
    void BufferManager::createUniformResources(VulkanContext& context, SwapChain& swapChain,
                                               RenderPassPipeline& pipeline)
    {
        createUniformBuffers(context, swapChain);
        createDescriptorPool(context, swapChain);
        createDescriptorSets(context, pipeline, swapChain);
    }

    // 与 createUniformResources 配对；内部对半初始化状态做防御，可安全重复调用
    void BufferManager::destroyUniformResources() noexcept
    {
        if (!m_context)
            return;

        const VkDevice device = m_context->getDevice();
        for (size_t i = 0; i < m_uniformBuffers.size(); ++i)
        {
            if (i < m_uniformBuffersMapped.size() && m_uniformBuffersMapped[i] != nullptr)
            {
                vkUnmapMemory(device, m_uniformBuffersMemory[i]);
                m_uniformBuffersMapped[i] = nullptr;
            }
            if (m_uniformBuffers[i] != VK_NULL_HANDLE)
            {
                vkDestroyBuffer(device, m_uniformBuffers[i], nullptr);
                m_uniformBuffers[i] = VK_NULL_HANDLE;
            }
            if (i < m_uniformBuffersMemory.size() && m_uniformBuffersMemory[i] != VK_NULL_HANDLE)
            {
                vkFreeMemory(device, m_uniformBuffersMemory[i], nullptr);
                m_uniformBuffersMemory[i] = VK_NULL_HANDLE;
            }
        }
        m_uniformBuffers.clear();
        m_uniformBuffersMemory.clear();
        m_uniformBuffersMapped.clear();

        if (m_descriptorPool)
        {
            vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
            m_descriptorPool = VK_NULL_HANDLE;
        }
        m_descriptorSets.clear();
    }

    // 析构兜底：先清理随交换链重建的资源，再销毁常驻几何缓冲
    void BufferManager::destroyResources() noexcept
    {
        if (!m_context)
            return;

        destroyUniformResources();

        const VkDevice device = m_context->getDevice();
        if (m_indexBuffer)
        {
            vkDestroyBuffer(device, m_indexBuffer, nullptr);
            m_indexBuffer = VK_NULL_HANDLE;
        }
        if (m_indexBufferMemory)
        {
            vkFreeMemory(device, m_indexBufferMemory, nullptr);
            m_indexBufferMemory = VK_NULL_HANDLE;
        }
        if (m_vertexBuffer)
        {
            vkDestroyBuffer(device, m_vertexBuffer, nullptr);
            m_vertexBuffer = VK_NULL_HANDLE;
        }
        if (m_vertexBufferMemory)
        {
            vkFreeMemory(device, m_vertexBufferMemory, nullptr);
            m_vertexBufferMemory = VK_NULL_HANDLE;
        }
    }

    // 遍历物理设备内存类型：先匹配缓冲要求的类型位（typeFilter），再匹配访问属性；
    // 常见属性组合：DEVICE_LOCAL（GPU 高速访问）、HOST_VISIBLE|HOST_COHERENT（CPU 可直接写入）
    uint32_t BufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const
    {
        for (uint32_t i = 0; i < m_memoryProperties.memoryTypeCount; ++i)
        {
            const bool typeMatches = (typeFilter & (1u << i)) != 0;
            const bool propertiesMatch = (m_memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeMatches && propertiesMatch)
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
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(context.getDevice(), &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            vkDestroyBuffer(context.getDevice(), buffer, nullptr);
            buffer = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to allocate buffer memory!");
        }

        if (vkBindBufferMemory(context.getDevice(), buffer, bufferMemory, 0) != VK_SUCCESS)
        {
            vkFreeMemory(context.getDevice(), bufferMemory, nullptr);
            vkDestroyBuffer(context.getDevice(), buffer, nullptr);
            bufferMemory = VK_NULL_HANDLE;
            buffer = VK_NULL_HANDLE;
            throw std::runtime_error("Failed to bind buffer memory!");
        }
    }

    void BufferManager::copyBuffer(VulkanContext& context, CommandManager& cmdManager, VkBuffer srcBuffer,
                                   VkBuffer dstBuffer, VkDeviceSize size)
    {
        // 一次性命令缓冲，拷贝完成后由 RAII 自动归还
        const VkCommandPool pool = cmdManager.getCommandPool();
        const ScopedCommandBuffer commandBuffer(context.getDevice(), pool);
        const VkCommandBuffer cmdBuffer = commandBuffer.get();

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to begin command buffer for copy!");
        }

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmdBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

        if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to record copy command buffer!");
        }

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        if (vkQueueSubmit(context.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit copy command buffer!");
        }
        if (vkQueueWaitIdle(context.getGraphicsQueue()) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for buffer copy!");
        }
    }

    // 顶点/索引缓冲统一走 staging 三步：主机填充 -> 一次性拷贝 -> 设备本地缓冲。
    // 异常路径由 destroyStaging 回收 staging 资源，目标缓冲交给 destroyResources 兜底。
    void BufferManager::createDeviceLocalBuffer(VulkanContext& context, CommandManager& cmdManager,
                                                std::span<const std::byte> data, VkBufferUsageFlags usage,
                                                VkBuffer& buffer, VkDeviceMemory& bufferMemory)
    {
        const VkDeviceSize size = static_cast<VkDeviceSize>(data.size());
        VkBuffer stagingBuffer{ VK_NULL_HANDLE };
        VkDeviceMemory stagingBufferMemory{ VK_NULL_HANDLE };

        const auto destroyStaging = [&]()
        {
            if (stagingBuffer)
            {
                vkDestroyBuffer(context.getDevice(), stagingBuffer, nullptr);
                stagingBuffer = VK_NULL_HANDLE;
            }
            if (stagingBufferMemory)
            {
                vkFreeMemory(context.getDevice(), stagingBufferMemory, nullptr);
                stagingBufferMemory = VK_NULL_HANDLE;
            }
        };

        try
        {
            createBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
                         stagingBufferMemory);

            void* mapped = nullptr;
            if (vkMapMemory(context.getDevice(), stagingBufferMemory, 0, size, 0, &mapped) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to map staging buffer memory!");
            }
            std::memcpy(mapped, data.data(), data.size());
            vkUnmapMemory(context.getDevice(), stagingBufferMemory);

            createBuffer(context, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                         buffer, bufferMemory);
            copyBuffer(context, cmdManager, stagingBuffer, buffer, size);
        }
        catch (...)
        {
            destroyStaging();
            throw;
        }

        destroyStaging();
    }

    void BufferManager::createVertexBuffer(VulkanContext& context, CommandManager& cmdManager)
    {
        createDeviceLocalBuffer(context, cmdManager, std::as_bytes(std::span(m_vertices)),
                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, m_vertexBuffer, m_vertexBufferMemory);
    }

    void BufferManager::createIndexBuffer(VulkanContext& context, CommandManager& cmdManager)
    {
        createDeviceLocalBuffer(context, cmdManager, std::as_bytes(std::span(m_indices)),
                                VK_BUFFER_USAGE_INDEX_BUFFER_BIT, m_indexBuffer, m_indexBufferMemory);
    }

    // 每个交换链图像一个 UBO：多帧在飞时互不覆盖（HOST_VISIBLE + 持久映射，直接 memcpy 写入）
    void BufferManager::createUniformBuffers(VulkanContext& context, SwapChain& swapChain)
    {
        const uint32_t imageCount = swapChain.getImageCount();
        constexpr VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        m_uniformBuffers.resize(imageCount);
        m_uniformBuffersMemory.resize(imageCount);
        m_uniformBuffersMapped.resize(imageCount);

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            createBuffer(context, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                         m_uniformBuffers[i], m_uniformBuffersMemory[i]);

            if (vkMapMemory(context.getDevice(), m_uniformBuffersMemory[i], 0, bufferSize, 0,
                            &m_uniformBuffersMapped[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to map uniform buffer memory!");
            }
        }
    }

    void BufferManager::createDescriptorPool(VulkanContext& context, SwapChain& swapChain)
    {
        const uint32_t imageCount = swapChain.getImageCount();

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

    // 每个交换链图像分配一套描述符集，各自指向对应的 UBO，与多帧在飞方案配合
    void BufferManager::createDescriptorSets(VulkanContext& context, RenderPassPipeline& pipeline,
                                             SwapChain& swapChain)
    {
        const uint32_t imageCount = swapChain.getImageCount();
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

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            VkDescriptorBufferInfo bufferInfo{ .buffer = m_uniformBuffers[i],
                                               .offset = 0,
                                               .range = sizeof(UniformBufferObject) };
            VkWriteDescriptorSet descriptorWrite{
                .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet = m_descriptorSets[i],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo = &bufferInfo,
            };

            vkUpdateDescriptorSets(context.getDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }

    void BufferManager::updateUniformBuffer(uint32_t currentImage, const UniformBufferObject& ubo)
    {
        if (currentImage >= m_uniformBuffersMapped.size())
        {
            throw std::out_of_range("Uniform buffer index out of range!");
        }
        std::memcpy(m_uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
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
