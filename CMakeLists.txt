cmake_minimum_required(VERSION 3.20)
project(VkProject VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(GLM REQUIRED)

add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/glfw/lib/libglfw3.a"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/glfw/include"
)

set(SHADER_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(SHADER_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SHADER_OUT_DIR}")

find_program(GLSLC_EXECUTABLE glslc REQUIRED)

function(compile_shader SHADER_SRC SHADER_SPV)
    add_custom_command(
        OUTPUT "${SHADER_SPV}"
        COMMAND "${GLSLC_EXECUTABLE}" -o "${SHADER_SPV}" "${SHADER_SRC}"
        DEPENDS "${SHADER_SRC}"
        COMMENT "Compiling shader ${SHADER_SRC}"
        VERBATIM
    )
endfunction()

compile_shader("${SHADER_SRC_DIR}/shader.vert" "${SHADER_OUT_DIR}/vert.spv")
compile_shader("${SHADER_SRC_DIR}/shader.frag" "${SHADER_OUT_DIR}/frag.spv")

add_custom_target(shaders DEPENDS "${SHADER_OUT_DIR}/vert.spv" "${SHADER_OUT_DIR}/frag.spv")

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

set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)

file(TO_CMAKE_PATH "${SHADER_OUT_DIR}" SHADER_DIR_DEF)
target_compile_definitions(${PROJECT_NAME} PRIVATE SHADER_DIR="${SHADER_DIR_DEF}/")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /std:c++20 /W4 /permissive-)
else()
    # C++20 指定初始化器省略的字段按标准零初始化，
    target_compile_options(${PROJECT_NAME} PRIVATE -std=c++20 -Wall -Wextra -Wpedantic
                                                     -Wno-missing-field-initializers)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glm::glm
    glfw
)
