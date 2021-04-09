#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 fragColor;
layout(location=1) in vec2 fragTexCoord;

layout(location=0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {
    vec4 fogColor;
    vec4 fogDistance;
    vec4 ambientColor;
} sceneData;

layout(set = 2, binding = 0) uniform sampler2D texSampler;


void main() {
    vec4 texColor = texture(texSampler, fragTexCoord);

    outColor = vec4(texColor.rgb + sceneData.ambientColor.rgb, 1.0);
}
