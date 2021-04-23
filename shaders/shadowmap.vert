#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

struct LightData {
    mat4 spaceMatrix;
    vec4 vector;
    vec3 color;
    float strength;
};

layout(std140, set = 0, binding = 1) uniform SceneData {
    vec4 fogColor;
    vec4 fogDistance;
    vec4 ambientColor;
    LightData lights[3];
} sceneData;

struct ObjectData {
    uint materialIndex;
    uint vertexIndex;
    uint indexOffset;
    uint padding;
    vec4 boundingSphere;
    mat4 model;
};

layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;

void main() {
    gl_Position = sceneData.lights[0].spaceMatrix * objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0);
}
