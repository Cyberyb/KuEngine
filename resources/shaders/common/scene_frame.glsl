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
layout(set=2,binding=0) uniform sampler2D environmentTexture;
