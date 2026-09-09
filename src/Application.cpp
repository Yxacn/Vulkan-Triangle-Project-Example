// Application.cpp
#include "Application.hpp"

#include <utility>

namespace vkp
{
    Application::Application(const WindowInfo& windowInfo, const VkApplicationInfo& appInfo,
                             const VkInstanceCreateInfo& instanceCreateInfo)
        : m_glw(windowInfo)
        , m_vke(m_glw.getWindowInstance(), appInfo, instanceCreateInfo)
    {
    }

    Application::~Application() {}

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
        while (!m_glw.shouldClose())
        {
            m_vke.drawFrame();
            glfwWaitEvents();
        }
    }
} // namespace vkp
