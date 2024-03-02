#version 450

#include "common.glsl"

layout (binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform sampler2D displacementMap;
layout (binding = 3) uniform samplerCube environmentMap;

layout(location = 0) in struct data {
    mat3 light;
    vec3 normal; // in world space
    vec4 tangent; // in world space
	vec2 texCoord;
} inData;

layout(location = 0) out vec4 outColor;

vec3 computeNormal() {
	// obtain normal from normal map in range [0,1]
	// transform normal vector to range [-1,1]
	vec3 sampledNormal = 2.0 * texture(normalMap, inData.texCoord).rgb - 1.0;
	
	vec3 N = normalize(inData.normal);
	vec3 T = normalize(inData.tangent.xyz);
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(cross(N, T) * inData.tangent.w);
	mat3 TBN = mat3(T, B, N);
	return normalize(TBN * sampledNormal);
}

void main() {
	// Normal mapping
	vec3 normal = computeNormal();

	// compute radiance from rgbe
	vec3 rad = toRadiance(texture(environmentMap, normalize(inData.light * normal)));
	// tone mapping
	//outColor = vec4(toneMapping(rad), 1.0f);
	outColor = vec4(rad, 1.0f);
	//outColor = vec4(texture(environmentMap,vec3(0,0,1)).rgb, 1.0); // read from positive z face
}
