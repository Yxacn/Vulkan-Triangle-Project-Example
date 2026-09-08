#include "FrameBufferManager.hpp"

#include <stdexcept>

#include "RenderPassPipeline.hpp"
#include "SwapChain.hpp"
#include "VulkanContext.hpp"

namespace vkp
{

    FrameBufferManager::FrameBufferManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline)
        : m_context(&context)
    {
        createFramebuffers(context, swapChain, pipeline.getRenderPass());
    }

    FrameBufferManager::~FrameBufferManager()
    {
        for (auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(m_context->getDevice(), framebuffer, nullptr);
        }
    }

    void FrameBufferManager::recreateFramebuffers(VulkanContext& context, SwapChain& swapChain, VkRenderPass renderPass)
    {
        for (auto framebuffer : m_swapChainFramebuffers)
        {
            vkDestroyFramebuffer(context.getDevice(), framebuffer, nullptr);
        }
        m_swapChainFramebuffers.clear();
        createFramebuffers(context, swapChain, renderPass);
    }

    void FrameBufferManager::createFramebuffers(VulkanContext& context, SwapChain& swapChain, VkRenderPass renderPass)
    {
        const auto& imageViews = swapChain.getImageViews();
        m_swapChainFramebuffers.resize(imageViews.size());

        for (size_t i = 0; i < imageViews.size(); i++)
        {
            VkImageView attachments[] = { imageViews[i] };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = swapChain.getExtent().width;
            framebufferInfo.height = swapChain.getExtent().height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(context.getDevice(), &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) !=
                VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create framebuffer!");
            }
        }
    }

} // namespace vkp