#version 450

#include "common.glsl"

layout (binding = 1) uniform sampler2D normalMap;
layout (binding = 2) uniform sampler2D displacementMap;
layout (binding = 3) uniform samplerCube irradianceMap;
layout (binding = 4) uniform samplerCube prefilteredMap;
layout (binding = 5) uniform sampler2D brdfLUT;
layout (binding = 6) uniform sampler2D albedoMap;
layout (binding = 7) uniform sampler2D metalnessMap;
layout (binding = 8) uniform sampler2D roughnessMap;

layout(location = 0) in struct data {
    mat3 light;
    vec3 N; // in world space
    vec4 tangent; // in world space
	vec2 texCoord;
	vec3 V; // V incident ray direction from camera in world space
} inData;

layout(location = 0) out vec4 outColor;

vec3 computeNormal() {
	// obtain normal from normal map in range [0,1]
	// transform normal vector to range [-1,1]
	vec3 sampledNormal = 2.0 * texture(normalMap, inData.texCoord).rgb - 1.0;
	
	vec3 N = normalize(inData.N);
	vec3 T = normalize(inData.tangent.xyz);
	vec3 B = normalize(cross(N, T) * inData.tangent.w);
	mat3 TBN = mat3(T, B, N);
	return normalize(TBN * sampledNormal);
}

vec3 prefilteredReflection(vec3 R, float roughness)
{
	const float MAX_REFLECTION_LOD = 4.0; // lod = 0,1,2,3,4
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = textureLod(prefilteredMap, R, lodf).rgb;
	vec3 b = textureLod(prefilteredMap, R, lodc).rgb;
	return mix(a, b, lod - lodf);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

void main() {
	// Normal mapping
	vec3 V = normalize(inData.V);
	vec3 N = normalize(computeNormal());
	vec3 R = normalize(reflect(-V, N)); 

	// compute radiance from rgbe
	vec3 irrad = toRadiance(texture(irradianceMap, normalize(inData.light * N)));

	float NdotV = min(max(dot(N, V), 0.0),1.0);
	
	float metalness = texture(metalnessMap, inData.texCoord).r;
	float roughness = texture(roughnessMap, inData.texCoord).r;
	vec3 albedo = texture(albedoMap, inData.texCoord).rgb;
	vec2 brdf = texture(brdfLUT, vec2(NdotV,roughness)).rg;
	vec3 prefilteredColor = prefilteredReflection(normalize(inData.light * R), roughness);

	vec3 ALBEDO = pow(texture(albedoMap, inData.texCoord).rgb, vec3(2.2));
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, ALBEDO, metalness);

	vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metalness;

	// Diffuse
	vec3 diffuse = irrad * albedo;

	// Specular
	vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
	
	// Ambient
	vec3 color = kD * diffuse + specular; 

	// tone mapping
	outColor = vec4(toneMapping(color), 1.0);
}
