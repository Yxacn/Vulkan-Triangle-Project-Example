#include "Application.hpp"

#include <utility>

namespace vkp
{
    Application::Application(const WindowInfo& window_info, const VkApplicationInfo& app_info,
                             const VkInstanceCreateInfo& instance_create_info)
        : m_glw(window_info)
        , m_vke(m_glw.getWindowInstance(), app_info, instance_create_info)
    {
    }

    Application::~Application()
    {
    }

    Application::Application(Application&& other) noexcept
        : m_glw(std::move(other.m_glw))
        , m_vke(std::move(other.m_vke))
    {
    }

    Application& Application::operator=(Application&& other) noexcept
    {
        if (this != &other)
        {
            m_glw = std::move(other.m_glw);
            m_vke = std::move(other.m_vke);
        }
        return *this;
    }

    void Application::run()
    {
        while (!glfwWindowShouldClose(m_glw.getWindowInstance()))
        {
            m_vke.drawFrame();
            glfwWaitEvents();
        }
    }
} // namespace vkp