#include "SyncManager.hpp"

#include <stdexcept>

#include "VulkanContext.hpp"

namespace vkp
{
    SyncManager::SyncManager(VulkanContext& context, uint32_t imageCount)
        : m_context(&context)
    {
        createSyncObjects(context, imageCount);
    }

    SyncManager::~SyncManager()
    {
        if (!m_context)
            return;

        VkDevice device = m_context->getDevice();
        for (auto semaphore : m_imageAvailableSemaphores)
        {
            if (semaphore)
                vkDestroySemaphore(device, semaphore, nullptr);
        }
        for (auto semaphore : m_renderFinishedSemaphores)
        {
            if (semaphore)
                vkDestroySemaphore(device, semaphore, nullptr);
        }
        for (auto fence : m_inFlightFences)
        {
            if (fence)
                vkDestroyFence(device, fence, nullptr);
        }
    }

    void SyncManager::createSyncObjects(VulkanContext& context, uint32_t imageCount)
    {
        m_imagesInFlight.assign(imageCount, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        m_renderFinishedSemaphores.resize(imageCount);

        const auto cleanup = [&]() {
            for (auto semaphore : m_imageAvailableSemaphores)
            {
                if (semaphore)
                    vkDestroySemaphore(context.getDevice(), semaphore, nullptr);
            }
            for (auto semaphore : m_renderFinishedSemaphores)
            {
                if (semaphore)
                    vkDestroySemaphore(context.getDevice(), semaphore, nullptr);
            }
            for (auto fence : m_inFlightFences)
            {
                if (fence)
                    vkDestroyFence(context.getDevice(), fence, nullptr);
            }
        };

        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) !=
                    VK_SUCCESS ||
                vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            {
                cleanup();
                throw std::runtime_error("Failed to create synchronization objects!");
            }
        }

        for (auto& semaphore : m_renderFinishedSemaphores)
        {
            if (vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &semaphore) != VK_SUCCESS)
            {
                cleanup();
                throw std::runtime_error("Failed to create synchronization objects!");
            }
        }
    }

} // namespace vkp
