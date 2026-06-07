#version 450
#extension GL_GOOGLE_include_directive : require

#include "lighting.glsl"

layout(location = 0) in vec3 inNormalMesh;
layout(location = 1) in vec2 inUv0;
layout(location = 2) in vec2 inUv1;
layout(location = 3) in vec3 inPositionMesh;
layout(location = 4) in vec3 inPositionWorld;
layout(location = 5) in vec4 inTangent;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uBaseColorTex;
layout(set = 0, binding = 1) uniform sampler2D uNormalTex;
layout(set = 0, binding = 2) uniform sampler2D uOrmTex;
layout(set = 2, binding = 0) uniform sampler2D uEnvironmentTex;

layout(set = 1, binding = 0) uniform FrameUniforms {
    mat4 model;
    vec4 cameraPos;
    mat4 invViewProj;
    vec4 lightDirIntensity;
    vec4 emissiveFactor;
    vec4 alphaParams;
} frameData;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 normalRow0;
    vec4 normalRow1;
    vec4 normalRow2;
    vec4 baseColorFactor;
    vec4 materialParams;
    vec4 materialFactors;
    vec4 baseUvScaleOffset;
    vec4 normalUvScaleOffset;
    vec4 ormUvScaleOffset;
    vec4 uvTransformParams0;
    vec4 uvTransformParams1;
} pc;

const uint KU_IBL_DIFFUSE_SAMPLES = 24u;
const uint KU_IBL_SPECULAR_SAMPLES = 48u;

vec2 selectUv(float texCoordSet) {
    return texCoordSet > 0.5 ? inUv1 : inUv0;
}

vec2 applyUvTransform(vec2 uv, vec4 scaleOffset, float rotation) {
    vec2 scaled = uv * scaleOffset.xy;
    float c = cos(rotation);
    float s = sin(rotation);
    vec2 rotated = vec2(
        c * scaled.x - s * scaled.y,
        s * scaled.x + c * scaled.y
    );
    return rotated + scaleOffset.zw;
}

vec2 makeUv(float texCoordSet, vec4 scaleOffset, float rotation) {
    vec2 uv = selectUv(texCoordSet);
    uv = applyUvTransform(uv, scaleOffset, rotation);
    if (pc.materialParams.y > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

mat3 cotangentFrame(vec3 n, vec3 p, vec2 uv) {
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, n);
    vec3 dp1perp = cross(n, dp1);
    vec3 t = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 b = dp2perp * duv1.y + dp1perp * duv2.y;

    float invMax = inversesqrt(max(dot(t, t), dot(b, b)));
    return mat3(t * invMax, b * invMax, n);
}

vec3 linearToSrgb(vec3 linearColor) {
    vec3 c = max(linearColor, vec3(0.0));
    vec3 low = 12.92 * c;
    vec3 high = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), c));
}

float kuRadicalInverseVdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

vec2 kuHammersley(uint i, uint n) {
    return vec2(float(i) / float(n), kuRadicalInverseVdC(i));
}

mat3 kuTbnFromNormal(vec3 n) {
    vec3 up = abs(n.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 t = normalize(cross(up, n));
    vec3 b = cross(n, t);
    return mat3(t, b, n);
}

vec2 directionToEquirectUv(vec3 dir);

vec3 kuImportanceSampleGGX(vec2 xi, vec3 n, float roughness) {
    float a = max(roughness, 0.04);
    float phi = 2.0 * KU_PI * xi.x;
    float cosTheta = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
    float sinTheta = sqrt(max(1.0 - cosTheta * cosTheta, 0.0));

    vec3 hTangent = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    return normalize(kuTbnFromNormal(n) * hTangent);
}

vec3 kuSampleEnvironment(vec3 dir) {
    return texture(uEnvironmentTex, directionToEquirectUv(dir)).rgb;
}

vec3 kuComputeDiffuseIrradiance(vec3 n) {
    vec3 irradiance = vec3(0.0);
    float weight = 0.0;

    for (uint i = 0u; i < KU_IBL_DIFFUSE_SAMPLES; ++i) {
        vec2 xi = kuHammersley(i, KU_IBL_DIFFUSE_SAMPLES);
        float phi = 2.0 * KU_PI * xi.x;
        float cosTheta = sqrt(1.0 - xi.y);
        float sinTheta = sqrt(xi.y);

        vec3 lTangent = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
        vec3 l = normalize(kuTbnFromNormal(n) * lTangent);
        float ndotl = max(dot(n, l), 0.0);
        irradiance += kuSampleEnvironment(l) * ndotl;
        weight += ndotl;
    }

    return irradiance / max(weight, 1e-4);
}

vec3 kuComputePrefilteredSpecular(vec3 n, vec3 v, float roughness) {
    vec3 prefiltered = vec3(0.0);
    float weight = 0.0;

    for (uint i = 0u; i < KU_IBL_SPECULAR_SAMPLES; ++i) {
        vec2 xi = kuHammersley(i, KU_IBL_SPECULAR_SAMPLES);
        vec3 h = kuImportanceSampleGGX(xi, n, roughness);
        vec3 l = normalize(2.0 * dot(v, h) * h - v);

        float ndotl = max(dot(n, l), 0.0);
        if (ndotl > 0.0) {
            prefiltered += kuSampleEnvironment(l) * ndotl;
            weight += ndotl;
        }
    }

    return prefiltered / max(weight, 1e-4);
}

vec2 directionToEquirectUv(vec3 dir) {
    vec3 d = normalize(dir);
    float u = atan(d.z, d.x) / (2.0 * KU_PI) + 0.5;
    float v = 0.5 - asin(clamp(d.y, -1.0, 1.0)) / KU_PI;
    return vec2(fract(u), clamp(v, 0.0, 1.0));
}

void main() {
    vec2 uvBase = makeUv(pc.uvTransformParams0.y, pc.baseUvScaleOffset, pc.uvTransformParams0.x);
    vec2 uvNormal = makeUv(pc.uvTransformParams0.w, pc.normalUvScaleOffset, pc.uvTransformParams0.z);
    vec2 uvOrm = makeUv(pc.uvTransformParams1.y, pc.ormUvScaleOffset, pc.uvTransformParams1.x);

    vec4 base = pc.baseColorFactor;
    if (pc.materialParams.x > 0.5) {
        base *= texture(uBaseColorTex, uvBase);
    }

    vec3 meshNormal = normalize(inNormalMesh);
    if (pc.materialParams.z > 0.5) {
        vec3 normalTex = texture(uNormalTex, uvNormal).xyz * 2.0 - 1.0;
        normalTex.xy *= pc.materialFactors.x;

        mat3 tbn = cotangentFrame(meshNormal, inPositionMesh, uvNormal);
        if (length(inTangent.xyz) > 0.0001) {
            vec3 t = normalize(inTangent.xyz);
            vec3 b = normalize(cross(meshNormal, t) * inTangent.w);
            tbn = mat3(t, b, meshNormal);
        }

        meshNormal = normalize(tbn * normalTex);
    }

    mat3 normalMatrix = mat3(pc.normalRow0.xyz, pc.normalRow1.xyz, pc.normalRow2.xyz);
    vec3 n = normalize(normalMatrix * meshNormal);

    vec3 lightColor = vec3(pc.normalRow0.w, pc.normalRow1.w, pc.normalRow2.w);
    vec3 lightDir = normalize(frameData.lightDirIntensity.xyz);
    float lightIntensity = max(frameData.lightDirIntensity.w, 0.0);
    vec3 viewDir = normalize(frameData.cameraPos.xyz - inPositionWorld);

    float ao = 1.0;
    float roughness = clamp(pc.materialFactors.z, 0.04, 1.0);
    float metallic = clamp(pc.materialFactors.y, 0.0, 1.0);
    if (pc.materialParams.w > 0.5) {
        vec3 orm = texture(uOrmTex, uvOrm).rgb;
        ao = mix(1.0, orm.r, clamp(pc.materialFactors.w, 0.0, 1.0));
        roughness = clamp(roughness * orm.g, 0.04, 1.0);
        metallic = clamp(metallic * orm.b, 0.0, 1.0);
    }

    vec3 litColor = kuComputeDirectLighting(
        base.rgb,
        n,
        viewDir,
        lightDir,
        lightColor,
        lightIntensity,
        roughness,
        metallic
    );

    vec3 envColor = vec3(0.0);
    float envStrength = max(pc.uvTransformParams1.w, 0.0);
    if (envStrength > 0.0001) {
        vec3 f0 = mix(vec3(0.04), base.rgb, metallic);
        float ndotv = max(dot(n, viewDir), 0.0);
        vec3 fresnel = kuFresnelSchlickRoughness(ndotv, f0, roughness);
        vec3 kd = (vec3(1.0) - fresnel) * (1.0 - metallic);

        vec3 irradiance = kuComputeDiffuseIrradiance(n);
        vec3 diffuseIbl = irradiance * base.rgb;

        vec3 specularPrefilter = kuComputePrefilteredSpecular(n, viewDir, roughness);
        vec2 envBrdf = kuEnvBRDFApprox(roughness, ndotv);
        vec3 specularIbl = specularPrefilter * (fresnel * envBrdf.x + envBrdf.y);

        envColor = (kd * diffuseIbl + specularIbl) * envStrength * ao;
    }

    vec3 outputColor = litColor + envColor + frameData.emissiveFactor.rgb;
    if (frameData.alphaParams.x > 0.5 && frameData.alphaParams.x < 1.5) {
        if (base.a < frameData.alphaParams.y) {
            discard;
        }
    }

    if (pc.uvTransformParams1.z > 0.5) {
        outputColor = linearToSrgb(outputColor);
    }

    float outAlpha = base.a;
    if (frameData.alphaParams.x < 0.5) {
        outAlpha = 1.0;
    }
    outColor = vec4(outputColor, outAlpha);
}
