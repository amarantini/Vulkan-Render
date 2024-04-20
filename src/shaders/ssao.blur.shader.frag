#version 450

layout (set = 0, binding = 6) uniform sampler2D ssaoMap;

layout(location = 0) out float outColor;

layout (location = 0) in struct data {
    vec2 uv;
} inData;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(ssaoMap, 0)); 
	int range = 2;

    float result = 0.0;
    int count = 0;
    for(int x = -range; x < range; x++){
        for(int y = -range; y<range; y++) {
            vec2 offset = texelSize * vec2(x,y);
            vec2 uv = inData.uv + offset;
            if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
                continue;
            }
            result += texture(ssaoMap, uv).r;
            count++;
        }
    }
    outColor = result / float(count);
}