// Application.hpp
#pragma once

#include "GLWindow.hpp"
#include "VKEngine.hpp"

namespace vkp
{
    // 应用程序主类，组合窗口和渲染引擎
    class Application
    {
    public:
        // 构造函数，可传入窗口参数、Vulkan应用信息和实例创建信息
        explicit Application(const WindowInfo& window_info = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                                               DEFAULT_WINDOW_TITLE },
                             const VkApplicationInfo& app_info = defaultVkappInfo,
                             const VkInstanceCreateInfo& instace_create_info = defaultVkInstanceCreateInfo);
        ~Application();

        // 移动构造
        Application(Application&& other) noexcept;
        // 移动赋值
        Application& operator=(Application&& other) noexcept;

        // 禁止拷贝
        Application(const Application&) = delete;
        Application& operator=(const Application&) = delete;

        // 主循环：处理事件并渲染每一帧
        void run();

    private:
        GLWindow m_glw; // 窗口对象
        VKEngine m_vke; // Vulkan引擎对象
    };
} // namespace vkp