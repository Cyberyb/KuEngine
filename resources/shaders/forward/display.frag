#version 450
#extension GL_GOOGLE_include_directive : require
#include "../common/display_transform.glsl"

layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(push_constant) uniform DisplayPushConstants {
    vec2 uvScale;
    vec2 uvOffset;
    vec4 params; // exposure, manual-sRGB, reserved
} display;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    vec4 color = texture(sceneColor, inUv * display.uvScale + display.uvOffset);
    outColor = vec4(displaySurface(max(color.rgb,vec3(0.0)),display.params.x,
        display.params.y>0.5),color.a);
}
