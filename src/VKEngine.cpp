#include "VKEngine.hpp"

#include <chrono>
#include <stdexcept>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "BufferManager.hpp"
#include "CommandManager.hpp"
#include "FrameBufferManager.hpp"
#include "RenderPassPipeline.hpp"
#include "SwapChain.hpp"
#include "SyncManager.hpp"
#include "VulkanContext.hpp"

namespace vkp
{

    VKEngine::VKEngine(GLFWwindow* window, const VkApplicationInfo& appInfo,
                       const VkInstanceCreateInfo& instanceCreateInfo)
        : m_window(window)
    {
        m_context = std::make_unique<VulkanContext>(window, appInfo, instanceCreateInfo);
        m_swapChain = std::make_unique<SwapChain>(*m_context, window);
        m_pipeline = std::make_unique<RenderPassPipeline>(*m_context, *m_swapChain);
        m_framebufferManager = std::make_unique<FrameBufferManager>(*m_context, *m_swapChain, *m_pipeline);
        m_commandManager =
            std::make_unique<CommandManager>(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager);
        m_bufferManager = std::make_unique<BufferManager>(*m_context, *m_swapChain, *m_pipeline, *m_commandManager);
        // 命令缓冲录制依赖 BufferManager，现在创建完成后录制
        m_commandManager->recordCommandBuffers(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager,
                                               *m_bufferManager);
        m_syncManager = std::make_unique<SyncManager>(*m_context, m_swapChain->getImageCount());
    }

    VKEngine::~VKEngine()
    {
        if (m_context)
            m_context->waitIdle();
    }

    VKEngine::VKEngine(VKEngine&& other) noexcept
        : m_window(other.m_window)
        , m_context(std::move(other.m_context))
        , m_swapChain(std::move(other.m_swapChain))
        , m_pipeline(std::move(other.m_pipeline))
        , m_framebufferManager(std::move(other.m_framebufferManager))
        , m_bufferManager(std::move(other.m_bufferManager))
        , m_commandManager(std::move(other.m_commandManager))
        , m_syncManager(std::move(other.m_syncManager))
        , m_currentFrame(other.m_currentFrame)
    {
    }

    VKEngine& VKEngine::operator=(VKEngine&& other) noexcept
    {
        if (this != &other)
        {
            m_window = other.m_window;
            m_context = std::move(other.m_context);
            m_swapChain = std::move(other.m_swapChain);
            m_pipeline = std::move(other.m_pipeline);
            m_framebufferManager = std::move(other.m_framebufferManager);
            m_bufferManager = std::move(other.m_bufferManager);
            m_commandManager = std::move(other.m_commandManager);
            m_syncManager = std::move(other.m_syncManager);
            m_currentFrame = other.m_currentFrame;
        }
        return *this;
    }

    void VKEngine::recreateSwapChain()
    {
        // 等待设备空闲，确保所有资源释放安全
        m_context->waitIdle();

        // 清理所有相关资源（按依赖逆序）
        m_syncManager.reset();
        m_commandManager.reset();
        m_bufferManager.reset();
        m_framebufferManager.reset();
        m_pipeline.reset();
        m_swapChain.reset();

        // 重新创建
        m_swapChain = std::make_unique<SwapChain>(*m_context, m_window);
        m_pipeline = std::make_unique<RenderPassPipeline>(*m_context, *m_swapChain);
        m_framebufferManager = std::make_unique<FrameBufferManager>(*m_context, *m_swapChain, *m_pipeline);
        m_commandManager =
            std::make_unique<CommandManager>(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager);
        m_bufferManager = std::make_unique<BufferManager>(*m_context, *m_swapChain, *m_pipeline, *m_commandManager);
        m_commandManager->recordCommandBuffers(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager,
                                               *m_bufferManager);
        m_syncManager = std::make_unique<SyncManager>(*m_context, m_swapChain->getImageCount());

        m_currentFrame = 0;
    }

    void VKEngine::drawFrame()
    {
        auto& fences = m_syncManager->getInFlightFences();
        VkFence inFlightFence = fences[m_currentFrame];
        vkWaitForFences(m_context->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
        VkResult result =
            m_swapChain->acquireNextImage(m_syncManager->getImageAvailableSemaphores()[m_currentFrame], imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        // 更新 Uniform
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();
        glm::mat4 model = glm::mat4(1.0f);                        // 单位矩阵，三角形保持初始位置
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), // 相机位置在 Z 轴正方向
                                     glm::vec3(0.0f, 0.0f, 0.0f), // 看向原点
                                     glm::vec3(0.0f, 1.0f, 0.0f)  // 世界向上方向为 Y 轴
        );
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), m_swapChain->getExtent().width / (float)m_swapChain->getExtent().height, 0.1f, 10.0f);
        proj[1][1] *= -1;
        m_bufferManager->updateUniformBuffer(imageIndex, model, view, proj);

        vkResetFences(m_context->getDevice(), 1, &inFlightFence);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { m_syncManager->getImageAvailableSemaphores()[m_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        VkCommandBuffer cmdBuffer = m_commandManager->getCommandBuffers()[imageIndex];
        submitInfo.pCommandBuffers = &cmdBuffer;

        VkSemaphore signalSemaphores[] = { m_syncManager->getRenderFinishedSemaphores()[m_currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapChains[] = { m_swapChain->getSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % m_swapChain->getImageCount();
    }

    void VKEngine::waitIdle()
    {
        if (m_context)
            m_context->waitIdle();
    }

} // namespace vkp