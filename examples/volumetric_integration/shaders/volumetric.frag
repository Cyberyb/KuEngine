#version 450
// Vulkan adaptation of SebH's VolumetricIntegration / Shadertoy XlBSRz.
// Source and modifications: ../NOTICE.md. CC BY-NC-SA 3.0.
layout(push_constant) uniform Scene {
    vec4 viewport; // x,y,width,height in Vulkan framebuffer pixels
    vec4 medium;   // time,density,light multiplier,strong fog
    ivec4 quality; // view steps,shadow steps,integration,noise/shadow flags
} pc;
layout(location=0) out vec4 outColor;
#define iTime pc.medium.x
#define iResolution pc.viewport.zw
#define fogNoise (((pc.quality.w & 1) != 0) ? 1.0 : 0.0)
#define D_DETAILED_WALLS 0
#define D_MAX_STEP_LENGTH_ENABLE 1
#define LPOS vec3(20.0+15.0*sin(iTime),15.0+12.0*cos(iTime),-20.0)
#define LCOL (pc.medium.z*600.0*vec3(1.0,0.9,0.5))

// Original external repeat noise texture was inaccessible (HTTP 403).
// Deterministic 256x256 scalar hash grid, bilinear filtering and repeat, no
// trigonometric hash or external assets. Its exact pattern differs from iChannel0.
float noiseCell(ivec2 p) {
    uvec2 q = uvec2(p & ivec2(255));
    uint n = q.x + q.y * 256u + 0x9e3779b9u;
    n ^= n >> 16; n *= 0x7feb352du; n ^= n >> 15;
    n *= 0x846ca68bu; n ^= n >> 16;
    return float(n & 65535u) / 65535.0;
}
float noiseTexture(vec2 uv) {
    vec2 p = fract(uv)*256.0-0.5;
    ivec2 cell = ivec2(floor(p)); vec2 f = fract(p);
    return mix(mix(noiseCell(cell),noiseCell(cell+ivec2(1,0)),f.x),
               mix(noiseCell(cell+ivec2(0,1)),noiseCell(cell+ivec2(1,1)),f.x),f.y);
}
/* Hi there!
 * Here is a demo presenting volumetric rendering single with shadowing.
 * Did it quickly so I hope I have not made any big mistakes :)
 *
 * I also added the improved scattering integration I propose in my SIGGRAPH'15 presentation
 * about Frostbite new volumetric system I have developed. See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
 * Basically it improves the scattering integration for each step with respect to extinction
 * The difference is mainly visible for some participating media having a very strong scattering value. 
 * I have setup some pre-defined settings for you to checkout below (to present the case it improves):
 * - D_DEMO_SHOW_IMPROVEMENT_xxx: shows improvement (on the right side of the screen). You can still see aliasing due to volumetric shadow and the low amount of sample we take for it.
 * - D_DEMO_SHOW_IMPROVEMENT_xxx_NOVOLUMETRICSHADOW: same as above but without volumetric shadow
 *
 * To increase the volumetric rendering accuracy, I constrain the ray marching steps to a maximum distance.
 *
 * Volumetric shadows are evaluated by raymarching toward the light to evaluate transmittance for each view ray steps (ouch!)
 *
 * Do not hesitate to contact me to discuss about all that :) 
 * SebH
 */



float displacementSimple( vec2 p )
{
    float f;
    f  = 0.5000* noiseTexture(p); p = p*2.0;
    f += 0.2500* noiseTexture(p); p = p*2.0;
    f += 0.1250* noiseTexture(p); p = p*2.0;
    f += 0.0625* noiseTexture(p); p = p*2.0;
    
    return f;
}


vec3 getSceneColor(vec3 p, float material)
{
	if(material==1.0)
	{
		return vec3(1.0, 0.5, 0.5);
	}
	else if(material==2.0)
	{
		return vec3(0.5, 1.0, 0.5);
	}
	else if(material==3.0)
	{
		return vec3(0.5, 0.5, 1.0);
	}
	
	return vec3(0.0, 0.0, 0.0);
}


float getClosestDistance(vec3 p, out float material)
{
	float d = 0.0;
#if D_MAX_STEP_LENGTH_ENABLE
    float minD = 1.0; // restrict max step for better scattering evaluation
#else
	float minD = 10000000.0;
#endif
	material = 0.0;
    
    float yNoise = 0.0;
    float xNoise = 0.0;
    float zNoise = 0.0;
#if D_DETAILED_WALLS
    yNoise = 1.0*clamp(displacementSimple(p.xz*0.005),0.0,1.0);
    xNoise = 2.0*clamp(displacementSimple(p.zy*0.005),0.0,1.0);
    zNoise = 0.5*clamp(displacementSimple(p.xy*0.01),0.0,1.0);
#endif
    
	d = max(0.0, p.y - yNoise);
	if(d<minD)
	{
		minD = d;
		material = 2.0;
	}
	
	d = max(0.0,p.x - xNoise);
	if(d<minD)
	{
		minD = d;
		material = 1.0;
	}
	
	d = max(0.0,40.0-p.x - xNoise);
	if(d<minD)
	{
		minD = d;
		material = 1.0;
	}
	
	d = max(0.0,-p.z - zNoise);
	if(d<minD)
	{
		minD = d;
		material = 3.0;
    }
    
	return minD;
}


vec3 calcNormal( in vec3 pos)
{
    float material = 0.0;
    vec3 eps = vec3(0.3,0.0,0.0);
	return normalize( vec3(
           getClosestDistance(pos+eps.xyy, material) - getClosestDistance(pos-eps.xyy, material),
           getClosestDistance(pos+eps.yxy, material) - getClosestDistance(pos-eps.yxy, material),
           getClosestDistance(pos+eps.yyx, material) - getClosestDistance(pos-eps.yyx, material) ) );

}

vec3 evaluateLight(in vec3 pos)
{
    vec3 lightPos = LPOS;
    vec3 lightCol = LCOL;
    vec3 L = lightPos-pos;
    return lightCol / max(dot(L,L), 0.0001); // finite even at the point light
}

vec3 evaluateLight(in vec3 pos, in vec3 normal)
{
    vec3 lightPos = LPOS;
    vec3 L = lightPos-pos;
    float distanceToL = max(length(L), 0.0001);
    vec3 Lnorm = L/distanceToL;
    return max(0.0,dot(normal,Lnorm)) * evaluateLight(pos);
}

// To simplify: wavelength independent scattering and extinction
void getParticipatingMedia(out float sigmaS, out float sigmaE, in vec3 pos)
{
    float heightFog = 7.0;
    if (fogNoise > 0.0) heightFog += 3.0*clamp(displacementSimple(pos.xz*0.005 + iTime*0.01),0.0,1.0);
    heightFog = 0.3*clamp((heightFog-pos.y)*1.0, 0.0, 1.0);
    
    float fogFactor = 1.0 + pc.medium.w * 5.0;
    
    const float sphereRadius = 5.0;
    float sphereFog = clamp((sphereRadius-length(pos-vec3(20.0,19.0,-17.0)))/sphereRadius, 0.0,1.0);
    
    const float constantFog = 0.02;

    sigmaS = pc.medium.y * (constantFog + heightFog*fogFactor + sphereFog);
   
    const float sigmaA = 0.0;
    sigmaE = max(0.000000001, sigmaA + sigmaS); // to avoid division by zero extinction
}

float phaseFunction()
{
    return 1.0/(4.0*3.14);
}

float volumetricShadow(in vec3 from, in vec3 to)
{
if ((pc.quality.w & 2) != 0) {
    float numStep = float(pc.quality.y); // quality control. Bump to avoid shadow alisaing
    float shadow = 1.0;
    float sigmaS = 0.0;
    float sigmaE = 0.0;
    float dd = length(to-from) / numStep;
    for(float s=0.5; s<(numStep-0.1); s+=1.0)// start at 0.5 to sample at center of integral part
    {
        vec3 pos = from + (to-from)*(s/(numStep));
        getParticipatingMedia(sigmaS, sigmaE, pos);
        shadow *= exp(-sigmaE * dd);
    }
    return shadow;
}
    return 1.0;
}

void traceScene(bool improvedScattering, vec3 rO, vec3 rD, inout vec3 finalPos, inout vec3 normal, inout vec3 albedo, inout vec4 scatTrans)
{
	int numIter = pc.quality.x;
	
    float sigmaS = 0.0;
    float sigmaE = 0.0;
    
    vec3 lightPos = LPOS;
    
    // Initialise volumetric scattering integration (to view)
    float transmittance = 1.0;
    vec3 scatteredLight = vec3(0.0, 0.0, 0.0);
    
	float d = 1.0; // hack: always have a first step of 1 unit to go further
	float material = 0.0;
	vec3 p = vec3(0.0, 0.0, 0.0);
    float dd = 0.0;
    bool hit = false;
	for(int i=0; i<numIter;++i)
	{
		vec3 p = rO + d*rD;
        
        
    	getParticipatingMedia(sigmaS, sigmaE, p);
        
        if(improvedScattering)
        {
            // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
            vec3 S = evaluateLight(p) * sigmaS * phaseFunction()* volumetricShadow(p,lightPos);// incoming light
            vec3 Sint = (S - S * exp(-sigmaE * dd)) / sigmaE; // integrate along the current step segment
            scatteredLight += transmittance * Sint; // accumulate and also take into account the transmittance from previous steps

            // Evaluate transmittance to view independentely
            transmittance *= exp(-sigmaE * dd);
        }
		else
        {
            // Basic scatering/transmittance integration
            if (pc.quality.z == 2) transmittance *= exp(-sigmaE * dd);
            scatteredLight += sigmaS * evaluateLight(p) * phaseFunction() * volumetricShadow(p,lightPos) * transmittance * dd;
            if (pc.quality.z != 2) transmittance *= exp(-sigmaE * dd);
        }
        
		
        dd = getClosestDistance(p, material);
        if(dd<0.2) {
            hit = true;
            break; // give back a lot of performance without too much visual loss
        }
		d += dd;
	}
	
	albedo = hit ? getSceneColor(p, material) : vec3(0.0);
	
    finalPos = rO + d*rD;
    
    // Low step budgets can end in empty space. Do not normalize a zero SDF
    // gradient or evaluate an artificial surface at the iteration limit.
    normal = hit ? calcNormal(finalPos) : vec3(0.0);
    
    scatTrans = vec4(scatteredLight, transmittance);
}


void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    //iTime
    //iMouse
    //iResolution
    
	vec2 uv = fragCoord.xy / iResolution.xy;
    
    float hfactor = float(iResolution.y) / float(iResolution.x); // make it screen ratio independent
	vec2 uv2 = vec2(2.0, 2.0*hfactor) * fragCoord.xy / iResolution.xy - vec2(1.0, hfactor);
	
	vec3 camPos = vec3( 20.0, 18.0,-50.0);

	vec3 camX   = vec3( 1.0, 0.0, 0.0);
	vec3 camY   = vec3( 0.0, 1.0, 0.0);
	vec3 camZ   = vec3( 0.0, 0.0, 1.0);
	
	vec3 rO = camPos;
	vec3 rD = normalize(uv2.x*camX + uv2.y*camY + camZ);
	vec3 finalPos = rO;
	vec3 albedo = vec3( 0.0, 0.0, 0.0 );
	vec3 normal = vec3( 0.0, 0.0, 0.0 );
    vec4 scatTrans = vec4( 0.0, 0.0, 0.0, 0.0 );
    traceScene( pc.quality.z == 0 || (pc.quality.z == 3 && fragCoord.x>(iResolution.x/2.0)),
        rO, rD, finalPos, normal, albedo, scatTrans);
	
    
    //lighting
    vec3 color = (albedo/3.14) * evaluateLight(finalPos, normal) * volumetricShadow(finalPos, LPOS);
    // Apply scattering/transmittance
    color = color * scatTrans.w + scatTrans.xyz;
    
    // Gamma correction
	// Original gamma correction moved to display.frag (RGBA16F remains linear).
   
if (pc.quality.z == 3) {
    // Separation line
    if(abs(fragCoord.x-(iResolution.x*0.5))<0.6)
        color.r = pow(0.5,2.2);
}
    
	fragColor = vec4(color ,1.0);
}




void main() {
    vec2 coord = gl_FragCoord.xy-pc.viewport.xy;
    // Shadertoy's fragCoord origin is bottom-left, Vulkan's is top-left.
    coord.y = pc.viewport.w-coord.y;
    mainImage(outColor,coord);
}
