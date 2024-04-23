#version 450

#define SSAO_SAMPLE_SIZE 64
#define RADIUS 5
#define BIAS 0.025

layout (set = 0, binding = 6) uniform sampler2D positionMap;
layout (set = 0, binding = 7) uniform sampler2D normalMap;
layout (set = 0, binding = 8) uniform sampler2D ssaoNoise;

layout (set = 0, binding = 1) uniform UniformBufferObjectSSAO {
    vec4 samples[SSAO_SAMPLE_SIZE];
} uboSSAO;

layout(location = 0) out float outColor;

layout (location = 0) in struct data {
    vec2 uv;
    mat4 proj;
    mat4 view;
} inData;



void main() {
    // Reference: https://learnopengl.com/Advanced-Lighting/SSAO

    ivec2 screenDim = textureSize(positionMap, 0); 
	ivec2 noiseSize = textureSize(ssaoNoise, 0);

    //vec3 fragPos = texture(positionMap, inData.uv).xyz;
    //fragPos = mat3(inData.view) * fragPos;
    vec3 fragPos = (inData.view * texture(positionMap, inData.uv)).xyz;
    
    vec3 normal = texture(normalMap, inData.uv).rgb;
    // tile noise texture over screen, based on screen dimensions divided by noise size
    vec2 noiseUV = inData.uv * vec2(float(screenDim.x)/float(noiseSize.x), float(screenDim.y)/float(noiseSize.y));
    vec3 randomVec = texture(ssaoNoise, noiseUV).xyz;  

    // convert fragPos and normal to view space
    normal = normalize(mat3(inData.view) * normal);
    //normal = normalize(transpose(inverse(mat3(inData.view))) * normal);

    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for(int i=0; i<SSAO_SAMPLE_SIZE; i++){
        vec3 samplePos = TBN * uboSSAO.samples[i].xyz; //convert samle from tangent space to view space
        samplePos = fragPos + samplePos * RADIUS;

        vec4 offset = vec4(samplePos, 1.0);
        offset = inData.proj * offset; // To clip space
        offset.xy /= offset.w; // perspective divide, range -1.0 to 1.0
        offset.xy = offset.xy * 0.5 + 0.5; // range 0.0 to 1.0;
        
        vec4 sampleWorldPos = texture(positionMap, offset.xy);
        if(sampleWorldPos != vec4(0,0,0,1)) {
            vec4 sampleViewPos = inData.view * sampleWorldPos;
            float sampleDepth = sampleViewPos.z;
            float rangeCheck = smoothstep(0.0, 1.0, RADIUS / abs(fragPos.z - sampleDepth));
            occlusion += (sampleDepth >= samplePos.z + BIAS ? 1.0 : 0.0) * rangeCheck;
        }
    }

    outColor = 1.0 - (occlusion / SSAO_SAMPLE_SIZE);
}