# Vulkan Triangle

这是一个基于 **Vulkan** 和 **GLFW** 的图形渲染示例项目，用于绘制一个带有顶点颜色的彩色三角形，并支持模型‑视图‑投影矩阵变换。项目采用现代 C++17 风格，使用 RAII 和移动语义管理 Vulkan 资源，结构清晰，适合学习 Vulkan 基础渲染管线。

---

## 特性

- 使用 **Vulkan 1.3** API
- 基于 **GLFW** 的窗口管理（无 OpenGL 上下文）
- 完整的交换链、渲染通道、图形管线、帧缓冲
- 顶点着色器与片段着色器（GLSL 450），支持 SPIR‑V 编译
- Uniform Buffer Object (UBO) 实现模型‑视图‑投影矩阵
- 双缓冲帧同步（信号量 + 围栏）
- 支持交换链重建（窗口大小变化自动适配）
- Debug 模式下默认启用 Vulkan 验证层
- 现代 C++17 设计，禁用拷贝，支持移动语义

---

## 依赖项

- **[Vulkan SDK](https://vulkan.lunarg.com/)** (≥ 1.3)  
  提供 Vulkan 头文件、库、验证层以及 `glslc` 着色器编译工具。
- **[GLFW](https://www.glfw.org/)** (≥ 3.3)  
  用于创建窗口并管理 Vulkan 表面。
- **[GLM](https://github.com/g-truc/glm)** (≥ 0.9.9)  
  用于数学运算（矩阵、向量）。
- **C++17 编译器**（GCC 9+ / Clang 10+ / MSVC 2019+）
- **CMake** (≥ 3.10)

---

## 安装依赖

### 1. 安装 Vulkan SDK

- **Windows**  
  下载并运行 [Vulkan SDK 安装程序](https://vulkan.lunarg.com/sdk/home)。  
  安装后，确保环境变量 `VULKAN_SDK` 指向安装目录（安装程序通常会自动设置）。  
  将 `%VULKAN_SDK%/Bin` 添加到 `PATH` 以便使用 `glslc`。

- **Linux (Ubuntu/Debian)**  
  可从 LunarG 官网下载 `.tar.gz` 包并解压，或使用包管理器安装（版本可能较旧）：  
  ```bash
  sudo apt update
  sudo apt install vulkan-sdk