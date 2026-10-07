#version 450
layout(location=0) out vec3 outDirection;
layout(set=1,binding=0) uniform FrameUniforms {
    mat4 viewProjection;
    mat4 inverseViewProjection;
    vec4 cameraPosition;
    vec4 directionalDirectionIntensity;
    vec4 directionalColorPointCount;
    vec4 pointPositionRange[4];
    vec4 pointColorIntensity[4];
    vec4 environmentParams;
} frameData;
void main() {
    vec2 position=gl_VertexIndex==0?vec2(-1,-1):
        (gl_VertexIndex==1?vec2(3,-1):vec2(-1,3));
    gl_Position=vec4(position,1,1);
    vec4 world=frameData.inverseViewProjection*vec4(position,1,1);
    outDirection=normalize(world.xyz/world.w-frameData.cameraPosition.xyz);
}
