// Application.hpp
#pragma once

#include "GLWindow.hpp"
#include "VKEngine.hpp"

namespace vkp
{
    inline constexpr VkApplicationInfo DEFAULT_APP_INFO{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "VkProject",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    inline constexpr VkInstanceCreateInfo DEFAULT_INSTANCE_CREATE_INFO{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = nullptr,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };

    class Application
    {
    public:
        explicit Application(const WindowInfo& windowInfo = {}, const VkApplicationInfo& appInfo = DEFAULT_APP_INFO,
                             const VkInstanceCreateInfo& instanceCreateInfo = DEFAULT_INSTANCE_CREATE_INFO);
        ~Application();

        Application(Application&& other) noexcept;
        Application& operator=(Application&& other) noexcept;

        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        void run();

    private:
        GLWindow m_glw; // 窗口对象
        VKEngine m_vke; // Vulkan 引擎对象
    };
} // namespace vkp
