// VkCheck.hpp
#pragma once

#include <stdexcept>
#include <string>

#include <vulkan/vulkan.h>

namespace vkp
{
    // 统一 Vulkan 返回码检查：失败时抛出带错误码的异常，避免各调用点遗漏处理
    inline void checkVk(VkResult result, const char* message)
    {
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error(std::string(message) + " (VkResult " +
                                     std::to_string(static_cast<int>(result)) + ")");
        }
    }
} // namespace vkp
