#version 450

layout(location = 0) in vec3 inViewDir;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D uEnvironmentTex;

layout(push_constant) uniform SkyboxPushConstants {
    vec4 params;
} pc;

const float KU_PI = 3.14159265359;

vec2 directionToEquirectUv(vec3 dir) {
    vec3 d = normalize(dir);
    float u = atan(d.z, d.x) / (2.0 * KU_PI) + 0.5;
    float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / KU_PI;
    return vec2(fract(u), clamp(v, 0.0, 1.0));
}

vec3 tonemapReinhard(vec3 color) {
    return color / (vec3(1.0) + color);
}

vec3 linearToSrgb(vec3 linearColor) {
    vec3 c = max(linearColor, vec3(0.0));
    vec3 low = 12.92 * c;
    vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), c));
}

void main() {
    float exposure = max(pc.params.x, 0.001);
    vec3 hdrColor = texture(uEnvironmentTex, directionToEquirectUv(inViewDir)).rgb * exposure;
    vec3 mapped = tonemapReinhard(hdrColor);

    if (pc.params.y > 0.5) {
        mapped = linearToSrgb(mapped);
    }

    outColor = vec4(mapped, 1.0);
}
