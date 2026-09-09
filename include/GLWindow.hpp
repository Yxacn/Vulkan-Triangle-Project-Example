// GLWindow.hpp
#pragma once

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace vkp
{
    inline constexpr int DEFAULT_WINDOW_WIDTH = 800;
    inline constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    inline constexpr const char* DEFAULT_WINDOW_TITLE = "Vulkan";

    struct WindowInfo
    {
        int width{ DEFAULT_WINDOW_WIDTH };
        int height{ DEFAULT_WINDOW_HEIGHT };
        std::string title{ DEFAULT_WINDOW_TITLE };
    };

    class GLWindow
    {
    public:
        explicit GLWindow(const WindowInfo& windowInfo = {});
        ~GLWindow();

        GLWindow(GLWindow&& other) noexcept;
        GLWindow& operator=(GLWindow&& other) noexcept;

        GLWindow(const GLWindow&) = delete;
        GLWindow& operator=(const GLWindow&) = delete;

        void createWindow();
        [[nodiscard]] GLFWwindow* getWindowInstance() const;
        [[nodiscard]] bool shouldClose() const;
        [[nodiscard]] bool isMinimized() const; // 最小化时帧缓冲尺寸为 0，调用方据此跳过绘制

    private:
        void initWindow();
        void centerWindow();
        void destroyWindow() noexcept;

        WindowInfo m_windowInfo;
        GLFWwindow* m_window{ nullptr };

        static int sm_glfwRefCount; // GLFW 全局引用计数
    };
} // namespace vkp
