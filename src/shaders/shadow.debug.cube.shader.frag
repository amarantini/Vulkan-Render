

#version 450

layout (binding = 1) uniform samplerCube shadowMapSampler;

layout(location = 0) in struct data {
  float zNear;
  float zFar;
  vec2 uv;
} inData;
layout (location = 0) out vec4 outFragColor;

// Reference https://github.com/SaschaWillems/Vulkan/blob/master/shaders/glsl/shadowmappingomni/cubemapdisplay.frag
vec3 calculateCubeMapSamplePos() {
	vec3 samplePos;
	int x = int(floor(inData.uv.x / 0.25f));
	int y = int(floor(inData.uv.y / (1.0 / 3.0))); 
	if (y == 1) {
		vec2 uv = vec2(inData.uv.x * 4.0f, (inData.uv.y - 1.0/3.0) * 3.0);
		uv = 2.0 * vec2(uv.x - float(x) * 1.0, uv.y) - 1.0;
		switch (x) {
			case 0:	// NEGATIVE_X
				samplePos = vec3(-1.0f, uv.y, uv.x);
				break;
			case 1: // POSITIVE_Z				
				samplePos = vec3(uv.x, uv.y, 1.0f);
				break;
			case 2: // POSITIVE_X
				samplePos = vec3(1.0, uv.y, -uv.x);
				break;				
			case 3: // NEGATIVE_Z
				samplePos = vec3(-uv.x, uv.y, -1.0f);
				break;
		}
	} else {
		if (x == 1) { 
			vec2 uv = vec2((inData.uv.x - 0.25) * 4.0, (inData.uv.y - float(y) / 3.0) * 3.0);
			uv = 2.0 * uv - 1.0;
			switch (y) {
				case 0: // NEGATIVE_Y
					samplePos = vec3(uv.x, -1.0f, uv.y);
					break;
				case 2: // POSITIVE_Y
					samplePos = vec3(uv.x, 1.0f, -uv.y);
					break;
			}
		}
	}
  return samplePos;
}

void main() 
{
	vec3 samplePos = calculateCubeMapSamplePos();
	//if ((samplePos.x != 0.0f) && (samplePos.y != 0.0f)) {
		float depth = texture(shadowMapSampler, samplePos).r / inData.zFar;
    	outFragColor = vec4(vec3(depth), 1.0);
	//}

}