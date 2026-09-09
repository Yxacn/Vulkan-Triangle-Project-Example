// GLWindow.cpp
#include "GLWindow.hpp"

#include <stdexcept>
#include <utility>

namespace vkp
{
    int GLWindow::sm_glfwRefCount{ 0 };

    GLWindow::GLWindow(const WindowInfo& windowInfo)
        : m_windowInfo(windowInfo)
    {
        initWindow();
        createWindow();
    }

    GLWindow::~GLWindow()
    {
        if (!m_window)
            return;

        destroyWindow();
        if (--sm_glfwRefCount == 0)
            glfwTerminate();
    }

    GLWindow::GLWindow(GLWindow&& other) noexcept
        : m_windowInfo(std::move(other.m_windowInfo))
        , m_window(other.m_window)
    {
        other.m_window = nullptr;
    }

    GLWindow& GLWindow::operator=(GLWindow&& other) noexcept
    {
        if (this != &other)
        {
            if (m_window)
            {
                destroyWindow();
                if (--sm_glfwRefCount == 0)
                    glfwTerminate();
            }

            m_windowInfo = std::move(other.m_windowInfo);
            m_window = other.m_window;
            other.m_window = nullptr;
        }
        return *this;
    }

    void GLWindow::initWindow()
    {
        // 尺寸非法直接拒绝，避免 glfwCreateWindow 失败后错误信息不明确
        if (m_windowInfo.width <= 0 || m_windowInfo.height <= 0)
        {
            throw std::invalid_argument("Window dimensions must be positive!");
        }

        if (sm_glfwRefCount++ == 0 && !glfwInit())
        {
            --sm_glfwRefCount;
            throw std::runtime_error("Failed to init GLFW!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // 无 OpenGL 上下文
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 固定窗口大小
    }

    void GLWindow::createWindow()
    {
        if (m_window)
            return;

        m_window =
            glfwCreateWindow(m_windowInfo.width, m_windowInfo.height, m_windowInfo.title.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            if (--sm_glfwRefCount == 0)
                glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window!");
        }
        centerWindow();
    }

    [[nodiscard]] GLFWwindow* GLWindow::getWindowInstance() const
    {
        return m_window;
    }

    [[nodiscard]] bool GLWindow::shouldClose() const
    {
        return m_window != nullptr && glfwWindowShouldClose(m_window);
    }

    [[nodiscard]] bool GLWindow::isMinimized() const
    {
        // 最小化时帧缓冲尺寸为 0，获取交换链图像必然失败，调用方应跳过绘制
        return m_window != nullptr && glfwGetWindowAttrib(m_window, GLFW_ICONIFIED) == GLFW_TRUE;
    }

    void GLWindow::centerWindow()
    {
        if (!m_window)
            return;

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (!monitor)
            return;

        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode)
            return;

        int windowWidth, windowHeight;
        glfwGetWindowSize(m_window, &windowWidth, &windowHeight);

        const int x = (mode->width - windowWidth) / 2;
        const int y = (mode->height - windowHeight) / 2;

        glfwSetWindowPos(m_window, x, y);
    }

    void GLWindow::destroyWindow() noexcept
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
    }
} // namespace vkp
