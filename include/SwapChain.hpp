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

        VkSwapchainKHR getSwapChain() const { return m_swapChain; }
        VkFormat getImageFormat() const { return m_swapChainImageFormat; }
        VkExtent2D getExtent() const { return m_swapChainExtent; }
        const std::vector<VkImageView>& getImageViews() const { return m_swapChainImageViews; }
        uint32_t getImageCount() const { return static_cast<uint32_t>(m_swapChainImages.size()); }

        VkResult acquireNextImage(VkSemaphore semaphore, uint32_t& imageIndex)
        {
            return vkAcquireNextImageKHR(m_context->getDevice(), m_swapChain, UINT64_MAX, semaphore, VK_NULL_HANDLE,
                                         &imageIndex);
        }

    private:
        void createSwapChain(VulkanContext& context, GLFWwindow* window);
        void createImageViews(VulkanContext& context);

        VulkanContext* m_context{ nullptr };
        VkSwapchainKHR m_swapChain{ VK_NULL_HANDLE };
        std::vector<VkImage> m_swapChainImages;
        VkFormat m_swapChainImageFormat{ VK_FORMAT_UNDEFINED };
        VkExtent2D m_swapChainExtent{};
        std::vector<VkImageView> m_swapChainImageViews;
    };

} // namespace vkp