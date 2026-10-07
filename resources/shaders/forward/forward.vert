#version 450

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUv0;
layout(location=3) in vec2 inUv1;
layout(location=4) in vec4 inTangent;

layout(location=0) out vec3 outWorldPosition;
layout(location=1) out vec3 outNormal;
layout(location=2) out vec2 outUv0;
layout(location=3) out vec2 outUv1;
layout(location=4) out vec4 outTangent;

layout(set=1,binding=0) uniform FrameUniforms {
    mat4 viewProjection;
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 directionalDirectionIntensity;
    vec4 directionalColorPointCount;
    vec4 pointPositionRange[4];
    vec4 pointColorIntensity[4];
    vec4 environmentParams;
} frameData;

layout(set=3,binding=0) uniform DrawUniforms {
    mat4 model;
    vec4 normalColumns[3];
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 materialFactors;
    vec4 textureScaleOffset[5];
    vec4 textureRotationTexCoord[5];
    vec4 flags;
} drawData;

void main() {
    vec4 world = drawData.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(
        drawData.normalColumns[0].xyz,
        drawData.normalColumns[1].xyz,
        drawData.normalColumns[2].xyz);
    outWorldPosition = world.xyz;
    outNormal = normalize(normalMatrix * inNormal);
    outUv0 = inUv0;
    outUv1 = inUv1;
    outTangent = inTangent;
    gl_Position = frameData.viewProjection * world;
}
