#include "GLWindow.hpp"

#include <stdexcept>
#include <utility>

namespace vkp
{
    int GLWindow::sm_glfwRefCount{ 0 };

    GLWindow::GLWindow(const WindowInfo& window_info)
        : m_w_info(window_info)
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
        : m_w_info(std::move(other.m_w_info))
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

            m_w_info = std::move(other.m_w_info);
            m_window = other.m_window;
            other.m_window = nullptr;
        }
        return *this;
    }

    void GLWindow::initWindow()
    {
        if (sm_glfwRefCount++ == 0 && !glfwInit())
        {
            --sm_glfwRefCount;
            throw std::runtime_error("Failed to init glfw!");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // 无 OpenGL
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 禁止调整大小
    }

    void GLWindow::createWindow()
    {
        if (m_window)
            return;

        m_window = glfwCreateWindow(m_w_info.WindowWidth, m_w_info.WindowHeight, m_w_info.WindowTitle.c_str(), nullptr,
                                    nullptr);
        if (!m_window)
        {
            if (--sm_glfwRefCount == 0)
                glfwTerminate();
            throw std::runtime_error("Failed create glfw window");
        }
        centerWindow();
    }

    GLFWwindow* GLWindow::getWindowInstance() const
    {
        return m_window;
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

        int winWidth, winHeight;
        glfwGetWindowSize(m_window, &winWidth, &winHeight);

        int x = (mode->width - winWidth) / 2;
        int y = (mode->height - winHeight) / 2;

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