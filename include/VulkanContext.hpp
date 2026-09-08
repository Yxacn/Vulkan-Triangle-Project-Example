#pragma once

#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

namespace vkp
{

    // Vulkan 底层设备上下文，管理实例、物理设备、逻辑设备、队列和表面
    class VulkanContext
    {
    public:
        VulkanContext(GLFWwindow* window, const VkApplicationInfo& appInfo,
                      const VkInstanceCreateInfo& instanceCreateInfo);
        ~VulkanContext();

        // 禁止拷贝
        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;

        // 对外接口，返回核心 Vulkan 句柄
        VkInstance getInstance() const { return m_instance; }
        VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }
        VkDevice getDevice() const { return m_device; }
        VkSurfaceKHR getSurface() const { return m_surface; }
        VkQueue getGraphicsQueue() const { return m_graphicsQueue; }
        VkQueue getPresentQueue() const { return m_presentQueue; }

        void waitIdle() const { vkDeviceWaitIdle(m_device); }

        // 将辅助查询函数设为 public，供其他类使用
        struct QueueFamilyIndices
        {
            int graphicsFamily = -1;
            int presentFamily = -1;
            bool isComplete() { return graphicsFamily >= 0 && presentFamily >= 0; }
        };
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    private:
        void createInstance(const VkApplicationInfo& appInfo, const VkInstanceCreateInfo& instanceCreateInfo);
        void setupDebugMessenger();
        void createSurface(GLFWwindow* window);
        void pickPhysicalDevice();
        void createLogicalDevice();

        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        bool isDeviceSuitable(VkPhysicalDevice device);

        // 成员变量
        VkInstance m_instance;
        VkDebugUtilsMessengerEXT m_debugMessenger;
        VkSurfaceKHR m_surface;
        VkPhysicalDevice m_physicalDevice;
        VkDevice m_device;
        VkQueue m_graphicsQueue;
        VkQueue m_presentQueue;

        // 配置常量（与原项目保持一致）
        const std::vector<const char*> m_validationLayers = { "VK_LAYER_KHRONOS_validation" };
        const std::vector<const char*> m_deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
#ifdef NDEBUG
        const bool m_enableValidationLayers = false;
#else
        const bool m_enableValidationLayers = true;
#endif
    };

} // namespace vkp