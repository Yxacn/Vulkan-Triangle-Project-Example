#pragma once

#include <array>
#include <vector>

#include <vulkan/vulkan.h>

namespace vkp
{

    class VulkanContext;

    inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    class SyncManager
    {
    public:
        SyncManager(VulkanContext& context, uint32_t imageCount);
        ~SyncManager();

        SyncManager(const SyncManager&) = delete;
        SyncManager& operator=(const SyncManager&) = delete;

        const std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>& getImageAvailableSemaphores() const
        {
            return m_imageAvailableSemaphores;
        }
        const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const
        {
            return m_renderFinishedSemaphores;
        }
        const std::array<VkFence, MAX_FRAMES_IN_FLIGHT>& getInFlightFences() const { return m_inFlightFences; }
        std::vector<VkFence>& getImagesInFlight() { return m_imagesInFlight; }

    private:
        void createSyncObjects(VulkanContext& context, uint32_t imageCount);

        VulkanContext* m_context;
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};
        std::vector<VkFence> m_imagesInFlight;
    };

} // namespace vkp
