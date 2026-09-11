// VKEngine.cpp
#include "VKEngine.hpp"

#include <algorithm>
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
#include "VkCheck.hpp"
#include "VulkanContext.hpp"

namespace vkp
{
    namespace
    {
        // noexcept 路径（析构/移动赋值）等待设备空闲：即使等待失败（如设备丢失）也不能让异常逃逸
        void waitIdleQuietly(VulkanContext* context) noexcept
        {
            if (!context)
                return;
            try
            {
                context->waitIdle();
            }
            catch (...)
            {
            }
        }
    } // namespace

    VKEngine::VKEngine(GLFWwindow* window, const VkApplicationInfo& appInfo,
                       const VkInstanceCreateInfo& instanceCreateInfo)
        : m_window(window)
    {
        m_context = std::make_unique<VulkanContext>(window, appInfo, instanceCreateInfo);
        createRenderingResources();
    }

    VKEngine::~VKEngine()
    {
        waitIdleQuietly(m_context.get());
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
            waitIdleQuietly(m_context.get());

            destroyFrameResources();
            m_swapChain.reset();
            // 常驻资源同样持有设备句柄，必须先于逻辑设备/实例销毁
            m_bufferManager.reset();
            m_commandManager.reset();
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

    // 常驻资源：交换链、命令池与几何缓冲只创建一次。命令池/几何缓冲与交换链无关，
    // 交换链重建时保留，避免重复上传顶点/索引数据
    void VKEngine::createRenderingResources()
    {
        m_swapChain = std::make_unique<SwapChain>(*m_context, m_window);
        m_commandManager = std::make_unique<CommandManager>(*m_context);
        m_bufferManager = std::make_unique<BufferManager>(*m_context, *m_commandManager);
        createFrameResources();
    }

    // 帧资源依赖交换链的格式与图像数量，交换链重建时整组销毁重建。
    // 创建顺序：管线 -> 帧缓冲 -> UBO/描述符 -> 命令录制 -> 同步对象
    void VKEngine::createFrameResources()
    {
        m_pipeline = std::make_unique<RenderPassPipeline>(*m_context, *m_swapChain);
        m_framebufferManager = std::make_unique<FrameBufferManager>(*m_context, *m_swapChain, *m_pipeline);
        m_bufferManager->createUniformResources(*m_context, *m_swapChain, *m_pipeline);
        updateUniformBuffers();
        m_commandManager->recordCommandBuffers(*m_context, *m_swapChain, *m_pipeline, *m_framebufferManager,
                                               *m_bufferManager);

        // 在飞帧数不超过交换链图像数，避免为无法并行使用的帧浪费同步对象
        const uint32_t frameCount = std::min(MAX_FRAMES_IN_FLIGHT, m_swapChain->getImageCount());
        m_syncManager = std::make_unique<SyncManager>(*m_context, m_swapChain->getImageCount(), frameCount);
        m_currentFrame = 0;
    }

    // 只销毁帧资源；常驻资源（命令池/几何缓冲）保留。帧缓冲先于管线销毁，
    // 因为帧缓冲引用渲染通道对象
    void VKEngine::destroyFrameResources() noexcept
    {
        m_syncManager.reset();
        if (m_bufferManager)
            m_bufferManager->destroyUniformResources();
        m_framebufferManager.reset();
        m_pipeline.reset();
    }

    void VKEngine::recreateSwapChain()
    {
        m_context->waitIdle();

        // 先销毁依赖旧交换链的上层资源，再原地重建交换链；
        // SwapChain::recreateSwapChain 内部保证旧链在新链构建成功后才销毁。
        destroyFrameResources();
        m_currentFrame = 0;
        try
        {
            m_swapChain->recreateSwapChain(*m_context, m_window);
            createFrameResources();
        }
        catch (...)
        {
            destroyFrameResources();
            throw;
        }
    }

    void VKEngine::updateUniformBuffers()
    {
        const glm::mat4 model(1.0f);
        const glm::mat4 view =
            glm::lookAt(glm::vec3(0.0f, 0.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // 交换链重建瞬间尺寸可能短暂为 0（最小化），height 为 0 会产生 NaN 投影矩阵，用 1.0 兜底
        const VkExtent2D extent = m_swapChain->getExtent();
        const float aspect = extent.height > 0 ? static_cast<float>(extent.width) / static_cast<float>(extent.height)
                                               : 1.0f;
        glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
        proj[1][1] *= -1; // GLM 默认右手系、裁剪空间 Y 向上，Vulkan NDC 的 Y 向下，翻转投影矩阵 Y 分量

        const UniformBufferObject ubo{ model, view, proj };
        for (uint32_t i = 0; i < m_swapChain->getImageCount(); ++i)
        {
            m_bufferManager->updateUniformBuffer(i, ubo);
        }
    }

    // 双缓冲同步：先等待当前帧围栏，再取图、提交、呈现；
    // imageAvailable 按帧分配，renderFinished 按交换链图像分配，避免同一图像的信号量被提前复用。
    // 只有呈现完成并推进 currentFrame 后，下一帧才会复用这套资源。
    void VKEngine::drawFrame()
    {
        const auto& imageAvailableSemaphores = m_syncManager->getImageAvailableSemaphores();
        const auto& renderFinishedSemaphores = m_syncManager->getRenderFinishedSemaphores();
        const auto& fences = m_syncManager->getInFlightFences();
        const uint32_t frameCount = m_syncManager->getFrameCount();
        const VkFence inFlightFence = fences[m_currentFrame];

        // 等待上一帧的栅栏，保证当前帧可安全复用
        checkVk(vkWaitForFences(m_context->getDevice(), 1, &inFlightFence, VK_TRUE, UINT64_MAX),
                "Failed to wait for in-flight fence!");

        uint32_t imageIndex = 0;
        VkResult result = m_swapChain->acquireNextImage(imageAvailableSemaphores[m_currentFrame], imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            // 表面尺寸与交换链不匹配（窗口调整/最小化恢复）
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
            checkVk(vkWaitForFences(m_context->getDevice(), 1, &imageFence, VK_TRUE, UINT64_MAX),
                    "Failed to wait for swap chain image fence!");
        }
        m_syncManager->setImageInFlight(imageIndex, inFlightFence);

        checkVk(vkResetFences(m_context->getDevice(), 1, &inFlightFence), "Failed to reset in-flight fence!");

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
        checkVk(vkQueueSubmit(m_context->getGraphicsQueue(), 1, &submitInfo, inFlightFence),
                "Failed to submit draw command buffer!");

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

        m_currentFrame = (m_currentFrame + 1) % frameCount;
    }

    void VKEngine::waitIdle()
    {
        if (m_context)
            m_context->waitIdle();
    }

} // namespace vkp
