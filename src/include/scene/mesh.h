#pragma once

#include <cfloat>
#include <string>
#include <fstream>
#include <map>
#include <iostream>

#include "constants.h"
#include "bbox.h"
#include "vertex.hpp"
#include "material.h"

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
    LoadInfo tex_info;
    LoadInfo tangent_info;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Bbox bbox;
    std::shared_ptr<Material> material;
    bool simple; //if "simple" material, then only load position, normal and color

    Mesh(std::string _name, std::string _topology, int _count, LoadInfo pos_info_, LoadInfo normal_info_, LoadInfo color_info_, LoadInfo tex_info_, LoadInfo tangent_info_)
        : name(_name), topology(_topology), count(_count), pos_info(pos_info_), normal_info(normal_info_), color_info(color_info_), tex_info(tex_info_), tangent_info(tangent_info_) {}

    void loadMesh(){
        if(simple){
            loadMeshSimple();
        } else {
            loadMeshNonSimple();
        }
        if(ENABLE_INDEX_BUFFER)
            calculateIndices();
    }

    void loadMeshSimple(){
        std::ifstream infile(SCENE_PATH+pos_info.src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / pos_info.stride;
        infile.seekg(pos_info.offset);
        
        vertices.resize(num_elements);
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            float f;

            // position
            infile.read((char*)&f, sizeof(float));
            v.pos[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[2] = f;
            bbox.enclose(v.pos);
            //normal
            infile.read((char*)&f, sizeof(float));
            v.normal[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[2] = f;
            
            //color
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

    void loadMeshNonSimple(){
        std::ifstream infile(SCENE_PATH+pos_info.src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / pos_info.stride;
        infile.seekg(pos_info.offset);
        
        vertices.resize(num_elements);
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            float f;

            // position
            infile.read((char*)&f, sizeof(float));
            v.pos[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.pos[2] = f;
            bbox.enclose(v.pos);
            //normal
            infile.read((char*)&f, sizeof(float));
            v.normal[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.normal[2] = f;
            //tangent
            infile.read((char*)&f, sizeof(float));
            v.tangent[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.tangent[1] = f;
            infile.read((char*)&f, sizeof(float));
            v.tangent[2] = f;
            infile.read((char*)&f, sizeof(float));
            v.tangent[3] = f;
            //texcoord
            infile.read((char*)&f, sizeof(float));
            v.texCoord[0] = f;
            infile.read((char*)&f, sizeof(float));
            v.texCoord[1] = f;
            //color
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

    typedef std::pair<Vertex, uint32_t> VPair;
    struct CmpClass
    {
        bool operator() (const Vertex& v1, const Vertex& v2) const
        {
            if (std::fabs(v1.pos[0]-v2.pos[0]) > FLT_EPSILON) return v1.pos[0] < v2.pos[0];
            if (std::fabs(v1.pos[1]-v2.pos[1]) > FLT_EPSILON) return v1.pos[1] < v2.pos[1];
            if (std::fabs(v1.pos[2]-v2.pos[2]) > FLT_EPSILON) return v1.pos[2] < v2.pos[2];
            return false;
        }
    };
    
    void calculateIndices() {
        std::map<Vertex, uint32_t, CmpClass> vertex_to_idx;
        uint32_t idx = 0;
        for(auto& v: vertices) {
            auto itr = vertex_to_idx.find(v);
            if(itr==vertex_to_idx.end()){
                vertex_to_idx[v] = idx;
                indices.push_back(idx++);
            } else {
                indices.push_back(itr->second);
            }
        }
        
        // std::vector<Vertex> new_vertices(vertex_to_idx.size());
        vertices.resize(vertex_to_idx.size());
        for(auto& [vertex, index]: vertex_to_idx){
            vertices[index] = vertex;
        }
        // vertices = new_vertices;

    }
};