// Preserve the public Forward direct-light and AO semantics in both paths.
const float PI=3.14159265359;
vec2 environmentUv(vec3 direction) {
    vec3 d=normalize(direction);
    return vec2(atan(d.z,d.x)/(2.0*PI)+0.5,
        0.5-asin(clamp(d.y,-1.0,1.0))/PI);
}
vec3 directLight(vec3 n,vec3 v,vec3 l,vec3 radiance,
    vec3 base,float metallic,float roughness) {
    float ndotl=max(dot(n,l),0.0);
    if(ndotl<=0.0) return vec3(0.0);
    vec3 h=normalize(v+l);
    float specPower=mix(256.0,4.0,roughness);
    vec3 f0=mix(vec3(0.04),base,metallic);
    vec3 diffuse=base*(1.0-metallic)/PI;
    vec3 specular=f0*pow(max(dot(n,h),0.0),specPower)*(specPower+2.0)/(2.0*PI);
    return (diffuse+specular)*radiance*ndotl;
}
vec3 lightSurface(vec3 position,vec3 n,vec3 base,float metallic,
    float roughness,float ao,vec3 emissive) {
    vec3 v=normalize(frameData.cameraPosition.xyz-position);
    vec3 color=directLight(n,v,normalize(frameData.directionalDirectionIntensity.xyz),
        frameData.directionalColorPointCount.rgb*max(frameData.directionalDirectionIntensity.w,0.0),
        base,metallic,roughness);
    int count=clamp(int(frameData.directionalColorPointCount.w+0.5),0,4);
    for(int i=0;i<count;++i) {
        vec3 delta=frameData.pointPositionRange[i].xyz-position;
        float distance=length(delta);
        float range=max(frameData.pointPositionRange[i].w,0.0001);
        float factor=clamp(1.0-distance/range,0.0,1.0);
        float attenuation=factor*factor/max(distance*distance,0.01);
        color+=directLight(n,v,normalize(delta),frameData.pointColorIntensity[i].rgb
            *frameData.pointColorIntensity[i].w*attenuation,base,metallic,roughness);
    }
    if(frameData.environmentParams.x>0.5) {
        vec3 environment=texture(environmentTexture,environmentUv(reflect(-v,n))).rgb;
        color+=environment*frameData.environmentParams.y*mix(0.12,1.0-metallic,0.35)*ao;
    }
    return color*ao+emissive;
}
