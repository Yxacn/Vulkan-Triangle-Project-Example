#pragma once

#include "GLWindow.hpp"
#include "VKEngine.hpp"

namespace vkp
{
    inline constexpr VkApplicationInfo defaultVkappInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "VkProject",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Vulkan",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };
    inline constexpr VkInstanceCreateInfo defaultVkInstanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &defaultVkappInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
    };
    class Application
    {
    public:
            explicit Application(const WindowInfo& window_info = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                                               DEFAULT_WINDOW_TITLE },
                             const VkApplicationInfo& app_info = defaultVkappInfo,
                             const VkInstanceCreateInfo& instace_create_info = defaultVkInstanceCreateInfo);
        ~Application();

            Application(Application&& other) noexcept;
            Application& operator=(Application&& other) noexcept;

            Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

            void run();

    private:
        GLWindow m_glw; // 窗口对象
        VKEngine m_vke; // Vulkan引擎对象
    };
} // namespace vkp