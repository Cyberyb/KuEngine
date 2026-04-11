#version 460

// Fragment shader - 输出纯色
// 可以通过 uniform 控制颜色，但 MVP 直接硬编码蓝色

layout(location = 0) out vec4 outColor;

void main() {
    // 固定颜色 - 可以后续通过 uniform 传递
    outColor = vec4(0.2, 0.4, 0.9, 1.0);
}
