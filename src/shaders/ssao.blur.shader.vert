#version 450

layout (location = 0) out struct data {
    vec2 uv;
} outData;

void main() 
{
	outData.uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	gl_Position = vec4(outData.uv * 2.0f - 1.0f, 0.0f, 1.0f);
}