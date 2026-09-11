// CommandManager.cpp
#include "CommandManager.hpp"

#include <stdexcept>

#include "BufferManager.hpp"
#include "FrameBufferManager.hpp"
#include "RenderPassPipeline.hpp"
#include "SwapChain.hpp"
#include "VulkanContext.hpp"

namespace vkp
{
    CommandManager::CommandManager(VulkanContext& context)
        : m_context(&context)
    {
        createCommandPool(context);
    }

    CommandManager::~CommandManager()
    {
        if (!m_context)
            return;

        if (m_commandPool)
            vkDestroyCommandPool(m_context->getDevice(), m_commandPool, nullptr);
    }

    void CommandManager::createCommandPool(VulkanContext& context)
    {
        const VulkanContext::QueueFamilyIndices indices = context.findQueueFamilies(context.getPhysicalDevice());
        if (!indices.isComplete())
        {
            throw std::runtime_error("Failed to find required queue families!");
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

        if (vkCreateCommandPool(context.getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create command pool!");
        }
    }

    // 交换链重建后，按当前图像数量重新录制全部命令缓冲
    void CommandManager::recordCommandBuffers(VulkanContext& context, SwapChain& swapChain,
                                              RenderPassPipeline& pipeline, FrameBufferManager& framebufferManager,
                                              BufferManager& bufferManager)
    {
        const auto& framebuffers = framebufferManager.getFramebuffers();
        const uint32_t imageCount = static_cast<uint32_t>(framebuffers.size());
        const uint32_t indexCount = bufferManager.getIndexCount();

        if (!m_commandBuffers.empty())
        {
            vkFreeCommandBuffers(context.getDevice(), m_commandPool, static_cast<uint32_t>(m_commandBuffers.size()),
                                 m_commandBuffers.data());
            m_commandBuffers.clear();
        }
        m_commandBuffers.assign(imageCount, VK_NULL_HANDLE);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = imageCount;

        if (vkAllocateCommandBuffers(context.getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS)
        {
            m_commandBuffers.clear();
            throw std::runtime_error("Failed to allocate command buffers!");
        }

        for (uint32_t i = 0; i < imageCount; ++i)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            if (vkBeginCommandBuffer(m_commandBuffers[i], &beginInfo) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to begin command buffer!");
            }

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = pipeline.getRenderPass();
            renderPassInfo.framebuffer = framebuffers[i];
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapChain.getExtent();

            VkClearValue clearColor = { { { 0.0f, 0.0f, 0.0f, 1.0f } } };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(m_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(m_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.getGraphicsPipeline());
            bufferManager.bindBuffers(m_commandBuffers[i], pipeline.getPipelineLayout(), i);
            vkCmdDrawIndexed(m_commandBuffers[i], indexCount, 1, 0, 0, 0);
            vkCmdEndRenderPass(m_commandBuffers[i]);

            if (vkEndCommandBuffer(m_commandBuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to record command buffer!");
            }
        }
    }

} // namespace vkp
