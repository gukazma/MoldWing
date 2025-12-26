# MoldWing 研发方案

> **版本**: 2.0
> **创建日期**: 2025-12-26
> **目标**: 开发一款操作简单的倾斜摄影三维模型编辑和修复软件

---

## 一、开发策略

### 核心原则：功能优先，验收后再模块化

```
┌─────────────────────────────────────────────────────────────┐
│  开发策略                                                    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  阶段一：功能开发（当前阶段）                                  │
│    └─ 所有代码写在 Apps/MoldWing/ 中                         │
│    └─ 单个 MoldWing.exe 输出                                │
│    └─ 功能可用性优先                                         │
│                                                             │
│  阶段二：模块化封装（验收通过后）                              │
│    └─ 拆分几何处理库到 Modules/                              │
│    └─ 提取可复用组件                                         │
│    └─ 单元测试覆盖                                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 二、技术栈

| 层级 | 技术选择 | 职责 |
|------|---------|------|
| **渲染层** | DiligentEngine | 3D模型渲染、GPU拾取 |
| **UI层** | **Qt 6 (Widgets)** | 专业级桌面UI、Docking布局 |
| **几何处理层** | CGAL + Eigen3 | 网格修复、布尔运算、孔洞填充 |
| **模型IO层** | assimp | OBJ/FBX/glTF/OSGB 导入导出 |
| **图像处理** | stb / Qt | 纹理读写 |

### 技术栈变更说明

| 原方案 | 新方案 | 原因 |
|-------|-------|------|
| GLFW + ImGui | **Qt Widgets** | 原生Docking支持、更专业的UI |
| ImGui Docking分支 | **QDockWidget** | 无需替换第三方库 |
| 自定义窗口管理 | **QMainWindow** | 成熟的窗口框架 |

### vcpkg 依赖

```json
{
  "dependencies": [
    "gtest",
    "assimp",
    "cgal",
    "eigen3",
    "stb",
    {
      "name": "qtbase",
      "features": ["widgets", "gui", "opengl"]
    }
  ]
}
```

---

## 三、Qt + DiligentEngine 集成方案

### 渲染窗口集成

```cpp
// DiligentWidget.hpp - 将DiligentEngine渲染到Qt窗口

#include <QWidget>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/SwapChain.h>

class DiligentWidget : public QWidget
{
    Q_OBJECT
public:
    DiligentWidget(QWidget* parent = nullptr);
    ~DiligentWidget();

    // DiligentEngine 对象
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     m_pSwapChain;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    // 鼠标事件
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void initializeDiligent();
    void render();

    QPlatformNativeInterface::NativeResourceForIntegrationFunction nativeHandle;
};
```

### 获取窗口句柄

```cpp
// Windows 平台
void DiligentWidget::initializeDiligent()
{
    // 获取Qt窗口的原生句柄
    HWND hwnd = (HWND)this->winId();

    // 创建 DiligentEngine SwapChain
    SwapChainDesc SCDesc;
    SCDesc.Width  = width();
    SCDesc.Height = height();

    // 使用 D3D11/D3D12/Vulkan 创建设备
    EngineD3D11CreateInfo EngineCI;
    auto* pFactoryD3D11 = GetEngineFactoryD3D11();
    pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, &m_pContext);

    Win32NativeWindow Window{hwnd};
    pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, m_pContext, SCDesc,
                                         FullScreenModeDesc{}, Window, &m_pSwapChain);
}
```

---

## 四、核心功能架构

```
┌─────────────────────────────────────────────────────────────────────┐
│  MoldWing 完整功能规划                                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ▸ 基础层                                                           │
│    ├─ 模型加载显示（OBJ/OSGB）                                       │
│    ├─ 相机交互（旋转/缩放/平移）                                      │
│    └─ 撤销/重做系统（QUndoStack）                                    │
│                                                                     │
│  ▸ 选择系统                                                         │
│    ├─ 框选（Box Select）                                            │
│    ├─ 刷选（Brush Select）                                          │
│    ├─ 套索选择（Lasso Select）                                       │
│    ├─ 连通选择（Link/Flood Select）                                  │
│    └─ 反选 / 全选 / 清空                                            │
│                                                                     │
│  ▸ 纹理编辑（类Photoshop）                                           │
│    ├─ 绘制工具：画笔、橡皮擦、填充、渐变                               │
│    ├─ 修复工具：克隆图章、修复画笔、涂抹、模糊/锐化                     │
│    ├─ 调整工具：亮度/对比度、色相/饱和度、色阶、曲线                    │
│    └─ 选区操作：复制/粘贴纹理、导入/导出纹理                           │
│                                                                     │
│  ▸ 几何修复                                                         │
│    ├─ 孔洞检测与填充                                                 │
│    ├─ 非流形边修复                                                   │
│    └─ 删除选中面片                                                   │
│                                                                     │
│  ▸ 导入导出                                                         │
│    ├─ OBJ 读写（带纹理和UV）                                         │
│    └─ OSGB 支持（倾斜摄影格式）                                       │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 五、撤销/重做系统（Qt QUndoStack）

### 使用 Qt 内置的撤销框架

```cpp
#include <QUndoStack>
#include <QUndoCommand>

// 选择命令
class SelectFacesCommand : public QUndoCommand
{
public:
    SelectFacesCommand(SelectionSystem* sel,
                       const std::unordered_set<uint32_t>& newSelection,
                       QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

private:
    SelectionSystem* m_selection;
    std::unordered_set<uint32_t> m_oldSelection;
    std::unordered_set<uint32_t> m_newSelection;
};

// 纹理编辑命令
class TextureEditCommand : public QUndoCommand
{
public:
    TextureEditCommand(TextureData* texture,
                       const QRect& region,
                       const QImage& oldPixels,
                       const QImage& newPixels,
                       QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

    // 支持命令合并（连续笔画合并为一个命令）
    bool mergeWith(const QUndoCommand* other) override;
    int id() const override { return 1001; }

private:
    TextureData* m_texture;
    QRect m_region;
    QImage m_oldPixels;
    QImage m_newPixels;
};

// 主窗口中使用
class MainWindow : public QMainWindow
{
    QUndoStack* m_undoStack;

    void setupUndo() {
        m_undoStack = new QUndoStack(this);

        // 创建撤销/重做Action
        QAction* undoAction = m_undoStack->createUndoAction(this, tr("撤销"));
        undoAction->setShortcut(QKeySequence::Undo);

        QAction* redoAction = m_undoStack->createRedoAction(this, tr("重做"));
        redoAction->setShortcut(QKeySequence::Redo);

        // 添加到菜单
        editMenu->addAction(undoAction);
        editMenu->addAction(redoAction);
    }
};
```

---

## 六、选择系统

### 选择模式

```cpp
enum class SelectMode {
    Box,        // 框选 - 矩形区域
    Brush,      // 刷选 - 圆形笔刷
    Lasso,      // 套索 - 自由绘制闭合区域
    Link,       // 连通选择 - 点击选中所有连通面
    Single      // 单选 - 点击选择单个面
};
```

### 选择系统API

```cpp
class SelectionSystem : public QObject
{
    Q_OBJECT
public:
    std::unordered_set<uint32_t> selectedFaces;
    SelectMode currentMode = SelectMode::Box;
    float brushRadius = 50.0f;

    // 选择操作
    void beginSelect(QPointF screenPos);
    void updateSelect(QPointF screenPos);
    void endSelect();

    // 选择修改
    void invertSelection();
    void clearSelection();
    void selectAll();
    void growSelection();
    void shrinkSelection();
    void selectLinked(uint32_t seedFace);

signals:
    void selectionChanged();
};
```

---

## 七、纹理编辑系统

### 工具列表

| 类别 | 工具 | 功能 |
|-----|------|------|
| **绘制** | 画笔 | 自由绘制 |
| | 橡皮擦 | 擦除到透明/底色 |
| | 颜色填充 | 油漆桶填充 |
| | 渐变填充 | 线性/径向渐变 |
| **修复** | 克隆图章 | 从源点复制纹理 |
| | 修复画笔 | 智能修复（混合边缘） |
| | 涂抹 | 涂抹模糊 |
| | 模糊/锐化 | 局部模糊或锐化 |
| **调整** | 亮度/对比度 | 选区内调整 |
| | 色相/饱和度 | 选区内调整 |
| | 色阶 | Levels调整 |
| | 曲线 | Curves调整 |

### 纹理编辑器

```cpp
class TextureEditor : public QObject
{
    Q_OBJECT
public:
    enum class Tool {
        Brush, Eraser, Fill, Gradient,
        CloneStamp, HealingBrush, Smudge, Blur, Sharpen
    };

    enum class BlendMode {
        Normal, Multiply, Screen, Overlay,
        SoftLight, HardLight, Difference
    };

    Tool currentTool = Tool::Brush;
    float brushSize = 20.0f;
    float brushHardness = 0.8f;
    float brushOpacity = 1.0f;
    QColor foregroundColor = Qt::white;
    QColor backgroundColor = Qt::black;
    BlendMode blendMode = BlendMode::Normal;

    void beginStroke(QPointF uv);
    void continueStroke(QPointF uv);
    void endStroke();

    void setCloneSource(QPointF uv);

    void adjustBrightness(float delta);
    void adjustContrast(float delta);
    void adjustHueSaturation(float h, float s, float l);

signals:
    void textureModified();
};
```

---

## 八、UI设计（Qt Docking）

### Qt Docking 优势

- **QDockWidget**: 原生支持窗口停靠、浮动、标签化
- **QMainWindow**: 内置 Dock 区域管理
- **状态保存**: saveState() / restoreState() 自动保存布局
- **专业外观**: 原生系统风格

### 主窗口布局

```cpp
// MainWindow.hpp
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget* parent = nullptr);

private:
    // 中央渲染窗口
    DiligentWidget* m_viewport3D;

    // 可停靠面板
    QDockWidget* m_toolsDock;      // 工具栏
    QDockWidget* m_propertiesDock; // 属性面板
    QDockWidget* m_historyDock;    // 历史记录
    QDockWidget* m_uvViewDock;     // UV/纹理视图

    // 撤销系统
    QUndoStack* m_undoStack;

    void setupUI();
    void setupDocks();
    void setupMenus();
    void setupToolBar();
};
```

### 布局代码

```cpp
void MainWindow::setupDocks()
{
    // 3D 视图作为中央窗口
    m_viewport3D = new DiligentWidget(this);
    setCentralWidget(m_viewport3D);

    // 左侧工具栏
    m_toolsDock = new QDockWidget(tr("工具"), this);
    m_toolsDock->setWidget(new ToolPanel(this));
    addDockWidget(Qt::LeftDockWidgetArea, m_toolsDock);

    // 右侧属性面板
    m_propertiesDock = new QDockWidget(tr("属性"), this);
    m_propertiesDock->setWidget(new PropertyPanel(this));
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // 右侧历史记录（与属性面板标签化）
    m_historyDock = new QDockWidget(tr("历史记录"), this);
    m_historyDock->setWidget(new QUndoView(m_undoStack));
    tabifyDockWidget(m_propertiesDock, m_historyDock);

    // 底部 UV 视图
    m_uvViewDock = new QDockWidget(tr("UV / 纹理"), this);
    m_uvViewDock->setWidget(new UVViewWidget(this));
    addDockWidget(Qt::BottomDockWidgetArea, m_uvViewDock);
}

void MainWindow::saveLayout()
{
    QSettings settings("MoldWing", "MoldWing");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::restoreLayout()
{
    QSettings settings("MoldWing", "MoldWing");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}
```

### UI 示意图

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  MoldWing                                                          [_][□][X]│
├─────────────────────────────────────────────────────────────────────────────┤
│ 文件(F)  编辑(E)  选择(S)  纹理(T)  修复(R)  视图(V)  帮助(H)                 │
├─────────────────────────────────────────────────────────────────────────────┤
│ [📂][💾] | [↶][↷] | [□框选][🖌刷选][〰套索][🔗连通] | [⊘反选][✕清空]        │
├───────────┬─────────────────────────────────────────────────┬───────────────┤
│           │                                                 │  [属性][历史] │
│  工具     │                                                 ├───────────────┤
│ ┌───────┐ │                                                 │ 笔刷设置      │
│ │ 选择  │ │                                                 │               │
│ │ ├框选 │ │              3D 视图                            │ 大小: [===●] │
│ │ ├刷选 │ │         (DiligentWidget)                        │ 硬度: [==●=] │
│ │ ├套索 │ │                                                 │ 透明: [====●]│
│ │ └连通 │ │                                                 │               │
│ ├───────┤ │                                                 │ 前景色: [■]  │
│ │ 绘制  │ │                                                 │ 背景色: [□]  │
│ │ ├画笔 │ │                                                 │               │
│ │ ├橡皮 │ │                                                 │ 混合模式:    │
│ │ └填充 │ │                                                 │ [正常    ▼]  │
│ ├───────┤ ├─────────────────────────────────────────────────┼───────────────┤
│ │ 修复  │ │                                                 │ 历史记录      │
│ │ ├克隆 │ │              UV / 纹理视图                       │ ├ 打开文件   │
│ │ └修复 │ │           (可拖拽到任意位置)                     │ ├ 框选       │
│ ├───────┤ │                                                 │ ├ 画笔绘制   │
│ │ 调整  │ │                                                 │ └ ● 当前     │
│ └───────┘ └─────────────────────────────────────────────────┴───────────────┤
│ 选中: 1234面 | 顶点: 56789 | 孔洞: 2 | 内存: 256MB                          │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## 九、文件结构

```
Apps/MoldWing/
├── CMakeLists.txt
├── src/
│   ├── main.cpp                 # Qt 应用入口
│   ├── MainWindow.hpp/cpp       # 主窗口 (QMainWindow)
│   │
│   ├── Core/
│   │   ├── MeshData.hpp         # 网格数据结构
│   │   ├── TextureData.hpp      # 纹理数据结构
│   │   └── Commands.hpp/cpp     # QUndoCommand 子类
│   │
│   ├── IO/
│   │   ├── MeshLoader.cpp       # OBJ/OSGB加载
│   │   └── MeshExporter.cpp     # 导出
│   │
│   ├── Render/
│   │   ├── DiligentWidget.hpp/cpp   # Qt + DiligentEngine 集成
│   │   ├── MeshRenderer.cpp         # 网格渲染
│   │   ├── SelectionRenderer.cpp    # 选择高亮渲染
│   │   └── OrbitCamera.cpp          # 轨道相机
│   │
│   ├── Selection/
│   │   ├── SelectionSystem.cpp  # 选择系统核心
│   │   ├── BoxSelect.cpp        # 框选
│   │   ├── BrushSelect.cpp      # 刷选
│   │   ├── LassoSelect.cpp      # 套索
│   │   └── FloodSelect.cpp      # 连通选择
│   │
│   ├── Texture/
│   │   ├── TextureEditor.cpp    # 纹理编辑器核心
│   │   ├── BrushEngine.cpp      # 笔刷引擎
│   │   ├── CloneStamp.cpp       # 克隆图章
│   │   ├── HealingBrush.cpp     # 修复画笔
│   │   └── ColorAdjust.cpp      # 颜色调整
│   │
│   ├── Repair/
│   │   ├── HoleDetector.cpp     # 孔洞检测
│   │   ├── HoleFiller.cpp       # 孔洞填充
│   │   └── MeshCleaner.cpp      # 非流形修复
│   │
│   └── UI/
│       ├── ToolPanel.hpp/cpp        # 工具面板 (QWidget)
│       ├── PropertyPanel.hpp/cpp    # 属性面板
│       ├── UVViewWidget.hpp/cpp     # UV视图
│       └── StatusBar.hpp/cpp        # 状态栏
│
├── resources/
│   ├── icons/                   # 工具图标
│   ├── styles/                  # Qt 样式表
│   └── MoldWing.qrc             # Qt 资源文件
│
└── assets/
    ├── shaders/                 # DiligentEngine 着色器
    └── test_models/             # 测试模型
```

---

## 十、里程碑计划

| 里程碑 | 功能 | 详细内容 | 验收标准 |
|-------|------|---------|---------|
| **M1** | 基础框架 | Qt主窗口、DiligentEngine集成、模型加载、相机 | 能加载OBJ并交互查看 |
| **M2** | 框选 | GPU拾取、矩形选择 | 框选后面片高亮 |
| **M3** | 刷选 | 圆形笔刷选择 | 笔刷大小可调 |
| **M4** | 套索选择 | 自由绘制闭合区域 | 闭合后选中内部面 |
| **M5** | 连通选择 | 点击选中连通区域 | 支持角度限制 |
| **M6** | 纹理视图 | UV预览窗口 | 3D选择同步到UV |
| **M7** | 基础绘制 | 画笔、橡皮擦、填充 | 实时预览 |
| **M8** | 克隆修复 | 克隆图章、修复画笔 | 能修复瑕疵 |
| **M9** | 颜色调整 | 亮度/对比度/色相 | 选区内调整 |
| **M10** | 几何修复 | 孔洞填充、非流形修复 | 一键修复 |
| **M11** | 导入导出 | OBJ/OSGB读写 | 保留修改 |

---

## 十一、Qt 相关技术要点

### CMake 配置

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets Gui OpenGL)

add_executable(MoldWing
    src/main.cpp
    src/MainWindow.cpp
    # ... 其他源文件
)

target_link_libraries(MoldWing PRIVATE
    Qt6::Widgets
    Qt6::Gui
    Qt6::OpenGL
    Diligent-GraphicsEngine
    # ... 其他库
)

# 启用 Qt MOC
set_target_properties(MoldWing PROPERTIES
    AUTOMOC ON
    AUTORCC ON
    AUTOUIC ON
)
```

### 信号槽机制

```cpp
// 选择变化时更新属性面板
connect(m_selectionSystem, &SelectionSystem::selectionChanged,
        m_propertyPanel, &PropertyPanel::updateFromSelection);

// 纹理修改时更新视图
connect(m_textureEditor, &TextureEditor::textureModified,
        m_viewport3D, &DiligentWidget::update);

// 撤销栈变化时更新菜单状态
connect(m_undoStack, &QUndoStack::canUndoChanged,
        undoAction, &QAction::setEnabled);
```

---

## 十二、快捷键规划

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
| G | 填充工具 |
| S | 克隆图章 |
| [ / ] | 减小/增大笔刷 |
| Delete | 删除选中面 |
| F | 聚焦到选中对象 |
| H | 隐藏选中 |

---

**文档版本**: 2.0 (Qt 方案)
**最后更新**: 2025-12-26
