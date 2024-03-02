#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 light; //light's world to local transformation
    vec4 eye; //world space eye position
} ubo;

layout(push_constant) uniform pushConstant
{
	mat4 model;
    mat4 invModel;
} pc;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec2 inTexCoord;

layout(location = 0) out struct data {
    mat3 light;
    vec3 N; // in world space
    vec4 tangent; // in world space
    vec2 texCoord;
    vec3 V; // incident ray direction from camera in world space
} outData;

void main() {
    gl_Position = ubo.proj * ubo.view * pc.model * vec4(inPosition, 1.0);

    mat3 normalMatrix = mat3(pc.invModel);
    outData.light = mat3(ubo.light);
    outData.N = normalize(normalMatrix * inNormal);
    outData.tangent = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);
    outData.texCoord = inTexCoord;
    outData.V = vec3(ubo.eye - (pc.model * vec4(inPosition, 1.0)));
}
