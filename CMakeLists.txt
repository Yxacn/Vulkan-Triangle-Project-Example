cmake_minimum_required(VERSION 3.20)
project(VkProject VERSION 1.0.0 LANGUAGES CXX)

# 查找包
find_package(Vulkan REQUIRED)
find_package(GLM REQUIRED)

# 创建 GLFW 导入目标
add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include"
)

# 着色器编译
set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_DIR})

function(compile_shader SHADER_SRC SHADER_SPV)
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND glslc -o ${SHADER_SPV} ${SHADER_SRC}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader ${SHADER_SRC}"
    )
endfunction()

compile_shader(${SHADER_DIR}/shader.vert ${SHADER_DIR}/vert.spv)
compile_shader(${SHADER_DIR}/shader.frag ${SHADER_DIR}/frag.spv)

add_custom_target(shaders DEPENDS ${SHADER_DIR}/vert.spv ${SHADER_DIR}/frag.spv)

# 创建可执行文件
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Application.cpp
    src/GLWindow.cpp
    src/VKEngine.cpp
)

add_dependencies(${PROJECT_NAME} shaders)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# 传递着色器目录宏，便于运行时定位文件
target_compile_definitions(${PROJECT_NAME} PRIVATE
    SHADER_DIR="${SHADER_DIR}"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
    stdc++exp
)