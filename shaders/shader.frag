#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 fragColor;
layout(location=1) in vec2 fragTexCoord;
layout(location=2) flat in uint objectIndex;

layout(location=0) out vec4 outColor;

layout(set = 0, binding = 1) uniform SceneData {
    vec4 fogColor;
    vec4 fogDistance;
    vec4 ambientColor;
} sceneData;

struct ObjectData {
    uint materialIndex;
    uint vertexIndex;
    mat4 model;
};

layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

struct MaterialData {
    uint albedoTexture;
    uint normalTexture;
    uint rougnessTexture;
};

layout(std140,set = 1, binding = 1) readonly buffer MaterialBuffer{
	MaterialData materials[];
} materialBuffer;

layout(set = 2, binding = 0) uniform sampler2D texSamplers[2];

void main() {
    uint materialIndex = objectBuffer.objects[objectIndex].materialIndex;

    vec4 texColor = texture(texSamplers[materialBuffer.materials[materialIndex].albedoTexture], fragTexCoord);

    outColor = vec4(texColor.rgb + sceneData.ambientColor.rgb, 1.0);
}
