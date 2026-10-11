#version 450
#extension GL_GOOGLE_include_directive : require
layout(location=0) out vec4 outColor;
layout(set=0,binding=0) uniform sampler2D baseMetallic;
layout(set=0,binding=1) uniform sampler2D normalRoughness;
layout(set=0,binding=2) uniform sampler2D positionModel;
layout(set=0,binding=3) uniform sampler2D emissiveAo;
#include "../common/scene_frame.glsl"
#include "../common/surface_lighting.glsl"
void main() {
    ivec2 pixel=ivec2(gl_FragCoord.xy);
    vec4 position=texelFetch(positionModel,pixel,0);
    int model=int(position.w+0.5);
    if(model==0) discard; // keep Graph output clear/background
    vec4 base=texelFetch(baseMetallic,pixel,0);
    vec4 normal=texelFetch(normalRoughness,pixel,0);
    vec4 emissive=texelFetch(emissiveAo,pixel,0);
    vec3 color=model==2 ? max(base.rgb+emissive.rgb,vec3(0.0))
        : lightSurface(position.xyz,normalize(normal.xyz),base.rgb,base.w,
            normal.w,emissive.w,emissive.rgb);
    outColor=vec4(color,1.0);
}
