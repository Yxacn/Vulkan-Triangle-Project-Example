// SyncManager.hpp
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
        SyncManager(VulkanContext& context, uint32_t imageCount, uint32_t frameCount = MAX_FRAMES_IN_FLIGHT);
        ~SyncManager();

        SyncManager(const SyncManager&) = delete;
        SyncManager& operator=(const SyncManager&) = delete;

        [[nodiscard]] uint32_t getFrameCount() const { return m_frameCount; }
        [[nodiscard]] const std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT>& getImageAvailableSemaphores() const
        {
            return m_imageAvailableSemaphores;
        }
        [[nodiscard]] const std::vector<VkSemaphore>& getRenderFinishedSemaphores() const
        {
            return m_renderFinishedSemaphores;
        }
        [[nodiscard]] const std::array<VkFence, MAX_FRAMES_IN_FLIGHT>& getInFlightFences() const
        {
            return m_inFlightFences;
        }
        [[nodiscard]] VkFence getImageInFlight(uint32_t imageIndex) const { return m_imagesInFlight[imageIndex]; }
        void setImageInFlight(uint32_t imageIndex, VkFence fence) { m_imagesInFlight[imageIndex] = fence; }

    private:
        void createSyncObjects(VulkanContext& context, uint32_t imageCount);
        void destroySyncObjects() noexcept;

        VulkanContext* m_context{ nullptr };
        uint32_t m_frameCount{ MAX_FRAMES_IN_FLIGHT };
        std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> m_imageAvailableSemaphores{};
        std::vector<VkSemaphore> m_renderFinishedSemaphores;
        std::array<VkFence, MAX_FRAMES_IN_FLIGHT> m_inFlightFences{};
        std::vector<VkFence> m_imagesInFlight;
    };

} // namespace vkp
