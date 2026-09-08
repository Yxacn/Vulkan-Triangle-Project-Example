#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkp
{

    class VulkanContext;
    class SwapChain;
    class RenderPassPipeline;

    // 管理帧缓冲
    class FrameBufferManager
    {
    public:
        FrameBufferManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline);
        ~FrameBufferManager();

        FrameBufferManager(const FrameBufferManager&) = delete;
        FrameBufferManager& operator=(const FrameBufferManager&) = delete;

        const std::vector<VkFramebuffer>& getFramebuffers() const { return m_swapChainFramebuffers; }

        void recreateFramebuffers(VulkanContext& context, SwapChain& swapChain, VkRenderPass renderPass);

    private:
        void createFramebuffers(VulkanContext& context, SwapChain& swapChain, VkRenderPass renderPass);

        VulkanContext* m_context;
        std::vector<VkFramebuffer> m_swapChainFramebuffers;
    };

} // namespace vkp