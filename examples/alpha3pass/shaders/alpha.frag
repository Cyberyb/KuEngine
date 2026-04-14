#version 460

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform AlphaPC {
    vec4 color;
    vec2 offset;
    vec2 scale;
} pc;

void main() {
    outColor = pc.color;
}
