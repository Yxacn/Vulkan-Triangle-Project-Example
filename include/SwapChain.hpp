// SwapChain.hpp
#pragma once

#include <vector>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#include "VulkanContext.hpp"

namespace vkp
{
    class SwapChain
    {
    public:
        SwapChain(VulkanContext& context, GLFWwindow* window);
        ~SwapChain();

        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;

        void recreateSwapChain(VulkanContext& context, GLFWwindow* window);
        void cleanupSwapChain();

        [[nodiscard]] VkSwapchainKHR getSwapChain() const { return m_swapChain; }
        [[nodiscard]] VkFormat getImageFormat() const { return m_swapChainImageFormat; }
        [[nodiscard]] VkExtent2D getExtent() const { return m_swapChainExtent; }
        [[nodiscard]] const std::vector<VkImageView>& getImageViews() const { return m_swapChainImageViews; }
        [[nodiscard]] uint32_t getImageCount() const { return static_cast<uint32_t>(m_swapChainImages.size()); }

        [[nodiscard]] VkResult acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex) const
        {
            return vkAcquireNextImageKHR(m_context->getDevice(), m_swapChain, UINT64_MAX, semaphore, VK_NULL_HANDLE,
                                         &imageIndex);
        }

    private:
        // 交换链创建结果先落在局部对象，全部成功后再一次性提交，
        // 保证重建失败时不会破坏仍可用的旧交换链状态。
        struct SwapChainResources
        {
            VkSwapchainKHR swapChain{ VK_NULL_HANDLE };
            std::vector<VkImage> images;
            std::vector<VkImageView> imageViews;
            VkFormat imageFormat{ VK_FORMAT_UNDEFINED };
            VkExtent2D extent{};
        };

        [[nodiscard]] SwapChainResources buildSwapChain(VulkanContext& context, GLFWwindow* window,
                                                        VkSwapchainKHR oldSwapChain);
        void commit(SwapChainResources&& resources);

        VulkanContext* m_context{ nullptr };
        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        std::vector<VkImage> m_swapChainImages;
        VkFormat m_swapChainImageFormat{ VK_FORMAT_UNDEFINED };
        VkExtent2D m_swapChainExtent{};
        std::vector<VkImageView> m_swapChainImageViews;
    };

} // namespace vkp
