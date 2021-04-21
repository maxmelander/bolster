#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location=0) in vec3 fragColor;
layout(location=1) in vec2 fragTexCoord;
layout(location=2) flat in uint objectIndex;
layout(location=3) in vec3 normal;
layout(location=4) in vec3 fragPos;

layout(location=0) out vec4 outColor;

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

struct MaterialData {
    uint albedoTexture;
    uint normalTexture;
    uint rougnessTexture;
    uint padding;
};

layout(std140,set = 1, binding = 1) readonly buffer MaterialBuffer{
	MaterialData materials[];
} materialBuffer;

layout(set = 2, binding = 0) uniform sampler2D texSamplers[2];

vec3 phongLighting(LightData light) {
    float distance = length(light.vector.xyz - fragPos);

    // Directional vs Point light
    float attenuation = light.vector.w == 0 ? 1 : 1.0 / (1.0 + 0.09 * distance + 0.032 * (distance * distance));

    // Diffuse
    vec3 norm = normalize(normal);

    // Directional vs Point light
    vec3 lightDir = light.vector.w == 0 ? normalize(-light.vector.xyz) : normalize(light.vector.xyz - fragPos);

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light.color;
    diffuse *= attenuation;

    // Specular
    float specularStrength = 0.9;
    vec3 viewDir = normalize(cameraBuffer.viewPos - fragPos);
    vec3 halfwayDir = normalize(lightDir + viewDir);

    float spec = pow(max(dot(normal, halfwayDir), 0.0), 128);
    vec3 specular = specularStrength * spec * light.color;
    specular *= attenuation;

    return diffuse + specular;
}

void main() {
    float gamma = 2.2;

    uint materialIndex = objectBuffer.objects[objectIndex].materialIndex;

    vec4 texColor = texture(texSamplers[materialBuffer.materials[materialIndex].albedoTexture], fragTexCoord);

    // Gamma correction
    vec3 diffuseColor = pow(texColor.rgb, vec3(gamma));

    vec3 result = vec3(0.);

    // Ambient
    float ambientStrength = 0.01;
    vec3 ambient = ambientStrength * sceneData.ambientColor.rgb;

    result += phongLighting(sceneData.lights[0]) * 1.2;
    result += phongLighting(sceneData.lights[1]) * 1.2;

    result +=  ambient;
    result *= diffuseColor.rgb;

    // Gamma correction
    result = pow(result, vec3(1.0/gamma));

    outColor = vec4(result, 1.0);
}
