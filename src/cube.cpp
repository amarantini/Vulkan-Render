#include <_types/_uint32_t.h>
#include <_types/_uint8_t.h>
#include <stdexcept>
#include <string>
#include <random>
#include <iostream>
#include <fstream>

#include "include/math/vec.h"
#define STB_IMAGE_IMPLEMENTATION
#include "include/utils/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "include/utils/stb_image_write.h"
#include "include/math/mathlib.h"
#include "include/utils/arg_parser.h"
#include "include/utils/constants.h"


enum Face {
	PositiveX = 0, NegativeX = 1,
	PositiveY = 2, NegativeY = 3,
	PositiveZ = 4, NegativeZ = 5,
};

float RadicalInverse_VdC(uint bits) 
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}

vec2 Hammersley(uint i, uint N)
{
    return vec2(float(i)/float(N), RadicalInverse_VdC(i));
} 

/* --------------------------- PBR --------------------------- */

vec3 make_sample(){
    //attempt to importance sample upper hemisphere (cos-weighted):
    //based on: http://www.rorydriscoll.com/2009/01/07/better-sampling/
    static std::mt19937 mt(0x12341234);

    float phi = mt() / float(mt.max()) * 2.0f * M_PI;
	float cos_t = std::sqrt(mt() / float(mt.max()));

	float sin_t = std::sqrt(1 - cos_t * cos_t);
	float x = std::cos(phi) * sin_t;
	float z = std::sin(phi) * sin_t;
	float y = cos_t;

	return vec3(x, y, z);
}


void prefilterEnvironmentMapLambertian(std::string in_file_path, std::string out_file_path, uint32_t out_size, int samples) {
    int texWidth, texHeight, texChannels;
    // load Rgbe Cubemap
    unsigned char* buffer = stbi_load(in_file_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!buffer) {
        throw std::runtime_error("failed to load texture image at: "+std::string(in_file_path));
    }

    // convert to float
    int total_pixels = texWidth*texHeight;
    uint8_t * in_data_rgbe = static_cast<uint8_t*>(buffer);
    std::vector<vec3> in_data;
    for(int i=0; i<total_pixels*4; i+=4) {
        u8vec4 rgbe = u8vec4(in_data_rgbe[i], in_data_rgbe[i+1], in_data_rgbe[i+2], in_data_rgbe[i+3]);
        vec3 rgb = rgbe_to_float(rgbe);
        
        in_data.push_back(rgb);
    }

    // Referenced https://github.com/ixchow/15-466-ibl/blob/master/cubes/blur_cube.cpp for cube look up
    std::string faces[6] = {"PositiveX", "NegativeX", "PositiveY", "NegativeY", "PositiveZ", "NegativeZ"};

    auto lookup = [&in_data,&texWidth , &faces](vec3 const &dir) -> vec3 {
		float sc, tc, ma;
		uint32_t f;
        if (std::abs(dir[0]) >= std::abs(dir[1]) && std::abs(dir[0]) >= std::abs(dir[2])) {
            if (dir[0] >= 0) { sc = -dir[2]; tc = -dir[1]; ma = dir[0]; f = PositiveX; }
			else            { sc =  dir[2]; tc = -dir[1]; ma =-dir[0]; f = NegativeX; }
            
        } else if (std::abs(dir[1]) >= std::abs(dir[2])) {
            if (dir[1] >= 0) { sc =  dir[0]; tc =  dir[2]; ma = dir[1]; f = PositiveY; }
			else            { sc =  dir[0]; tc = -dir[2]; ma =-dir[1]; f = NegativeY; }
        } else {
            if (dir[2] >= 0) { sc =  dir[0]; tc = -dir[1]; ma = dir[2]; f = PositiveZ; }
			else            { sc = -dir[0]; tc = -dir[1]; ma =-dir[2]; f = NegativeZ; }
        }

		int32_t s = std::floor(0.5f * (sc / ma + 1.0f) * texWidth);
		s = std::max(0, std::min(int32_t(texWidth)-1, s));
		int32_t t = std::floor(0.5f * (tc / ma + 1.0f) * texWidth);
		t = std::max(0, std::min(int32_t(texWidth)-1, t));
        if(false)
            std::cout<<faces[f]<<"\n";
		return in_data[(f*texWidth+t)*texWidth+s];
	};

    std::vector<vec3> out_data;
    out_data.reserve(out_size * out_size * 6);
    // sample each face
    for(uint32_t f = 0; f < 6; ++f) {
        vec3 sc; //maps to rightward axis on face
		vec3 tc; //maps to upward axis on face
		vec3 ma; //direction to face, normal direction
		//See OpenGL 4.4 Core Profile specification, Table 8.18:
		if      (f == PositiveX) { sc = vec3( 0.0f, 0.0f,-1.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3( 1.0f, 0.0f, 0.0f); }
		else if (f == NegativeX) { sc = vec3( 0.0f, 0.0f, 1.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3(-1.0f, 0.0f, 0.0f); }
		else if (f == PositiveY) { sc = vec3( 1.0f, 0.0f, 0.0f); tc = vec3( 0.0f, 0.0f, 1.0f); ma = vec3( 0.0f, 1.0f, 0.0f); }
		else if (f == NegativeY) { sc = vec3( 1.0f, 0.0f, 0.0f); tc = vec3( 0.0f, 0.0f,-1.0f); ma = vec3( 0.0f,-1.0f, 0.0f); }
		else if (f == PositiveZ) { sc = vec3( 1.0f, 0.0f, 0.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3( 0.0f, 0.0f, 1.0f); }
		else if (f == NegativeZ) { sc = vec3(-1.0f, 0.0f, 0.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3( 0.0f, 0.0f,-1.0f); }
        std::cout<<"Sampling face "<<f<<"\n";
        for (uint32_t t = 0; t < out_size; ++t) {
			for (uint32_t s = 0; s < out_size; ++s) {
                vec3 N = (ma
				            + (2.0f * (s + 0.5f) / out_size - 1.0f) * sc
				            + (2.0f * (t + 0.5f) / out_size - 1.0f) * tc).normalized();
				vec3 temp = (abs(N[2]) < 0.99f ? vec3(0.0f, 0.0f, 1.0f) : vec3(1.0f, 0.0f, 0.0f));
				vec3 TX = cross(N, temp).normalized();
				vec3 TY = cross(N, TX);

                // Importance sampling
                vec3 acc = vec3(0.0f);
                // float total_weight = 0.0f;
                for(uint32_t i=0; i<samples; i++) {
                    vec2 u = Hammersley(i, samples);   
                    float cosTheta = sqrt(1.0 - u[1]);
                    float sinTheta = sqrt(u[1]);
                    float phi = 2 * M_PI * u[0];
                    // float pdf = cosTheta * sinTheta / M_PI;
   

                    vec3 cartesianCoord =
                        vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

                    vec3 sampleVector =
                        (cartesianCoord[0] * TX + cartesianCoord[1] * TY +
                                cartesianCoord[2] * N).normalized();

                    acc += lookup(sampleVector);
                }

                acc = acc * 1.0f / float(samples) ;
                // acc = acc * 1.0f / total_weight;


                // Fixed pattern sampling
                // int phiSample = 360;
                // int thetaSample = 90;
                // float sampleDeltaPhi = 2.0 * M_PI / phiSample;
                // float sampleDeltaTheta = 0.5 * M_PI / thetaSample;
                // int samples = phiSample * thetaSample;
                // for(float phi = 0.0; phi < 2.0 * M_PI; phi += sampleDeltaPhi)
                // {
                //     for(float theta = 0.0; theta < 0.5 * M_PI; theta += sampleDeltaTheta)
                //     {
                //         // spherical to cartesian (in tangent space)
                //         vec3 dir = vec3(sin(theta) * cos(phi),  sin(theta) * sin(phi), cos(theta));
                //         vec3 world_dir = dir[0] * TX + dir[1] * TY + dir[2] * N;
                //         float cos_theta = std::max(0.0f, dot(world_dir, N));
                //         float sin_theta;
                //         if(cos_theta*cos_theta >= 1.0f){
                //             sin_theta = 0.0f;
                //         } else {
                //             sin_theta = sqrt(1.0f - cos_theta*cos_theta);
                //         }
                //         // float sin_theta = std::min(1.0f, sqrt(1.0f - cos_theta*cos_theta));
                //         // tangent space to world
                //         acc += lookup(world_dir) * cos_theta * sin_theta;
                //     }
                // }
                // acc *= M_PI* 1.0f / float(samples) ; 

                // Random sampling
                // for (uint32_t i = 0; i < uint32_t(samples); ++i) {
                //     vec3 dir = make_sample(); // sample direction in tangent space
                //     vec3 world_dir = (dir[0] * TX + dir[1] * TY + dir[2] * N).normalized();
                //     float cos_theta = std::max(0.0f, dot(world_dir, N));
                //     // float sin_theta = sqrt(1.0f - cos_theta*cos_theta);
                //     acc += lookup(world_dir) * cos_theta;
                // }

                
                // std::cout<<acc<<"\n";
                out_data.emplace_back(acc);
            }
        }
    }
    std::cout<<"Finished sampling\n";

    // Convert rgb back to rgbe
    std::vector<u8vec4> out_data_rgbe;
    out_data_rgbe.reserve(out_size * 6 * out_size);
    for (auto const &pix : out_data) {
        out_data_rgbe.emplace_back( float_to_rgbe( pix ) );
    }

    // for(int i=0; i<10; i++){
    //     std::cout<<out_data_rgbe[i];
    // }

    std::cout<<"Save to file: "<<out_file_path<<"\n";
    stbi_write_png(out_file_path.c_str(), out_size, out_size*6, 4, out_data_rgbe.data(), out_size * 4);
}

/* --------------------------- PBR --------------------------- */

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness)
{
    float a = roughness*roughness;
	
    float phi = 2.0 * M_PI * Xi[0];
    float cosTheta = sqrt((1.0 - Xi[1]) / (1.0 + (a*a - 1.0) * Xi[1]));
    float sinTheta = sqrt(1.0 - cosTheta*cosTheta);
	
    // from spherical coordinates to cartesian coordinates
    vec3 H;
    H[0] = cos(phi) * sinTheta;
    H[1] = sin(phi) * sinTheta;
    H[2] = cosTheta;
	
    // from tangent-space vector to world-space sample vector
    vec3 up        = abs(N[2]) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent   = cross(up, N).normalized();
    vec3 bitangent = cross(N, tangent);
	
    vec3 sampleVec = tangent * H[0] + bitangent * H[1] + N * H[2];
    return sampleVec.normalized();
}  

void prefilterEnvironmentMapPbr(std::string in_file_path, std::string out_file_path, uint32_t samples, uint32_t mip_width = 128, uint32_t max_mip_levels = 5) {
    int texWidth, texHeight, texChannels;
    // load Rgbe Cubemap
    unsigned char* buffer = stbi_load(in_file_path.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    if (!buffer) {
        throw std::runtime_error("failed to load texture image at: "+std::string(in_file_path));
    }

    // convert to float
    int total_pixels = texWidth*texHeight;
    uint8_t * in_data_rgbe = static_cast<uint8_t*>(buffer);
    std::vector<vec3> in_data;
    for(int i=0; i<total_pixels*4; i+=4) {
        u8vec4 rgbe = u8vec4(in_data_rgbe[i], in_data_rgbe[i+1], in_data_rgbe[i+2], in_data_rgbe[i+3]);
        vec3 rgb = rgbe_to_float(rgbe);
        
        in_data.push_back(rgb);
    }

    // Referenced https://github.com/ixchow/15-466-ibl/blob/master/cubes/blur_cube.cpp for cube look up
    std::string faces[6] = {"PositiveX", "NegativeX", "PositiveY", "NegativeY", "PositiveZ", "NegativeZ"};

    auto lookup = [&in_data,&texWidth , &faces](vec3 const &dir) -> vec3 {
		float sc, tc, ma;
		uint32_t f;
        if (std::abs(dir[0]) >= std::abs(dir[1]) && std::abs(dir[0]) >= std::abs(dir[2])) {
            if (dir[0] >= 0) { sc = -dir[2]; tc = -dir[1]; ma = dir[0]; f = PositiveX; }
			else            { sc =  dir[2]; tc = -dir[1]; ma =-dir[0]; f = NegativeX; }
            
        } else if (std::abs(dir[1]) >= std::abs(dir[2])) {
            if (dir[1] >= 0) { sc =  dir[0]; tc =  dir[2]; ma = dir[1]; f = PositiveY; }
			else            { sc =  dir[0]; tc = -dir[2]; ma =-dir[1]; f = NegativeY; }
        } else {
            if (dir[2] >= 0) { sc =  dir[0]; tc = -dir[1]; ma = dir[2]; f = PositiveZ; }
			else            { sc = -dir[0]; tc = -dir[1]; ma =-dir[2]; f = NegativeZ; }
        }

		int32_t s = std::floor(0.5f * (sc / ma + 1.0f) * texWidth);
		s = std::max(0, std::min(int32_t(texWidth)-1, s));
		int32_t t = std::floor(0.5f * (tc / ma + 1.0f) * texWidth);
		t = std::max(0, std::min(int32_t(texWidth)-1, t));
        if(false)
            std::cout<<faces[f]<<"\n";
		return in_data[(f*texWidth+t)*texWidth+s];
	};

    
    std::string file_name = out_file_path.substr(0, out_file_path.find_last_of('.'));
    for (uint32_t mip = 0; mip < max_mip_levels; ++mip){
        std::cout<<"Generate mip map "<<mip<<"\n";
        uint32_t out_size  = mip_width * std::pow(0.5, mip);

        float roughness = (float)mip / (float)(max_mip_levels - 1);

        std::vector<vec3> out_data;
        out_data.reserve(out_size * out_size * 6);

            // sample each face
        for(uint32_t f = 0; f < 6; ++f) {
            vec3 sc; //maps to rightward axis on face
            vec3 tc; //maps to upward axis on face
            vec3 ma; //direction to face, normal direction
            //See OpenGL 4.4 Core Profile specification, Table 8.18:
            if      (f == PositiveX) { sc = vec3( 0.0f, 0.0f,-1.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3( 1.0f, 0.0f, 0.0f); }
            else if (f == NegativeX) { sc = vec3( 0.0f, 0.0f, 1.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3(-1.0f, 0.0f, 0.0f); }
            else if (f == PositiveY) { sc = vec3( 1.0f, 0.0f, 0.0f); tc = vec3( 0.0f, 0.0f, 1.0f); ma = vec3( 0.0f, 1.0f, 0.0f); }
            else if (f == NegativeY) { sc = vec3( 1.0f, 0.0f, 0.0f); tc = vec3( 0.0f, 0.0f,-1.0f); ma = vec3( 0.0f,-1.0f, 0.0f); }
            else if (f == PositiveZ) { sc = vec3( 1.0f, 0.0f, 0.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3( 0.0f, 0.0f, 1.0f); }
            else if (f == NegativeZ) { sc = vec3(-1.0f, 0.0f, 0.0f); tc = vec3( 0.0f,-1.0f, 0.0f); ma = vec3( 0.0f, 0.0f,-1.0f); }

            for (uint32_t t = 0; t < out_size; ++t) {
                for (uint32_t s = 0; s < out_size; ++s) {
                    vec3 N = (ma
                                + (2.0f * (s + 0.5f) / out_size - 1.0f) * sc
                                + (2.0f * (t + 0.5f) / out_size - 1.0f) * tc).normalized(); // normal direction
                    vec3 R = N; // specular reflection direction
                    vec3 V = R; // view direction

                    // Reference https://learnopengl.com/PBR/IBL/Specular-IBL for ggx sampling
                    float total_weight = 0.0;
                    vec3 acc = vec3(0.0f);
                    for(uint32_t i=0; i<samples; i++) {
                        vec2 Xi = Hammersley(i, samples);   
                        vec3 H = ImportanceSampleGGX(Xi, N, roughness);
                        vec3 L  = (2.0 * dot(V, H) * H - V).normalized();

                        float NdotL = std::max(dot(N, L), 0.0f);
                        NdotL = std::min(NdotL, 1.0f);
                        if(NdotL > 0.0)
                        {
                            acc += lookup(L) * NdotL;
                            total_weight += NdotL;
                        }
                    }
                    acc = acc / total_weight;

                    out_data.emplace_back(acc);
                }
            }
        }
        std::cout<<"Finished sampling\n";

        // Convert rgb back to rgbe
        std::vector<u8vec4> out_data_rgbe;
        out_data_rgbe.reserve(out_size * 6 * out_size);
        for (auto const &pix : out_data) {
            out_data_rgbe.emplace_back( float_to_rgbe( pix ) );
        }
        
        std::string mip_file_name = file_name+"."+std::to_string(mip)+".png";
        std::cout<<"Save to file: "<<mip_file_name<<"\n";

        
        stbi_write_png(mip_file_name.c_str(), out_size, out_size*6, 4, out_data_rgbe.data(), out_size * 4);
    }
    
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float a = roughness;
    float k = (a * a) / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = std::max(dot(N, V), 0.0f);
    float NdotL = std::max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}  

vec2 IntegrateBRDF(float NdotV, float roughness, uint32_t samples){
    vec3 V;
    V[0] = sqrt(1.0 - NdotV*NdotV);
    V[1] = 0.0;
    V[2] = NdotV;

    float A = 0.0;
    float B = 0.0;

    vec3 N = vec3(0.0, 0.0, 1.0);

    for(uint i = 0u; i < samples; ++i)
    {
        vec2 Xi = Hammersley(i, samples);
        vec3 H  = ImportanceSampleGGX(Xi, N, roughness);
        vec3 L  = (2.0 * dot(V, H) * H - V).normalized();

        float NdotL = std::max(L[2], 0.0f);
        float NdotH = std::max(H[2], 0.0f);
        float VdotH = std::max(dot(V, H), 0.0f);

        if(NdotL > 0.0)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV);

            float Fc = pow(1.0 - VdotH, 5.0);

            A += (1.0 - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(samples);
    B /= float(samples);
    return vec2(A, B);
};

void precomputeBrdfLUT(std::string out_file_path, uint32_t samples, uint32_t out_size=512) {
    std::vector<u8vec4> out_data;
    out_data.reserve(out_size*out_size);
    for (int32_t t = out_size-1; t >=0; --t) {
        float roughness  = (t+0.5f) / out_size;
        for (int32_t s = 0; s < out_size; ++s) {
            float NdotV = (s+0.5f) / out_size;
            vec2 v = IntegrateBRDF(NdotV, roughness, samples);
            std::cout<<v;
            v *= 255;
            out_data.emplace_back(u8vec4(static_cast<uint8_t>(v[0]),static_cast<uint8_t>(v[1]), 0,128));
        }
    }
    
    std::cout<<"Save to file: "<<out_file_path<<"\n";

    
    stbi_write_png(out_file_path.c_str(), out_size, out_size, 4, out_data.data(), out_size * 4);

    
}

void precomputeBrdfLutToBinary(std::string out_file_path, uint32_t samples, uint32_t out_size=512){
    float *out_data = new float[out_size*out_size*2];
    int i = 0;
    for (int32_t t = 0; t < out_size; t++) {
        float roughness  = (t+0.5f) / out_size;
        for (int32_t s = 0; s < out_size; ++s) {
            float NdotV = (s+0.5f) / out_size;
            vec2 v = IntegrateBRDF(NdotV, roughness, samples)*255;
            out_data[i++] = v[0];
            out_data[i++] = v[1];
        }
    }
    std::ofstream zOut(out_file_path, std::ios::out | std::ios::binary);
    zOut.write(reinterpret_cast<char*>(out_data), out_size*out_size*2);
    zOut.close();
}



int main(int argc, char ** argv) {
    if(argc == 4) {
        std::string input = argv[1];
        std::string flag = argv[2];
        std::string output = argv[3];

        if(flag == LAMBERTIAN) {
            const uint32_t OUT_SIZE = 256;
            const uint32_t SAMPLES = 1048576;
            prefilterEnvironmentMapLambertian(input, output, OUT_SIZE, SAMPLES);
        } else if (flag == GGX) {
            const uint32_t SAMPLES = 1048576/4;
            const uint32_t OUT_SIZE = 512;
            prefilterEnvironmentMapPbr(input, output, SAMPLES, OUT_SIZE, 5);
        } else {
            throw std::runtime_error("Invalid flag");
        }
    } else if (argc == 3) {
        std::string flag = argv[2];
        std::string output = argv[1];

        const uint32_t SAMPLES = 4000;
        const uint32_t OUT_SIZE = 512;

        if (flag == LUT) {
            precomputeBrdfLutToBinary(output, SAMPLES, OUT_SIZE);
        } else {
            throw std::runtime_error("Invalid flag");
        }
    }
    else {
        throw std::runtime_error("Require 4 arguments: ./cube <input> --<flag> <output>");
    }


    // int out_size = 128;
    // std::vector<u8vec4> out_data_rgbe;
    // out_data_rgbe.reserve(out_size * 6 * out_size);
    // for(uint32_t f = 0; f < 6; ++f) {
    //     for (uint32_t t = 0; t < out_size; ++t) {
	// 		for (uint32_t s = 0; s < out_size; ++s) {
    //             if(f==1) {
    //                 out_data_rgbe.emplace_back(u8vec4(255,255,255,128));
    //             } else {
    //                 out_data_rgbe.emplace_back(u8vec4(0,0,0,0));
    //             }
                
    //         }
    //     }
    // }
    // std::string out = "../scene/env-cube/test-cube.png";
    // stbi_write_png(out.c_str(), out_size, out_size*6, 4, out_data_rgbe.data(), out_size * 4);

    return 0;
}