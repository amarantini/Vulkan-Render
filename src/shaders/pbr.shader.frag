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
layout (std140, binding = 9) uniform UniformBufferObjectLight {
	int spotLightCount;
    int sphereLightCount;
    int directionalLightCount;
    SpotLight spotLights[MAX_LIGHT_COUNT];
    SphereLight sphereLights[MAX_LIGHT_COUNT];
    DirectionalLight directionalLights[MAX_LIGHT_COUNT];
} uboLight;

layout(location = 0) in struct data {
    mat3 light;
    vec3 N; // normal in world space
    vec4 T; // tangent in world space
    vec3 V; // incident ray direction from camera in world space 
    vec2 texCoord;
    vec3 fragPos; // vertex position in world space
} inData;

layout(location = 0) out vec4 outColor;

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

const float height_scale = 0.1;

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

vec3 prefilteredReflection(vec3 R, float roughness) {
	const float MAX_REFLECTION_LOD = 4.0; // lod = 0,1,2,3,4
	float lod = roughness * MAX_REFLECTION_LOD;
	float lodf = floor(lod);
	float lodc = ceil(lod);
	vec3 a = toRadiance(textureLod(prefilteredMap, R, lodf));
	vec3 b = toRadiance(textureLod(prefilteredMap, R, lodc));
	return mix(a, b, lod - lodf);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

// Reference https://learnopengl.com/PBR/Lighting
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = M_PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  


vec3 calculateSphereLight(SphereLight l, vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec3 albedo) {
	vec3 pos = l.pos.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	vec3 color = l.color.rgb;

	vec3 closestPoint = calculateClosestPoint(pos, inData.fragPos, R, radius);
	vec3 L = closestPoint - inData.fragPos;

	float distance = length(L);
	L = normalize(L);
	vec3 H = normalize(V + L);
	float attenuation = 1.0 / (distance * distance);
	vec3 radiance = color * attenuation;   

	if(limit>0) {
		float limit_factor = max(0, pow((1-distance/limit),4));
		radiance *= limit_factor;
	}
	
	// cook-torrance brdf
	float NDF = DistributionGGX(N, H, roughness);        
	float G = GeometrySmith(N, V, L, roughness);      
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);       
	
	vec3 kS = F;
	vec3 kD = (vec3(1.0) - kS) *(1.0 - metallness);	

	float NdotL = clamp(dot(N, L), 0.0, 1.0);  
	float NdotV = clamp(dot(N, V), 0.0, 1.0);  
	
	vec3 numerator  = NDF * G * F;
	float denominator = 4.0 * NdotV * NdotL + 0.0001;
	vec3 specular = numerator / denominator;  
		
	return (kD * albedo / M_PI + specular) * radiance * NdotL; 
}

vec3 calculateSpotLight(SpotLight l, vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec3 albedo) {
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

	vec3 H = normalize(V + L);
	float attenuation = 1.0 / (distance * distance);
	vec3 radiance = color * attenuation; 
	if(theta > inner) {
		radiance *= (theta - inner) / (outter - inner);
	}

	if(limit>0) {
		float limit_factor = max(0, pow((1-distance/limit),4));
		radiance *= limit_factor;
	}  
	
	// cook-torrance brdf
	float NDF = DistributionGGX(N, H, roughness);        
	float G = GeometrySmith(N, V, L, roughness);      
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);       
	
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallness;	

	float NdotL = clamp(dot(N, L), 0.0, 1.0);  
	float NdotV = clamp(dot(N, V), 0.0, 1.0);  
	
	vec3 numerator  = NDF * G * F;
	float denominator = 4.0 * NdotV * NdotL + 0.0001;
	vec3 specular = numerator / denominator;  
		
	return (kD * albedo / M_PI + specular) * radiance * NdotL; 
}

vec3 calculateDirLight(DirectionalLight l, vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec3 albedo) {
	vec3 direction = l.direction.rgb;
	float angle = l.others[0];
	vec3 color = l.color.rgb;

	vec3 lightDir = normalize(-direction);
	float radius = tan(angle / 2.0f);
	vec3 centerToRay = normalize((dot(lightDir, R)*R - lightDir));
	vec3 closestPoint = lightDir + centerToRay * radius;
	vec3 L = closestPoint - inData.fragPos;

	vec3 H = normalize(V + L);
	vec3 radiance = color;     
	 
	// cook-torrance brdf
	float NDF = DistributionGGX(N, H, roughness);        
	float G = GeometrySmith(N, V, L, roughness);      
	vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);       
	
	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallness;	

	float NdotL = clamp(dot(N, L), 0.0, 1.0);  
	float NdotV = clamp(dot(N, V), 0.0, 1.0);  
	
	vec3 numerator  = NDF * G * F;
	float denominator = 4.0 * NdotV * NdotL + 0.0001;
	vec3 specular = numerator / denominator;  
		
	return (kD * albedo / M_PI + specular) * radiance * NdotL;
}

vec3 calculateLights(vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec3 albedo) {
	vec3 Lo = vec3(0.0);
	for(int i=0; i<uboLight.sphereLightCount; i++) {
		Lo += calculateSphereLight(uboLight.sphereLights[i], F0, metallness, roughness, N, V, R, albedo);
	}
	for(int i=0; i<uboLight.spotLightCount; i++) {
		Lo += calculateSpotLight(uboLight.spotLights[i], F0, metallness, roughness, N, V, R, albedo);
	}
	for(int i=0; i<uboLight.directionalLightCount; i++) {
		Lo += calculateDirLight(uboLight.directionalLights[i], F0, metallness, roughness, N, V, R, albedo);
	}
	return Lo;
}


void main() {
	mat3 TBN = computeTBN();

	// Displacement mapping
	vec2 texCoords = ParallaxMapping(transpose(TBN) * inData.V);
	if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0) {
		discard;
	}

	// Normal mapping
	vec3 V = normalize(inData.V);
	vec3 N = computeNormal(TBN, texCoords);
	vec3 R = normalize(reflect(-V, N)); 

	// compute radiance from rgbe
	vec3 irrad = toRadiance(texture(irradianceMap, normalize(inData.light * N)));

	float NdotV = min(max(dot(N, V), 0.0),1.0);
	
	float metallness = texture(metalnessMap, texCoords).r;
	float roughness = texture(roughnessMap, texCoords).r;
	vec3 albedo = texture(albedoMap, texCoords).rgb;
	vec2 brdf = texture(brdfLUT, vec2(NdotV,roughness)).rg;
	vec3 prefilteredColor = prefilteredReflection(normalize(inData.light * R), roughness);

	//Reference https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/pbrtexture/pbrtexture.frag
	vec3 F0 = vec3(0.04); 
	F0 = mix(F0, albedo, metallness);

	vec3 F = fresnelSchlickRoughness(NdotV, F0, roughness);

	vec3 kS = F;
	vec3 kD = 1.0 - kS;
	kD *= 1.0 - metallness;

	// Diffuse
	vec3 diffuse = irrad * albedo;

	// Specular
	vec3 specular = prefilteredColor * (F * brdf.x + brdf.y);
	
	// Ambient
	vec3 ambient = kD * diffuse + specular; 

	// From light sources
	vec3 Lo = calculateLights(F0, metallness, roughness, N, V, R, albedo);

	vec3 color = ambient + Lo;
	//vec3 color = ambient;

	// tone mapping
	outColor = vec4(toneMapping(color), 1.0);
}
