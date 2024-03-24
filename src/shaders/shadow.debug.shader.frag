

#version 450

layout (binding = 1) uniform sampler2D shadowMapSampler;

layout(location = 0) in struct data {
  float zNear;
  float zFar;
  vec2 uv;
} inData;
layout (location = 0) out vec4 outFragColor;

const float SHADOW_ZNEAR = 0.1f;
const float SHADOW_ZFAR = 20.0f;

//Reference https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/shadowmapping/quad.frag
float LinearizeDepth(float depth)
{
  float n = inData.zNear;
  float f = inData.zFar;
  float z = depth;
  return (2.0 * n) / (f + n - z * (f - n));	
}

void main() 
{
	float depth = texture(shadowMapSampler, inData.uv).r;
	outFragColor = vec4(vec3(LinearizeDepth(depth)), 1.0);
}