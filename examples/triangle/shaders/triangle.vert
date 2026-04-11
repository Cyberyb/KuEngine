#version 460

// 使用 VK_KHR_vulkan_memory_model 时不需要扩展
// 但对于基本 triangle，我们不需要任何扩展

// 全屏三角形顶点着色器
// 顶点着色器只输出裁剪空间位置
// 三角形由三个顶点组成，覆盖整个屏幕
// 这些顶点已经在 NDC 范围内 [-1, 1]

void main() {
    // 全屏三角形的顶点位置（三个顶点覆盖屏幕）
    // 使用 gl_VertexIndex 来选择每个顶点
    // gl_VertexIndex 的范围是 0, 1, 2
    const vec2 positions[3] = vec2[3](
        vec2(-1.0, -1.0),
        vec2(-1.0,  3.0),
        vec2( 3.0, -1.0)
    );
    
    // 输出顶点在裁剪空间的位置
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
