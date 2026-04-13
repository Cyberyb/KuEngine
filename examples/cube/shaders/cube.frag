#version 460

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
} pc;

void main() {
    outColor = pc.color;
}
