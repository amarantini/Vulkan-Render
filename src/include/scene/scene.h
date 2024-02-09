//
//  scene.h
//  VulkanTesting
//
//  Created by qiru hu on 1/31/24.
//

#pragma once

#include <__config>
#include <cfloat>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <sys/_types/_size_t.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include "bbox.h"
#include "math_util.h"
#include "mathlib.h"
#include "json_parser.h"
#include "vertex.hpp"
#include "constants.h"



struct Mesh;
struct Transform;

struct Transform {
    std::string name;
    vec3 translation;
    vec4 rotation;
    vec3 scale;
    std::shared_ptr<Transform> parent;
    std::vector<std::shared_ptr<Mesh> > meshes;
    std::vector<std::shared_ptr<Transform> > children;

    Transform(const std::string _name, const vec3 _translation, const vec4 _rotation, const vec3 _scale)
        : name(_name), translation(_translation), rotation(_rotation), scale(_scale) {}

    mat4 localToParent() const {
        return translationMat(translation) * rotationMat(rotation) * scaleMat(scale);
    }

    mat4 parentToLocal() const {
        return scaleMat(1.0f / scale) * rotationMat(quaInv(rotation)) * translationMat(-translation);
    }

    mat4 localToWorld() const {
        if(parent!=nullptr){
            return parent->localToWorld() * localToParent();
        }
        return localToParent();
    }

    mat4 worldToLocal() const {
        if(parent!=nullptr){
            return parentToLocal() * parent->worldToLocal();
        }
        return parentToLocal();
    }
};

struct Mesh {
    std::string name;
    std::string topology; //Assume TRIANGLE_LIST
    int count;
    std::string src;
    int offset;
    int stride;
    std::vector<Vertex> vertices;
    Bbox bbox;

    Mesh(std::string _name, std::string _topology, int _count, std::string _src, int _offset, int _stride)
        : name(_name), topology(_topology), count(_count), src(_src), offset(_offset), stride(_stride) {
          }

    void loadMesh(){
        std::ifstream infile(SCENE_PATH+src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / stride;
        infile.seekg(offset);
        
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
    }
};



struct Camera {
    float aspect;
    float vfov;
    float near;
    float far;
    std::shared_ptr<Transform> transform; // equivalent to view matrix
    bool movable = false;
    bool debug = false;
    vec3 euler;

    Camera(float _aspect, float _vfov, float _near, float _far)
        : aspect(_aspect), vfov(_vfov), near(_near), far(_far) {}

    mat4 getPerspective(float aspect_){
        return perspective(vfov, aspect_, near, far);
    };

    mat4 getPerspective(){
        return perspective(vfov, aspect, near, far);
    };

    mat4 getView() {
        if(transform == nullptr)
            return mat4::I;
        return transform->worldToLocal();
    }

    float pitch()
	{
        vec4 q = transform->rotation;
		
		float const y = 2.0f * (q[1] * q[2] + q[3] * q[0]);
		float const x = q[3] * q[3] - q[0] * q[0] - q[1] * q[1] + q[2] * q[2];

		if(std::abs(x)<FLT_EPSILON && std::abs(y)<FLT_EPSILON)
			return 2.0f * std::atan2(q[0], q[3]);

		return std::atan2(y, x);
	}

    float yaw()
	{
        vec4 q = transform->rotation;
		return std::asin(std::clamp(-2.0f * (q[0] * q[2] - q[3] * q[1]), -1.0f, 1.0f));
	}

    float roll(){
        vec4 q = transform->rotation;
        float x = q[3] * q[3] + q[0] * q[0] - q[2] * q[2] - q[2] * q[2];
        float y = 2.0f * (q[0] * q[1] + q[3] * q[2]);

		if(std::abs(x)<FLT_EPSILON && std::abs(y)<FLT_EPSILON)
			return 0;

		return std::atan2(y, x);
    }

    vec3 getEuler() {
        return vec3(pitch(), yaw(), roll());
    }

};

struct Driver {
    std::string name;
    std::string channel;
    std::vector<double> times;
    std::vector<vec3> values3d;
    std::vector<vec4> values4d;
    std::string interpolation;
    std::shared_ptr<Transform> transform;
    size_t frame_idx = 0;
    bool loop = false;
    bool finished = false;
    float frameTime = 0.0f;

    Driver(const std::string& _name, const std::string& _channel, const std::vector<double> _times, 
           const std::string& _interpolation)
        : name(_name), channel(_channel), times(_times), interpolation(_interpolation) {}

    void restart(){
        finished = false;
        frameTime = 0.0f;
        frame_idx = 0;
    }

    bool isFinished(const float t){
        frameTime += t;
        if(frameTime > times.back()){
            if(!loop) {
                finished = true;
                return true;
            } 
            else {
                frameTime -= times.back();
                frame_idx = 0;
            }
        }
        while(times[frame_idx+1]<t){
            frame_idx++;
        }
        return false;
    }

    void linearInterp(){
        float t = (frameTime-times[frame_idx])/(times[frame_idx+1]-times[frame_idx]);
        if(channel==CHANEL_SCALE){
            transform->scale = lerp(values3d[frame_idx], values3d[frame_idx+1], t);
        } else if (channel==CHANEL_TRANSLATION){
            transform->translation = lerp(values3d[frame_idx], values3d[frame_idx+1], t);
        } else if (channel==CHANEL_ROTATION){
            transform->rotation = quaLerp(values4d[frame_idx], values4d[frame_idx+1], t);
        }
    }

    void animate(const float deltaTime){
        if(finished)
            return;
        if(isFinished(deltaTime)){
            std::cout<<name<<" animation finished\n";
            return;
        } else {
            if(interpolation==INTERP_LINEAR){
                linearInterp();
            } // else TODO
        }
    }
};

struct ModelInfo {
    std::shared_ptr<Transform> transform;
    std::shared_ptr<Mesh> mesh;

    ModelInfo(std::shared_ptr<Transform> transform_, std::shared_ptr<Mesh> mesh_):
    transform(transform_), mesh(mesh_) {}

    mat4 model(){
        return transform->localToWorld();
    }
};

class Scene {
public:
    std::vector<std::shared_ptr<ModelInfo>> modelInfos;
    std::vector<std::shared_ptr<Driver> > drivers;

    void init(const std::string& file_path) {
        const JsonList jsonList = parseScene(file_path);
        loadScene(jsonList);
    }

    std::unordered_map<std::string, std::shared_ptr<Camera> > getAllCameras(){
        return cameras;
    }

    const std::vector<std::shared_ptr<ModelInfo>>& getModelInfos() const {
        return modelInfos;
    }

private:
    std::string name;
    std::vector<std::shared_ptr<Transform> > roots;
    std::vector<std::shared_ptr<Transform> > transforms;
    std::vector<std::shared_ptr<Mesh> > meshs;
    std::unordered_map<std::string, std::shared_ptr<Camera> > cameras;
    
    

    JsonList parseScene(const std::string& file_path) {
        JsonParser parser;
        std::string output;
        parser.load("./scene/sg-Articulation.s72", output);

        return parser.parse(output);
    }

    void loadScene(const JsonList& jsonList){
        std::unordered_map<uint32_t,std::shared_ptr<Mesh> > idx_to_mesh;
        std::unordered_map<uint32_t,std::shared_ptr<Transform> > idx_to_trans;
        std::unordered_map<uint32_t,std::shared_ptr<Camera> > idx_to_cam;
        std::unordered_map<uint32_t,std::shared_ptr<Driver> > idx_to_dri;

        enum Type {Me, Trans, Cam, Dri};

        struct Reference {
            size_t from;
            Type fromType;
            size_t to;
            Type toType;

            Reference(size_t _from, Type _fromType, size_t _to, Type _toType):
            from(_from), fromType(_fromType), to(_to), toType(_toType) {}
        };

        std::vector<Reference> references;

        auto load_mesh = [=](JsonObject& jmap) {
            JsonObject attr = jmap["attributes"]->as_obj().value();
            JsonObject pos = attr["POSITION"]->as_obj().value();
            JsonObject normal = attr["NORMAL"]->as_obj().value();
            JsonObject color = attr["COLOR"]->as_obj().value();
            std::shared_ptr<Mesh> mesh_ptr = std::make_shared<Mesh>(
                jmap["name"]->as_str().value(),
                jmap["topology"]->as_str().value(),
                jmap["count"]->as_num().value(),
                pos["src"]->as_str().value(),
                pos["offset"]->as_num().value(),
                pos["stride"]->as_num().value()
        );
            mesh_ptr->loadMesh();
            meshs.push_back(mesh_ptr);
            return mesh_ptr;
        };

        auto load_transform = [=](JsonObject& jmap){
            std::shared_ptr<Transform> trans_ptr = std::make_shared<Transform>(
                jmap["name"]->as_str().value(),
                vec3(jmap["translation"]->as_array().value()),
                vec4(jmap["rotation"]->as_array().value()),
                vec3(jmap["scale"]->as_array().value())
            );
            transforms.push_back(trans_ptr);
            return trans_ptr;
        };

        auto load_camera = [=](JsonObject& jmap){
            JsonObject perspective = jmap["perspective"]->as_obj().value();
            std::shared_ptr<Camera> cam_ptr = std::make_shared<Camera>(
                perspective["aspect"]->as_num().value(),
                perspective["vfov"]->as_num().value(),
                perspective["near"]->as_num().value(),
                perspective["far"]->as_num().value()
        );
            cameras.insert(std::make_pair(jmap["name"]->as_str().value(),cam_ptr));
            return cam_ptr;
        };

        auto load_driver = [=](JsonObject& jmap){
            
            std::shared_ptr<Driver> driver_ptr = std::make_shared<Driver>(
                jmap["name"]->as_str().value(),
                jmap["channel"]->as_str().value(),
                jmap["times"]->as_array().value(),
                jmap["interpolation"]->as_str().value()
            );  
            const std::vector<double> vec = jmap["values"]->as_array().value();
            if(driver_ptr->channel==CHANEL_ROTATION){
                for(size_t i=0; i<vec.size(); i+=4){
                    driver_ptr->values4d.push_back(vec4(vec[i],vec[i+1],vec[i+2],vec[i+3]));
                }
            } else {
                for(size_t i=0; i<vec.size(); i+=3){
                    driver_ptr->values3d.push_back(vec3(vec[i],vec[i+1],vec[i+2]));
                }
            }
            
            drivers.push_back(driver_ptr);
            return driver_ptr;
        };

        
        std::vector<int> root_idxes;

        for(size_t i=1; i<jsonList.size(); i++){
            const auto& jsonPtr = jsonList[i];
            JsonObject jmap = jsonPtr->as_obj().value();
            std::string type = jmap["type"]->as_str().value();
            if(type == "MESH") {
                std::shared_ptr<Mesh> mesh_ptr = load_mesh(jmap);
                idx_to_mesh[i] = mesh_ptr;
            } else if (type == "NODE") {
                std::shared_ptr<Transform> trans_ptr = load_transform(jmap);
                if(jmap.count("mesh")){
                    int mesh_id=jmap["mesh"]->as_num().value();
                    references.push_back(Reference(mesh_id, Type::Me, i, Type::Trans));
                }
                if(jmap.count("camera")){
                    int cam_id=jmap["camera"]->as_num().value();
                    references.push_back(Reference(cam_id, Type::Cam, i, Type::Trans));
                }
                if(jmap.count("children")) {
                    std::vector<double> children = jmap["children"]->as_array().value();
                    for(auto n: children){
                        references.emplace_back(i, Type::Trans, static_cast<int>(n), Type::Trans);
                    }
                }
                idx_to_trans[i] = trans_ptr;
            } else if (type == "CAMERA") {
                std::shared_ptr<Camera> cam_ptr = load_camera(jmap);
                idx_to_cam[i] = cam_ptr;
            } else if (type == "DRIVER") {
                std::shared_ptr<Driver> driver_ptr = load_driver(jmap);
                idx_to_dri[i] = driver_ptr;
                int tran_id=jmap["node"]->as_num().value();
                references.push_back(Reference(i, Type::Dri, tran_id, Type::Trans));
            } else if (type == "SCENE") {
                name = jmap["name"]->as_str().value();
                std::vector<double> vec = jmap["roots"]->as_array().value();
                for(size_t i=0; i<vec.size(); i++){
                    root_idxes.push_back(static_cast<int>(vec[i]));
                }
            }
        }

        for(Reference& r: references){
            if(r.fromType == Type::Dri) {
                idx_to_dri[r.from]->transform = idx_to_trans[r.to];
            } else if (r.fromType == Type::Cam) {
                idx_to_cam[r.from]->transform = idx_to_trans[r.to];
            } else if (r.fromType == Type::Me) {
                idx_to_trans[r.to]->meshes.push_back(idx_to_mesh[r.from]);
                std::shared_ptr<ModelInfo> info = std::make_shared<ModelInfo>(idx_to_trans[r.to], 
                                idx_to_mesh[r.from]);
                modelInfos.push_back(info);
            } else if (r.fromType == Type::Trans) {
                idx_to_trans[r.from]->children.push_back(idx_to_trans[r.to]);
                idx_to_trans[r.to]->parent = idx_to_trans[r.from];
            }
        }

        for(int n: root_idxes){
            roots.push_back(idx_to_trans[n]);
        }
    }
};

 /* scene_h */
