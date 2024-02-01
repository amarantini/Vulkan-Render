//
//  scene.h
//  VulkanTesting
//
//  Created by qiru hu on 1/31/24.
//

#ifndef scene_h
#define scene_h

#include <memory>
#include <sys/_types/_size_t.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include "vec.h"
#include "mat.h"
#include "json_parser.h"
#include "math_util.h"
// #include "vertex.hpp"

const std::string SCENE_PATH = "./scene/";

struct Vertex {
    vec3 pos;
    vec3 normal;
    vec<uint8_t,3> color;
};

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
        : name(_name), translation(_translation), rotation(_rotation), scale(_scale), parent(nullptr) {}
};

struct Mesh {
    std::string name;
    std::string topology;
    int count;
    std::vector<Vertex> vertices;
    

    Mesh(std::string _name, std::string _topology, int _count, const std::string src, const int offset)
        : name(_name), topology(_topology), count(_count) {
            loadMesh(src, offset);
          }

    void loadMesh(const std::string src, const int offset){
        std::ifstream infile(SCENE_PATH+src, std::ifstream::binary);

        infile.seekg(0, infile.end);
        const size_t num_elements = infile.tellg() / 28;
        infile.seekg(offset);
        
        vertices.resize(num_elements);
        for(size_t i=0; i<num_elements; i++) {
            Vertex& v = vertices[i];
            infile.read((char*)&v.pos[0], sizeof(float));
            infile.read((char*)&v.pos[1], sizeof(float));
            infile.read((char*)&v.pos[2], sizeof(float));
        
            infile.read((char*)&v.normal[0], sizeof(float));
            infile.read((char*)&v.normal[1], sizeof(float));
            infile.read((char*)&v.normal[2], sizeof(float));
            
            infile.read((char*)&v.color[0], sizeof(uint8_t));
            infile.read((char*)&v.color[1], sizeof(uint8_t));
            infile.read((char*)&v.color[2], sizeof(uint8_t));
        }
    }
};

struct Camera {
    std::shared_ptr<Transform> transform;
    float aspect;
    float vfov;
    float near;
    float far;
    Camera(std::shared_ptr<Transform> _transform, float _aspect, float _vfov, float _near, float _far)
        : transform(_transform), aspect(_aspect), vfov(_vfov), near(_near), far(_far) {}

    mat4 getPerspective(){
        return perspective(vfov, aspect, near, far);
    };
};

struct Driver {
    std::string name;
    std::shared_ptr<Transform> transform;
    std::string channel;
    std::vector<uint32_t> times;
    std::vector<vec3> values;
    std::string interpolation;

    Driver(const std::string& _name, std::shared_ptr<Transform> _transform, const std::string& _channel,
           const std::vector<uint32_t>& _times, const std::vector<vec3>& _values, const std::string& _interpolation)
        : name(_name), transform(_transform), channel(_channel),
          times(_times), values(_values), interpolation(_interpolation) {}
};

struct Scene {
    std::string name;
    std::vector<std::shared_ptr<Transform> > roots;
    std::vector<std::shared_ptr<Transform> > transforms;
    std::vector<std::shared_ptr<Mesh> > meshs;
    std::vector<std::shared_ptr<Driver> > drivers;
    std::unordered_map<std::string, std::shared_ptr<Camera> > cameras;
    

    void load_scene(const JsonList& jsonList){
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
                pos["offset"]->as_num().value()
        );
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
                nullptr,
                perspective["aspect"]->as_num().value(),
                perspective["vfov"]->as_num().value(),
                perspective["near"]->as_num().value(),
                perspective["far"]->as_num().value()
        );
            cameras.insert(std::make_pair(jmap["name"]->as_str().value(),cam_ptr));
            return cam_ptr;
        };

        auto load_driver = [=](JsonObject& jmap){
            const std::vector<double> vec = jmap["values"]->as_array().value();
            std::vector<vec3> values(vec.size());
            for(size_t i=0; i<vec.size(); i+=3){
                values[i] = vec3(vec[i],vec[i+1],vec[i+2]);
            }
            const std::vector<double> time_vec = jmap["times"]->as_array().value();
            std::vector<uint32_t> times(time_vec.size());
            for(size_t i=0; i<time_vec.size(); i++){
                times[i] = static_cast<uint32_t>(time_vec[i]);
            }
            std::shared_ptr<Driver> driver_ptr = std::make_shared<Driver>(
                jmap["name"]->as_str().value(),
                nullptr,
                jmap["channel"]->as_str().value(),
                times,
                values,
                jmap["interpolation"]->as_str().value()
        );
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

#endif /* scene_h */
