# Vulkan Triangle

## 项目声明

- 本项目是作者学习 **Vulkan** 图形编程的练习项目，旨在深入理解 Vulkan 渲染管线及资源管理。
- 部分代码由 **Vibe coding生成**，可能含有不足的地方及问题，注意甄别。

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

### 使用 MSYS2 (UCRT64 环境)

在 Windows 上，推荐使用 **MSYS2** 的 **UCRT64** 环境快速安装所有必需工具和库。  
安装 [MSYS2](https://www.msys2.org/) 后，启动 **UCRT64** 终端，执行以下命令：

```bash
pacman -Syu                 # 更新包数据库
pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-vulkan-headers \
          mingw-w64-ucrt-x86_64-vulkan-loader \
          mingw-w64-ucrt-x86_64-glfw \
          mingw-w64-ucrt-x86_64-glm \
          mingw-w64-ucrt-x86_64-gdb
```
---
## 构建与运行
### 1. 克隆项目
```
git clone <repository-url>
cd <project-directory>
```

### 2.使用 CMake 构建
```
mkdir build && cd build
cmake ..
cmake --build .
```

### 3.运行
```
./VkProject.exe
```

## 注意事项
### 验证层：
在 Debug 模式下默认启用（NDEBUG 未定义）。若 Vulkan SDK 未安装验证层，程序将抛出异常。可在 Release 模式下禁用。

### 着色器路径：
代码中通过宏 SHADER_DIR 指定 .spv 文件所在目录，若未定义则默认 "shaders/"。请确保运行目录可访问该路径。

### 窗口大小：
当前窗口大小固定（800×600），不可调整。如需可调整，可修改 GLWindow::initWindow() 中的 GLFW_RESIZABLE 为 GLFW_TRUE，并实现 recreateSwapChain 逻辑（已支持）