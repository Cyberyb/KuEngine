#version 460

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 color;
    vec4 params;
} pc;

const vec3 kCorners[8] = vec3[8](
    vec3(-0.5, -0.5, -0.5),
    vec3( 0.5, -0.5, -0.5),
    vec3( 0.5,  0.5, -0.5),
    vec3(-0.5,  0.5, -0.5),
    vec3(-0.5, -0.5,  0.5),
    vec3( 0.5, -0.5,  0.5),
    vec3( 0.5,  0.5,  0.5),
    vec3(-0.5,  0.5,  0.5)
);

const int kEdgeIndices[24] = int[24](
    0, 1, 1, 2, 2, 3, 3, 0,
    4, 5, 5, 6, 6, 7, 7, 4,
    0, 4, 1, 5, 2, 6, 3, 7
);

const int kFaceIndices[36] = int[36](
    // -Z
    0, 2, 1, 0, 3, 2,
    // +Z
    4, 5, 6, 4, 6, 7,
    // -X
    0, 7, 3, 0, 4, 7,
    // +X
    1, 2, 6, 1, 6, 5,
    // -Y
    0, 1, 5, 0, 5, 4,
    // +Y
    3, 7, 6, 3, 6, 2
);

void main() {
    bool wireframe = (pc.params.x > 0.5);
    int cornerIndex = wireframe
        ? kEdgeIndices[gl_VertexIndex]
        : kFaceIndices[gl_VertexIndex];
    vec3 pos = kCorners[cornerIndex];
    gl_Position = pc.mvp * vec4(pos, 1.0);
}
