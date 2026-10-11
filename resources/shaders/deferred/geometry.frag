#version 450
#extension GL_GOOGLE_include_directive : require
layout(location=0) in vec3 inWorldPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUv0;
layout(location=3) in vec2 inUv1;
layout(location=4) in vec4 inTangent;
layout(location=0) out vec4 outBaseMetallic;
layout(location=1) out vec4 outNormalRoughness;
layout(location=2) out vec4 outPositionModel;
layout(location=3) out vec4 outEmissiveAo;
#include "../common/material_sample.glsl"
void main() {
    int shadingModel=int(drawData.emissiveFactor.w+0.5); // 0 clear, 1 PBR, 2 Unlit
    SurfaceMaterial m=sampleMaterial(shadingModel==2);
    outBaseMetallic=vec4(m.base.rgb,m.metallic);
    outNormalRoughness=vec4(m.normal,m.roughness);
    outPositionModel=vec4(inWorldPosition,float(shadingModel));
    outEmissiveAo=vec4(m.emissive,m.ao);
}
