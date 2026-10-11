#version 450
#extension GL_GOOGLE_include_directive : require
#include "../common/display_transform.glsl"
layout(set=0,binding=0) uniform sampler2D baseMetallic;
layout(set=0,binding=1) uniform sampler2D normalRoughness;
layout(set=0,binding=2) uniform sampler2D positionModel;
layout(set=0,binding=3) uniform sampler2D emissiveAo;
layout(push_constant) uniform DebugPush { vec2 scale; vec2 offset; vec4 params; } debug;
layout(location=0) in vec2 uv;
layout(location=0) out vec4 outputColor;
void main() {
    ivec2 size=textureSize(positionModel,0);
    ivec2 pixel=clamp(ivec2((uv*debug.scale+debug.offset)*vec2(size)),ivec2(0),size-1);
    vec4 position=texelFetch(positionModel,pixel,0);
    int model=int(position.a+0.5), mode=int(debug.params.x+0.5);
    vec4 base=texelFetch(baseMetallic,pixel,0),normal=texelFetch(normalRoughness,pixel,0),emissive=texelFetch(emissiveAo,pixel,0);
    vec3 color=vec3(0);
    if(model!=0) {
        if(mode==1) color=base.rgb;
        if(mode==2) color=vec3(base.a);
        if(mode==3) color=normal.rgb*.5+.5;
        if(mode==4) color=vec3(normal.a);
        if(mode==5) color=(position.rgb+2.0)/4.0;
        if(mode==6) color=model==1 ? vec3(1,0,0) : model==2 ? vec3(0,1,0) : vec3(1,0,1);
        if(mode==7) color=emissive.rgb;
        if(mode==8) color=vec3(emissive.a);
    }
    color=clamp(color,vec3(0),vec3(1));
    outputColor=vec4(debug.params.y>.5 ? linearToSrgb(color) : color,1);
}
