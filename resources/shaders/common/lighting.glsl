#ifndef KU_COMMON_LIGHTING_GLSL
#define KU_COMMON_LIGHTING_GLSL

const float KU_PI = 3.14159265359;

float kuDistributionGGX(vec3 n, vec3 h, float roughness) {
    float a = max(roughness, 0.04);
    float a2 = a * a;
    float ndoth = max(dot(n, h), 0.0);
    float ndoth2 = ndoth * ndoth;

    float denom = ndoth2 * (a2 - 1.0) + 1.0;
    return a2 / max(KU_PI * denom * denom, 1e-5);
}

float kuGeometrySchlickGGX(float ndotv, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return ndotv / max(ndotv * (1.0 - k) + k, 1e-5);
}

float kuGeometrySmith(vec3 n, vec3 v, vec3 l, float roughness) {
    float ndotv = max(dot(n, v), 0.0);
    float ndotl = max(dot(n, l), 0.0);
    float ggxV = kuGeometrySchlickGGX(ndotv, roughness);
    float ggxL = kuGeometrySchlickGGX(ndotl, roughness);
    return ggxV * ggxL;
}

vec3 kuFresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
}

vec3 kuFresnelSchlickRoughness(float cosTheta, vec3 f0, float roughness) {
    vec3 oneMinusRough = vec3(1.0 - roughness);
    return f0 + (max(oneMinusRough, f0) - f0) * pow(1.0 - cosTheta, 5.0);
}

vec3 kuComputeBasicLighting(
    vec3 baseColor,
    vec3 normal,
    vec3 viewDir,
    vec3 lightDir,
    vec3 lightColor,
    float lightIntensity,
    float roughness,
    float metallic,
    float ao
) {
    vec3 n = normalize(normal);
    vec3 v = normalize(viewDir);
    vec3 l = normalize(lightDir);
    vec3 h = normalize(v + l);

    float ndotl = max(dot(n, l), 0.0);
    float ndotv = max(dot(n, v), 0.0);
    float hdotv = max(dot(h, v), 0.0);

    vec3 f0 = mix(vec3(0.04), baseColor, clamp(metallic, 0.0, 1.0));

    float d = kuDistributionGGX(n, h, roughness);
    float g = kuGeometrySmith(n, v, l, roughness);
    vec3 f = kuFresnelSchlick(hdotv, f0);

    vec3 numerator = d * g * f;
    float denominator = max(4.0 * ndotv * ndotl, 1e-5);
    vec3 specular = numerator / denominator;

    vec3 kS = f;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 radiance = lightColor * lightIntensity;
    vec3 direct = (kD * baseColor / KU_PI + specular) * radiance * ndotl;

    vec3 fAmbient = kuFresnelSchlickRoughness(ndotv, f0, roughness);
    vec3 kdAmbient = (vec3(1.0) - fAmbient) * (1.0 - metallic);
    vec3 ambientDiffuse = kdAmbient * baseColor * 0.22;
    vec3 ambientSpec = fAmbient * (0.04 + 0.30 * (1.0 - roughness));
    vec3 ambient = (ambientDiffuse + ambientSpec) * ao;

    return ambient + direct;
}

vec3 kuComputeDirectLighting(
    vec3 baseColor,
    vec3 normal,
    vec3 viewDir,
    vec3 lightDir,
    vec3 lightColor,
    float lightIntensity,
    float roughness,
    float metallic
) {
    vec3 n = normalize(normal);
    vec3 v = normalize(viewDir);
    vec3 l = normalize(lightDir);
    vec3 h = normalize(v + l);

    float ndotl = max(dot(n, l), 0.0);
    float ndotv = max(dot(n, v), 0.0);
    float hdotv = max(dot(h, v), 0.0);

    vec3 f0 = mix(vec3(0.04), baseColor, clamp(metallic, 0.0, 1.0));

    float d = kuDistributionGGX(n, h, roughness);
    float g = kuGeometrySmith(n, v, l, roughness);
    vec3 f = kuFresnelSchlick(hdotv, f0);

    vec3 numerator = d * g * f;
    float denominator = max(4.0 * ndotv * ndotl, 1e-5);
    vec3 specular = numerator / denominator;

    vec3 kS = f;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);

    vec3 radiance = lightColor * lightIntensity;
    vec3 direct = (kD * baseColor / KU_PI + specular) * radiance * ndotl;

    return direct;
}

vec2 kuEnvBRDFApprox(float roughness, float ndotv) {
    vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;

    float a004 = min(r.x * r.x, exp2(-9.28 * ndotv)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

#endif
