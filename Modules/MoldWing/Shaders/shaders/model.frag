#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragWorldPos;
layout(location = 2) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
    // Sample texture
    vec4 texColor = texture(texSampler, fragTexCoord);

    // Simple directional light
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    vec3 normal = normalize(fragNormal);

    // Ambient lighting
    float ambient = 0.3;

    // Diffuse lighting
    float diff = max(dot(normal, lightDir), 0.0);
    float diffuse = diff * 0.7;

    // Combine texture with lighting
    vec3 result = (ambient + diffuse) * texColor.rgb;

    outColor = vec4(result, texColor.a);
}
