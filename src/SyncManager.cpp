// SyncManager.cpp
#include "SyncManager.hpp"

#include <algorithm>
#include <stdexcept>

#include "VulkanContext.hpp"

namespace vkp
{
    SyncManager::SyncManager(VulkanContext& context, uint32_t imageCount, uint32_t frameCount)
        : m_context(&context)
        , m_frameCount(std::clamp(frameCount, 1u, MAX_FRAMES_IN_FLIGHT))
    {
        // frameCount 做防御性钳制：为 0 会导致取模除零/索引失效，超出数组容量会越界
        createSyncObjects(context, imageCount);
    }

    SyncManager::~SyncManager()
    {
        destroySyncObjects();
    }

    void SyncManager::destroySyncObjects() noexcept
    {
        if (!m_context)
            return;

        const VkDevice device = m_context->getDevice();
        for (VkSemaphore& semaphore : m_imageAvailableSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }
        for (VkSemaphore& semaphore : m_renderFinishedSemaphores)
        {
            if (semaphore != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
        }
        for (VkFence& fence : m_inFlightFences)
        {
            if (fence != VK_NULL_HANDLE)
            {
                vkDestroyFence(device, fence, nullptr);
                fence = VK_NULL_HANDLE;
            }
        }
        m_renderFinishedSemaphores.clear();
        m_imagesInFlight.clear();
        m_context = nullptr;
    }

    // imageAvailable 与帧围栏按“在飞帧数”创建，限制 CPU 最多提前 m_frameCount 帧；
    // renderFinished 按交换链图像数量分配，与 imageIndex 一一对应。
    // 在飞帧数不超过交换链图像数（见 VKEngine::createFrameResources），
    // 避免为永远无法并行使用的帧创建多余的同步对象。
    void SyncManager::createSyncObjects(VulkanContext& context, uint32_t imageCount)
    {
        m_imagesInFlight.assign(imageCount, VK_NULL_HANDLE);
        m_renderFinishedSemaphores.assign(imageCount, VK_NULL_HANDLE);

        const VkSemaphoreCreateInfo semaphoreInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
        const VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                           .flags = VK_FENCE_CREATE_SIGNALED_BIT };

        for (uint32_t i = 0; i < m_frameCount; ++i)
        {
            if (vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) !=
                    VK_SUCCESS ||
                vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            {
                destroySyncObjects();
                throw std::runtime_error("Failed to create synchronization objects!");
            }
        }

        for (VkSemaphore& semaphore : m_renderFinishedSemaphores)
        {
            if (vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
            {
                destroySyncObjects();
                throw std::runtime_error("Failed to create synchronization objects!");
            }
        }
    }

} // namespace vkp
