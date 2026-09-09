#include "VKEngine.hpp"

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
            std::make_unique<CommandManager>(*m_context);
        m_bufferManager = std::make_unique<BufferManager>(*m_context, *m_swapChain, *m_pipeline, *m_commandManager);
        updateUniformBuffers();
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
        m_context->waitIdle();

        m_syncManager.reset();
        m_commandManager.reset();
        m_bufferManager.reset();
        m_framebufferManager.reset();
        m_pipeline.reset();
        m_swapChain.reset();

        m_swapChain = std::make_unique<SwapChain>(*m_context, m_window);
        m_pipeline = std::make_unique<RenderPassPipeline>(*m_context, *m_swapChain);
        m_framebufferManager = std::make_unique<FrameBufferManager>(*m_context, *m_swapChain, *m_pipeline);
        m_commandManager =
            std::make_unique<CommandManager>(*m_context);
        m_bufferManager = std::make_unique<BufferManager>(*m_context, *m_swapChain, *m_pipeline, *m_commandManager);
        updateUniformBuffers();
        m_commandManager->recordCommandBuffers(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager,
                                               *m_bufferManager);
        m_syncManager = std::make_unique<SyncManager>(*m_context, m_swapChain->getImageCount());

        m_currentFrame = 0;
    }

    void VKEngine::updateUniformBuffers()
    {
        const glm::mat4 model(1.0f);
        const glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f),
                                           glm::vec3(0.0f, 1.0f, 0.0f));
        glm::mat4 proj = glm::perspective(
            glm::radians(45.0f), m_swapChain->getExtent().width / (float)m_swapChain->getExtent().height, 0.1f, 10.0f);
        proj[1][1] *= -1;

        for (uint32_t i = 0; i < m_swapChain->getImageCount(); i++)
        {
            m_bufferManager->updateUniformBuffer(i, model, view, proj);
        }
    }

    void VKEngine::drawFrame()
    {
        const auto& imageAvailableSemaphores = m_syncManager->getImageAvailableSemaphores();
        const auto& renderFinishedSemaphores = m_syncManager->getRenderFinishedSemaphores();
        const auto& fences = m_syncManager->getInFlightFences();
        auto& imagesInFlight = m_syncManager->getImagesInFlight();
        VkFence inFlightFence = fences[m_currentFrame];

        vkWaitForFences(m_context->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX);

        uint32_t imageIndex;
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

        if (imagesInFlight[imageIndex] != VK_NULL_HANDLE && imagesInFlight[imageIndex] != inFlightFence)
        {
            vkWaitForFences(m_context->getDevice(), 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }
        imagesInFlight[imageIndex] = inFlightFence;

        vkResetFences(m_context->getDevice(), 1, &inFlightFence);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[m_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        VkCommandBuffer cmdBuffer = m_commandManager->getCommandBuffers()[imageIndex];
        submitInfo.pCommandBuffers = &cmdBuffer;

        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[imageIndex] };
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
            return;
        }
        else if (result != VK_SUCCESS)
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
