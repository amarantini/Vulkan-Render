#version 450

#include "common.glsl"


layout (binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform sampler2D displacementMap;
layout (binding = 3) uniform samplerCube environmentMap;
layout (binding = 4) uniform sampler2D albedoMap;
layout (std140, binding = 9) uniform UniformBufferObjectLight {
	int spotLightCount;
    int sphereLightCount;
    int directionalLightCount;
    SpotLight spotLights[MAX_LIGHT_COUNT];
    SphereLight sphereLights[MAX_LIGHT_COUNT];
    DirectionalLight directionalLights[MAX_LIGHT_COUNT];
} uboLight;

layout(location = 0) flat in struct data {
    mat3 light;
    vec3 N; // normal in world space
    vec4 T; // tangent in world space
    vec3 V; // incident ray direction from camera in world space 
    vec2 texCoord;
    vec3 fragPos; // vertex position in world space
	vec3 color;
} inData;

layout(location = 0) out vec4 outColor;

const float height_scale = 0.1;

mat3 computeTBN() {
	vec3 N = normalize(inData.N);
	vec3 T = normalize(inData.T.xyz);
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(cross(N, T) * inData.T.w);
	mat3 TBN = mat3(T, B, N);
	return TBN;
}

vec3 computeNormal(mat3 TBN, vec2 texCoords) {
	// obtain normal from normal map in range [0,1]
	// transform normal vector to range [-1,1]
	vec3 sampledNormal = 2.0 * texture(normalMap, texCoords).rgb - 1.0;
	
	return normalize(TBN * sampledNormal);
}

vec2 ParallaxMapping(vec3 viewDir)
{ 
	// parallax occlusion mapping (referenced from learnopengl)
	// taking less samples when looking straight at a surface and more samples when looking at an angle
   	const float minLayers = 8.0;
	const float maxLayers = 32.0;
	float numLayers = mix(maxLayers, minLayers, max(dot(vec3(0.0, 0.0, 1.0), viewDir), 0.0)); 
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy * height_scale; 
    vec2 deltaTexCoords = P / numLayers;

	vec2 currentTexCoords = inData.texCoord;
	float currentDepthMapValue = texture(displacementMap, currentTexCoords).r;
	
	while(currentLayerDepth < currentDepthMapValue)
	{
		// shift texture coordinates along direction of P
		currentTexCoords -= deltaTexCoords;
		// get displacement value at current texture coordinates
		currentDepthMapValue = texture(displacementMap, currentTexCoords).r;  
		// get depth of next layer
		currentLayerDepth += layerDepth;  
	}

	// interpolate between previous and current depth layer's coordinates
	// get texture coordinates before collision (reverse operations)
	vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

	// get depth after and before collision for linear interpolation
	float afterDepth  = currentDepthMapValue - currentLayerDepth;
	float beforeDepth = texture(displacementMap, prevTexCoords).r - currentLayerDepth + layerDepth;
	
	// interpolation of texture coordinates
	float weight = afterDepth / (afterDepth - beforeDepth);
	vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

	return finalTexCoords;     
} 

vec3 calculateSphereLight(SphereLight l, vec3 N, vec3 R, vec3 albedo) {
	vec3 pos = l.pos.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	vec3 color = l.color.rgb;
	vec3 closestPoint = calculateClosestPoint(pos, inData.fragPos, R, radius);
	vec3 L = closestPoint - inData.fragPos;

	float distance = length(L);
	L = normalize(L);
	float attenuation = 1.0 / (distance * distance);
	vec3 radiance = color * attenuation;   

	if(limit>0) {
		float limit_factor = max(0, pow((1-distance/limit),4));
		radiance *= limit_factor;
	}

	float NdotL = clamp(dot(N, L), 0.0, 1.0);  
	return radiance * NdotL; 
}

vec3 calculateSpotLight(SpotLight l, vec3 N, vec3 R, vec3 albedo) {
	vec3 pos = l.pos.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	vec3 color = l.color.rgb;
	vec3 direction = l.direction.rgb;
	float outter = l.others[2];
	float inner = l.others[3];
	vec3 closestPoint = calculateClosestPoint(pos, inData.fragPos, R, radius);
	vec3 L = closestPoint - inData.fragPos;

	float distance = length(L);
	L = normalize(L);
	float theta = acos(dot(L, normalize(-direction)));
    
	if(theta > outter) 
	{       
		return vec3(0.0);
	}

	float attenuation = 1.0 / (distance * distance);
	vec3 radiance = color * attenuation; 
	if(theta > inner) {
		radiance *= (theta - inner) / (outter - inner);
	}

	if(limit>0) {
		float limit_factor = max(0, pow((1-distance/limit),4));
		radiance *= limit_factor;
	}  
	
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	return radiance * NdotL; 
}

vec3 calculateDirLight(DirectionalLight l, vec3 N, vec3 R, vec3 albedo) {
	vec3 direction = l.direction.rgb;
	float angle = l.others[0];
	vec3 color = l.color.rgb;

	vec3 lightDir = normalize(-direction);
	float radius = tan(angle / 2.0f);
	vec3 centerToRay = normalize((dot(lightDir, R)*R - lightDir));
	vec3 closestPoint = lightDir + centerToRay * radius;
	vec3 L = closestPoint - inData.fragPos;
	vec3 radiance = color;
	 
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	return radiance * NdotL; 
}


vec3 calculateLights(vec3 N, vec3 R, vec3 albedo) {
	vec3 Lo = vec3(0.0);
	for(int i=0; i<uboLight.sphereLightCount; i++) {
		Lo += calculateSphereLight(uboLight.sphereLights[i], N, R, albedo);
	}
	for(int i=0; i<uboLight.spotLightCount; i++) {
		Lo += calculateSpotLight(uboLight.spotLights[i], N, R, albedo);
	}
	for(int i=0; i<uboLight.directionalLightCount; i++) {
		Lo += calculateDirLight(uboLight.directionalLights[i], N,  R, albedo);
	}
	return Lo / M_PI;
}

void main() {
	mat3 TBN = computeTBN();

	// Displacement mapping
	vec2 texCoords = ParallaxMapping(transpose(TBN) * inData.V);
	if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0) {
		discard;
	}

	// Normal mapping
	vec3 N = computeNormal(TBN, texCoords);
	vec3 V = normalize(inData.V);
	vec3 R = normalize(reflect(-V, N)); 

	// compute radiance from rgbe
	vec3 rad = toRadiance(texture(environmentMap, normalize(inData.light * N)));
	
	vec3 albedo = texture(albedoMap, texCoords).rgb;
	vec3 color = rad * albedo;

	// From light sources
	vec3 Lo = calculateLights(N, R, albedo);
	color += Lo;

	// tone mapping
	outColor = vec4(toneMapping(color), 1.0);
}
