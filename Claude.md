# MoldWing 项目开发日志

## 项目概述

**MoldWing** 是一个基于 CMake 和 vcpkg 的现代 C++ 项目，专注于 Vulkan 图形引擎开发。

- **语言标准**: C++17
- **构建系统**: CMake 3.16+
- **包管理**: vcpkg
- **支持平台**: Windows/Linux

## 项目结构

```
MoldWing/
├── CMakeLists.txt                    # 根 CMake 配置
├── CMakePresets.json                 # CMake 预设配置
├── vcpkg.json                        # 依赖管理
├── CMakeModules/                     # 自定义 CMake 模块
│   ├── doxygen.cmake                # 文档生成
│   ├── formatting.cmake             # 代码格式化
│   ├── dependency-graph.cmake       # 依赖图生成
│   └── compile-shaders.cmake        # Shader 编译模块
├── Modules/                          # 库模块
│   └── MoldWing/                    # MoldWing 命名空间
│       └── VulkanEngine/            # Vulkan 引擎动态库
├── Apps/                             # 应用程序
│   ├── DemoApp/                     # 简单控制台应用
│   ├── QtDemoApp/                   # Qt 图形应用（可选）
│   └── VulkanDemo/                  # Vulkan + ImGui 演示应用
├── Tests/                            # 单元测试
│   └── TEST.cpp                     # GTest 测试
└── builds/                           # 编译输出目录
```

## 核心依赖

### vcpkg 管理的依赖
```json
{
  "dependencies": [
    "gtest",                          // 单元测试框架
    "vulkan",                         // Vulkan 图形 API
    "vulkan-hpp",                     // Vulkan C++ 封装（已废弃，现集成在 vulkan-headers）
    "glfw3",                          // 窗口和输入管理
    {
      "name": "imgui",
      "features": ["glfw-binding", "vulkan-binding"]
    }
  ],
  "builtin-baseline": "80d54ff62d528339c626a6fbc3489a7f25956ade"
}
```

## VulkanEngine 动态库

### 模块位置
`Modules/MoldWing/VulkanEngine/`

### 架构设计

VulkanEngine 是一个完全使用 **vulkan-hpp**（C++ 风格的 Vulkan API）封装的模块化图形引擎库。

#### 组件列表

1. **Instance.h/cpp** - Vulkan 实例管理
   - 创建 Vulkan 实例
   - 管理扩展和验证层
   - RAII 自动清理

2. **Device.h/cpp** - 设备管理
   - 物理设备选择
   - 逻辑设备创建
   - 队列族查找（图形队列 + 呈现队列）
   - 结构体: `QueueFamilyIndices`

3. **Swapchain.h/cpp** - 交换链管理
   - 交换链创建和配置
   - 图像视图管理
   - 表面格式选择
   - 呈现模式配置

4. **RenderPass.h/cpp** - 渲染通道
   - 渲染通道创建
   - 帧缓冲管理
   - 颜色附件配置
   - 子通道依赖

5. **CommandBufferManager.h/cpp** - 命令缓冲管理
   - 命令池创建
   - 命令缓冲分配
   - 同步对象管理（信号量、栅栏）
   - 支持多帧并发（默认 2 帧）

6. **Engine.h/cpp** - 主引擎类
   - 整合所有组件
   - 提供简洁的初始化 API
   - 回调机制录制命令缓冲
   - 完整的帧渲染循环

#### 关键 API

```cpp
namespace VulkanEngine {

// 引擎配置
struct EngineConfig {
    std::string appName = "VulkanApp";
    uint32_t appVersion = VK_MAKE_VERSION(1, 0, 0);
    uint32_t width = 800;
    uint32_t height = 600;
    uint32_t maxFramesInFlight = 2;
};

// 主引擎类
class Engine {
public:
    Engine(GLFWwindow* window, const EngineConfig& config);

    // 获取组件
    Instance* getInstance();
    Device* getDevice();
    Swapchain* getSwapchain();
    RenderPass* getRenderPass();
    CommandBufferManager* getCommandBufferManager();

    // 渲染帧（使用回调机制）
    using RecordCommandBufferCallback =
        std::function<void(vk::CommandBuffer, uint32_t imageIndex)>;
    void drawFrame(RecordCommandBufferCallback recordCallback);

    void waitIdle();
};

} // namespace VulkanEngine
```

#### 使用示例

```cpp
// 1. 创建引擎
VulkanEngine::EngineConfig config;
config.appName = "My Vulkan App";
config.width = 1280;
config.height = 720;

auto engine = new VulkanEngine::Engine(window, config);

// 2. 渲染循环
while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    engine->drawFrame([&](vk::CommandBuffer cmd, uint32_t imageIndex) {
        // 录制渲染命令
        vk::RenderPassBeginInfo renderPassInfo{};
        renderPassInfo.renderPass = engine->getRenderPass()->getHandle();
        renderPassInfo.framebuffer = engine->getRenderPass()->getFramebuffers()[imageIndex];
        renderPassInfo.renderArea.extent = engine->getSwapchain()->getExtent();

        cmd.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
        // ... 渲染命令 ...
        cmd.endRenderPass();
    });
}

// 3. 清理
engine->waitIdle();
delete engine;
```

### 编译输出

- **Debug**: `VulkanEngined.dll` + `VulkanEngined.lib`
- **Release**: `VulkanEngine.dll` + `VulkanEngine.lib`
- **命名空间**: `MoldWing::VulkanEngine`

### CMake 集成

```cmake
# 在你的 CMakeLists.txt 中
find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)

target_link_libraries(YourApp
    PRIVATE
        MoldWing::VulkanEngine
        Vulkan::Vulkan
        glfw
)
```

## VulkanDemo 应用

### 位置
`Apps/VulkanDemo/`

### 功能
- 使用 VulkanEngine 库的最小化演示
- 集成 ImGui UI 框架
- 显示交互式演示窗口
- 实时 FPS 显示

### 代码统计
- **重构前**: 574 行（直接使用 Vulkan C API）
- **重构后**: 205 行（使用 VulkanEngine 库）
- **减少**: 64% 代码量

### 关键特性

```cpp
class VulkanDemo {
    GLFWwindow* window;
    VulkanEngine::Engine* engine;
    VkDescriptorPool imguiDescriptorPool;

    void initWindow();           // GLFW 窗口初始化
    void initVulkanEngine();     // 创建 VulkanEngine 实例
    void initImGui();            // ImGui 初始化
    void mainLoop();             // 主循环
    void drawFrame();            // 帧渲染
    void cleanup();              // 资源清理
};
```

### ImGui 集成

```cpp
// ImGui 初始化信息
ImGui_ImplVulkan_InitInfo init_info = {};
init_info.Instance = engine->getInstance()->getHandle();
init_info.PhysicalDevice = engine->getDevice()->getPhysicalDevice();
init_info.Device = engine->getDevice()->getHandle();
init_info.QueueFamily = engine->getDevice()->getQueueFamilyIndices().graphicsFamily.value();
init_info.Queue = engine->getDevice()->getGraphicsQueue();
init_info.RenderPass = engine->getRenderPass()->getHandle();

ImGui_ImplVulkan_Init(&init_info);
```

## CMake 构建系统

### 预设配置

**配置预设**:
- `vs2022-x64` - Visual Studio 2022 (x64)
- `ninja-multi-vcpkg` - Ninja Multi-Config

**编译预设**:
- `vs2022-x64-debug` / `vs2022-x64-release`
- `ninja-vcpkg-debug` / `ninja-vcpkg-release`

### 构建命令

```bash
# 配置
cmake --preset vs2022-x64

# 编译
cmake --build --preset vs2022-x64-debug

# 编译特定目标
cmake --build --preset vs2022-x64-debug --target VulkanEngine
cmake --build --preset vs2022-x64-debug --target VulkanDemo
```

### 输出目录

- **编译输出**: `builds/vs2022-x64/`
- **安装目录**: `install/vs2022-x64/bin/`
- **库后缀**: Debug 版本带 `d` 后缀（如 `VulkanEngined.dll`）

## 开发工具集成

### 代码质量工具

1. **clang-format** - 代码格式化
   - 配置文件: `.clang-format`
   - 使用: `cmake --build --preset ninja-vcpkg-format`

2. **Doxygen** - API 文档生成（可选）
   - 输出: `Docs/`

3. **Graphviz** - 依赖关系图（可选）
   - 输出: `Docs/DependencyGraph/`

### Shader 编译系统

**模块**: [compile-shaders.cmake](CMakeModules/compile-shaders.cmake)

自动化 GLSL/HLSL shader 编译成 SPIR-V 格式，支持 glslc 和 glslangValidator。

#### 功能特性

- 自动查找 Vulkan SDK 中的 shader 编译器（优先 glslc）
- 构建时增量编译（仅编译修改过的 shader）
- 支持 include 目录和预处理器定义
- 支持优化选项（Release 模式自动启用）
- 自动安装编译后的 .spv 文件

#### 使用方法

```cmake
# 在 CMakeLists.txt 中
compile_shaders(
    TARGET YourApp
    SOURCES
        shaders/shader.vert
        shaders/shader.frag
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders
    INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/shaders/include
    DEFINITIONS DEBUG_MODE
    OPTIMIZE
)

# 安装到输出目录
install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/shaders/
        DESTINATION bin/shaders
        FILES_MATCHING PATTERN "*.spv")
```

#### API 参数

- **TARGET**: 依赖 shader 的目标名称（必需）
- **SOURCES**: shader 源文件列表（必需）
- **OUTPUT_DIR**: 输出目录（默认: `${CMAKE_CURRENT_BINARY_DIR}/shaders`）
- **INCLUDE_DIRS**: shader include 目录列表
- **DEFINITIONS**: 预处理器定义
- **OPTIMIZE**: 启用优化（Debug 默认 OFF，Release 默认 ON）

#### 示例输出

```
-- Found glslc: C:/VulkanSDK/1.4.321.1/Bin/glslc.exe
-- Configured shader compilation for target: VulkanDemo
--   - Shaders: shader.vert;shader.frag
--   - Output: builds/vs2022-x64/Apps/VulkanDemo/shaders
...
Compiling shader: shader.vert -> shader.vert.spv
Compiling shader: shader.frag -> shader.frag.spv
```

#### VulkanDemo 示例

VulkanDemo 包含两个示例 shader:
- [shader.vert](Apps/VulkanDemo/shaders/shader.vert) - 顶点着色器（彩色三角形）
- [shader.frag](Apps/VulkanDemo/shaders/shader.frag) - 片段着色器

编译输出:
- `builds/vs2022-x64/Apps/VulkanDemo/shaders/shader.vert.spv`
- `builds/vs2022-x64/Apps/VulkanDemo/shaders/shader.frag.spv`
- 安装至: `install/vs2022-x64/bin/shaders/`

### IDE 支持

**VS Code 配置** (`.vscode/settings.json`):
```json
{
  "cmake.configureOnEdit": false,
  "cmake.configureArgs": []
}
```

## Git 提交历史

### 最新提交

**Commit c000d2d** - Refactor: Remove LibEx and rename MyLibs to MoldWing
- 删除 LibEx 示例模块
- 重命名 MyLibs → MoldWing
- 更新所有命名空间引用
- 删除 659 行不需要的代码

**Commit 1c187f1** - Implement Vulkan Engine Module
- 实现完整的 VulkanEngine 模块化库
- 使用 vulkan-hpp 封装
- 创建 6 个核心组件类
- 简化 VulkanDemo 使用

**Commit 2e0046f** - Add VulkanDemo: minimal Vulkan + GLFW + ImGui application
- 添加 Vulkan、GLFW3、ImGui 依赖
- 创建初始 VulkanDemo 应用
- 完整的 Vulkan 初始化代码

**Commit 7dc4bc3** - Initial commit
- 项目初始化

## 技术亮点

### 1. 现代 C++ 实践
- 使用 C++17 标准特性
- RAII 资源管理（智能指针）
- 强类型封装（vk::Handle 而非 VkHandle）
- Lambda 回调机制

### 2. 模块化设计
- 清晰的组件划分
- 单一职责原则
- 依赖注入模式
- 可复用的动态库

### 3. 跨平台支持
- 自动检测平台并配置 vcpkg 三元组
- Windows: `x64-windows`
- Linux: `x64-linux-dynamic`

### 4. 灵活的构建系统
- CMake Presets 支持
- 多种构建器（Visual Studio、Ninja）
- 多种配置（Debug、Release、RelWithDebInfo）

## 性能考虑

### 双缓冲机制
- 默认 `maxFramesInFlight = 2`
- 使用栅栏和信号量同步
- CPU 和 GPU 并行工作

### 资源管理
- 所有 Vulkan 对象在析构函数中自动清理
- 使用 `std::unique_ptr` 管理子组件
- 避免手动内存管理

## 已知限制

1. **窗口大小固定** - 当前不支持窗口调整大小
   - 解决方案: 需要实现交换链重建

2. **单队列族** - 假设图形队列和呈现队列相同
   - 对大多数现代 GPU 成立

3. **固定的渲染通道** - 单一颜色附件
   - 可扩展为支持深度缓冲、多重采样等

## 未来改进方向

### 短期
- [ ] 添加窗口调整大小支持
- [ ] 实现深度缓冲
- [ ] 添加验证层支持（Debug 模式）
- [ ] 错误处理改进

### 中期
- [ ] 图形管线封装
- [ ] 纹理加载和管理
- [ ] 顶点缓冲和索引缓冲管理
- [ ] Uniform Buffer 对象（UBO）

### 长期
- [ ] 材质系统
- [ ] 着色器管理
- [ ] 场景图
- [ ] 多线程命令缓冲录制

## 环境要求

### 必需
- CMake 3.16+
- C++17 编译器
  - MSVC 19.14+ (Visual Studio 2022)
  - GCC 7+
  - Clang 5+
- Vulkan SDK 1.3+
- vcpkg 包管理器

### 可选
- Doxygen（文档生成）
- Graphviz（依赖图）
- clang-format（代码格式化）

## 构建和运行

```bash
# 1. 克隆项目
git clone <repository-url>
cd MoldWing

# 2. 设置 vcpkg（如果未设置）
export VCPKG_ROOT=/path/to/vcpkg

# 3. 配置
cmake --preset vs2022-x64

# 4. 编译
cmake --build --preset vs2022-x64-debug

# 5. 运行
./install/vs2022-x64/bin/VulkanDemo.exe
```

## 许可证

（待添加许可证信息）

## 贡献者

- gukazma <18017158729@163.com>
- Claude Sonnet 4.5 (AI Assistant)

---

**最后更新**: 2025-12-23
**文档版本**: 1.0
**项目版本**: latest
