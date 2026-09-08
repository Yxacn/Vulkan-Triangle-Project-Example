#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkp
{

    class VulkanContext;
    class SwapChain;
    class RenderPassPipeline;
    class FrameBufferManager;
    class BufferManager;

    // 管理命令池和命令缓冲
    class CommandManager
    {
    public:
        CommandManager(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                       FrameBufferManager& framebufferManager);
        ~CommandManager();

        CommandManager(const CommandManager&) = delete;
        CommandManager& operator=(const CommandManager&) = delete;

        // 录制命令缓冲（需要在 BufferManager 创建后调用）
        void recordCommandBuffers(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                                  FrameBufferManager& framebufferManager, BufferManager& bufferManager);

        const std::vector<VkCommandBuffer>& getCommandBuffers() const { return m_commandBuffers; }
        VkCommandPool getCommandPool() const { return m_commandPool; }

    private:
        void createCommandPool(VulkanContext& context);
        void createCommandBuffers(VulkanContext& context);

        VulkanContext* m_context;
        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;
    };

} // namespace vkp