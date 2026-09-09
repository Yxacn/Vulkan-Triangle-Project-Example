// VKEngine.hpp
#pragma once

#include <memory>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vkp
{
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

        VKEngine(const VKEngine&) = delete;
        VKEngine& operator=(const VKEngine&) = delete;

        VKEngine(VKEngine&& other) noexcept;
        VKEngine& operator=(VKEngine&& other) noexcept;

        void drawFrame();
        void waitIdle();

    private:
        // 常驻资源（交换链/命令池/几何缓冲）：整个引擎生命周期只创建一次
        void createRenderingResources();
        // 帧资源（管线/帧缓冲/UBO/命令录制/同步对象）：依赖交换链，交换链重建时整体重建
        void createFrameResources();
        void destroyFrameResources() noexcept;
        void recreateSwapChain();
        void updateUniformBuffers();

        GLFWwindow* m_window{ nullptr };

        std::unique_ptr<VulkanContext> m_context;
        std::unique_ptr<SwapChain> m_swapChain;
        std::unique_ptr<RenderPassPipeline> m_pipeline;
        std::unique_ptr<FrameBufferManager> m_framebufferManager;
        std::unique_ptr<BufferManager> m_bufferManager;
        std::unique_ptr<CommandManager> m_commandManager;
        std::unique_ptr<SyncManager> m_syncManager;

        uint32_t m_currentFrame{ 0 };
    };

} // namespace vkp
