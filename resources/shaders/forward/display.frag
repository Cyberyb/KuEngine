#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneColor;
layout(push_constant) uniform DisplayPushConstants {
    vec2 uvScale;
    vec2 uvOffset;
} display;

layout(location = 0) in vec2 inUv;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(sceneColor, inUv * display.uvScale + display.uvOffset);
}
