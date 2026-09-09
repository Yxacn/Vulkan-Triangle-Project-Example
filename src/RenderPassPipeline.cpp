// RenderPassPipeline.cpp
#include "RenderPassPipeline.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "BufferManager.hpp"
#include "SwapChain.hpp"
#include "VulkanContext.hpp"

#ifndef SHADER_DIR
#define SHADER_DIR "shaders/"
#endif

namespace
{
    [[nodiscard]] std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open())
        {
            throw std::runtime_error("Failed to open file: " + filename);
        }

        std::error_code error;
        const auto fileSize = std::filesystem::file_size(filename, error);
        if (error)
        {
            throw std::runtime_error("Failed to get size of file: " + filename);
        }

        std::vector<char> buffer(static_cast<size_t>(fileSize));
        if (!buffer.empty() && !file.read(buffer.data(), static_cast<std::streamsize>(buffer.size())))
        {
            throw std::runtime_error("Failed to read file: " + filename);
        }
        return buffer;
    }

    // Vulkan 不管理 shader module 生命周期，用 RAII 兜底管线创建失败路径
    class ShaderModule
    {
    public:
        ShaderModule(VkDevice device, const std::vector<char>& code)
            : m_device(device)
        {
            if (code.empty() || code.size() % sizeof(uint32_t) != 0)
            {
                throw std::runtime_error("Invalid SPIR-V code!");
            }

            // char 缓冲只保证 1 字节对齐，reinterpret_cast 成 uint32_t 指针读取属于未定义行为；
            // 先按字拷贝到对齐的 uint32_t 容器再交给 Vulkan
            m_code.resize(code.size() / sizeof(uint32_t));
            std::memcpy(m_code.data(), code.data(), code.size());

            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = m_code.size() * sizeof(uint32_t);
            createInfo.pCode = m_code.data();

            if (vkCreateShaderModule(m_device, &createInfo, nullptr, &m_module) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create shader module!");
            }
        }

        ~ShaderModule()
        {
            if (m_module)
            {
                vkDestroyShaderModule(m_device, m_module, nullptr);
            }
        }

        ShaderModule(const ShaderModule&) = delete;
        ShaderModule& operator=(const ShaderModule&) = delete;

        [[nodiscard]] VkShaderModule get() const { return m_module; }

    private:
        VkDevice m_device{ VK_NULL_HANDLE };
        VkShaderModule m_module{ VK_NULL_HANDLE };
        std::vector<uint32_t> m_code; // 持有转换后的 SPIR-V 代码，保证 pCode 在模块存活期间有效
    };
} // namespace

namespace vkp
{
    RenderPassPipeline::RenderPassPipeline(VulkanContext& context, SwapChain& swapChain, const PipelineConfig& config)
        : m_context(&context)
        , m_config(config)
    {
        try
        {
            createRenderPass(context, swapChain);
            createDescriptorSetLayout(context);
            createGraphicsPipeline(context, swapChain);
        }
        catch (...)
        {
            destroyResources();
            throw;
        }
    }

    RenderPassPipeline::~RenderPassPipeline()
    {
        destroyResources();
    }

    void RenderPassPipeline::destroyResources() noexcept
    {
        if (!m_context)
            return;

        const VkDevice device = m_context->getDevice();
        if (m_graphicsPipeline)
        {
            vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
            m_graphicsPipeline = VK_NULL_HANDLE;
        }
        if (m_pipelineLayout)
        {
            vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }
        if (m_renderPass)
        {
            vkDestroyRenderPass(device, m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
        if (m_descriptorSetLayout)
        {
            vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);
            m_descriptorSetLayout = VK_NULL_HANDLE;
        }
    }

    // 单附件渲染通道：颜色附件从 UNDEFINED 清屏后写入，最终转为适合呈现的布局。
    // subpass 依赖保证外部(呈现队列)到本 subpass 的颜色写入按序执行
    void RenderPassPipeline::createRenderPass(VulkanContext& context, SwapChain& swapChain)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = swapChain.getImageFormat();
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(context.getDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create render pass!");
        }
    }

    // binding 0 = UBO，与 shader.vert 中 layout(binding = 0) 一一对应
    void RenderPassPipeline::createDescriptorSetLayout(VulkanContext& context)
    {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(context.getDevice(), &layoutInfo, nullptr, &m_descriptorSetLayout) !=
            VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create descriptor set layout!");
        }
    }

    void RenderPassPipeline::createGraphicsPipeline(VulkanContext& context, SwapChain& swapChain)
    {
        // SHADER_DIR 由 CMake 注入（末尾带分隔符），着色器文件名来自 PipelineConfig
        const auto vertShaderCode = readFile(std::string(SHADER_DIR) + m_config.vertexShader);
        const auto fragShaderCode = readFile(std::string(SHADER_DIR) + m_config.fragmentShader);
        const ShaderModule vertShaderModule(context.getDevice(), vertShaderCode);
        const ShaderModule fragShaderModule(context.getDevice(), fragShaderCode);

        VkPipelineShaderStageCreateInfo vertShaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertShaderModule.get(),
            .pName = "main",
        };
        VkPipelineShaderStageCreateInfo fragShaderStageInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragShaderModule.get(),
            .pName = "main",
        };
        const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages{ vertShaderStageInfo, fragShaderStageInfo };

        // 顶点输入：位置 + 颜色
        VkVertexInputBindingDescription bindingDescription{
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        const std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{
            VkVertexInputAttributeDescription{
                .location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, pos) },
            VkVertexInputAttributeDescription{
                .location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color) },
        };
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDescription,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
            .pVertexAttributeDescriptions = attributeDescriptions.data(),
        };

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = m_config.topology,
            .primitiveRestartEnable = VK_FALSE,
        };

        VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(swapChain.getExtent().width),
            .height = static_cast<float>(swapChain.getExtent().height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        VkRect2D scissor{ .offset = { 0, 0 }, .extent = swapChain.getExtent() };
        VkPipelineViewportStateCreateInfo viewportState{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };

        VkPipelineRasterizationStateCreateInfo rasterizer{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = m_config.polygonMode,
            .cullMode = m_config.cullMode,
            .frontFace = m_config.frontFace,
            .depthBiasEnable = VK_FALSE,
            .lineWidth = m_config.lineWidth,
        };

        // 注意：启用多重采样时渲染通道还需提供解析附件，本示例的渲染通道固定单采样，
        // 因此 samples 保持 VK_SAMPLE_COUNT_1_BIT 才能与渲染通道匹配
        VkPipelineMultisampleStateCreateInfo multisampling{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = m_config.samples,
            .sampleShadingEnable = VK_FALSE,
        };

        // 标准 alpha 混合系数；blendEnable 为 VK_FALSE 时以下混合字段不起作用
        VkPipelineColorBlendAttachmentState colorBlendAttachment{
            .blendEnable = m_config.blendEnable,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = m_config.colorWriteMask,
        };
        VkPipelineColorBlendStateCreateInfo colorBlending{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
        };

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &m_descriptorSetLayout,
        };
        if (vkCreatePipelineLayout(context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaderStages.data(),
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pColorBlendState = &colorBlending,
            .layout = m_pipelineLayout,
            .renderPass = m_renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
        };
        if (vkCreateGraphicsPipelines(context.getDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                      &m_graphicsPipeline) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create graphics pipeline!");
        }
    }

} // namespace vkp
