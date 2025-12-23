# Camera Control Demo (OSG-Style)

这是一个演示基于**OpenSceneGraph风格**的相机操作系统的Vulkan应用程序。

## 功能特性

- **鼠标左键拖拽**: Trackball旋转（围绕中心点的虚拟球面旋转）
- **鼠标中键拖拽**: 平移相机和场景（沿视图平面移动）
- **鼠标滚轮**: 缩放（移动相机靠近/远离中心点）

## 技术实现

### Camera类 - OSG TrackballManipulator风格

位于 [Camera.h](Camera.h)，完全基于OpenSceneGraph的TrackballManipulator设计：

#### 核心概念

```cpp
class Camera {
    glm::vec3 eye_;      // 相机位置
    glm::vec3 center_;   // 注视中心（旋转轴心）
    glm::vec3 up_;       // 上方向向量
};
```

#### 1. Trackball旋转 (Virtual Trackball)

```cpp
void rotate(float dx, float dy, float screenWidth, float screenHeight);
```

**工作原理**:
- 将2D鼠标移动映射到3D虚拟球面上
- 使用四元数进行平滑旋转
- 相机围绕`center_`点旋转
- 上向量同步旋转，避免视角倒置

**特点**:
- 自然直观的旋转体验
- 无万向节锁问题
- 平滑的旋转插值

#### 2. 视图平面平移

```cpp
void pan(float dx, float dy, float screenWidth, float screenHeight);
```

**工作原理**:
- 计算相机的right和up向量（视图平面坐标系）
- 沿这些方向移动`eye_`和`center_`
- 平移速度与距离成正比（近处慢，远处快）

**特点**:
- 物体跟随鼠标移动
- 保持相机和中心的相对位置
- 自适应移动速度

#### 3. 缩放（Dolly）

```cpp
void zoom(float delta);
```

**工作原理**:
- 沿视线方向移动相机
- 保持`center_`点不变
- 强制最小距离限制

**特点**:
- 始终朝向/远离中心点
- 平滑的缩放体验
- 防止穿透中心点

### 与原始实现的对比

| 特性 | 原实现 | OSG风格 |
|------|--------|---------|
| 旋转方式 | 欧拉角（Yaw/Pitch） | 四元数+Trackball |
| 旋转参考系 | 球坐标系 | 虚拟球面映射 |
| 万向节锁 | 有限制（-89° to 89°） | 无万向节锁 |
| 平移计算 | 简单偏移 | 视图平面投影 |
| 缩放方式 | 调整距离 | Dolly移动 |
| 参数传递 | 预处理offset | 原始坐标+屏幕尺寸 |

### 主要优势

1. **更自然的旋转**: Trackball方式更符合物理直觉
2. **无旋转限制**: 四元数避免了欧拉角的限制
3. **统一的API**: 所有方法接收屏幕坐标和尺寸，内部自动归一化
4. **自适应速度**: 平移和缩放速度随距离自动调整
5. **专业标准**: 与OSG、Blender等工具的操作一致

## 使用方法

### 编译

```bash
# 配置项目
cmake --preset vs2022-x64

# 编译CameraControl demo
cmake --build --preset vs2022-x64-debug --target 04_CameraControl
```

### 运行

```bash
# 运行可执行文件
./install/vs2022-x64/bin/04_CameraControl.exe
```

### 操作说明

- **旋转视角**: 按住鼠标左键拖动
  - 水平拖动：绕垂直轴旋转
  - 垂直拖动：俯仰旋转
  - 对角拖动：组合旋转

- **平移视角**: 按住鼠标中键拖动
  - 物体跟随鼠标移动
  - 移动速度随缩放自动调整

- **缩放**: 滚动鼠标滚轮
  - 向上滚：拉近视角
  - 向下滚：推远视角

- **退出**: 关闭窗口

## 代码结构

```
04_CameraControl/
├── Camera.h              # OSG风格相机操作器类
├── main.cpp             # 主应用程序（Vulkan渲染）
├── CMakeLists.txt       # CMake构建配置
└── README.md            # 本文档
```

## 技术细节

### 相机参数

- **初始位置**: (3.0, 3.0, 3.0)
- **中心点**: (0.0, 0.0, 0.0) - 旋转轴心
- **上向量**: (0.0, 1.0, 0.0) - Y轴向上
- **视场角**: 45°
- **近平面**: 0.1
- **远平面**: 100.0
- **最小距离**: 0.1

### 灵敏度参数（可调整）

```cpp
camera.setRotationSensitivity(1.0f);  // 旋转灵敏度
camera.setPanSensitivity(1.0f);       // 平移灵敏度
camera.setZoomSensitivity(1.0f);      // 缩放灵敏度
camera.setMinimumDistance(0.1f);      // 最小距离
```

### 渲染设置

- **分辨率**: 1280x720
- **深度测试**: 启用
- **背面剔除**: 启用
- **并发帧数**: 2帧

## OpenSceneGraph参考

本实现参考了OSG的以下特性：

1. **Eye-Center-Up模型**: 使用eye/center/up三元组定义相机
2. **Trackball旋转**: 虚拟球面映射实现自然旋转
3. **View-plane panning**: 沿视图平面的二维平移
4. **Distance-based scaling**: 操作速度随距离调整
5. **Quaternion rotation**: 使用四元数避免万向节锁

### OSG相关文档

- [osgGA::TrackballManipulator](https://github.com/openscenegraph/OpenSceneGraph/blob/master/include/osgGA/TrackballManipulator)
- [Trackball rotation algorithm](https://www.khronos.org/opengl/wiki/Object_Mouse_Trackball)

## 依赖项

- **VulkanEngine**: MoldWing引擎库
- **GLFW**: 窗口和输入管理
- **GLM**: 数学库（矩阵、向量、四元数）
  - 使用GLM_ENABLE_EXPERIMENTAL启用四元数扩展
- **Vulkan SDK**: 图形API

## 扩展建议

### 已实现
- ✅ Trackball旋转
- ✅ 视图平面平移
- ✅ Dolly缩放
- ✅ 最小距离限制

### 待扩展
- [ ] 键盘控制（WASD移动，QE上下）
- [ ] 惯性/动量效果
- [ ] 平滑过渡动画
- [ ] Home键重置视角
- [ ] 多视角预设（前/后/左/右/顶/底）
- [ ] 自动旋转模式
- [ ] 聚焦到物体（Frame All）
- [ ] 第一人称模式切换

## 与其他3D工具的对比

| 操作 | 本Demo | Blender | Maya | 3ds Max |
|------|--------|---------|------|---------|
| 旋转 | 左键拖动 | 中键拖动 | Alt+左键 | 中键拖动 |
| 平移 | 中键拖动 | Shift+中键 | Alt+中键 | 中键+滚轮 |
| 缩放 | 滚轮 | 滚轮 | Alt+右键 | 滚轮 |

本实现采用与OSG一致的操作方式，也接近AutoCAD和大部分CAD软件。

## 性能注意事项

- 四元数旋转比欧拉角略慢，但差异可忽略
- 归一化操作（normalize）在每次旋转时执行
- 对于高性能需求场景，可考虑缓存计算结果
- 当前实现优先考虑代码清晰度和正确性

## 调试技巧

如果遇到操作不符合预期：

1. **旋转方向反转**: 检查 `rotateAxis` 计算的符号
2. **平移方向错误**: 检查 `offset` 计算中的符号
3. **缩放速度异常**: 调整 `zoomSensitivity` 参数
4. **视角卡住**: 检查最小距离限制

## 许可证

与MoldWing项目保持一致

---

**参考资源**:
- OpenSceneGraph: https://github.com/openscenegraph/OpenSceneGraph
- Trackball Rotation: https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball
- Quaternion Tutorial: https://www.3dgep.com/understanding-quaternions/

**最后更新**: 2025-12-23
**版本**: 2.0 (OSG-style implementation)
