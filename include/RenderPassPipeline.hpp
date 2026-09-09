// RenderPassPipeline.hpp
#pragma once

#include <string>

#include <vulkan/vulkan.h>

namespace vkp
{
    class VulkanContext;
    class SwapChain;

    // 着色器与光栅化状态集中配置；默认值与当前示例的渲染方式一致
    struct PipelineConfig
    {
        std::string vertexShader{ "vert.spv" };
        std::string fragmentShader{ "frag.spv" };
        VkPrimitiveTopology topology{ VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
        VkPolygonMode polygonMode{ VK_POLYGON_MODE_FILL };
        VkCullModeFlags cullMode{ VK_CULL_MODE_NONE };
        VkFrontFace frontFace{ VK_FRONT_FACE_CLOCKWISE };
        float lineWidth{ 1.0f };
        VkSampleCountFlagBits samples{ VK_SAMPLE_COUNT_1_BIT };
        VkBool32 blendEnable{ VK_FALSE };
        VkColorComponentFlags colorWriteMask{ VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT };
    };

    class RenderPassPipeline
    {
    public:
        RenderPassPipeline(VulkanContext& context, SwapChain& swapChain, const PipelineConfig& config = {});
        ~RenderPassPipeline();

        RenderPassPipeline(const RenderPassPipeline&) = delete;
        RenderPassPipeline& operator=(const RenderPassPipeline&) = delete;

        [[nodiscard]] VkRenderPass getRenderPass() const { return m_renderPass; }
        [[nodiscard]] VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }
        [[nodiscard]] VkPipeline getGraphicsPipeline() const { return m_graphicsPipeline; }
        [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const { return m_descriptorSetLayout; }

    private:
        void createRenderPass(VulkanContext& context, SwapChain& swapChain);
        void createDescriptorSetLayout(VulkanContext& context);
        void createGraphicsPipeline(VulkanContext& context, SwapChain& swapChain);
        void destroyResources() noexcept;

        VulkanContext* m_context{ nullptr };
        PipelineConfig m_config;
        VkRenderPass m_renderPass{ VK_NULL_HANDLE };
        VkPipelineLayout m_pipelineLayout{ VK_NULL_HANDLE };
        VkPipeline m_graphicsPipeline{ VK_NULL_HANDLE };
        VkDescriptorSetLayout m_descriptorSetLayout{ VK_NULL_HANDLE };
    };

} // namespace vkp
