// GLWindow.cpp

#include "GLWindow.hpp"

#include <stdexcept>
#include <utility>

int vkp::GLWindow::sm_glfwRefCount{ 0 };

vkp::GLWindow::GLWindow(const WindowInfo& window_info)
    : m_w_info(window_info)
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
    m_ownsGlfwRef = true; // 成功获取 GLFW 引用

    try
    {
        createWindow();
    }
    catch (...)
    {
        // 回滚：销毁窗口并释放引用
        destroyWindow();
        if (m_ownsGlfwRef && --sm_glfwRefCount == 0)
            glfwTerminate();
        // 对象未完全构造，析构不会执行，因此无需额外操作
        throw;
    }
}

vkp::GLWindow::~GLWindow()
{
    if (m_ownsGlfwRef)
    {
        destroyWindow();
        if (--sm_glfwRefCount == 0)
            glfwWaitEvents();
    }
}

vkp::GLWindow::GLWindow(GLWindow&& other) noexcept
    : m_w_info(std::move(other.m_w_info))
    , m_window(other.m_window)
    , m_ownsGlfwRef(other.m_ownsGlfwRef)
{
    other.m_window = nullptr;
    other.m_ownsGlfwRef = false; // 源对象放弃所有权
}

vkp::GLWindow& vkp::GLWindow::operator=(GLWindow&& other) noexcept
{
    if (this != &other)
    {
        // 1. 释放当前对象资源（如有）
        if (m_ownsGlfwRef)
        {
            destroyWindow();
            if (--sm_glfwRefCount == 0)
                glfwTerminate();
            m_ownsGlfwRef = false;
        }

        // 2. 转移资源
        m_w_info = std::move(other.m_w_info);
        m_window = other.m_window;
        m_ownsGlfwRef = other.m_ownsGlfwRef;

        // 3. 清空源对象
        other.m_window = nullptr;
        other.m_ownsGlfwRef = false;
    }
    return *this;
}

void vkp::GLWindow::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        glfwWaitEvents(); // 可考虑改为 glfwWaitEvents() 降低 CPU 占用
    }
}

void vkp::GLWindow::createWindow()
{
    m_window =
        glfwCreateWindow(m_w_info.WindowWidth, m_w_info.WindowHeight, m_w_info.WindowTitle.c_str(), nullptr, nullptr);
    if (!m_window)
        throw std::runtime_error("glfw窗口创建失败");
    centerWindow();
}

void vkp::GLWindow::centerWindow()
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

void vkp::GLWindow::destroyWindow() noexcept
{
    if (m_window)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
}