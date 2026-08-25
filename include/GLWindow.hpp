// GLWindow.hpp
#pragma once

#include <string>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace vkp
{
    constexpr int DEFAULT_WINDOW_WIDTH = 400;
    constexpr int DEFAULT_WINDOW_HEIGHT = 300;
    constexpr const char* DEFAULT_WINDOW_TITLE = "Vulkan";

    struct WindowInfo
    {
        int WindowWidth;
        int WindowHeight;
        std::string WindowTitle;
    };

    class GLWindow
    {
    public:
        explicit GLWindow(const WindowInfo& window_info = { DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                                                            DEFAULT_WINDOW_TITLE });
        ~GLWindow();

        GLWindow(const GLWindow&) = delete;
        GLWindow& operator=(const GLWindow&) = delete;

        GLWindow(GLWindow&& other) noexcept;
        GLWindow& operator=(GLWindow&& other) noexcept;

        void run();

    private:
        void createWindow();
        void centerWindow();
        void destroyWindow() noexcept;

    private:
        WindowInfo m_w_info;
        GLFWwindow* m_window{ nullptr };

        bool m_ownsGlfwRef{ false };
        static int sm_glfwRefCount;
    };

} // namespace vkp