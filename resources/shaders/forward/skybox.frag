#version 450
layout(location=0) in vec3 inDirection;
layout(location=0) out vec4 outColor;
layout(set=2,binding=0) uniform sampler2D environmentTexture;
layout(push_constant) uniform SkyboxPush { vec4 params; } pushData;
const float PI=3.14159265359;
void main() {
    vec3 d=normalize(inDirection);
    vec2 uv=vec2(atan(d.z,d.x)/(2.0*PI)+0.5,
        0.5-asin(clamp(d.y,-1.0,1.0))/PI);
    vec3 color=texture(environmentTexture,uv).rgb;
    outColor=vec4(color,1.0);
}
