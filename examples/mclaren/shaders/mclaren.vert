#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv0;
layout(location = 3) in vec2 inUv1;
layout(location = 4) in vec4 inTangent;

layout(location = 0) out vec3 outNormalMesh;
layout(location = 1) out vec2 outUv0;
layout(location = 2) out vec2 outUv1;
layout(location = 3) out vec3 outPositionMesh;
layout(location = 4) out vec3 outPositionWorld;
layout(location = 5) out vec4 outTangent;

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

void main() {
    outNormalMesh = normalize(inNormal);
    outUv0 = inUv0;
    outUv1 = inUv1;
    outPositionMesh = inPosition;
    outPositionWorld = vec3(frameData.model * vec4(inPosition, 1.0));
    outTangent = inTangent;
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
}
