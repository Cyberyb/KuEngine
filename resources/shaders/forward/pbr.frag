#version 450
#extension GL_GOOGLE_include_directive : require
layout(location=0) in vec3 inWorldPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUv0;
layout(location=3) in vec2 inUv1;
layout(location=4) in vec4 inTangent;
layout(location=0) out vec4 outColor;
#include "../common/material_sample.glsl"
#include "../common/scene_frame.glsl"
#include "../common/surface_lighting.glsl"
void main() {
    SurfaceMaterial m=sampleMaterial(false);
    vec3 color=lightSurface(inWorldPosition,m.normal,m.base.rgb,
        m.metallic,m.roughness,m.ao,m.emissive);
    outColor=vec4(color,
        int(drawData.flags.y+0.5)==0 ? 1.0 : m.base.a);
}
