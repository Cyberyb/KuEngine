vec3 linearToSrgb(vec3 value) {
    value=max(value,vec3(0.0));
    return mix(12.92*value,1.055*pow(value,vec3(1.0/2.4))-0.055,
        step(vec3(0.0031308),value));
}
vec3 displaySurface(vec3 color,float exposure,bool gamma) {
    color *= exposure;
    color=color/(vec3(1.0)+color);
    return gamma ? linearToSrgb(color) : color;
}
