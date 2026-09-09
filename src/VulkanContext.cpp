#include "VulkanContext.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <GLFW/glfw3.h>

namespace vkp
{
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                                        void* pUserData)
    {
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
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
            auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
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

    void VulkanContext::createInstance(const VkApplicationInfo& appInfo, const VkInstanceCreateInfo& instanceCreateInfo)
    {
        if (m_enableValidationLayers)
        {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char* layerName : m_validationLayers)
            {
                bool found = false;
                for (const auto& layer : availableLayers)
                {
                    if (strcmp(layerName, layer.layerName) == 0)
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    throw std::runtime_error("Validation layer not available!");
                }
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
            throw std::runtime_error("Failed to get required glfw extensions!");

        std::vector<const char*> extensions;
        extensions.reserve(glfwExtensionCount + (m_enableValidationLayers ? 1u : 0u));
        extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (m_enableValidationLayers)
        {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
        if (m_enableValidationLayers)
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
            createInfo.ppEnabledLayerNames = m_validationLayers.data();

            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = debugCallback;
            createInfo.pNext = &debugCreateInfo;
        }
        else
        {
            createInfo.enabledLayerCount = 0;
            createInfo.pNext = nullptr;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan instance!");
        }
    }

    void VulkanContext::setupDebugMessenger()
    {
        if (!m_enableValidationLayers)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;

        auto func =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
        if (func)
        {
            if (func(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to set up debug messenger!");
            }
        }
    }

    void VulkanContext::createSurface(GLFWwindow* window)
    {
        if (glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface!");
        }
    }

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

        for (const auto& device : devices)
        {
            if (isDeviceSuitable(device))
            {
                m_physicalDevice = device;
                break;
            }
        }

        if (m_physicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find a suitable GPU!");
        }
    }

    bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device)
    {
        if (!checkDeviceExtensionSupport(device))
            return false;

        QueueFamilyIndices indices = findQueueFamilies(device);
        if (!indices.isComplete())
            return false;

        SwapChainSupportDetails details = querySwapChainSupport(device);
        return !details.formats.empty() && !details.presentModes.empty();
    }

    VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device)
    {
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            if (indices.graphicsFamily < 0 && (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT))
            {
                indices.graphicsFamily = static_cast<int>(i);
            }
            if (indices.presentFamily < 0)
            {
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
                if (presentSupport)
                    indices.presentFamily = static_cast<int>(i);
            }
            if (indices.isComplete())
                break;
        }
        return indices;
    }

    VulkanContext::SwapChainSupportDetails VulkanContext::querySwapChainSupport(VkPhysicalDevice device)
    {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data());
        }
        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount,
                                                      details.presentModes.data());
        }
        return details;
    }

    bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device)
    {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        for (const char* requiredExtension : m_deviceExtensions)
        {
            bool found = false;
            for (const auto& extension : availableExtensions)
            {
                if (strcmp(requiredExtension, extension.extensionName) == 0)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        return true;
    }

    void VulkanContext::createLogicalDevice()
    {
        QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

        const uint32_t graphicsFamily = static_cast<uint32_t>(indices.graphicsFamily);
        const uint32_t presentFamily = static_cast<uint32_t>(indices.presentFamily);

        float queuePriority = 1.0f;
        std::array<VkDeviceQueueCreateInfo, 2> queueCreateInfos{};
        queueCreateInfos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfos[0].queueFamilyIndex = graphicsFamily;
        queueCreateInfos[0].queueCount = 1;
        queueCreateInfos[0].pQueuePriorities = &queuePriority;

        uint32_t queueCreateInfoCount = 1;
        if (graphicsFamily != presentFamily)
        {
            queueCreateInfos[1] = queueCreateInfos[0];
            queueCreateInfos[1].queueFamilyIndex = presentFamily;
            ++queueCreateInfoCount;
        }

        VkPhysicalDeviceFeatures deviceFeatures{};

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfoCount;
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(m_deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = m_deviceExtensions.data();

        createInfo.enabledLayerCount = 0;
        createInfo.ppEnabledLayerNames = nullptr;

        if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create logical device!");
        }

        vkGetDeviceQueue(m_device, indices.graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, indices.presentFamily, 0, &m_presentQueue);
    }

} // namespace vkp