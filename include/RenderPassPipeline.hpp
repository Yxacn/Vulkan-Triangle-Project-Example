#pragma once

#include <vulkan/vulkan.h>

namespace vkp
{

    class VulkanContext;
    class SwapChain;

    // 管理渲染通道、图形管线和描述符集布局
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

        VulkanContext* m_context;
        VkRenderPass m_renderPass;
        VkPipelineLayout m_pipelineLayout;
        VkPipeline m_graphicsPipeline;
        VkDescriptorSetLayout m_descriptorSetLayout;
    };

} // namespace vkp