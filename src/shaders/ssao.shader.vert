#version 450

layout (location = 0) out struct data {
    vec2 uv;
    mat4 proj;
    mat4 view;
} outData;

layout(set = 0, binding = 0) uniform UniformBufferObjectScene {
    mat4 view;
    mat4 proj;
    mat4 light; //light's world to local transformation
    vec4 eye; //world space eye position
} ubo;

void main() 
{
	outData.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outData.uv * 2.0f - 1.0f, 0.0f, 1.0f);

    outData.proj = ubo.proj;
    outData.view = ubo.view;
}