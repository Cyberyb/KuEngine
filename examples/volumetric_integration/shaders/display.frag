#version 450
layout(set=0,binding=0) uniform sampler2D sceneColor;
layout(push_constant) uniform Display { vec4 params; } pc;
layout(location=0) out vec4 outColor;
void main()
{
    vec3 linear = texelFetch(sceneColor,ivec2(gl_FragCoord.xy),0).rgb;
    // Preserve SebH's original pow(1/2.2) display, exposure=1, no tone mapping.
    vec3 encoded = pow(max(linear,vec3(0)),vec3(1.0/2.2));
    // An sRGB attachment encodes in hardware. Undo only that transfer here so
    // both sRGB and UNORM targets display the same original encoded values.
    if (pc.params.z > 0.5) {
        encoded = clamp(encoded,0.0,1.0);
        encoded = mix(pow((encoded+0.055)/1.055,vec3(2.4)),encoded/12.92,lessThanEqual(encoded,vec3(0.04045)));
    }
    outColor = vec4(encoded,1);
}
