#pragma once

#include <memory>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vkp
{

    // 前向声明
    class VulkanContext;
    class SwapChain;
    class RenderPassPipeline;
    class FrameBufferManager;
    class BufferManager;
    class CommandManager;
    class SyncManager;

    class VKEngine
    {
    public:
        VKEngine(GLFWwindow* window, const VkApplicationInfo& appInfo, const VkInstanceCreateInfo& instanceCreateInfo);
        ~VKEngine();

        // 禁止拷贝
        VKEngine(const VKEngine&) = delete;
        VKEngine& operator=(const VKEngine&) = delete;

        // 声明移动操作（定义在 .cpp）
        VKEngine(VKEngine&& other) noexcept;
        VKEngine& operator=(VKEngine&& other) noexcept;

        void drawFrame();
        void waitIdle();

    private:
        void recreateSwapChain();

        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<SwapChain> m_swapChain;
        std::unique_ptr<RenderPassPipeline> m_pipeline;
        std::unique_ptr<FrameBufferManager> m_framebufferManager;
        std::unique_ptr<BufferManager> m_bufferManager;
        std::unique_ptr<CommandManager> m_commandManager;
        std::unique_ptr<SyncManager> m_syncManager;

        uint32_t m_currentFrame = 0;
    };

} // namespace vkp