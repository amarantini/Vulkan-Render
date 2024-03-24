// Reference https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/shadowmapping/quad.vert
#version 450

layout(std140, binding = 0) uniform UniformBufferObjectScene {
    float zNear;
    float zFar;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out struct data {
    float zNear;
    float zFar;
	vec2 uv;
} outData;

void main() 
{
	outData.zNear = ubo.zNear;
    outData.zFar = ubo.zFar;
	outData.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outData.uv * 2.0f - 1.0f, 0.0f, 1.0f);
	
}