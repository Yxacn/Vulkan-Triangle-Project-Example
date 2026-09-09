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

    class CommandManager
    {
    public:
        explicit CommandManager(VulkanContext& context);
        ~CommandManager();

        CommandManager(const CommandManager&) = delete;
        CommandManager& operator=(const CommandManager&) = delete;

        void recordCommandBuffers(VulkanContext& context, SwapChain& swapChain, RenderPassPipeline& pipeline,
                                  FrameBufferManager& framebufferManager, BufferManager& bufferManager);

        const std::vector<VkCommandBuffer>& getCommandBuffers() const { return m_commandBuffers; }
        VkCommandPool getCommandPool() const { return m_commandPool; }

    private:
        void createCommandPool(VulkanContext& context);

        VulkanContext* m_context;
        VkCommandPool m_commandPool;
        std::vector<VkCommandBuffer> m_commandBuffers;
    };

} // namespace vkp
