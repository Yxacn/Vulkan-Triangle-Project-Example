// Application.cpp
#include "Application.hpp"

#include <utility>

vkp::Application::Application(const WindowInfo& window_info, const VkApplicationInfo& app_info,
                              const VkInstanceCreateInfo& instance_create_info)
    : m_vke(m_glw.getWindowInstance(), app_info, instance_create_info) // 先初始化引擎，需要窗口实例
    , m_glw(window_info)                                               // 后初始化窗口
{
    // 初始化顺序依赖：引擎需要窗口句柄，故此处m_glw在初始化列表中排在m_vke之后，
    // 但实际构造顺序按声明顺序（先m_glw后m_vke），因此需确保在构造函数体内不依赖未构造的对象。
    // 这里将m_glw放在前面声明，但初始化列表顺序不影响构造顺序，因此m_glw先构造，再构造m_vke。
    // 为避免歧义，实际声明顺序为 m_glw, m_vke，因此构造顺序就是m_glw先，m_vke后。
    // 但m_vke的构造参数需要m_glw.getWindowInstance()，此时m_glw已构造完成，所以安全。
}

vkp::Application::~Application()
{
    // 资源由成员析构自动释放
}

vkp::Application::Application(vkp::Application&& other) noexcept
    : m_glw(std::move(other.m_glw))
    , m_vke(std::move(other.m_vke))
{
    // 移动构造，转移成员所有权
}

vkp::Application& vkp::Application::operator=(vkp::Application&& other) noexcept
{
    if (this != &other)
    {
        m_glw = std::move(other.m_glw);
        m_vke = std::move(other.m_vke);
    }
    return *this;
}

void vkp::Application::run()
{
    // 主循环：等待事件并渲染
    while (!glfwWindowShouldClose(m_glw.getWindowInstance()))
    {
        glfwWaitEvents();  // 等待事件（减少CPU占用）
        m_vke.drawFrame(); // 绘制一帧
    }
}