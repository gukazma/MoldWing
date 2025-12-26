# MoldWing 项目开发日志

## 项目概述

**MoldWing** 是一款基于 DiligentEngine + Qt 的倾斜摄影三维模型编辑和修复软件。

- **语言标准**: C++17
- **构建系统**: CMake 3.20+
- **包管理**: vcpkg
- **渲染引擎**: DiligentEngine
- **UI框架**: Qt 6 Widgets
- **支持平台**: Windows/Linux

## 产品目标

- 操作简单的专业级模型修复工具
- 支持倾斜摄影三维模型（OBJ/OSGB）
- 多种选择方式：框选、刷选、套索、连通选择
- 类Photoshop的纹理编辑功能
- 完整的撤销/重做系统（QUndoStack）
- 专业的Docking界面（QDockWidget）

## 研发文档

- **研发方案**: [Docs/MoldWing-Development-Plan.md](Docs/MoldWing-Development-Plan.md)
- **任务拆分**: [Docs/MoldWing-Task-Breakdown.md](Docs/MoldWing-Task-Breakdown.md)

## 技术栈

| 组件 | 技术 |
|-----|------|
| UI框架 | **Qt 6 Widgets** |
| 渲染引擎 | DiligentEngine |
| 几何处理 | CGAL + Eigen3 |
| 模型加载 | assimp |
| 日志系统 | **spdlog** |

## 项目结构

```
MoldWing/
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
├── ThirdParty/
│   └── DiligentEngine/
├── Apps/
│   ├── HelloTriangle/        # DiligentEngine 测试
│   └── MoldWing/             # 主应用程序
│       ├── CMakeLists.txt
│       └── src/
│           ├── main.cpp
│           ├── MainWindow.hpp/cpp
│           ├── Render/DiligentWidget.hpp/cpp
│           ├── Core/
│           ├── IO/
│           ├── Selection/
│           ├── Texture/
│           └── UI/
├── Docs/
│   ├── MoldWing-Development-Plan.md
│   └── MoldWing-Task-Breakdown.md
└── Builds/
```

## 核心依赖 (vcpkg)

```json
{
  "dependencies": [
    "gtest",
    "assimp",
    "cgal",
    "eigen3",
    "stb",
    "spdlog",
    { "name": "qtbase", "features": ["widgets", "gui", "opengl"] }
  ]
}
```

## 开发里程碑

| 里程碑 | 功能 | 状态 |
|-------|------|------|
| M1 | 基础框架（Qt+DiligentEngine集成） | 🔄 75% |
| M2 | 框选 | ⏳ 待开始 |
| M3 | 刷选 | ⏳ 待开始 |
| M4 | 套索选择 | ⏳ 待开始 |
| M5 | 连通选择 | ⏳ 待开始 |
| M6 | UV/纹理视图 | ⏳ 待开始 |
| M7 | 基础绘制（画笔/橡皮/填充） | ⏳ 待开始 |
| M8 | 克隆/修复工具 | ⏳ 待开始 |
| M9 | 颜色调整 | ⏳ 待开始 |
| M10 | 几何修复（孔洞/非流形） | ⏳ 待开始 |
| M11 | 导入导出（OBJ/OSGB） | ⏳ 待开始 |

### M1 已完成模块
- ✅ 项目骨架搭建（CMake + vcpkg）
- ✅ Qt + DiligentEngine 集成（DiligentWidget）
- ✅ 网格数据结构（Vertex, MeshData, BoundingBox）
- ✅ 模型加载（MeshLoader + assimp）
- ✅ 网格渲染（MeshRenderer + PSO）
- ✅ 相机系统（OrbitCamera）
- ✅ 主窗口基础布局（菜单/工具栏/状态栏）
- 🔄 撤销系统（QUndoStack 已集成，QUndoView 待完成）
- ⏳ DockWidget 布局（待完成）
- 🔄 **日志系统（spdlog + 崩溃捕获）- 开发中**

## 构建命令

```bash
# 配置
cmake --preset vs2022-x64

# 编译
cmake --build --preset vs2022-x64-debug

# 编译 MoldWing
cmake --build --preset vs2022-x64-debug --target MoldWing
```

## 开发策略

**阶段一：功能开发**
- 所有代码写在 `Apps/MoldWing/` 中
- 单个 `MoldWing.exe` 输出
- 功能可用性优先，不考虑模块化

**阶段二：模块化封装**（验收通过后）
- 拆分几何处理库到 `Modules/`
- 提取可复用组件
- 单元测试覆盖

## Qt 技术要点

### CMake 配置
```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui OpenGL)

set_target_properties(MoldWing PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)

target_link_libraries(MoldWing PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
)
```

### 撤销系统
```cpp
#include <QUndoStack>
m_undoStack = new QUndoStack(this);
m_undoStack->push(new SelectFacesCommand(...));
```

### Docking 布局
```cpp
QDockWidget* dock = new QDockWidget(tr("工具"), this);
addDockWidget(Qt::LeftDockWidgetArea, dock);
tabifyDockWidget(dock1, dock2);  // 标签化
```

## 快捷键

| 快捷键 | 功能 |
|-------|------|
| Ctrl+Z | 撤销 |
| Ctrl+Y | 重做 |
| Ctrl+O | 打开文件 |
| Ctrl+S | 保存 |
| Ctrl+A | 全选 |
| Ctrl+D | 取消选择 |
| Ctrl+Shift+I | 反选 |
| B | 画笔工具 |
| E | 橡皮擦 |
| S | 克隆图章 |
| Delete | 删除选中面 |

---

**最后更新**: 2025-12-26
**项目版本**: 0.1-dev (Qt 方案)
