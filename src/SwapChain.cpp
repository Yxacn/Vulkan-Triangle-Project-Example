// SwapChain.cpp
#include "SwapChain.hpp"

#include <algorithm>
#include <stdexcept>

#include "VkCheck.hpp"
#include "VulkanContext.hpp"

namespace vkp
{
    namespace
    {
        // 偏好 SRGB + FIFO（垂直同步），不支持时回退到可用格式
        [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(
            const std::vector<VkSurfaceFormatKHR>& availableFormats)
        {
            for (const auto& format : availableFormats)
            {
                if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                    format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    return format;
                }
            }
            return availableFormats[0];
        }

        // FIFO（垂直同步）：规范保证所有平台可用，按显示器刷新率逐帧呈现，静态场景下最省电；
        // 若追求更低延迟，可改为优先选择 Mailbox（无撕裂，但持续刷新、功耗更高）
        [[nodiscard]] VkPresentModeKHR chooseSwapPresentMode(
            const std::vector<VkPresentModeKHR>& availablePresentModes)
        {
            for (const auto& mode : availablePresentModes)
            {
                if (mode == VK_PRESENT_MODE_FIFO_KHR)
                {
                    return mode;
                }
            }
            return VK_PRESENT_MODE_FIFO_KHR; // FIFO 必在列表中，此分支仅为防御
        }

        [[nodiscard]] VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window)
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

        void destroyImageViews(VkDevice device, std::vector<VkImageView>& imageViews) noexcept
        {
            for (VkImageView imageView : imageViews)
            {
                if (imageView != VK_NULL_HANDLE)
                    vkDestroyImageView(device, imageView, nullptr);
            }
            imageViews.clear();
        }

        void createImageViews(VkDevice device, const std::vector<VkImage>& images, VkFormat format,
                              std::vector<VkImageView>& imageViews)
        {
            imageViews.assign(images.size(), VK_NULL_HANDLE);
            for (size_t i = 0; i < images.size(); ++i)
            {
                VkImageViewCreateInfo viewInfo{};
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = images[i];
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = format;
                viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;

                checkVk(vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]), "Failed to create image view!");
            }
        }
    } // namespace

    SwapChain::SwapChain(VulkanContext& context, GLFWwindow* window)
        : m_context(&context)
    {
        commit(buildSwapChain(context, window, VK_NULL_HANDLE));
    }

    SwapChain::~SwapChain()
    {
        cleanupSwapChain();
    }

    void SwapChain::cleanupSwapChain()
    {
        if (!m_context)
            return;

        const VkDevice device = m_context->getDevice();
        destroyImageViews(device, m_swapChainImageViews);
        m_swapChainImages.clear();
        m_swapChainImageFormat = VK_FORMAT_UNDEFINED;
        m_swapChainExtent = {};

        if (m_swapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device, m_swapChain, nullptr);
            m_swapChain = VK_NULL_HANDLE;
        }
    }

    void SwapChain::recreateSwapChain(VulkanContext& context, GLFWwindow* window)
    {
        // 新交换链完全构建成功后才提交，旧的 image views 与 swapchain 在 commit 中销毁，
        // 因此任一创建步骤失败都不会留下半初始化状态。
        commit(buildSwapChain(context, window, m_swapChain));
    }

    SwapChain::SwapChainResources SwapChain::buildSwapChain(VulkanContext& context, GLFWwindow* window,
                                                            VkSwapchainKHR oldSwapChain)
    {
        SwapChainResources resources;
        try
        {
            const VulkanContext::SwapChainSupportDetails support =
                context.querySwapChainSupport(context.getPhysicalDevice());
            if (support.formats.empty() || support.presentModes.empty())
            {
                throw std::runtime_error("Swap chain support is incomplete!");
            }

            const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
            const VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
            const VkExtent2D extent = chooseSwapExtent(support.capabilities, window);

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

            const VulkanContext::QueueFamilyIndices indices = context.findQueueFamilies(context.getPhysicalDevice());
            if (!indices.isComplete())
            {
                throw std::runtime_error("Failed to find required queue families!");
            }
            const uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
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
            createInfo.oldSwapchain = oldSwapChain;

            checkVk(vkCreateSwapchainKHR(context.getDevice(), &createInfo, nullptr, &resources.swapChain),
                    "Failed to create swap chain!");

            uint32_t imageCountOut = 0;
            checkVk(vkGetSwapchainImagesKHR(context.getDevice(), resources.swapChain, &imageCountOut, nullptr),
                    "Failed to query swap chain images!");
            resources.images.resize(imageCountOut);
            checkVk(vkGetSwapchainImagesKHR(context.getDevice(), resources.swapChain, &imageCountOut,
                                            resources.images.data()),
                    "Failed to query swap chain images!");

            resources.imageFormat = surfaceFormat.format;
            resources.extent = extent;
            createImageViews(context.getDevice(), resources.images, resources.imageFormat, resources.imageViews);
            return resources;
        }
        catch (...)
        {
            destroyImageViews(context.getDevice(), resources.imageViews);
            if (resources.swapChain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(context.getDevice(), resources.swapChain, nullptr);
                resources.swapChain = VK_NULL_HANDLE;
            }
            throw;
        }
    }

    void SwapChain::commit(SwapChainResources&& resources)
    {
        cleanupSwapChain();

        m_swapChain = resources.swapChain;
        m_swapChainImages = std::move(resources.images);
        m_swapChainImageViews = std::move(resources.imageViews);
        m_swapChainImageFormat = resources.imageFormat;
        m_swapChainExtent = resources.extent;

        resources.swapChain = VK_NULL_HANDLE;
    }

} // namespace vkp
