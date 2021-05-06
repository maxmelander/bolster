#version 460

layout(location = 0) in vec3 fragTexCoord;
layout(location = 0) out vec4 outColor;

// NOTE: If we keep the same bindings as everything else,
// is it okay to just use what we need and leave the bindings
// as is? Or is it better to bind only what we need for this
// specific pipeline?
layout(set = 2, binding = 1) uniform sampler2D hdrSampler[4];

// TODO: Cubemap
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 sampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = sampleSphericalMap(normalize(fragTexCoord));
    vec3 color = texture(hdrSampler[2], uv).rgb;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0/2.2));
    outColor = vec4(color, 1.0);
}
