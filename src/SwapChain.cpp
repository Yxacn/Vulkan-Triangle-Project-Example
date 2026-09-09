#include "SwapChain.hpp"

#include <algorithm>
#include <stdexcept>

#include "VulkanContext.hpp"

namespace vkp
{
    SwapChain::SwapChain(VulkanContext& context, GLFWwindow* window)
        : m_context(&context)
    {
        try
        {
            createSwapChain(context, window);
            createImageViews(context);
        }
        catch (...)
        {
            cleanupSwapChain();
            throw;
        }
    }

    SwapChain::~SwapChain()
    {
        cleanupSwapChain();
    }

    void SwapChain::cleanupSwapChain()
    {
        for (auto imageView : m_swapChainImageViews)
        {
            vkDestroyImageView(m_context->getDevice(), imageView, nullptr);
        }
        m_swapChainImageViews.clear();
        m_swapChainImages.clear();

        if (m_swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_context->getDevice(), m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
    }

    void SwapChain::recreateSwapChain(VulkanContext& context, GLFWwindow* window)
    {
        cleanupSwapChain();
        createSwapChain(context, window);
        createImageViews(context);
    }

    static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& format : availableFormats)
        {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return format;
            }
        }
        return availableFormats[0];
    }

    static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        for (const auto& mode : availablePresentModes)
        {
            if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return mode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
    {
        if (capabilities.currentExtent.width != UINT32_MAX)
            return capabilities.currentExtent;

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
        actualExtent.width =
            std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height =
            std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }

    void SwapChain::createSwapChain(VulkanContext& context, GLFWwindow* window)
    {
        auto support = context.querySwapChainSupport(context.getPhysicalDevice());
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
        VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
        VkExtent2D extent = chooseSwapExtent(support.capabilities, window);

        uint32_t imageCount = support.capabilities.minImageCount + 1;
        if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
        {
            imageCount = support.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = context.getSurface();
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        auto indices = context.findQueueFamilies(context.getPhysicalDevice());
        uint32_t queueFamilyIndices[] = { static_cast<uint32_t>(indices.graphicsFamily),
                                          static_cast<uint32_t>(indices.presentFamily) };

        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        createInfo.preTransform = support.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(context.getDevice(), &createInfo, nullptr, &m_swapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create swap chain!");
        }

        vkGetSwapchainImagesKHR(context.getDevice(), m_swapChain, &imageCount, nullptr);
        m_swapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(context.getDevice(), m_swapChain, &imageCount, m_swapChainImages.data());

        m_swapChainImageFormat = surfaceFormat.format;
        m_swapChainExtent = extent;
    }

    void SwapChain::createImageViews(VulkanContext& context)
    {
        m_swapChainImageViews.resize(m_swapChainImages.size());
        for (size_t i = 0; i < m_swapChainImages.size(); i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = m_swapChainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = m_swapChainImageFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(context.getDevice(), &viewInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS)
            {
                for (size_t j = 0; j < i; j++)
                    vkDestroyImageView(context.getDevice(), m_swapChainImageViews[j], nullptr);
                m_swapChainImageViews.clear();
                throw std::runtime_error("Failed to create image views!");
            }
        }
    }

} // namespace vkp