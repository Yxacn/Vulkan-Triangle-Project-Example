cmake_minimum_required(VERSION 3.20)
project(VkProject VERSION 1.0.0 LANGUAGES CXX)

# 查找 Vulkan
find_package(Vulkan REQUIRED)
find_package(GLM REQUIRED)

# 创建 GLFW 导入目标
add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include"
)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Application.cpp
    src/GLWindow.cpp
    src/VKEngine.cpp
)

# 设置 C++ 标准
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

# 包含自定义头文件目录
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# 链接 Vulkan 和 GLFW
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    stdc++exp
)