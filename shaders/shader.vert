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
    mat4 model;
    vec4 boundingSphere;
    uint materialIndex;
    uint padding1;
    uint padding2;
    uint padding3;
};

layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;


layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inTangent;
layout(location = 3) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out uint objectIndex;
layout(location = 2) out vec3 normal;
layout(location = 3) out vec3 fragPos;
layout(location = 4) out vec4 fragPosLightSpace;

layout(location = 5) out vec3 lightPosTangentSpace[3];
layout(location = 8) out vec3 viewPosTangentSpace;
layout(location = 9) out vec3 fragPosTangentSpace;
layout(location = 10) out mat3 TBNTest;

void main() {
    fragTexCoord = inTexCoord;


    mat4 model = objectBuffer.objects[gl_BaseInstance].model;
    normal = mat3(transpose(inverse(model))) * inNormal;  // TODO: Calculate inverse normal matrix on the cpu

    vec3 biTangent = inTangent.w * cross(inNormal, inTangent.xyz);

    vec3 T = normalize(vec3(model * vec4(inTangent.xyz, 0.0)));
    vec3 N = normalize(vec3(model * vec4(inNormal, 0.0)));

    // re-orthogonalize T with respect to N
    T = normalize(T - dot(T, N) * N);
    // then retrieve perpendicular vector B with the cross product of T and N
    vec3 B = cross(N, T);

    // TBN must form a right handed coord system.
    // Some models have symetric UVs. Check and fix.
    if (dot(cross(N, T), B) < 0.0)
                T = T * -1.0;

    TBNTest = mat3(T, B, N);
    mat3 TBN = transpose(mat3(T, B, N));

    for (int i = 0; i < 3; i++) {
        lightPosTangentSpace[i] = TBN * sceneData.lights[i].vector.xyz;
    }
    viewPosTangentSpace = TBN * cameraBuffer.viewPos;
    fragPosTangentSpace  = TBN * vec3(model * vec4(inPosition, 1.0));


    fragPos = vec3(model * vec4(inPosition, 1.0));
    fragPosLightSpace = sceneData.lights[0].spaceMatrix * vec4(fragPos, 1.0);

    objectIndex = gl_BaseInstance;
    gl_Position = cameraBuffer.proj * cameraBuffer.view * vec4(fragPos, 1.0);
}
