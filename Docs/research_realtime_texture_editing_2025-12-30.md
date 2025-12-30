# 实时 OBJ 纹理编辑技术研究报告

> **生成日期**: 2025-12-30
> **研究目标**: 调研实时 3D 模型纹理编辑的实现方法，重点分析 Blender 的纹理绘制系统
> **置信度**: 高 (基于官方文档、源码分析和多个技术资源)

---

## 执行摘要

本报告调研了实时 3D 纹理编辑的核心技术，重点分析了 Blender 的纹理绘制实现。研究发现，实时纹理编辑的核心流程包括：

1. **屏幕空间射线投射** - 从相机通过鼠标位置发射射线
2. **射线-三角形求交** - 使用 Möller-Trumbore 算法找到命中面
3. **重心坐标插值** - 计算精确的 UV 纹理坐标
4. **纹理空间绘制** - 在计算出的 UV 位置进行像素操作
5. **GPU 纹理更新** - 使用 `glTexSubImage2D` 或 PBO 优化同步

这些技术与 MoldWing 项目 M6 已实现的 `ScreenToTextureMapper` 高度吻合，为 M7 克隆图章工具提供了坚实的技术基础。

---

## 1. Blender 纹理绘制系统分析

### 1.1 架构概述

Blender 的纹理绘制系统采用模块化设计，主要组件包括：

| 组件 | 功能 |
|------|------|
| **Texture Slots** | 管理图像与 UV 映射的组合 |
| **Paint Mode** | 绘制模式切换（Material / Single Image） |
| **Brush System** | 笔刷属性管理（大小、硬度、混合模式） |
| **Projection Painting** | 3D 投影绘制算法 |

### 1.2 纹理槽系统 (Texture Slots)

Blender 使用纹理槽将图像与 UV 映射组合：

```
Texture Slot = Image Texture + UV Map
```

**两种绘制模式：**

- **Material Mode**: 自动从网格材质中检测纹理槽
- **Single Image Mode**: 手动选择单个图像进行绘制

**关键属性：**
- `Active Paint Texture Index`: 当前活动绘制槽索引
- `UV Map`: 绘制目标的 UV 映射选择

### 1.3 投影绘制 (Projection Painting)

Blender 的投影绘制是真正的 3D 绘制，而非简单的 2D 贴图：

- 笔刷根据表面法向自动调整
- 支持多视角同时投影
- 处理 UV 接缝和拉伸问题

---

## 2. 屏幕空间到纹理空间映射

### 2.1 核心算法流程

```
鼠标位置 (屏幕坐标)
    ↓
射线生成 (相机 → 世界空间)
    ↓
射线-三角形求交 (Möller-Trumbore)
    ↓
重心坐标计算 (α, β, γ)
    ↓
UV 插值 (UV = α*UV0 + β*UV1 + γ*UV2)
    ↓
纹理坐标 (像素位置)
```

### 2.2 Möller-Trumbore 射线-三角形求交算法

这是计算机图形学中最高效的射线-三角形求交算法：

```cpp
bool rayTriangleIntersect(
    const Vec3& orig, const Vec3& dir,
    const Vec3& v0, const Vec3& v1, const Vec3& v2,
    float& t, float& u, float& v)
{
    Vec3 edge1 = v1 - v0;
    Vec3 edge2 = v2 - v0;
    Vec3 pvec = cross(dir, edge2);
    float det = dot(edge1, pvec);

    // 背面剔除检查
    if (det < EPSILON) return false;

    float invDet = 1.0f / det;
    Vec3 tvec = orig - v0;

    // 计算重心坐标 u
    u = dot(tvec, pvec) * invDet;
    if (u < 0.0f || u > 1.0f) return false;

    Vec3 qvec = cross(tvec, edge1);

    // 计算重心坐标 v
    v = dot(dir, qvec) * invDet;
    if (v < 0.0f || u + v > 1.0f) return false;

    // 计算距离 t
    t = dot(edge2, qvec) * invDet;
    return t > EPSILON;
}
```

**算法特点：**
- 时间复杂度 O(1)
- 无需预计算三角形平面
- 同时输出重心坐标 (u, v)

### 2.3 重心坐标 UV 插值

重心坐标 (α, β, γ) 满足 α + β + γ = 1：

```cpp
// 从 Möller-Trumbore 输出转换
float alpha = 1.0f - u - v;  // 对应顶点 v0
float beta = u;               // 对应顶点 v1
float gamma = v;              // 对应顶点 v2

// UV 插值
Vec2 uv = alpha * uv0 + beta * uv1 + gamma * uv2;

// 转换为像素坐标
int pixelX = (int)(uv.x * textureWidth);
int pixelY = (int)((1.0f - uv.y) * textureHeight);  // Y 轴翻转
```

---

## 3. 克隆图章工具实现

### 3.1 基本原理

克隆图章工具的核心是**源点-目标点相对偏移**：

```
1. Alt+点击 → 设置克隆源点 (sourceUV)
2. 拖拽绘制 → 计算当前点 (destUV)
3. 采样偏移 → sampleUV = destUV + (sourceUV - firstDestUV)
4. 像素复制 → dest[destUV] = src[sampleUV]
```

### 3.2 实现伪代码

```cpp
class CloneStampTool : public TextureEditTool {
private:
    Vec2 m_sourceUV;           // 克隆源 UV
    Vec2 m_firstDestUV;        // 首次目标 UV
    bool m_sourceSet = false;  // 源点是否已设置

public:
    void onMousePress(int x, int y, Qt::MouseButton btn, Qt::KeyboardModifiers mods) {
        if (mods & Qt::AltModifier) {
            // Alt+点击：设置克隆源
            m_sourceUV = screenToTextureUV(x, y);
            m_sourceSet = true;
            m_firstDestUV = Vec2(-1, -1);  // 重置
            return;
        }

        if (!m_sourceSet) return;

        // 开始克隆绘制
        Vec2 destUV = screenToTextureUV(x, y);
        if (m_firstDestUV.x < 0) {
            m_firstDestUV = destUV;
        }

        applyClone(destUV);
    }

    void applyClone(Vec2 destUV) {
        Vec2 offset = m_sourceUV - m_firstDestUV;

        // 笔刷区域内的每个像素
        for (int dy = -brushRadius; dy <= brushRadius; dy++) {
            for (int dx = -brushRadius; dx <= brushRadius; dx++) {
                float dist = sqrt(dx*dx + dy*dy);
                if (dist > brushRadius) continue;

                // 计算硬度衰减
                float falloff = calculateFalloff(dist, brushRadius, hardness);

                // 源和目标像素位置
                Vec2 srcPixel = uvToPixel(destUV + offset) + Vec2(dx, dy);
                Vec2 dstPixel = uvToPixel(destUV) + Vec2(dx, dy);

                // 混合复制
                Color srcColor = sampleTexture(srcPixel);
                Color dstColor = getPixel(dstPixel);
                Color blended = lerp(dstColor, srcColor, falloff * opacity);
                setPixel(dstPixel, blended);
            }
        }
    }
};
```

### 3.3 关键考虑

| 问题 | 解决方案 |
|------|----------|
| UV 边界处理 | Clamp 或 Wrap 模式 |
| 跨材质克隆 | 检查材质 ID 一致性 |
| 源区域无效 | 提示用户重新设置源点 |
| 实时预览 | 显示源区域指示器 |

---

## 4. GPU 纹理更新优化

### 4.1 glTexSubImage2D vs glTexImage2D

**关键区别：**

| 方法 | 用途 | 性能 |
|------|------|------|
| `glTexImage2D` | 完整重建纹理 | 慢（重新分配内存） |
| `glTexSubImage2D` | 部分区域更新 | 快（原地修改） |

```cpp
// 推荐：部分更新
glTexSubImage2D(
    GL_TEXTURE_2D,
    0,                    // mipmap level
    dirtyRect.x,          // x offset
    dirtyRect.y,          // y offset
    dirtyRect.width,      // width
    dirtyRect.height,     // height
    GL_RGBA,              // format
    GL_UNSIGNED_BYTE,     // type
    dirtyPixels           // data pointer
);
```

### 4.2 性能优化技巧

1. **格式匹配**：内部格式与传输格式一致避免转换开销
   ```cpp
   // 好：格式匹配
   glTexImage2D(..., GL_RGBA8, ..., GL_RGBA, GL_UNSIGNED_BYTE, ...);

   // 差：需要格式转换
   glTexImage2D(..., GL_RGBA8, ..., GL_RGB, GL_FLOAT, ...);
   ```

2. **脏区域追踪**：只更新修改过的区域
   ```cpp
   struct DirtyRegion {
       int minX, minY, maxX, maxY;
       void expand(int x, int y, int radius) {
           minX = min(minX, x - radius);
           minY = min(minY, y - radius);
           maxX = max(maxX, x + radius);
           maxY = max(maxY, y + radius);
       }
   };
   ```

3. **批量更新**：每帧结束时统一提交

### 4.3 Pixel Buffer Objects (PBO) 异步更新

对于高频更新场景，PBO 可以避免 CPU-GPU 同步等待：

```cpp
// 创建 PBO
GLuint pbo;
glGenBuffers(1, &pbo);
glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbo);
glBufferData(GL_PIXEL_UNPACK_BUFFER, dataSize, nullptr, GL_STREAM_DRAW);

// 异步更新流程
void* ptr = glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
memcpy(ptr, cpuData, dataSize);  // CPU 端填充
glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

// 异步传输到纹理（不阻塞）
glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, 0);

glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
```

**双缓冲 PBO 模式**：使用两个 PBO 轮换，实现完全异步。

---

## 5. 撤销系统设计

### 5.1 基于脏区域的撤销

存储每次操作影响的矩形区域：

```cpp
class TextureEditCommand : public QUndoCommand {
private:
    QRect m_region;           // 影响区域
    QImage m_beforeData;      // 操作前像素
    QImage m_afterData;       // 操作后像素

public:
    void undo() override {
        // 恢复 before 数据
        copyRegionToTexture(m_region, m_beforeData);
        syncToGPU(m_region);
    }

    void redo() override {
        // 应用 after 数据
        copyRegionToTexture(m_region, m_afterData);
        syncToGPU(m_region);
    }
};
```

### 5.2 内存优化

| 策略 | 描述 |
|------|------|
| 矩形裁剪 | 只存储最小包围矩形 |
| 增量存储 | 存储差异而非完整图像 |
| 压缩 | 对撤销数据进行 RLE 或 LZ4 压缩 |
| 历史限制 | 限制撤销步数或总内存 |

---

## 6. MoldWing 项目适配建议

### 6.1 现有基础 (M6 已实现)

MoldWing 已经具备：
- ✅ `ScreenToTextureMapper` - 屏幕到纹理坐标映射
- ✅ `TextureEditBuffer` - CPU 纹理副本 + 脏区域追踪
- ✅ `TextureEditCommand` - 矩形区域撤销支持
- ✅ `TextureEditTool` - 工具基类框架

### 6.2 M7 克隆图章实现建议

```cpp
// Apps/MoldWing/src/Texture/CloneStampTool.hpp

class CloneStampTool : public TextureEditTool {
public:
    // 继承 TextureEditTool 的笔刷属性

    void setCloneSource(const QPoint& screenPos);  // Alt+点击
    void paint(const QPoint& screenPos) override;   // 克隆绘制

private:
    QPointF m_sourceUV;      // 克隆源 UV
    QPointF m_offsetUV;      // 源-目标偏移
    bool m_sourceValid;      // 源点有效性

    // 可选：源区域预览指示器
    void drawSourceIndicator(QPainter& painter);
};
```

### 6.3 DiligentEngine 纹理更新

DiligentEngine 中对应的纹理更新方法：

```cpp
// 获取暂存纹理数据
MappedTextureSubresource mappedData;
m_pImmediateContext->MapTextureSubresource(
    m_pStagingTexture, 0, 0, MAP_WRITE, MAP_FLAG_DISCARD, nullptr, mappedData);

// 复制脏区域数据
// ...

m_pImmediateContext->UnmapTextureSubresource(m_pStagingTexture, 0, 0);

// 复制到 GPU 纹理
CopyTextureAttribs copyAttribs;
copyAttribs.pSrcTexture = m_pStagingTexture;
copyAttribs.pDstTexture = m_pGPUTexture;
copyAttribs.SrcBox = dirtyBox;  // 只复制脏区域
m_pImmediateContext->CopyTexture(copyAttribs);
```

---

## 7. 参考资源

### 官方文档
- [Blender Manual - Texture Slots](https://docs.blender.org/manual/en/latest/sculpt_paint/texture_paint/tool_settings/texture_slots.html)
- [Blender Manual - Texture Paint Mode](https://docs.blender.org/manual/en/latest/sculpt_paint/texture_paint/)

### 技术文章
- Möller-Trumbore Ray-Triangle Intersection Algorithm
- Real-Time Rendering, Chapter on Texture Mapping
- OpenGL glTexSubImage2D Performance Optimization

### 开源实现参考
- Blender Source: `source/blender/editors/sculpt_paint/paint_image.c`
- Godot Engine: Runtime texture painting example

---

## 8. 结论

实时 OBJ 纹理编辑的核心技术已经非常成熟，MoldWing 项目 M6 阶段实现的基础设施（射线求交、重心插值、脏区域追踪）与业界标准方案高度一致。

M7 克隆图章工具的实现重点在于：
1. 克隆源点管理（Alt+点击设置）
2. 源-目标偏移计算
3. 笔刷区域像素复制
4. 利用现有的 `TextureEditBuffer` 进行 GPU 同步

预计实现难度：**中等**，核心算法已在 M6 中验证。

---

*报告生成完成*
