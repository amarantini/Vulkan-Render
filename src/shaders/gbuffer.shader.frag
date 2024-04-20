#version 450

#include "common.glsl"


layout (set = 1, binding = 0) uniform sampler2D normalMap;
layout (set = 1, binding = 1) uniform sampler2D displacementMap;
layout (set = 1, binding = 2) uniform sampler2D albedoMap;
layout (set = 1, binding = 3) uniform sampler2D metalnessMap;
layout (set = 1, binding = 4) uniform sampler2D roughnessMap;

layout(location = 0) in struct data {
    vec3 N; // normal in world space
    vec4 T; // tangent in world space
    vec3 V; // incident ray direction from camera in world space 
    vec2 texCoord;
    vec4 fragPos; // vertex position in world space
} inData;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec4 outNormal;
layout (location = 2) out vec4 outAlbedo;
layout (location = 3) out float outRoughness;
layout (location = 4) out float outMetalness;

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

vec2 ParallaxMapping(vec3 viewDir){ 
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
    vec2 P = viewDir.xy * HEIGHT_SCALE; 
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

void main() {
	mat3 TBN = computeTBN();

	// Displacement mapping
	vec2 texCoords = ParallaxMapping(transpose(TBN) * inData.V);
	if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0) {
		discard;
	}

	// Normal mapping
	vec3 N = computeNormal(TBN, texCoords);

    outPosition = inData.fragPos;
    outNormal = vec4(N, 1.0);
    outAlbedo = texture(albedoMap, texCoords);
    outRoughness = texture(roughnessMap, texCoords).r;
    outMetalness = texture(metalnessMap, texCoords).r;
}