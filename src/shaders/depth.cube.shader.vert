#version 450
#include "common.glsl"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec2 inTexCoord;

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec3 outLightPos;

layout (binding = 0) uniform UniformBufferObjectSphereLight {
	SphereLight sphereLights[MAX_LIGHT_COUNT];
    mat4 lightVPs[MAX_LIGHT_COUNT*6];
} uboLight;

layout(push_constant) uniform PushConstantCubeShadow
{
	mat4 model;
    vec4 lightData;//light index, face index
} pc;
 
void main()
{
    int light_id = int(pc.lightData[0]);
    int face_id = int(pc.lightData[1]);
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
	gl_Position = uboLight.lightVPs[face_id] * worldPos;
    outPos = vec3(worldPos);
    outLightPos = vec3(uboLight.sphereLights[light_id].pos);
}