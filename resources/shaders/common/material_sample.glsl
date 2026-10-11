// Shared by Forward and GBuffer. Inputs are the public mesh vertex outputs.
layout(set=0,binding=0) uniform sampler2D baseTexture;
layout(set=0,binding=1) uniform sampler2D normalTexture;
layout(set=0,binding=2) uniform sampler2D metallicRoughnessTexture;
layout(set=0,binding=3) uniform sampler2D occlusionTexture;
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
    vec2 uv = drawData.textureRotationTexCoord[index].y > 0.5 ? inUv1 : inUv0;
    vec4 transform = drawData.textureScaleOffset[index];
    uv *= transform.xy;
    float angle = drawData.textureRotationTexCoord[index].x;
    float c = cos(angle), s = sin(angle);
    return mat2(c,s,-s,c)*uv + transform.zw;
}
struct SurfaceMaterial {
    vec4 base;
    vec3 normal;
    float metallic;
    float roughness;
    float ao;
    vec3 emissive;
};
SurfaceMaterial sampleMaterial(bool unlit) {
    SurfaceMaterial m;
    m.base = drawData.baseColorFactor;
    if (drawData.textureRotationTexCoord[0].z > 0.5)
        m.base *= texture(baseTexture,transformedUv(0));
    if (int(drawData.flags.y+0.5)==1 && m.base.a < drawData.flags.x) discard;
    m.normal = normalize(inNormal);
    m.metallic = clamp(drawData.materialFactors.x,0.0,1.0);
    m.roughness = clamp(drawData.materialFactors.y,0.04,1.0);
    m.ao = 1.0;
    if (!unlit) {
        if (drawData.textureRotationTexCoord[1].z > 0.5) {
            vec3 mapNormal=texture(normalTexture,transformedUv(1)).xyz*2.0-1.0;
            mapNormal.xy *= drawData.materialFactors.z;
            vec3 dp1=dFdx(inWorldPosition),dp2=dFdy(inWorldPosition);
            vec2 duv1=dFdx(transformedUv(1)),duv2=dFdy(transformedUv(1));
            vec3 tangent=normalize(dp1*duv2.y-dp2*duv1.y);
            vec3 bitangent=normalize(-dp1*duv2.x+dp2*duv1.x);
            m.normal=normalize(mat3(tangent,bitangent,m.normal)*mapNormal);
        }
        if (drawData.textureRotationTexCoord[2].z > 0.5) {
            vec4 mr=texture(metallicRoughnessTexture,transformedUv(2));
            m.roughness=clamp(m.roughness*mr.g,0.04,1.0);
            m.metallic=clamp(m.metallic*mr.b,0.0,1.0);
        }
        if (drawData.textureRotationTexCoord[3].z > 0.5)
            m.ao=mix(1.0,texture(occlusionTexture,transformedUv(3)).r,
                clamp(drawData.materialFactors.w,0.0,1.0));
    }
    m.emissive=drawData.emissiveFactor.rgb;
    if (drawData.textureRotationTexCoord[4].z > 0.5)
        m.emissive *= texture(emissiveTexture,transformedUv(4)).rgb;
    return m;
}
