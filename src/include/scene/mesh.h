#pragma once

#include <string>
#include <fstream>

#include "constants.h"
#include "bbox.h"
#include "vertex.hpp"

struct LoadInfo{
    std::string src;
    int offset;
    int stride;
};

struct Mesh {
    std::string name;
    std::string topology; //Assume TRIANGLE_LIST
    int count;
    LoadInfo pos_info;
    LoadInfo normal_info;
    LoadInfo color_info;
    std::vector<Vertex> vertices;
    Bbox bbox;
    bool single_file = true;

    Mesh(std::string _name, std::string _topology, int _count, LoadInfo pos_info_, LoadInfo normal_info_, LoadInfo color_info_)
        : name(_name), topology(_topology), count(_count), pos_info(pos_info_), normal_info(normal_info_), color_info(color_info_) {}

    void loadMesh(){
        if(single_file){
            loadMeshSingleFile();
        } else {
            loadMeshPosition();
            loadMeshNormal();
            loadMeshColor();
        }
        
    }

    void loadMeshPosition(){
        std::ifstream infile(SCENE_PATH+pos_info.src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / pos_info.stride;
        infile.seekg(pos_info.offset);
        
        vertices.resize(num_elements);
        int buffer_size = pos_info.stride - sizeof(float)*3;
        char* buffer[buffer_size];
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            float f;
            infile.read((char*)&f, sizeof(float));
            v.pos[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[2] = f;
            bbox.enclose(v.pos);
            
            infile.read((char*)buffer, buffer_size);
        }

        infile.close();
    }

    void loadMeshNormal(){
        std::ifstream infile(SCENE_PATH+normal_info.src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / normal_info.stride;
        infile.seekg(normal_info.offset);
        
        int buffer_size = normal_info.stride - sizeof(float)*3;
        char* buffer[buffer_size];
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            float f;
            infile.read((char*)&f, sizeof(float));
            v.normal[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[2] = f;
            
            infile.read((char*)buffer, buffer_size);
        }

        infile.close();
    }

    void loadMeshColor(){
        std::ifstream infile(SCENE_PATH+color_info.src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / color_info.stride;
        infile.seekg(color_info.offset);
        
        int buffer_size = normal_info.stride - sizeof(uint8_t)*4;
        char* buffer[buffer_size];
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            uint8_t color;
            infile.read((char*)&color, sizeof(uint8_t));
            v.color[0] = static_cast<float>(color) / 255.0f;
            infile.read((char*)&color, sizeof(uint8_t));
            v.color[1] = static_cast<float>(color) / 255.0f;
            infile.read((char*)&color, sizeof(uint8_t));
            v.color[2] = static_cast<float>(color) / 255.0f;
            infile.read((char*)&color, sizeof(uint8_t));
            // v.color[3] = static_cast<float>(color);
            
            infile.read((char*)buffer, buffer_size);
        }

        infile.close();
    }

    void loadMeshSingleFile(){
        std::ifstream infile(SCENE_PATH+pos_info.src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / pos_info.stride;
        infile.seekg(pos_info.offset);
        
        vertices.resize(num_elements);
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            float f;
            infile.read((char*)&f, sizeof(float));
            v.pos[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[2] = f;
            bbox.enclose(v.pos);

            infile.read((char*)&f, sizeof(float));
            v.normal[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[2] = f;
            
            uint8_t color;
            infile.read((char*)&color, sizeof(uint8_t));
            v.color[0] = static_cast<float>(color) / 255.0f;
            infile.read((char*)&color, sizeof(uint8_t));
            v.color[1] = static_cast<float>(color) / 255.0f;
            infile.read((char*)&color, sizeof(uint8_t));
            v.color[2] = static_cast<float>(color) / 255.0f;
            infile.read((char*)&color, sizeof(uint8_t));
            // v.color[3] = static_cast<float>(color);
        }

        infile.close();
    }
};