#version 450

layout(location = 0) in vec3 inNormalMesh;
layout(location = 1) in vec2 inUv0;
layout(location = 2) in vec2 inUv1;
layout(location = 3) in vec3 inPositionMesh;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uBaseColorTex;
layout(set = 0, binding = 1) uniform sampler2D uNormalTex;
layout(set = 0, binding = 2) uniform sampler2D uOrmTex;

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
        meshNormal = normalize(tbn * normalTex);
    }

    mat3 normalMatrix = mat3(pc.normalRow0.xyz, pc.normalRow1.xyz, pc.normalRow2.xyz);
    vec3 n = normalize(normalMatrix * meshNormal);

    vec3 lightDir = normalize(vec3(0.35, 1.0, 0.45));
    float ndotl = max(dot(n, lightDir), 0.0);

    float ao = 1.0;
    float roughness = clamp(pc.materialFactors.z, 0.04, 1.0);
    float metallic = clamp(pc.materialFactors.y, 0.0, 1.0);
    if (pc.materialParams.w > 0.5) {
        vec3 orm = texture(uOrmTex, uvOrm).rgb;
        ao = mix(1.0, orm.r, clamp(pc.materialFactors.w, 0.0, 1.0));
        roughness = clamp(roughness * orm.g, 0.04, 1.0);
        metallic = clamp(metallic * orm.b, 0.0, 1.0);
    }

    float ambient = 0.10 * ao;
    float diffuse = ndotl;

    vec3 viewDir = normalize(vec3(0.0, 0.0, 1.0));
    vec3 halfVec = normalize(lightDir + viewDir);
    float ndoth = max(dot(n, halfVec), 0.0);
    float specPower = mix(256.0, 8.0, roughness);
    float spec = pow(ndoth, specPower) * (1.0 - roughness * 0.5);

    vec3 f0 = mix(vec3(0.04), base.rgb, metallic);
    vec3 diffuseColor = base.rgb * (1.0 - metallic) * diffuse;
    vec3 specColor = f0 * spec;

    vec3 litColor = base.rgb * ambient + diffuseColor + specColor;
    outColor = vec4(litColor, base.a);
}
