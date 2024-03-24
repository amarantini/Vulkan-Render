#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec2 inTexCoord;

layout(push_constant) uniform pushConstant
{
	mat4 model;
    mat4 lightVP;
} pc;
 
void main()
{
	gl_Position =  pc.lightVP * pc.model * vec4(inPosition, 1.0);
}