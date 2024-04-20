#version 450

#include "common.glsl"

layout (set = 1, binding = 0) uniform sampler2D normalMap;
layout (set = 1, binding = 1) uniform sampler2D displacementMap;
layout (set = 1, binding = 2) uniform samplerCube irradianceMap;
layout (set = 1, binding = 3) uniform sampler2D albedoMap;
layout (set = 1, binding = 4) uniform sampler2D metalnessMap;
layout (set = 1, binding = 5) uniform sampler2D roughnessMap;
layout (set = 1, binding = 6) uniform samplerCube prefilteredMap;
layout (set = 1, binding = 7) uniform sampler2D brdfLUT;


layout (std140, set = 0, binding = 1) uniform UniformBufferObjectLight {
	int spotLightCount;
    int sphereLightCount;
    int directionalLightCount;
	SphereLight sphereLights[MAX_LIGHT_COUNT];
	SpotLight spotLights[MAX_LIGHT_COUNT];
    DirectionalLight directionalLights[MAX_LIGHT_COUNT];
} uboLight;
layout(set = 0, binding = 2) uniform sampler shadowMapSampler;
layout(set = 0, binding = 3) uniform texture2D shadowMaps[MAX_LIGHT_COUNT];
layout(set = 0, binding = 4) uniform sampler shadowCubemapSampler;
layout(set = 0, binding = 5) uniform textureCube shadowCubeMaps[MAX_LIGHT_COUNT]; //Shadow map for sphere light

layout(location = 0) in struct data {
    mat3 light;
    vec3 N; // normal in world space
    vec4 T; // tangent in world space
    vec3 V; // incident ray direction from camera in world space 
    vec2 texCoord;
    vec4 fragPos; // vertex position in world space
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

// =============== Light =================
float calculateShadow(vec4 fragPosLightSpace, int shadow_res, int shadow_map_idx, vec3 N, vec3 L)
{
	// Reference https://learnopengl.com/Advanced-Lighting/Shadows/Shadow-Mapping
   // perform perspective divide, map coordinates to range [-1, 1]
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

	if (projCoords.z > 1.0)
      return 1.0;

	// map coordinates from range [-1,1] to [0,1] (Vulkan's Z is already in [0,1])
    projCoords.xy = projCoords.xy * 0.5 + 0.5; 

	// get depth of current fragment from light's perspective 
	float currentDepth = projCoords.z;
	
	// check whether current frag pos is in shadow
	// percentage-closer filtering, PCF
	float shadow = 0.0; //percentage in shadow
	vec2 texelSize = 1.5 * vec2(1.0 / shadow_res);
	int range = 2;
	int count = 0;
	for(int x = -range; x <= range; ++x)
	{
		for(int y = -range; y <= range; ++y)
		{
			float pcfDepth = texture(sampler2D(shadowMaps[shadow_map_idx], shadowMapSampler), projCoords.xy + vec2(x, y) * texelSize).r; 
			shadow += currentDepth > pcfDepth ? 1.0 : 0.0;
			count++;       
		}    
	}
	shadow /= count;

	return shadow;
}

float calculateShadowCube(vec3 fragToLight, int shadow_res, int shadow_map_idx)
{
	// get depth of current fragment from light's perspective 
	float currentDepth = length(fragToLight);
	fragToLight = normalize(fragToLight);
	
	// Reference https://learnopengl.com/Advanced-Lighting/Shadows/Point-Shadows
	// check whether current frag pos is in shadow
	// percentage-closer filtering, PCF
	float shadow = 0.0; //percentage in shadow
	float samples = 4.0;
	float offset  = 0.01;
	int count = 0;
	float bias = 0.05; 
	for(float x = -offset; x <= offset; x += offset / (samples * 0.5))
	{
		for(float y = -offset; y <= offset; y += offset / (samples * 0.5))
		{
			for(float z = -offset; z <= offset; z += offset / (samples * 0.5)) {
				float pcfDepth = texture(samplerCube(shadowCubeMaps[shadow_map_idx], shadowCubemapSampler), fragToLight + vec3(x, y, z)).r; 
				shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
				count++;  
			}          
		}    
	}
	shadow /= count;

	return shadow;
}

float calculateSphereNormalization(float roughness, float radius, float distance) {
	float a = roughness * roughness;
	float a_p = clamp(a + radius/distance, 0, 1);
	return a * a / (a_p * a_p);
}

vec3 calculateSphereLightDiffuse(SphereLight l, vec3 N, vec3 R, vec4 fragPos) {
	vec3 pos = l.pos.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	vec3 color = l.color.rgb;
	vec3 L = pos - vec3(fragPos);
	
	float distance = length(L);
	L = normalize(L);
	
    vec3 radiance = color;
    float attenuation = calculateFalloff(distance, radius);
	radiance *= attenuation;   
	
    float limit_factor = calculateLimit(distance, limit);
    radiance *= limit_factor;

	float NdotL = clamp(dot(N, L), 0.0, 1.0);  
	return radiance * NdotL; 
}

vec3 calculateSpotLightDiffuse(SpotLight l, vec3 N, vec3 R, vec4 fragPos) {
	vec3 pos = l.pos.rgb;
	vec3 color = l.color.rgb;
	vec3 direction = l.direction.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	float outter = l.others[2];
	float inner = l.others[3];
	
	vec3 L = pos - vec3(fragPos);

	float distance = length(L);
	L = normalize(L);

	vec3 radiance = color;
    float attenuation = calculateFalloff(distance, radius);
	radiance *= attenuation;   

    float limit_factor = calculateLimit(distance, limit);
    radiance *= limit_factor;

	float cos_theta = dot(L, normalize(-direction));
	float cos_outter = cos(outter);
	float cos_inner = cos(inner);
	radiance *= clamp((cos_theta - cos_outter) / (cos_inner - cos_outter), 0.0f, 1.0f);
	
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	vec3 total_radiance = radiance * NdotL;

	return total_radiance; 
}

vec3 calculateDirLightDiffuse(DirectionalLight l, vec3 N, vec3 R) {
	vec3 direction = l.direction.rgb;
	vec3 color = l.color.rgb;

	vec3 L = normalize(-direction);
	vec3 radiance = color;
	 
	float NdotL = clamp(dot(N, L), 0.0, 1.0);
	return radiance * NdotL; 
}

vec3 calculateSphereLight(SphereLight l, vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec4 fragPos) {
	// Calculate diffuse
	vec3 diffuse = calculateSphereLightDiffuse(l, N, R, fragPos)  / M_PI;

	// Calculate specular
	vec3 pos = l.pos.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	vec3 color = l.color.rgb;

	vec3 closestPoint = calculateClosestPoint(pos, vec3(fragPos), R, radius);
	vec3 L = closestPoint - vec3(fragPos);

	float distance = length(L);
	L = normalize(L);
	vec3 H = normalize(V + L);
	vec3 radiance = color;
    float attenuation = calculateFalloff(distance, radius);
	radiance *= attenuation;   

    float limit_factor = calculateLimit(distance, limit);
    radiance *= limit_factor;
	
	// Reference https://learnopengl.com/PBR/Lighting
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
	vec3 specular = numerator / denominator * radiance * NdotL;  
	specular *= calculateSphereNormalization(roughness, radius, distance);

	vec3 total_radiance = kD * diffuse + specular; 

	int shadow_res = int(l.shadow[0]);
	int shadow_map_idx = int(l.shadow[1]);
	float shadow = 0.0f;
	if(shadow_res != 0) {
		vec3 fragToLight = vec3(fragPos - l.pos);
		shadow = calculateShadowCube(fragToLight, shadow_res, shadow_map_idx);
	}
	return total_radiance * (1.0-shadow);
}

vec3 calculateSpotLight(SpotLight l, vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec4 fragPos) {
	// Calculate diffuse
	vec3 diffuse = calculateSpotLightDiffuse(l, N, R, fragPos) / M_PI;

	// Calculate specular
	vec3 pos = l.pos.rgb;
	float radius = l.others[0];
	float limit = l.others[1];
	vec3 color = l.color.rgb;
	vec3 direction = l.direction.rgb;
	float outter = l.others[2];
	float inner = l.others[3];

	vec3 closestPoint = calculateClosestPoint(pos, vec3(fragPos), R, radius);
	vec3 L = closestPoint - vec3(fragPos);

	float distance = length(L);
	L = normalize(L);

	vec3 radiance = color;
	//float attenuation = 1.0 / (distance * distance);
    float attenuation = calculateFalloff(distance, radius);
	radiance *= attenuation;   

    float limit_factor = calculateLimit(distance, limit);
    radiance *= limit_factor;

	float cos_theta = dot(L, normalize(-direction));
	float cos_outter = cos(outter);
	float cos_inner = cos(inner);
	radiance *= clamp((cos_theta - cos_outter) / (cos_inner - cos_outter), 0.0f, 1.0f);
	
	vec3 H = normalize(V + L);

	// Reference https://learnopengl.com/PBR/Lighting
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
	vec3 specular = numerator / denominator * radiance * NdotL;  
	specular *= calculateSphereNormalization(roughness, radius, distance);

	vec3 total_radiance = kD * diffuse + specular;

	int shadow_res = int(l.shadow[0]);
	int shadow_map_idx = int(l.shadow[1]);
	float shadow = 0.0;
	if(shadow_res != 0) {
		shadow = calculateShadow(l.lightVP * fragPos, shadow_res, shadow_map_idx, N, L);
		//return vec3(shadow,0,0);
	}
	return total_radiance * (1.0-shadow); 
}

vec3 calculateDirLight(DirectionalLight l, vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R, vec4 fragPos) {
	// Calculate diffuse
	vec3 diffuse = calculateDirLightDiffuse(l, N, R) / M_PI;

	// Calculate specular
	vec3 direction = l.direction.rgb;
	float angle = l.others[0];
	vec3 color = l.color.rgb;

	vec3 lightDir = normalize(-direction);
	float radius = tan(angle / 2.0f);
	vec3 centerToRay = normalize((dot(lightDir, R)*R - lightDir));
	vec3 closestPoint = lightDir + centerToRay * radius;
	vec3 L = closestPoint - vec3(fragPos);
	float distance = length(L);
	L = normalize(L);

	vec3 H = normalize(V + L);
	vec3 radiance = color;     
	
	// Reference https://learnopengl.com/PBR/Lighting
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
	vec3 specular = numerator / denominator * radiance * NdotL;  
	specular *= calculateSphereNormalization(roughness, radius, distance);

	return kD * diffuse + specular; 
}

vec3 calculateLights(vec3 F0, float metallness, float roughness, vec3 N, vec3 V, vec3 R) {
	vec3 Lo = vec3(0.0);
	for(int i=0; i<uboLight.sphereLightCount; i++) {
		Lo += calculateSphereLight(uboLight.sphereLights[i], F0, metallness, roughness, N, V, R, inData.fragPos);
	}
	for(int i=0; i<uboLight.spotLightCount; i++) {
		Lo += calculateSpotLight(uboLight.spotLights[i], F0, metallness, roughness, N, V, R, inData.fragPos);
	}
	for(int i=0; i<uboLight.directionalLightCount; i++) {
		Lo += calculateDirLight(uboLight.directionalLights[i], F0, metallness, roughness, N, V, R, inData.fragPos);
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
	vec3 color = ambient;

	// From light sources
	vec3 Lo = calculateLights(F0, metallness, roughness, N, V, R) * albedo;
	color += Lo;

	// tone mapping
	outColor = vec4(toneMapping(color), 1.0);
	outColor = vec4(1);
}
