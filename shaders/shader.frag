#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) flat in uint objectIndex;
layout(location = 2) in vec3 normal;
layout(location = 3) in vec3 fragPos;
layout(location = 4) in vec4 fragPosLightSpace;

layout(location = 5) in vec3 lightPosTangentSpace[3];
layout(location = 8) in vec3 viewPosTangentSpace;
layout(location = 9) in vec3 fragPosTangentSpace;
layout(location = 10) in mat3 TBNTest;

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
  mat4 transform;
  vec4 boundingSphere;
  uint materialIndex;
  uint unused1;  // Pad to vec4
  uint unused2;  // Pad to vec4
  uint unused3;  // Pad to vec4
};

layout(std140,set = 1, binding = 0) readonly buffer ObjectBuffer{
	ObjectData objects[];
} objectBuffer;


struct MaterialData {
    uint albedoTexture;
    uint armTexture;
    uint emissiveTexture;
    uint normalTexture;
};

layout(std140,set = 1, binding = 1) readonly buffer MaterialBuffer{
	MaterialData materials[];
} materialBuffer;

layout(set = 2, binding = 0) uniform sampler2D texSamplers[56];
layout(set = 2, binding = 1) uniform sampler2D hdrSampler[4];

const float PI = 3.14159265359;

float shadowColor() {
    float shadow = 0.0;
    // vec3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    vec3 projCoord = fragPosLightSpace.xyz / fragPosLightSpace.w;
    vec3 sampleCoord = projCoord * 0.5 + 0.5;
    // projCoord = projCoord * 0.5 + 0.5;

    float closestDepth = texture(texSamplers[0], sampleCoord.xy).r;

    if (projCoord.z >= 0.0 && projCoord.z <= 1.0) {

        if (closestDepth  <  projCoord.z){
            return 0.6;
        }
    }

    return 0.0;
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}  

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = geometrySchlickGGX(NdotV, roughness);
    float ggx1  = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// TODO: Convert equirectangular map to cubemap
// in a pre-processing step
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v)
{
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}


void main() {
    float gamma = 2.2;
    uint materialIndex = objectBuffer.objects[objectIndex].materialIndex;

    vec4 texColor = texture(texSamplers[materialBuffer.materials[materialIndex].albedoTexture], fragTexCoord);
    vec3 albedo = pow(texColor.rgb, vec3(gamma));

    vec4 emissiveColor = texture(texSamplers[materialBuffer.materials[materialIndex].emissiveTexture], fragTexCoord);
    vec3 emissive = pow(emissiveColor.rgb, vec3(gamma));

    // TODO: Do the matrix multiplication in the vertex shader
    // and then do all the lighting calculations in tangent space here
    vec3 N = texture(texSamplers[materialBuffer.materials[materialIndex].normalTexture], fragTexCoord).rgb;
    N = normalize(TBNTest * N);

    vec3 armColor  = texture(texSamplers[materialBuffer.materials[materialIndex].armTexture], fragTexCoord).rgb;

    float ao = armColor.r;
    float roughness = armColor.g;
    float metallic = armColor.b;

    // vec3 N = normalize(normal);
    vec3 V = normalize(cameraBuffer.viewPos - fragPos);
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);

    float shadow = shadowColor();

    for (int i = 1; i < 2; i++) {
        // Per-Light Radiance
        LightData light = sceneData.lights[i];
        vec3 L = light.vector.w == 0 ? normalize(light.vector.xyz) : normalize(light.vector.xyz - fragPos);
        vec3 H = normalize(V + L);
        float distance = length(light.vector.xyz  - fragPos);

        float attenuation = light.vector.w == 0 ? 1.0 : 1.0 / (distance * distance);
        vec3 radiance = light.color * attenuation;

        // Cook-Torrance BRDF
        float NDF = distributionGGX(N, H, roughness);
        float G = geometrySmith(N, V, L, roughness);
        vec3 F = fresnelSchlickRoughness(max(dot(H, V), 0.0), F0, roughness);

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;

        vec3 numerator    = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
        vec3 specular     = numerator / max(denominator, 0.001);


        // Add to outgoing radiance Lo
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // IBL Specular
    vec3 R = reflect(-V, N);
    vec3 F  = fresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    // TODO: Convert to cubemaps
    vec2 ambientUV = SampleSphericalMap(N);
    vec2 specularUV = SampleSphericalMap(R);

    vec3 irradiance = texture(hdrSampler[1], ambientUV).rgb;
    vec3 diffuse    = irradiance * albedo;

    vec3 prefilteredColor = textureLod(hdrSampler[2], specularUV, roughness * 6).rgb; //TODO: Roughness
    vec2 envBRDF  = texture(hdrSampler[3], vec2(max(dot(N, V), 0.0), roughness)).rg;
    vec3 specular = prefilteredColor * (F * envBRDF.x + envBRDF.y);

    vec3 ambient    = (kD * diffuse + specular) * ao * (1.0 - shadow);
    vec3 color   = ambient + Lo;//+ emissive; //TODO: deal with emissive not always being a thing

    //color = prefilteredColor;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}
