#version 450

layout(location = 0) out vec3 outViewDir;

layout(set = 0, binding = 0) uniform FrameUniforms {
    mat4 model;
    vec4 cameraPos;
    mat4 invViewProj;
} frameData;

void main() {
    vec2 pos;
    if (gl_VertexIndex == 0) {
        pos = vec2(-1.0, -1.0);
    } else if (gl_VertexIndex == 1) {
        pos = vec2(3.0, -1.0);
    } else {
        pos = vec2(-1.0, 3.0);
    }

    gl_Position = vec4(pos, 1.0, 1.0);

    vec4 worldPos = frameData.invViewProj * vec4(pos, 1.0, 1.0);
    vec3 world = worldPos.xyz / max(worldPos.w, 1e-6);
    outViewDir = normalize(world - frameData.cameraPos.xyz);
}
