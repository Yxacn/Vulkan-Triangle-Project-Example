// VKEngine.cpp
#include "VKEngine.hpp"

#include <array>
#include <stdexcept>
#include <utility>

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
        createRenderingResources();
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
        other.m_window = nullptr;
        other.m_currentFrame = 0;
    }

    VKEngine& VKEngine::operator=(VKEngine&& other) noexcept
    {
        if (this != &other)
        {
            if (m_context)
                m_context->waitIdle();

            destroyRenderingResources();
            m_context.reset();

            m_window = other.m_window;
            m_context = std::move(other.m_context);
            m_swapChain = std::move(other.m_swapChain);
            m_pipeline = std::move(other.m_pipeline);
            m_framebufferManager = std::move(other.m_framebufferManager);
            m_bufferManager = std::move(other.m_bufferManager);
            m_commandManager = std::move(other.m_commandManager);
            m_syncManager = std::move(other.m_syncManager);
            m_currentFrame = other.m_currentFrame;
            other.m_window = nullptr;
            other.m_currentFrame = 0;
        }
        return *this;
    }

    // 资源创建顺序：交换链 -> 管线 -> 帧缓冲 -> 命令 -> 同步对象
    void VKEngine::createRenderingResources()
    {
        m_swapChain = std::make_unique<SwapChain>(*m_context, m_window);
        m_pipeline = std::make_unique<RenderPassPipeline>(*m_context, *m_swapChain);
        m_framebufferManager = std::make_unique<FrameBufferManager>(*m_context, *m_swapChain, *m_pipeline);
        m_commandManager = std::make_unique<CommandManager>(*m_context);
        m_bufferManager = std::make_unique<BufferManager>(*m_context, *m_swapChain, *m_pipeline, *m_commandManager);
        updateUniformBuffers();
        m_commandManager->recordCommandBuffers(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager,
                                               *m_bufferManager);
        m_syncManager = std::make_unique<SyncManager>(*m_context, m_swapChain->getImageCount());
    }

    void VKEngine::destroyRenderingResources() noexcept
    {
        m_syncManager.reset();
        m_commandManager.reset();
        m_bufferManager.reset();
        m_framebufferManager.reset();
        m_pipeline.reset();
        m_swapChain.reset();
    }

    void VKEngine::recreateSwapChain()
    {
        m_context->waitIdle();

        destroyRenderingResources();
        m_currentFrame = 0;
        try
        {
            createRenderingResources();
        }
        catch (...)
        {
            destroyRenderingResources();
            throw;
        }
    }

    void VKEngine::updateUniformBuffers()
    {
        const glm::mat4 model(1.0f);
        const glm::mat4 view =
            glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), m_swapChain->getExtent().width / (float)m_swapChain->getExtent().height, 0.1f, 10.0f);
        proj[1][1] *= -1;

        const UniformBufferObject ubo{ model, view, proj };
        for (uint32_t i = 0; i < m_swapChain->getImageCount(); ++i)
        {
            m_bufferManager->updateUniformBuffer(i, ubo);
        }
    }

    // 双缓冲同步：先等待当前帧围栏，再取图、提交、呈现；
    // 只有呈现完成并推进 currentFrame 后，下一帧才会复用这套资源。
    void VKEngine::drawFrame()
    {
        const auto& imageAvailableSemaphores = m_syncManager->getImageAvailableSemaphores();
        const auto& renderFinishedSemaphores = m_syncManager->getRenderFinishedSemaphores();
        const auto& fences = m_syncManager->getInFlightFences();
        const VkFence inFlightFence = fences[m_currentFrame];

        // 等待上一帧的栅栏，保证当前帧可安全复用
        if (vkWaitForFences(m_context->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to wait for in-flight fence!");
        }

        uint32_t imageIndex = 0;
        VkResult result = m_swapChain->acquireNextImage(imageAvailableSemaphores[m_currentFrame], imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            throw std::runtime_error("Failed to acquire swap chain image!");
        }

        const VkFence imageFence = m_syncManager->getImageInFlight(imageIndex);
        if (imageFence != VK_NULL_HANDLE && imageFence != inFlightFence)
        {
            // 该图像仍被更早的帧占用，先等其完成
            if (vkWaitForFences(m_context->getDevice(), 1, &imageFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to wait for swap chain image fence!");
            }
        }
        m_syncManager->setImageInFlight(imageIndex, inFlightFence);

        if (vkResetFences(m_context->getDevice(), 1, &inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to reset in-flight fence!");
        }

        const std::array<VkSemaphore, 1> waitSemaphores{ imageAvailableSemaphores[m_currentFrame] };
        const std::array<VkPipelineStageFlags, 1> waitStages{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        const std::array<VkSemaphore, 1> signalSemaphores{ renderFinishedSemaphores[imageIndex] };
        const VkCommandBuffer commandBuffer = m_commandManager->getCommandBuffers()[imageIndex];

        VkSubmitInfo submitInfo{
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores.data(),
            .pWaitDstStageMask = waitStages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = signalSemaphores.data(),
        };
        if (vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, inFlightFence) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to submit draw command buffer!");
        }

        const std::array<VkSwapchainKHR, 1> swapChains{ m_swapChain->getSwapChain() };
        VkPresentInfoKHR presentInfo{
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = signalSemaphores.data(),
            .swapchainCount = 1,
            .pSwapchains = swapChains.data(),
            .pImageIndices = &imageIndex,
        };

        result = vkQueuePresentKHR(m_context->getPresentQueue(), &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            recreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to present swap chain image!");
        }

        m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    void VKEngine::waitIdle()
    {
        if (m_context)
            m_context->waitIdle();
    }

} // namespace vkp
