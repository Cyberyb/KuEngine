#version 460

// Fragment shader - 输出纯色
// 可以通过 uniform 控制颜色，但 MVP 直接硬编码蓝色

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform TriangleColorPC {
    vec4 color;
} pc;

void main() {
    outColor = pc.color;
}
