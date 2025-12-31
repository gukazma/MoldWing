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
    "osg",
    "proj",
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
| M7 | 克隆图章工具 | ✅ 100% |
| M7.5 | OBJ 带纹理导出 | ✅ 100% |
| M8 | 多模型选择与编辑（方案B：复合面ID） | ✅ 100% |
| M9 | 模型选择导出 | ✅ 100% |
| M10 | OSGB 标准瓦片导出 | ✅ 100% |
| M11 | 橡皮擦工具 | ⏳ 待开始 |
| M12 | 颜色调整 | ⏳ 待开始 |
| M13 | 几何修复（孔洞/非流形） | ⏳ 待开始 |

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

### M7 已完成模块：克隆图章工具

**M7.1 屏幕-纹理映射验证**
- ✅ ScreenToTextureMapper 集成到 DiligentWidget
- ✅ Alt+点击显示 UV 坐标和像素位置（状态栏）
- ✅ Moller-Trumbore 射线-三角形求交算法
- ✅ 重心坐标插值计算精确纹理坐标

**M7.2 实时纹理绘制**
- ✅ TextureEditBuffer 创建和初始化
- ✅ MeshRenderer::updateTextureFromBuffer() GPU 纹理更新
- ✅ 脏区域跟踪和高效同步

**M7.3 拖拽绘制连续性**
- ✅ InteractionMode::TextureEdit 模式
- ✅ 纹理编辑鼠标事件处理
- ✅ beginTexturePaint/updateTexturePaint/endTexturePaint 状态管理

**M7.4 克隆源选择**
- ✅ Alt+左键设置克隆源
- ✅ cloneSourceSet 信号通知状态栏
- ✅ 克隆源坐标存储

**M7.5 克隆像素绘制**
- ✅ 首次绘制点记录用于偏移计算
- ✅ 源-目标偏移动态计算
- ✅ 圆形笔刷区域像素复制
- ✅ 纹理空间实际操作（保持原生分辨率）

**M7.6 撤销/重做支持**
- ✅ TextureEditCommand 记录像素变更
- ✅ 笔画开始时创建命令
- ✅ 笔画结束时提交到 QUndoStack
- ✅ undo/redo 恢复像素数据
- ✅ syncTextureToGPU() 撤销后同步渲染

**M7.7 完整 UI 集成**
- ✅ Texture 菜单（进入/退出编辑模式、保存纹理）
- ✅ 快捷键支持
- ✅ 状态栏编辑模式提示
- ✅ saveTexture() 保存到 PNG/JPG/BMP/TGA

### M7.5 已完成：OBJ 带纹理导出

**功能目标**：将编辑后的模型和纹理导出为新的 OBJ 文件

- ✅ MeshExporter 类（IO/MeshExporter.hpp/cpp）
- ✅ OBJ 文件写入（顶点/法向量/UV/面索引）
- ✅ MTL 材质文件生成
- ✅ 编辑后纹理导出（保持原格式 PNG/JPG/TGA）
- ✅ File → Export As... 菜单（Ctrl+Shift+S）
- ✅ 纹理与 OBJ 同目录，MTL 使用相对路径
- ✅ UV 坐标翻转修复（导入时 assimp FlipUVs，导出时翻回）

### M8 已完成：多模型选择与编辑（方案B：复合面ID）

**功能目标**：支持跨模型面选择和纹理编辑

**方案说明**：采用复合面ID（高8位=meshId, 低24位=faceId）实现跨模型统一选择

**M8.1 图层树基础** ✅
- ✅ 图层树 Dock 面板（QDockWidget + QTreeWidget）
- ✅ 多 OBJ 追加导入 + 异步加载
- ✅ 图层显示/隐藏切换
- ✅ MeshInstance 架构 + 多模型渲染

**M8.1.1 文件夹批量导入** ✅

**功能目标**：一键导入倾斜摄影瓦片目录下的所有 OBJ 模型

**数据结构示例**：
```
D:\datas\obj\baiguan\data\
├── Tile_+031_+056\
│   ├── Tile_+031_+056.obj
│   ├── Tile_+031_+056.mtl
│   ├── Tile_+031_+056_0.jpg
│   └── Tile_+031_+056_1.jpg
├── Tile_+032_+056\
│   └── ...
└── ...
```

**实现步骤**：

| 步骤 | 功能 | 状态 |
|------|------|------|
| F1 | 菜单项 "Import Folder..." (Ctrl+Shift+O) | ✅ |
| F2 | QFileDialog::getExistingDirectory 选择目录 | ✅ |
| F3 | 递归扫描目录下所有 .obj 文件 (QDirIterator) | ✅ |
| F4 | 批量异步加载 + 进度对话框 | ✅ |
| F5 | 自动添加到图层树 + 相机适配 | ✅ |

**技术方案**：
```cpp
// MainWindow 新增成员
QAction* m_importFolderAction;
QStringList m_pendingFiles;      // 待加载的文件列表
int m_loadedCount = 0;           // 已加载计数
QProgressDialog* m_batchProgressDialog;

// 菜单项
m_importFolderAction = m_fileMenu->addAction(tr("Import &Folder..."));
m_importFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));

// 实现流程
void MainWindow::onImportFolder() {
    // 1. 选择目录
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Folder"));

    // 2. 递归扫描 .obj 文件
    QDirIterator it(dir, {"*.obj"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        m_pendingFiles.append(it.next());
    }

    // 3. 显示进度对话框
    m_batchProgressDialog = new QProgressDialog(
        tr("Loading %1 models...").arg(m_pendingFiles.size()),
        tr("Cancel"), 0, m_pendingFiles.size(), this);

    // 4. 开始加载第一个文件
    loadNextPendingFile();
}

void MainWindow::loadNextPendingFile() {
    if (m_pendingFiles.isEmpty()) {
        // 全部完成，适配相机
        m_viewport3D->camera().fitToScene(combinedBounds);
        return;
    }

    QString file = m_pendingFiles.takeFirst();
    // 复用现有的异步加载机制
    QFuture<std::shared_ptr<MeshData>> future = QtConcurrent::run([file]() {
        MeshLoader loader;
        return loader.load(file);
    });
    m_loadWatcher.setFuture(future);
}

void MainWindow::onModelLoadComplete() {
    // 添加到场景
    m_viewport3D->addMesh(loadedMesh);
    addLayerItem(fileName, meshIndex);

    // 更新进度
    m_loadedCount++;
    m_batchProgressDialog->setValue(m_loadedCount);

    // 加载下一个
    loadNextPendingFile();
}
```

**验收方式**：
- 选择 `D:\datas\obj\baiguan\data` 目录
- 自动加载所有瓦片 OBJ
- 进度条显示加载进度
- 图层树显示所有模型
- 相机自动适配场景范围

**M8.2 方案B实现步骤**

| 步骤 | 功能 | 状态 | 验收方式 |
|------|------|------|---------|
| B1 | CompositeId 工具类 | ✅ | 编译时 static_assert 通过 |
| B2 | 多模型面拾取 | ✅ | Alt+点击显示 Mesh:Face |
| B3 | 跨模型框选 | ✅ | 框选多模型都被选中 |
| B4 | 跨模型选择渲染 | ✅ | 多模型高亮显示 |
| B5 | 多纹理编辑缓冲区 | ✅ | 跨模型克隆生效 |
| B6 | 模型选择导出 | ✅ | 勾选导出正确 |

**已完成**：
- ✅ B1: CompositeId 工具类（Core/CompositeId.hpp）- 编码/解码/验证函数
- ✅ B2: FacePicker 多模型复合ID渲染 - 着色器输出 (meshId << 24) | faceId
- ✅ B3: 跨模型框选 - SelectionSystem 存储复合ID + 状态栏显示跨模型统计
- ✅ B4: SelectionRenderer 多模型高亮 - addMesh/clearMeshes + 解码复合ID渲染
- ✅ B5: 多纹理编辑缓冲区 - MeshInstance.editBuffers + 跨模型克隆图章
  - MeshInstance 扩展：每个模型持有自己的 editBuffers 向量
  - 克隆源记录 meshId：m_cloneSourceMeshId + m_cloneFirstDestMeshId
  - updateTexturePaint：解码复合ID，使用正确的 mesh 和 editBuffer
  - paintBrushAtPosition：支持跨模型像素复制（源和目标可在不同模型）
  - syncTextureToGPU：同步所有模型的纹理到 GPU
- ✅ B6: 模型选择导出 - ExportDialog + 批量OBJ导出
  - ExportDialog 对话框：模型勾选列表 + 面数显示
  - 全选/取消全选按钮
  - 输出目录选择
  - 批量导出进度显示
  - MeshData.sourcePath 保存源文件路径
- ✅ 所有选择模式多模型支持：
  - 框选：直接使用 FacePicker 返回的复合ID
  - 刷选：直接使用 FacePicker 返回的复合ID
  - 套索：解码复合ID查找正确mesh计算面中心
  - 连通选择：解码复合ID使用正确mesh邻接数据 + BFS结果转复合ID

**实现依赖**：B1 → B2 → B3 → B4 → B5 → B6 ✅ 全部完成

### M9 已完成：模型选择导出

**功能目标**：导出对话框让用户勾选要导出的模型

- ✅ 模型勾选列表（显示面数）
- ✅ 全选/全不选按钮
- ✅ 输出目录选择
- ✅ 批量 OBJ 导出

**说明**：M9 功能已在 M8 B6 中实现（ExportDialog）

### M10 已完成：OSGB 标准瓦片导出

**功能目标**：将 OBJ 导出为标准倾斜摄影瓦片目录结构，支持 LOD 和地理坐标系

**新增依赖**：
- osg (OpenSceneGraph) - OSGB 文件写入
- proj (PROJ) - 地理坐标系转换

**已完成功能**：
- ✅ File → Export OSGB 菜单项（Ctrl+Shift+G）
- ✅ OSGBExportDialog 导出对话框
  - 模型选择列表
  - 源/目标坐标系选择（EPSG 代码）
  - SRS Origin 设置
  - LOD 配置（层数、简化比例）
- ✅ CoordinateSystem 坐标系管理
  - 常用 EPSG 代码（4326/4490/UTM）
  - PROJ 坐标转换
- ✅ OSGBExporter 导出器
  - MeshData → osg::Geometry 转换
  - 纹理绑定到 StateSet
  - LOD 自动生成（osgUtil::Simplifier）
  - 标准目录结构（Data/Tile_+XXX_+YYY/）
  - metadata.xml 生成
- ✅ 批量异步导出 + 进度显示

**新增文件**：
- IO/CoordinateSystem.hpp/cpp
- IO/OSGBExporter.hpp/cpp
- UI/OSGBExportDialog.hpp/cpp

### M11 待实现：橡皮擦工具

- 恢复到编辑前的原始纹理
- 笔刷大小调节
- 撤销支持

### 渲染系统改进（2025-12-30）

**光照模型优化**
- ✅ 纹理模式：完全无光照（Unlit），直接输出纹理原色
- ✅ 白模模式：MeshLab 风格 Headlight + Lambertian（无高光）
- ✅ 环境光 0.15 + 漫反射 0.85，基础色 (0.8, 0.8, 0.8)

**渲染模式切换**
- ✅ 白模模式切换（View → White Model，快捷键 W）
- ✅ 线框模式切换（View → Wireframe，快捷键 F）
- ✅ 线框 PSO（FILL_MODE_WIREFRAME + CULL_MODE_NONE）

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
| Ctrl+Shift+O | 导入文件夹（批量导入） |
| Ctrl+S | 保存 |
| Ctrl+Shift+S | 导出（Export As） |
| Ctrl+Shift+G | 导出 OSGB |
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
| W | 白模模式切换 |
| F | 线框模式切换 |
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

**最后更新**: 2025-12-31
**项目版本**: 0.1-dev (Qt 方案) - M10 OSGB 导出完成
