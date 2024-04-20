#version 450

layout(set = 0, binding = 0) uniform UniformBufferObject {
    mat4 view;
    mat4 proj;
    mat4 light; // environment lighting's world to local transformation
    vec4 eye; // world space camera position
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
    vec3 normal; // in world space
    vec4 tangent; // in world space
    vec2 texCoord;
    vec3 view; // incident ray direction from camera in world space
} outData;

void main() {
    gl_Position = ubo.proj * ubo.view * pc.model * vec4(inPosition, 1.0);

    //mat3 normalMatrix = transpose(inverse(mat3(pc.model)));
    mat3 normalMatrix = mat3(pc.invModel);
    outData.normal = normalize(normalMatrix * inNormal);
    outData.tangent = vec4(normalize(normalMatrix * inTangent.xyz), inTangent.w);
    outData.view = normalize(vec3(ubo.eye - (pc.model * vec4(inPosition, 1.0))));
    outData.light = mat3(ubo.light);
    outData.texCoord = inTexCoord;

}
