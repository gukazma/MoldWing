#version 450

// Vertex attributes
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Instance attributes (per-instance data)
layout(location = 2) in mat4 instanceModel;  // 占用 location 2,3,4,5
layout(location = 6) in vec4 instanceColor;  // 高亮颜色 (w=0 使用顶点颜色, w>0 使用实例颜色)

// Uniform buffer
layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
} ubo;

// Output to fragment shader
layout(location = 0) out vec3 fragColor;
layout(location = 1) out flat int fragInstanceID;

void main() {
    gl_Position = ubo.proj * ubo.view * instanceModel * vec4(inPosition, 1.0);

    // 如果 instanceColor.w > 0，混合高亮颜色
    if (instanceColor.w > 0.0) {
        fragColor = mix(inColor, instanceColor.rgb, instanceColor.w);
    } else {
        fragColor = inColor;
    }

    fragInstanceID = gl_InstanceIndex;
}
