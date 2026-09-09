#pragma once

#include <vulkan/vulkan.h>

namespace vkp
{
    class VulkanContext;
    class SwapChain;

    class RenderPassPipeline
    {
    public:
        RenderPassPipeline(VulkanContext& context, SwapChain& swapChain);
        ~RenderPassPipeline();

        RenderPassPipeline(const RenderPassPipeline&) = delete;
        RenderPassPipeline& operator=(const RenderPassPipeline&) = delete;

        VkRenderPass getRenderPass() const { return m_renderPass; }
        VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
        VkPipeline getGraphicsPipeline() const { return m_graphicsPipeline; }
        VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

    private:
        void createRenderPass(VulkanContext& context, SwapChain& swapChain);
        void createDescriptorSetLayout(VulkanContext& context);
        void createGraphicsPipeline(VulkanContext& context, SwapChain& swapChain);
        void destroyResources() noexcept;

        VulkanContext* m_context{ nullptr };
        VkRenderPass m_renderPass{ VK_NULL_HANDLE };
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
        VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
    };

} // namespace vkp