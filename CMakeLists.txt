cmake_minimum_required(VERSION 3.20)
project(VkProject VERSION 1.0.0 LANGUAGES CXX)

find_package(Vulkan REQUIRED)
find_package(GLM REQUIRED)

add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include"
)

# 着色器编译（输出到构建目录）
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

function(compile_shader SHADER_SRC SHADER_SPV)
    add_custom_command(
        OUTPUT ${SHADER_SPV}
        COMMAND glslc -o ${SHADER_SPV} ${SHADER_SRC}
        DEPENDS ${SHADER_SRC}
        COMMENT "Compiling shader ${SHADER_SRC}"
    )
endfunction()

compile_shader(${SHADER_SRC_DIR}/shader.vert ${SHADER_OUT_DIR}/vert.spv)
compile_shader(${SHADER_SRC_DIR}/shader.frag ${SHADER_OUT_DIR}/frag.spv)

add_custom_target(shaders DEPENDS ${SHADER_OUT_DIR}/vert.spv ${SHADER_OUT_DIR}/frag.spv)

# 可执行文件
add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Application.cpp
    src/GLWindow.cpp
    src/VKEngine.cpp
    src/VulkanContext.cpp
    src/SwapChain.cpp
    src/RenderPassPipeline.cpp
    src/FrameBufferManager.cpp
    src/BufferManager.cpp
    src/CommandManager.cpp
    src/SyncManager.cpp
)

add_dependencies(${PROJECT_NAME} shaders)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
)