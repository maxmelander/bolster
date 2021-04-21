#version 460

layout(set = 0, binding = 0) uniform CameraBuffer {
    vec3 viewPos;
    uint padding;
    vec4 frustum;
    mat4 view;
    mat4 proj;
    float zNear;
    float zFar;
    uint padding2;
    uint padding3;
} cameraBuffer;

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


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragTexCoord;
layout(location = 2) out uint objectIndex;
layout(location = 3) out vec3 normal;
layout(location = 4) out vec3 fragPos;

// layout(push_constant) uniform constants {
    //mat4 model;
// } PushConstants;

void main() {
    fragColor = inColor;
    fragTexCoord = inTexCoord;
    normal = mat3(transpose(inverse(objectBuffer.objects[gl_BaseInstance].model))) * inNormal;  // TODO: Calculate inverse normal matrix on the cpu
    fragPos = vec3(objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0));
    objectIndex = gl_BaseInstance;
    gl_Position = cameraBuffer.proj * cameraBuffer.view * objectBuffer.objects[gl_BaseInstance].model * vec4(inPosition, 1.0);
}
