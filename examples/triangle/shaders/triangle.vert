#version 460

// 基础三角形顶点着色器

void main() {
    // 直接在 NDC 中定义一个居中三角形
    const vec2 positions[3] = vec2[3](
        vec2( 0.0, -0.6),
        vec2( 0.6,  0.6),
        vec2(-0.6,  0.6)
    );

    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
}
