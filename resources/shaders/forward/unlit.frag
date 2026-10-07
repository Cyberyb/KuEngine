#version 450

layout(location=2) in vec2 inUv0;
layout(location=3) in vec2 inUv1;
layout(location=0) out vec4 outColor;

layout(set=0,binding=0) uniform sampler2D baseTexture;
layout(set=0,binding=4) uniform sampler2D emissiveTexture;
layout(set=3,binding=0) uniform DrawUniforms {
    mat4 model;
    vec4 normalColumns[3];
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    vec4 materialFactors;
    vec4 textureScaleOffset[5];
    vec4 textureRotationTexCoord[5];
    vec4 flags;
} drawData;

vec2 transformedUv(int index) {
    vec2 uv=drawData.textureRotationTexCoord[index].y>0.5?inUv1:inUv0;
    vec4 transform=drawData.textureScaleOffset[index];
    uv*=transform.xy;
    float angle=drawData.textureRotationTexCoord[index].x;
    float c=cos(angle),s=sin(angle);
    return mat2(c,s,-s,c)*uv+transform.zw;
}

vec3 linearToSrgb(vec3 value) {
    value=max(value,vec3(0.0));
    return mix(12.92*value,1.055*pow(value,vec3(1.0/2.4))-0.055,
        step(vec3(0.0031308),value));
}

void main() {
    vec4 base=drawData.baseColorFactor;
    if(drawData.textureRotationTexCoord[0].z>0.5)
        base*=texture(baseTexture,transformedUv(0));
    int alphaMode=int(drawData.flags.y+0.5);
    if(alphaMode==1 && base.a<drawData.flags.x) discard;
    vec3 emissive=drawData.emissiveFactor.rgb;
    if(drawData.textureRotationTexCoord[4].z>0.5)
        emissive*=texture(emissiveTexture,transformedUv(4)).rgb;
    vec3 color=max(base.rgb+emissive,vec3(0.0))*drawData.flags.z;
    color=color/(vec3(1.0)+color);
    if(drawData.flags.w>0.5) color=linearToSrgb(color);
    outColor=vec4(color,alphaMode==0?1.0:base.a);
}
