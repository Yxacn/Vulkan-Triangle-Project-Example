// VulkanContext.hpp
#pragma once

#include <optional>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vkp
{
    class VulkanContext
    {
    public:
        VulkanContext(GLFWwindow* window, const VkApplicationInfo& appInfo,
                      const VkInstanceCreateInfo& instanceCreateInfo);
        ~VulkanContext();

        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;

        [[nodiscard]] VkInstance getInstance() const { return m_instance; }
        [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
        [[nodiscard]] VkDevice getDevice() const { return m_device; }
        [[nodiscard]] VkSurfaceKHR getSurface() const { return m_surface; }
        [[nodiscard]] VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
        [[nodiscard]] VkQueue getPresentQueue() const { return m_presentQueue; }

        void waitIdle() const { vkDeviceWaitIdle(m_device); }

        struct QueueFamilyIndices
        {
            // 找不到时保持空值，避免使用 -1 哨兵
            std::optional<uint32_t> graphicsFamily;
            std::optional<uint32_t> presentFamily;

            [[nodiscard]] bool isComplete() const { return graphicsFamily.has_value() && presentFamily.has_value(); }
        };
        [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities{};
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };
        [[nodiscard]] SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) const;

    private:
        void createInstance(const VkApplicationInfo& appInfo, const VkInstanceCreateInfo& instanceCreateInfo);
        void setupDebugMessenger();
        void createSurface(GLFWwindow* window);
        void pickPhysicalDevice();
        void createLogicalDevice();

        [[nodiscard]] bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
        [[nodiscard]] bool isDeviceSuitable(VkPhysicalDevice device) const;
        void destroyResources() noexcept;

        VkInstance m_instance{ VK_NULL_HANDLE };
        VkDebugUtilsMessengerEXT m_debugMessenger{ VK_NULL_HANDLE };
        VkSurfaceKHR m_surface{ VK_NULL_HANDLE };
        VkPhysicalDevice m_physicalDevice{ VK_NULL_HANDLE };
        VkDevice m_device{ VK_NULL_HANDLE };
        VkQueue m_graphicsQueue{ VK_NULL_HANDLE };
        VkQueue m_presentQueue{ VK_NULL_HANDLE };
    };

} // namespace vkp
