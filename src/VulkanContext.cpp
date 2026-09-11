// VulkanContext.cpp
#include "VulkanContext.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "VkCheck.hpp"

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

    // 与创建顺序相反销毁：surface/messenger 是 instance 的子对象，必须先于 instance 销毁
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

    // 合并调用方配置与 GLFW 必需的实例扩展。
    // Debug 构建默认启用验证层，但 SDK 未安装时自动降级为无验证层运行（仅打印警告），
    // 保证学习项目在任何机器上都能启动
    void VulkanContext::createInstance(const VkApplicationInfo& appInfo, const VkInstanceCreateInfo& instanceCreateInfo)
    {
        m_validationEnabled = kEnableValidationLayers;
        if (m_validationEnabled)
        {
            uint32_t layerCount = 0;
            checkVk(vkEnumerateInstanceLayerProperties(&layerCount, nullptr), "Failed to query validation layers!");
            std::vector<VkLayerProperties> availableLayers(layerCount);
            checkVk(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()),
                    "Failed to query validation layers!");

            const bool allLayersAvailable =
                std::ranges::all_of(kValidationLayers, [&availableLayers](const char* layerName)
                                    { return hasLayer(availableLayers, layerName); });
            if (!allLayersAvailable)
            {
                std::cerr << "Warning: validation layer not available, running without it "
                             "(install the Vulkan SDK to enable validation).\n";
                m_validationEnabled = false;
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

        // 调用方的扩展/层先保存，降级重试时会重新合并，保证两次尝试的列表一致
        const uint32_t callerExtensionCount = createInfo.enabledExtensionCount;
        const char* const* callerExtensions = createInfo.ppEnabledExtensionNames;
        const uint32_t callerLayerCount = createInfo.enabledLayerCount;
        const char* const* callerLayers = createInfo.ppEnabledLayerNames;

        for (;;)
        {
            // 合并调用方自定义扩展、GLFW 平台必需扩展与（验证启用时的）调试扩展。
            // 直接覆盖 ppEnabledExtensionNames 会静默丢弃调用方传入的扩展，因此先收集再赋值
            std::vector<const char*> extensions;
            extensions.reserve(callerExtensionCount + glfwExtensionCount + (m_validationEnabled ? 1u : 0u));
            if (callerExtensions != nullptr)
            {
                extensions.insert(extensions.end(), callerExtensions, callerExtensions + callerExtensionCount);
            }
            extensions.insert(extensions.end(), glfwExtensions, glfwExtensions + glfwExtensionCount);
            if (m_validationEnabled)
            {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
            // 去重：同一扩展重复出现会触发验证层告警；按字符串内容而非指针地址比较
            const auto toName = [](const char* name) { return std::string_view(name); };
            std::ranges::sort(extensions, {}, toName);
            const auto [uniqueBegin, uniqueEnd] = std::ranges::unique(extensions, {}, toName);
            extensions.erase(uniqueBegin, uniqueEnd);

            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            // 合并调用方层与（验证启用时的）验证层
            std::vector<const char*> layers;
            layers.reserve(callerLayerCount + (m_validationEnabled ? kValidationLayers.size() : 0u));
            if (callerLayers != nullptr)
            {
                layers.insert(layers.end(), callerLayers, callerLayers + callerLayerCount);
            }
            if (m_validationEnabled)
            {
                layers.insert(layers.end(), kValidationLayers.begin(), kValidationLayers.end());
            }
            std::ranges::sort(layers, {}, toName);
            const auto [uniqueLayerBegin, uniqueLayerEnd] = std::ranges::unique(layers, {}, toName);
            layers.erase(uniqueLayerBegin, uniqueLayerEnd);
            createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
            createInfo.ppEnabledLayerNames = layers.data();

            // 调试回调随实例创建（pNext 链保留调用方原有链）；每次循环重置，避免引用上轮局部变量
            createInfo.pNext = instanceCreateInfo.pNext;
            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            if (m_validationEnabled)
            {
                debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugCreateInfo.pfnUserCallback = debugCallback;
                debugCreateInfo.pNext = createInfo.pNext;
                createInfo.pNext = &debugCreateInfo;
            }

            const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
            if (result == VK_SUCCESS)
            {
                break;
            }
            if (result == VK_ERROR_LAYER_NOT_PRESENT && m_validationEnabled)
            {
                // 验证层已注册但 DLL 加载失败（如残留清单、未装 SDK）：去掉验证层降级重试
                std::cerr << "Warning: validation layer failed to load, retrying without it.\n";
                m_validationEnabled = false;
                continue;
            }
            throw std::runtime_error("Failed to create Vulkan instance! (VkResult " +
                                     std::to_string(static_cast<int>(result)) + ")");
        }
    }

    void VulkanContext::setupDebugMessenger()
    {
        if (!m_validationEnabled)
            return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = debugCallback;

        const auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));
        if (!func)
        {
            throw std::runtime_error("vkCreateDebugUtilsMessengerEXT is not available!");
        }
        checkVk(func(m_instance, &createInfo, nullptr, &m_debugMessenger), "Failed to set up debug messenger!");
    }

    void VulkanContext::createSurface(GLFWwindow* window)
    {
        checkVk(glfwCreateWindowSurface(m_instance, window, nullptr, &m_surface), "Failed to create window surface!");
    }

    // 选择首个满足扩展、队列族与交换链要求的物理设备
    void VulkanContext::pickPhysicalDevice()
    {
        uint32_t deviceCount = 0;
        checkVk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr), "Failed to enumerate physical devices!");
        if (deviceCount == 0)
        {
            throw std::runtime_error("No Vulkan-supported GPU found!");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        checkVk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()),
                "Failed to enumerate physical devices!");

        // 简单策略：取第一个满足全部条件的设备（集显/独显混用时按枚举顺序）；
        // 若需优先独立显卡，可在此处为候选设备评分排序
        const auto device = std::ranges::find_if(devices, [this](VkPhysicalDevice candidate)
                                                 { return isDeviceSuitable(candidate); });
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
                checkVk(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport),
                        "Failed to query surface support!");
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
        checkVk(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities),
                "Failed to query surface capabilities!");

        uint32_t formatCount = 0;
        checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr),
                "Failed to query surface formats!");
        if (formatCount != 0)
        {
            details.formats.resize(formatCount);
            checkVk(vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data()),
                    "Failed to query surface formats!");
        }

        uint32_t presentModeCount = 0;
        checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr),
                "Failed to query present modes!");
        if (presentModeCount != 0)
        {
            details.presentModes.resize(presentModeCount);
            checkVk(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount,
                                                              details.presentModes.data()),
                    "Failed to query present modes!");
        }
        return details;
    }

    bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const
    {
        uint32_t extensionCount = 0;
        checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr),
                "Failed to enumerate device extensions!");
        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        checkVk(vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data()),
                "Failed to enumerate device extensions!");

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

        checkVk(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "Failed to create logical device!");

        vkGetDeviceQueue(m_device, graphicsFamily, 0, &m_graphicsQueue);
        vkGetDeviceQueue(m_device, presentFamily, 0, &m_presentQueue);
    }

} // namespace vkp
