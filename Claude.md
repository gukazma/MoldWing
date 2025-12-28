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
    { "name": "qtbase", "features": ["widgets", "gui", "opengl", "jpeg", "png"] }
  ]
}
```

## 开发里程碑

| 里程碑 | 功能 | 状态 |
|-------|------|------|
| M1 | 基础框架（Qt+DiligentEngine集成） | ✅ 100% |
| M2 | 框选 | ✅ 100% |
| M3 | 刷选 | ✅ 100% |
| M4 | 套索选择 | ✅ 100% |
| M5 | 连通选择 | ✅ 100% |
| M6 | 纹理渲染 + 编辑模式框架 | ✅ 100% |
| M7 | 克隆图章工具 | ⏳ 待开始 |
| M8 | 橡皮擦工具 | ⏳ 待开始 |
| M9 | 颜色调整 | ⏳ 待开始 |
| M10 | 几何修复（孔洞/非流形） | ⏳ 待开始 |
| M11 | 导入导出（OBJ/OSGB） | ⏳ 待开始 |

### 纹理编辑方案说明

**屏幕空间投影编辑**：与传统 UV 展开方案不同，本项目采用"屏幕空间投影"方式进行纹理编辑：
- 用户在 3D 视图上直接操作（编辑模式下锁定视角）
- 系统将屏幕坐标映射到纹理坐标
- 实际编辑在纹理空间进行（保持原生分辨率）
- 所见即所得，避免了传统 UV 展开的复杂性

### M1 已完成模块
- ✅ 项目骨架搭建（CMake + vcpkg）
- ✅ Qt + DiligentEngine 集成（DiligentWidget）
- ✅ 网格数据结构（Vertex, MeshData, BoundingBox）
- ✅ 模型加载（MeshLoader + assimp）
- ✅ 网格渲染（MeshRenderer + PSO）
- ✅ 相机系统（OrbitCamera）
- ✅ 主窗口基础布局（菜单/工具栏/状态栏）
- ✅ 撤销系统（QUndoStack + QUndoView）
- ✅ DockWidget 布局（工具/属性/历史面板 + 布局保存）
- ✅ 日志系统（spdlog + Windows MiniDump 崩溃捕获）

### M2 已完成模块
- ✅ GPU 拾取系统（FacePicker + SV_PrimitiveID 渲染）
- ✅ 选择系统核心（SelectionSystem + Qt 信号）
- ✅ 框选实现（SelectionBoxRenderer + 鼠标交互）
- ✅ 选择可视化（SelectionRenderer + DepthBias 解决 z-fighting）
- ✅ 选择操作快捷键（Ctrl+A/D, Ctrl+Shift+I）
- ✅ 撤销支持（SelectFacesCommand）

### M3 已完成模块
- ✅ 刷选圆形光标渲染（BrushCursorRenderer + NDC 空间 2D 叠加）
- ✅ 圆形区域面 ID 读取（FacePicker::readFaceIDsInCircle）
- ✅ 刷选状态管理（beginBrushSelect/updateBrushSelect/endBrushSelect）
- ✅ 连续拖拽选择（拖拽过程中累积选择面）
- ✅ [ ] 快捷键调整笔刷大小
- ✅ 属性面板笔刷大小滑块（QSlider + QSpinBox 双向同步）

### M4 已完成模块
- ✅ 套索路径渲染（LassoRenderer + NDC 空间 2D 叠加）
- ✅ 鼠标轨迹记录（QPolygonF + 采样点简化）
- ✅ 套索状态管理（beginLassoSelect/updateLassoSelect/endLassoSelect）
- ✅ 点在多边形内判断（QPolygonF::containsPoint）
- ✅ 面中心投影到屏幕空间（OrbitCamera::worldToScreen）
- ✅ 撤销支持（SelectFacesCommand）

### M5 已完成模块
- ✅ 面-面邻接表构建（MeshData::buildAdjacency）
- ✅ 面法向量计算（MeshData::computeFaceNormals）
- ✅ 连通选择 BFS 算法（SelectionSystem::selectLinked）
- ✅ 基于角度的连通选择（SelectionSystem::selectByAngle）
- ✅ 选择扩展功能（SelectionSystem::growSelection）
- ✅ 选择收缩功能（SelectionSystem::shrinkSelection）
- ✅ 属性面板角度阈值滑块（QSlider + QDoubleSpinBox）
- ✅ 菜单快捷键（Ctrl++ 扩展选择, Ctrl+- 收缩选择）

### M6 已完成模块：纹理渲染 + 编辑模式框架

**M6.1 纹理渲染基础**
- ✅ Material 数据结构（漫反射纹理路径、纹理ID）
- ✅ TextureData（QImage 加载 PNG/JPG/TGA）
- ✅ MeshData 扩展（材质列表、面-材质映射）
- ✅ MeshLoader 扩展（读取 MTL 材质文件）
- ✅ MeshRenderer 改造（GPU 纹理 + 采样器 + 着色器采样）
- ✅ Qt JPEG/PNG 插件部署（qjpeg + libjpeg-turbo）

**M6.2 屏幕-纹理映射系统**
- ✅ FacePicker 扩展（单点拾取 pickPoint + 三角形数据获取）
- ✅ ScreenToTextureMapper（Moller-Trumbore 射线-三角形求交）
- ✅ 重心坐标插值 UV（barycentric → texture coordinates）

**M6.3 编辑模式框架**
- ✅ TextureEditMode（进入/退出、视角锁定、状态栏提示）
- ✅ TextureEditBuffer（CPU 纹理副本、脏区域跟踪、GPU 同步）
- ✅ TextureEditCommand（QUndoCommand，矩形区域撤销）
- ✅ TextureEditTool（工具基类：笔刷大小、硬度、鼠标事件）
- ✅ 手动保存功能（saveTexture 写入 PNG/JPG/BMP/TGA）

### M7 待实现：克隆图章工具

- Alt+点击选取克隆源（屏幕坐标→纹理坐标）
- 拖拽进行克隆绘制
- 纹理空间实际操作（保持原生分辨率）
- 撤销支持

### M8 待实现：橡皮擦工具

- 恢复到编辑前的原始纹理
- 笔刷大小调节
- 撤销支持

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
| Ctrl++ | 扩展选择 |
| Ctrl+- | 收缩选择 |
| [ | 减小笔刷大小 |
| ] | 增大笔刷大小 |
| B | 画笔工具 |
| E | 橡皮擦 |
| S | 克隆图章 |
| Delete | 删除选中面 |

## 鼠标操作

| 操作 | 功能 |
|------|------|
| 鼠标中键拖动 | 旋转视角 |
| Shift + 鼠标中键拖动 | 平移视角 |
| 滚轮 | 缩放（缩放到光标位置） |
| 左键拖动（选择模式） | 选择（框选/刷选/套索） |
| Ctrl + 左键拖动 | 增选 |
| Shift + 左键拖动 | 减选 |

---

**最后更新**: 2025-12-28
**项目版本**: 0.1-dev (Qt 方案) - M6 纹理渲染+编辑框架完成，下一步 M7 克隆图章
