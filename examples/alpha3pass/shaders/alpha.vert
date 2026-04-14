#version 460

layout(push_constant) uniform AlphaPC {
    vec4 color;
    vec2 offset;
    vec2 scale;
} pc;

void main() {
    const vec2 base[3] = vec2[3](
        vec2( 0.0, -0.65),
        vec2( 0.65,  0.65),
        vec2(-0.65,  0.65)
    );

    vec2 p = base[gl_VertexIndex] * pc.scale + pc.offset;
    gl_Position = vec4(p, 0.0, 1.0);
}
