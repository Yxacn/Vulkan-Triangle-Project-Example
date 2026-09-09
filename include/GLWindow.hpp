// GLWindow.hpp
#pragma once

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct GLFWwindow;

namespace vkp
{
    // 默认窗口参数
    constexpr inline int DEFAULT_WINDOW_WIDTH = 800;
    constexpr inline int DEFAULT_WINDOW_HEIGHT = 600;
    constexpr inline const char* DEFAULT_WINDOW_TITLE = "Vulkan";

    // 窗口参数
    struct WindowInfo
    {
        int WindowWidth;         // 宽
        int WindowHeight;        // 高
        std::string WindowTitle; // 标签
    };

    class GLWindow
    {
    public:
        explicit GLWindow(const WindowInfo& window_info = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                                            DEFAULT_WINDOW_TITLE });
        ~GLWindow();

        GLWindow(GLWindow&& other) noexcept;
        GLWindow& operator=(GLWindow&& other) noexcept;

        GLWindow(const GLWindow&) = delete;
        GLWindow& operator=(const GLWindow&) = delete;

        void createWindow();
        GLFWwindow* getWindowInstance() const;

    private:
        void initWindow();             // 初始化窗口
        void centerWindow();           // 将窗口置于屏幕中心
        void destroyWindow() noexcept; // 销毁窗口

    private:
        GLFWwindow* m_window{ nullptr }; // 窗口对象
        WindowInfo m_w_info;             // 窗口信息对象

        static int sm_glfwRefCount; // 静态引用计数
    };
} // namespace vkp