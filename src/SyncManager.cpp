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
        VkDevice device = m_context->getDevice();
        for (auto semaphore : m_imageAvailableSemaphores)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        for (auto semaphore : m_renderFinishedSemaphores)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
        for (auto fence : m_inFlightFences)
        {
            vkDestroyFence(device, fence, nullptr);
        }
    }

    void SyncManager::createSyncObjects(VulkanContext& context, uint32_t imageCount)
    {
        m_imageAvailableSemaphores.resize(imageCount);
        m_renderFinishedSemaphores.resize(imageCount);
        m_inFlightFences.resize(imageCount);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (uint32_t i = 0; i < imageCount; i++)
        {
            if (vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) !=
                    VK_SUCCESS ||
                vkCreateSemaphore(context.getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) !=
                    VK_SUCCESS ||
                vkCreateFence(context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to create synchronization objects!");
            }
        }
    }

} // namespace vkp