// GLWindow.cpp
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
        if (m_window)
        {
            destroyWindow();
            if (--sm_glfwRefCount == 0)
                glfwTerminate();
        }
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
            // 1. 释放当前对象资源
            destroyWindow();
            // 2. 转移资源
            m_w_info = std::move(other.m_w_info);
            m_window = other.m_window;

            // 3. 清空源对象
            other.m_window = nullptr;
        }
        return *this;
    }

    void GLWindow::initWindow()
    {
        if (sm_glfwRefCount++ == 0)
        {
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // 防止glfw产生opengl相关文件 (glfw是为opengl两针打造)
            glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);   // 禁止自定义调整窗口大小

            if (!glfwInit())
            {
                --sm_glfwRefCount;
                throw std::runtime_error("glfw初始化异常!");
            }
        }
    }

    void GLWindow::createWindow()
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        m_window = glfwCreateWindow(m_w_info.WindowWidth, m_w_info.WindowHeight, m_w_info.WindowTitle.c_str(), nullptr,
                                    nullptr);
        if (!m_window)
            throw std::runtime_error("glfw窗口创建失败");
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