#version 460

// TODO: We don't need this many fields here, just the view and proj
// Some spirv-reflect business might help with this.
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

// TODO: We don't need all these vertex attributes here
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec3 fragTexCoord;

// https://gamedev.stackexchange.com/questions/60313/implementing-a-skybox-with-glsl-version-330
void main() {
    mat4 inverseProjection = inverse(cameraBuffer.proj); //TODO: Do on CPU side
    mat3 inverseModelview = transpose(mat3(cameraBuffer.view));

    vec3 unprojected = (inverseProjection * vec4(inPosition, 1.0)).xyz;

    fragTexCoord = inverseModelview * unprojected;
    gl_Position = vec4(inPosition,  1.0);
    //gl_Position = inPosition.xyww;
}
