// VulkanContext.cpp
#include "VulkanContext.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace
{
#ifdef NDEBUG
    inline constexpr bool kEnableValidationLayers = false;
#else
    inline constexpr bool kEnableValidationLayers = true;
#endif

    // 验证层与设备扩展：编译期常量，Debug 构建启用验证层
    inline constexpr std::array<const char*, 1> kValidationLayers = { "VK_LAYER_KHRONOS_validation" };
    inline constexpr std::array<const char*, 1> kDeviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    [[nodiscard]] bool hasLayer(const std::vector<VkLayerProperties>& availableLayers, const char* layerName)
    {
        return std::ranges::any_of(availableLayers, [layerName](const auto& layer)
                                   { return std::string_view(layer.layerName) == layerName; });
    }

    [[nodiscard]] bool hasExtension(const std::vector<VkExtensionProperties>& availableExtensions,
                                    const char* extensionName)
    {
        return std::ranges::any_of(availableExtensions, [extensionName](const auto& extension)
                                   { return std::string_view(extension.extensionName) == extensionName; });
    }
} // namespace

namespace vkp
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT /*messageSeverity*/,
                                                        VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* /*pUserData*/)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << '\n';
        return VK_FALSE;
    }

    VulkanContext::VulkanContext(GLFWwindow* window, const VkApplicationInfo& appInfo,
                                 const VkInstanceCreateInfo& instanceCreateInfo)
    {
        try
        {
            createInstance(appInfo, instanceCreateInfo);
            setupDebugMessenger();
            createSurface(window);
            pickPhysicalDevice();
            createLogicalDevice();
        }
        catch (...)
        {
            destroyResources();
            throw;
        }
    }

    VulkanContext::~VulkanContext()
    {
        destroyResources();
    }

    void VulkanContext::destroyResources() noexcept
    {
        if (m_device)
        {
            vkDestroyDevice(m_device, nullptr);
            m_device = VK_NULL_HANDLE;
        }
        if (m_debugMessenger && m_instance)
        {
            const auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (func)
                func(m_instance, m_debugMessenger, nullptr);
            m_debugMessenger = VK_NULL_HANDLE;
        }
        if (m_surface && m_instance)
        {
            vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
            m_surface = VK_NULL_HANDLE;
        }
        if (m_instance)
        {
            vkDestroyInstance(m_instance, nullptr);
            m_instance = VK_NULL_HANDLE;
        }
    }

    // 合并调用方配置与 GLFW 必需的实例扩展
    void VulkanContext::createInstance(const VkApplicationInfo& appInfo, const VkInstanceCreateInfo& instanceCreateInfo)
    {
        if (kEnableValidationLayers)
        {
            uint32_t layerCount = 0;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            const bool allLayersAvailable =
                std::ranges::all_of(kValidationLayers, [&availableLayers](const char* layerName)
                                    { return hasLayer(availableLayers, layerName); });
            if (!allLayersAvailable)
            {
                throw std::runtime_error("Validation layer not available!");
            }
        }

        VkInstanceCreateInfo createInfo = instanceCreateInfo;
        if (createInfo.pApplicationInfo == nullptr)
        {
            createInfo.pApplicationInfo = &appInfo;
        }

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (!glfwExtensions)
        {
            throw std::runtime_error("Failed to get required GLFW extensions!");
        }

        std::vector<const char*> extensions;
        extensions.reserve(glfwExtensionCount + (kEnableValidationLayers ? 1u : 0u));
        extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (kEnableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (kEnableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();

            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity =
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback;
            createInfo.pNext = &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.ppEnabledLayerNames = nullptr;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    void VulkanContext::setupDebugMessenger()
    {
        if (!kEnableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;

        const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!func || func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to set up debug messenger!");
        }
    }

    void VulkanContext::createSurface(GLFWwindow* window)
    {
        if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

    // 选择首个满足扩展、队列族与交换链要求的物理设备
    void VulkanContext::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
        if (deviceCount == 0)
        {
            throw std::runtime_error("No Vulkan-supported GPU found!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

        const auto device =
            std::ranges::find_if(devices, [this](VkPhysicalDevice candidate) { return isDeviceSuitable(candidate); });
        if (device == devices.end())
        {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }

        m_physicalDevice = *device;
    }

    bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const
    {
        if (!checkDeviceExtensionSupport(device))
            return false;

        const QueueFamilyIndices indices = findQueueFamilies(device);
        if (!indices.isComplete())
            return false;

        const SwapChainSupportDetails details = querySwapChainSupport(device);
        return !details.formats.empty() && !details.presentModes.empty();
    }

    // 图形队列与呈现队列可能不在同一队列族，因此分别查询；
    // optional 用于区分“尚未找到”与“队列族 0”，避免 int 哨兵值。
    VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device) const
    {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if (!indices.graphicsFamily.has_value() && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.graphicsFamily = i;
            }
            if (!indices.presentFamily.has_value())
            {
                VkBool32 presentSupport = VK_FALSE;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
                if (presentSupport)
                {
                    indices.presentFamily = i;
                }
            }
            if (indices.isComplete())
                break;
        }
        return indices;
    }

    VulkanContext::SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device) const
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount,
                                                      details.presentModes.data());
        }
        return details;
    }

    bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        return std::ranges::all_of(kDeviceExtensions, [&availableExtensions](const char* requiredExtension)
                                   { return hasExtension(availableExtensions, requiredExtension); });
    }

    // 图形与呈现队列族不同时，需要分别创建队列
    void VulkanContext::createLogicalDevice()
    {
        const QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
        const uint32_t graphicsFamily = indices.graphicsFamily.value();
        const uint32_t presentFamily = indices.presentFamily.value();

        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = graphicsFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        uint32_t queueCreateInfoCount = 1;
        std::array<VkDeviceQueueCreateInfo, 2> queueCreateInfos{ queueCreateInfo, VkDeviceQueueCreateInfo{} };
        if (graphicsFamily != presentFamily)
        {
            queueCreateInfos[1] = queueCreateInfo;
            queueCreateInfos[1].queueFamilyIndex = presentFamily;
            ++queueCreateInfoCount;
        }

        const VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfoCount;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();
        createInfo.pEnabledFeatures = &deviceFeatures;

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(m_device, graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, presentFamily, 0, &m_presentQueue);
    }

} // namespace vkp
