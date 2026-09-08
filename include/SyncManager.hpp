#pragma once

#include <vector>

#include <vulkan/vulkan.h>

namespace vkp
{

    class VulkanContext;

    // 管理同步对象
    class SyncManager
    {
    public:
        SyncManager(VulkanContext& context, uint32_t imageCount);
        ~SyncManager();

        SyncManager(const SyncManager&) = delete;
        SyncManager& operator=(const SyncManager&) = delete;

        const std::vector<VkSemaphore>& getImageAvailableSemaphores() const { return m_imageAvailableSemaphores; }
        const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const { return m_renderFinishedSemaphores; }
        const std::vector<VkFence>& getInFlightFences() const { return m_inFlightFences; }

    private:
        void createSyncObjects(VulkanContext& context, uint32_t imageCount);

        VulkanContext* m_context;
        std::vector<VkSemaphore> m_imageAvailableSemaphores;
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::vector<VkFence> m_inFlightFences;
    };

} // namespace vkp