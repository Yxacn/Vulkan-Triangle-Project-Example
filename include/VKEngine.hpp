#pragma once

#include <vulkan/vulkan.h>

namespace vkp
{
    inline VkApplicationInfo defaultVkappInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "VkProject",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };
    class VKEngine
    {
    public:
        explicit VKEngine(const VkApplicationInfo app_info = defaultVkappInfo);
        ~VKEngine();

        VKEngine(const VKEngine&) = delete;
        VKEngine& operator=(const VKEngine&) = delete;

        VKEngine(const VKEngine&&) noexcept;
        VKEngine& operator=(const VKEngine&&) noexcept;

    private:
        void initVulkan();

    private:
        VkApplicationInfo m_appInfo{};
        VkInstanceCreateInfo m_createInfo{};
    };
} // namespace vkp