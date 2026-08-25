#include "VKEngine.hpp"

vkp::VKEngine::VKEngine(const VkApplicationInfo app_info)
    : m_appInfo(app_info)
{
    //
}

vkp::VKEngine::~VKEngine()
{
    //
}

vkp::VKEngine::VKEngine(const VKEngine&&) noexcept
{
    //
}

vkp::VKEngine& vkp::VKEngine::operator=(const VKEngine&&) noexcept
{
    return *this;
}

void vkp::VKEngine::initVulkan() {}