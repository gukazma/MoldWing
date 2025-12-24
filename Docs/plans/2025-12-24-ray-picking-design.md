# 06_RayPicking 设计文档

## 概述

基于 Vulkan RTX 硬件光线追踪的射线检测 Demo，使用 Ray Query 在 Compute Shader 中实现高性能射线拾取。

## 需求

- **交互方式**：实时跟踪鼠标 + 点击固定高亮
- **场景**：3x3x3 Cube 网格（27个立方体）
- **实现**：VK_KHR_ray_query + Compute Shader
- **显示**：ImGui 调试信息（交点、射线、性能、相机参数）

## 文件结构

```
Apps/06_RayPicking/
├── main.cpp                 # 主应用程序
├── CMakeLists.txt          # 构建配置
└── shaders/
    ├── cube.vert           # 立方体顶点着色器（实例化）
    ├── cube.frag           # 立方体片段着色器
    └── raypick.comp        # Ray Query 计算着色器
```

## 架构设计

### 核心组件

1. **RayPickDemo 类** - 基于 04_CameraControl 结构
   - 管理 27 个 Cube 的实例化渲染
   - 处理鼠标输入（实时追踪 + 点击固定）
   - 集成 ImGui 显示调试信息

2. **加速结构 (Acceleration Structure)**
   - BLAS：单个 Cube 的几何数据
   - TLAS：27 个 Cube 实例的顶层结构

3. **Compute Pipeline**
   - 输入：射线原点、方向
   - 输出：命中信息（位置、距离、实例ID、法线）

### 数据流

```
鼠标位置 → CPU计算射线 → Upload到GPU → Compute Shader查询
→ Storage Buffer → CPU回读 → ImGui显示
```

## 加速结构设计

### 所需扩展

```cpp
VK_KHR_acceleration_structure
VK_KHR_ray_query
VK_KHR_deferred_host_operations
VK_KHR_buffer_device_address
```

### 所需特性

```cpp
accelerationStructureFeatures.accelerationStructure = VK_TRUE
rayQueryFeatures.rayQuery = VK_TRUE
bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE
```

### BLAS

- 复用 04_CameraControl 的立方体顶点数据（24顶点，36索引）
- 使用 VK_GEOMETRY_TYPE_TRIANGLES_KHR

### TLAS

- 27 个实例，3x3x3 网格排列
- 位置: (x*1.5, y*1.5, z*1.5)，x,y,z ∈ {-1, 0, 1}
- 间距 1.5 保证 Cube 不重叠

### Buffer 需求

- Vertex Buffer（带 `eAccelerationStructureBuildInputReadOnlyKHR` flag）
- Index Buffer（同上）
- BLAS Buffer + Scratch Buffer
- TLAS Buffer + Scratch Buffer + Instance Buffer

## Compute Shader 设计

### raypick.comp

```glsl
#version 460
#extension GL_EXT_ray_query : require

layout(local_size_x = 1) in;

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;

layout(set = 0, binding = 1) uniform RayParams {
    vec3 origin;
    vec3 direction;
    float tMin;
    float tMax;
} ray;

layout(set = 0, binding = 2) buffer HitResult {
    int   hit;
    int   instanceId;
    int   primitiveId;
    float hitT;
    vec3  hitPoint;
    vec3  hitNormal;
    vec2  barycentrics;
} result;

void main() {
    rayQueryEXT rayQuery;
    rayQueryInitializeEXT(rayQuery, tlas,
        gl_RayFlagsOpaqueEXT, 0xFF,
        ray.origin, ray.tMin, ray.direction, ray.tMax);

    while (rayQueryProceedEXT(rayQuery)) {}

    if (rayQueryGetIntersectionTypeEXT(rayQuery, true)
        == gl_RayQueryCommittedIntersectionTriangleEXT) {
        result.hit = 1;
        result.hitT = rayQueryGetIntersectionTEXT(rayQuery, true);
        result.instanceId = rayQueryGetIntersectionInstanceIdEXT(rayQuery, true);
        result.primitiveId = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
        result.barycentrics = rayQueryGetIntersectionBarycentricsEXT(rayQuery, true);
        result.hitPoint = ray.origin + ray.direction * result.hitT;
        // 法线需要从几何数据计算
    } else {
        result.hit = 0;
    }
}
```

### CPU 端射线计算

```cpp
glm::vec3 computeRayDirection(float mouseX, float mouseY) {
    // 屏幕坐标 → NDC [-1, 1]
    float ndcX = (2.0f * mouseX / WIDTH) - 1.0f;
    float ndcY = 1.0f - (2.0f * mouseY / HEIGHT);

    // NDC → 世界空间射线
    glm::mat4 invProj = glm::inverse(projMatrix);
    glm::mat4 invView = glm::inverse(viewMatrix);

    glm::vec4 rayClip(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 rayEye = invProj * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);

    return glm::normalize(glm::vec3(invView * rayEye));
}
```

## ImGui 界面设计

```
┌─────────────────────────────────────────────┐
│  Ray Picking Debug                      [x] │
├─────────────────────────────────────────────┤
│  ◆ Hit Status                               │
│    Hit: ✓ Yes / ✗ No                        │
│    Instance ID: 13                          │
│    Primitive ID: 4                          │
│                                             │
│  ◆ Hit Point                                │
│    Position: (0.234, 0.500, -0.123)         │
│    Distance: 3.456                          │
│    Normal: (0.0, 1.0, 0.0)                  │
│                                             │
│  ◆ Ray Info                                 │
│    Origin: (3.00, 2.00, 3.00)               │
│    Direction: (-0.57, -0.38, -0.57)         │
│                                             │
│  ◆ Performance                              │
│    Query Time: 0.023 ms                     │
│    FPS: 144.2                               │
│                                             │
│  ◆ Camera                                   │
│    Position: (3.00, 2.00, 3.00)             │
│    Center: (0.00, 0.00, 0.00)               │
│    Distance: 4.69                           │
│                                             │
│  ◆ Fixed Point (Right-click to set)         │
│    [Clear] Position: (0.5, 0.5, 0.0)        │
└─────────────────────────────────────────────┘
```

## 交互设计

### 鼠标控制

| 操作 | 功能 |
|------|------|
| 左键拖动 | 相机旋转（Trackball） |
| 中键拖动 | 相机平移 |
| 滚轮 | 缩放 |
| 右键单击 | 固定当前交点 |
| 鼠标移动 | 实时射线检测 |

### 状态管理

```cpp
struct RayPickState {
    // 实时追踪
    glm::vec3 currentHitPoint;
    int currentInstanceId = -1;
    bool isHit = false;

    // 点击固定
    glm::vec3 fixedHitPoint;
    int fixedInstanceId = -1;
    bool hasFixedPoint = false;

    // 性能统计
    double queryTimeMs = 0.0;
    double fps = 0.0;
};
```

### 高亮效果

- 实时命中 Cube：边框高亮（黄色）
- 固定选中 Cube：填充高亮（绿色半透明）

## 渲染流程

```
1. glfwPollEvents()
        ↓
2. 更新射线参数 (CPU → GPU)
        ↓
3. Compute Shader 执行 Ray Query
        ↓
4. Pipeline Barrier (Compute → Host)
        ↓
5. 回读结果 (GPU → CPU)
        ↓
6. 更新 ImGui 状态
        ↓
7. Graphics Pipeline 渲染
   - 绘制 27 个 Cube（实例化）
   - 绘制高亮效果
   - 绘制 ImGui
        ↓
8. Present
```

## 同步策略

```cpp
vk::MemoryBarrier barrier{};
barrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
barrier.dstAccessMask = vk::AccessFlagBits::eHostRead;

cmd.pipelineBarrier(
    vk::PipelineStageFlagBits::eComputeShader,
    vk::PipelineStageFlagBits::eHost,
    {}, barrier, {}, {});
```

## 实例化渲染

```cpp
struct InstanceData {
    glm::mat4 model;
    glm::vec4 color;  // 用于高亮
};

// 单次绘制所有 Cube
cmd.drawIndexed(36, 27, 0, 0, 0);
```

## 性能优化

- 结果 Buffer 使用 `HOST_VISIBLE | HOST_COHERENT`
- 只在鼠标移动时更新射线
- ImGui 窗口可折叠

---

**设计日期**: 2025-12-24
**基于**: 04_CameraControl
