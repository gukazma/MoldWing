# 05_ModelDemo 纹理支持实现计划

## 项目状态

**当前阶段**: 已完成基础OBJ模型加载，正在添加纹理支持

**目标**: 使用OpenCV加载纹理，并在Vulkan中显示带纹理的cube模型

## 已完成的工作

### 1. ✅ 基础设施 (2025-12-23)

- [x] 创建Mesh模块 (`Modules/MoldWing/Mesh`)
  - 基于CGAL Surface_mesh包装
  - 支持OBJ加载/保存
  - 自动法线计算
  - 转换为渲染数据

- [x] 创建05_ModelDemo应用
  - 使用VulkanEngine + Mesh模块
  - 成功加载cube.obj
  - 旋转动画显示
  - 基于位置的顶点颜色

- [x] 资源文件
  - `cube.obj` - 带UV坐标的立方体模型
  - `cube_texture.png` - 彩色棋盘格纹理 (512x512)

### 2. ✅ 纹理支持准备 (2025-12-23)

- [x] 更新vcpkg.json添加opencv4依赖
- [x] 创建Image类 (`VulkanEngine/Image.h/cpp`)
  - Image: Vulkan图像封装
  - Sampler: 纹理采样器
  - 布局转换
  - Buffer到Image拷贝

- [x] 创建TextureLoader (`Apps/05_ModelDemo/TextureLoader.h/cpp`)
  - 使用OpenCV加载PNG/JPG
  - BGR到RGBA转换
  - Staging buffer传输

- [x] 更新05_ModelDemo CMakeLists.txt
  - 添加OpenCV链接

## 待完成的工作

### 步骤1: 完善VulkanEngine纹理支持

**文件**: `Modules/MoldWing/VulkanEngine/CMakeLists.txt`

**任务**:
```cmake
# 在 target_sources 中添加:
src/Image.cpp
```

**验证**: 编译VulkanEngine模块成功

---

### 步骤2: 更新Mesh类保留UV坐标

**问题**: 当前Mesh::toRenderData()输出UV为0，因为CGAL不保留UV

**解决方案**:

**文件1**: `Modules/MoldWing/Mesh/include/MoldWing/Mesh/Mesh.h`

在Mesh类中添加：
```cpp
private:
    std::unordered_map<size_t, glm::vec2> m_vertexUVs; // 存储每个顶点的UV
```

**文件2**: `Modules/MoldWing/Mesh/src/Mesh.cpp`

修改`loadFromOBJ()`:
```cpp
// 手动解析OBJ以保留UV坐标
// 需要在CGAL::IO::read_OBJ之前或之后手动解析UV
```

**或者使用tinyobjloader**:
```cpp
#include <tiny_obj_loader.h>

bool Mesh::loadFromOBJ(const std::string& filepath) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
        throw std::runtime_error(warn + err);
    }

    // 处理加载的数据...
    // 保存UV到m_vertexUVs
    // 同时构建CGAL mesh
}
```

**验证**: Mesh::toRenderData()输出正确的UV坐标

---

### 步骤3: 创建/启用纹理着色器

**文件**: `Apps/05_ModelDemo/shaders/model.vert` 和 `model.frag`

这两个文件已存在！需要：

**修改CMakeLists.txt**:
```cmake
# 改为使用model着色器而不是cube
compile_shaders_to_headers(
    TARGET ${TARGET_NAME}
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/model.vert
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/model.frag
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders
    OPTIMIZE
)
```

**验证**: 生成model.vert.h和model.frag.h

---

### 步骤4: 更新main.cpp支持纹理

**文件**: `Apps/05_ModelDemo/main.cpp`

**关键修改**:

1. **添加头文件**:
```cpp
#include "TextureLoader.h"
#include <MoldWing/VulkanEngine/Image.h>
#include "model.vert.h"  // 改为model
#include "model.frag.h"
```

2. **修改Vertex结构** (支持UV):
```cpp
struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;  // 新增
    glm::vec2 texCoord; // 新增

    static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions() {
        // location 0: position
        // location 1: normal
        // location 2: texCoord
    }
};
```

3. **添加成员变量**:
```cpp
MoldWing::Image* textureImage = nullptr;
MoldWing::Sampler* textureSampler = nullptr;
```

4. **loadModel()修改**:
```cpp
// 使用包含UV的顶点数据
for (size_t i = 0; i < vertexCount; ++i) {
    vertex.texCoord = glm::vec2(renderData[offset + 6], renderData[offset + 7]);
}
```

5. **新增loadTexture()**:
```cpp
void loadTexture() {
    textureImage = TextureLoader::loadTexture(
        engine->getDevice(),
        "assets/textures/cube_texture.png"
    );
    textureSampler = new MoldWing::Sampler(engine->getDevice());
}
```

6. **createDescriptorSets()修改** (添加纹理绑定):
```cpp
// Binding 0: UBO
// Binding 1: Combined Image Sampler (NEW)

vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
samplerLayoutBinding.binding = 1;
samplerLayoutBinding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
samplerLayoutBinding.descriptorCount = 1;
samplerLayoutBinding.stageFlags = vk::ShaderStageFlagBits::eFragment;

// 更新descriptor pool size
```

7. **updateDescriptorSets()** (绑定纹理):
```cpp
vk::DescriptorImageInfo imageInfo{};
imageInfo.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
imageInfo.imageView = textureImage->getImageView();
imageInfo.sampler = textureSampler->getHandle();

vk::WriteDescriptorSet imageWrite{};
imageWrite.dstBinding = 1;
imageWrite.descriptorType = vk::DescriptorType::eCombinedImageSampler;
imageWrite.pImageInfo = &imageInfo;
```

8. **cleanup()修改**:
```cpp
delete textureSampler;
delete textureImage;
```

**验证**: 编译通过并运行显示带纹理的cube

---

## 编译顺序

```bash
# 1. 重新配置（安装OpenCV）
cmake --preset vs2022-x64

# 2. 编译VulkanEngine（包含Image类）
cmake --build --preset vs2022-x64-debug --target VulkanEngine

# 3. 编译Mesh（如果修改了UV支持）
cmake --build --preset vs2022-x64-debug --target Mesh

# 4. 编译05_ModelDemo
cmake --build --preset vs2022-x64-debug --target 05_ModelDemo

# 5. 运行测试
./install/vs2022-x64/bin/05_ModelDemo.exe
```

## 注意事项

1. **OpenCV安装**: 首次配置时会下载和编译OpenCV，耗时较长（10-30分钟）

2. **UV坐标来源**:
   - 当前cube.obj已有UV坐标
   - CGAL不保留UV，需要额外处理
   - 可选方案：使用tinyobjloader完全替代CGAL的OBJ加载

3. **着色器变量名**:
   - cube着色器生成 `Shaders::cube_vert_data`
   - model着色器生成 `Shaders::model_vert_data`

4. **纹理格式**:
   - 使用SRGB格式: `vk::Format::eR8G8B8A8Srgb`
   - OpenCV加载为BGR，需转RGBA

## 后续优化

- [ ] 实现mipmap生成
- [ ] 支持多纹理材质
- [ ] 添加法线贴图
- [ ] 优化descriptor管理
- [ ] 支持更多纹理格式

## 参考文件位置

```
MoldWing/
├── vcpkg.json (已更新opencv4)
├── Modules/MoldWing/
│   ├── Mesh/
│   │   ├── include/MoldWing/Mesh/Mesh.h (需更新UV)
│   │   └── src/Mesh.cpp (需更新UV)
│   └── VulkanEngine/
│       ├── CMakeLists.txt (需添加Image.cpp)
│       ├── Image.h (已创建)
│       └── src/Image.cpp (已创建)
└── Apps/05_ModelDemo/
    ├── CMakeLists.txt (已更新OpenCV)
    ├── TextureLoader.h (已创建)
    ├── TextureLoader.cpp (已创建)
    ├── main.cpp (需大幅修改)
    ├── shaders/
    │   ├── model.vert (已存在，支持UV)
    │   └── model.frag (已存在，支持纹理采样)
    └── assets/
        ├── models/cube.obj
        └── textures/cube_texture.png
```

## 当前进度

- [x] 步骤0: 准备工作 (依赖、基础类)
- [ ] 步骤1: VulkanEngine CMakeLists
- [ ] 步骤2: Mesh UV支持
- [ ] 步骤3: 着色器切换
- [ ] 步骤4: main.cpp纹理集成
- [ ] 步骤5: 测试验证

**下一步**: 更新VulkanEngine的CMakeLists.txt添加Image.cpp
