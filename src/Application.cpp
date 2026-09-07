// Application.cpp
#include "Application.hpp"

#include <utility>

vkp::Application::Application(const WindowInfo& window_info, const VkApplicationInfo& app_info,
                              const VkInstanceCreateInfo& instance_create_info)
    : m_vke(m_glw.getWindowInstance(), app_info, instance_create_info)
    , m_glw(window_info)
{
    //
}

vkp::Application::~Application()
{
    //
}

vkp::Application::Application(vkp::Application&& other) noexcept
    : m_glw(std::move(other.m_glw))
    , m_vke(std::move(other.m_vke))
{
    //
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
    while (!glfwWindowShouldClose(m_glw.getWindowInstance()))
    {
        glfwWaitEvents();
        m_vke.drawFrame();
    }
}