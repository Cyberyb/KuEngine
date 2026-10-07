#version 450

layout(location=0) in vec3 inWorldPosition;
layout(location=1) in vec3 inNormal;
layout(location=2) in vec2 inUv0;
layout(location=3) in vec2 inUv1;
layout(location=4) in vec4 inTangent;
layout(location=0) out vec4 outColor;

layout(set=0,binding=0) uniform sampler2D baseTexture;
layout(set=0,binding=1) uniform sampler2D normalTexture;
layout(set=0,binding=2) uniform sampler2D metallicRoughnessTexture;
layout(set=0,binding=3) uniform sampler2D occlusionTexture;
layout(set=0,binding=4) uniform sampler2D emissiveTexture;
layout(set=2,binding=0) uniform sampler2D environmentTexture;

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

const float PI = 3.14159265359;

vec2 transformedUv(int index) {
    vec2 uv = drawData.textureRotationTexCoord[index].y > 0.5 ? inUv1 : inUv0;
    vec4 transform = drawData.textureScaleOffset[index];
    uv *= transform.xy;
    float angle = drawData.textureRotationTexCoord[index].x;
    float c = cos(angle), s = sin(angle);
    uv = mat2(c, s, -s, c) * uv;
    return uv + transform.zw;
}

vec2 environmentUv(vec3 direction) {
    vec3 d = normalize(direction);
    return vec2(atan(d.z,d.x)/(2.0*PI)+0.5,
        0.5-asin(clamp(d.y,-1.0,1.0))/PI);
}

vec3 linearToSrgb(vec3 value) {
    value = max(value, vec3(0.0));
    return mix(12.92*value, 1.055*pow(value,vec3(1.0/2.4))-0.055,
        step(vec3(0.0031308),value));
}

vec3 directLight(vec3 n, vec3 v, vec3 l, vec3 radiance,
    vec3 base, float metallic, float roughness) {
    float ndotl = max(dot(n,l),0.0);
    if (ndotl <= 0.0) return vec3(0.0);
    vec3 h = normalize(v+l);
    float specPower = mix(256.0, 4.0, roughness);
    vec3 f0 = mix(vec3(0.04), base, metallic);
    vec3 diffuse = base * (1.0-metallic) / PI;
    vec3 specular = f0 * pow(max(dot(n,h),0.0), specPower)
        * (specPower+2.0)/(2.0*PI);
    return (diffuse+specular)*radiance*ndotl;
}

void main() {
    vec4 base = drawData.baseColorFactor;
    if (drawData.textureRotationTexCoord[0].z > 0.5)
        base *= texture(baseTexture, transformedUv(0));
    int alphaMode = int(drawData.flags.y + 0.5);
    if (alphaMode == 1 && base.a < drawData.flags.x) discard;

    vec3 n = normalize(inNormal);
    if (drawData.textureRotationTexCoord[1].z > 0.5) {
        vec3 mapNormal = texture(normalTexture, transformedUv(1)).xyz*2.0-1.0;
        mapNormal.xy *= drawData.materialFactors.z;
        vec3 dp1=dFdx(inWorldPosition), dp2=dFdy(inWorldPosition);
        vec2 duv1=dFdx(transformedUv(1)), duv2=dFdy(transformedUv(1));
        vec3 tangent=normalize(dp1*duv2.y-dp2*duv1.y);
        vec3 bitangent=normalize(-dp1*duv2.x+dp2*duv1.x);
        n=normalize(mat3(tangent,bitangent,n)*mapNormal);
    }
    float metallic=clamp(drawData.materialFactors.x,0.0,1.0);
    float roughness=clamp(drawData.materialFactors.y,0.04,1.0);
    if (drawData.textureRotationTexCoord[2].z > 0.5) {
        vec4 mr=texture(metallicRoughnessTexture,transformedUv(2));
        roughness=clamp(roughness*mr.g,0.04,1.0);
        metallic=clamp(metallic*mr.b,0.0,1.0);
    }
    float ao=1.0;
    if (drawData.textureRotationTexCoord[3].z > 0.5) {
        float source=texture(occlusionTexture,transformedUv(3)).r;
        ao=mix(1.0,source,clamp(drawData.materialFactors.w,0.0,1.0));
    }
    vec3 v=normalize(frameData.cameraPosition.xyz-inWorldPosition);
    vec3 l=normalize(frameData.directionalDirectionIntensity.xyz);
    vec3 color=directLight(n,v,l,
        frameData.directionalColorPointCount.rgb
            * max(frameData.directionalDirectionIntensity.w,0.0),
        base.rgb,metallic,roughness);
    int pointCount=clamp(int(frameData.directionalColorPointCount.w+0.5),0,4);
    for (int index=0; index<pointCount; ++index) {
        vec3 delta=frameData.pointPositionRange[index].xyz-inWorldPosition;
        float distance=length(delta);
        float range=max(frameData.pointPositionRange[index].w,0.0001);
        float rangeFactor=clamp(1.0-distance/range,0.0,1.0);
        float attenuation=rangeFactor*rangeFactor/max(distance*distance,0.01);
        color += directLight(n,v,normalize(delta),
            frameData.pointColorIntensity[index].rgb
                * frameData.pointColorIntensity[index].w*attenuation,
            base.rgb,metallic,roughness);
    }
    if (frameData.environmentParams.x > 0.5) {
        vec3 reflection=reflect(-v,n);
        vec3 environment=texture(environmentTexture,environmentUv(reflection)).rgb;
        color += environment*frameData.environmentParams.y
            * mix(0.12,1.0-metallic,0.35)*ao;
    }
    vec3 emissive=drawData.emissiveFactor.rgb;
    if (drawData.textureRotationTexCoord[4].z > 0.5)
        emissive *= texture(emissiveTexture,transformedUv(4)).rgb;
    color = color*ao + emissive;
    color *= frameData.environmentParams.z;
    color = color/(vec3(1.0)+color);
    if (frameData.environmentParams.w > 0.5) color=linearToSrgb(color);
    outColor=vec4(color,alphaMode==0 ? 1.0 : base.a);
}
