//
//  scene.h
//  VulkanTesting
//
//  Created by qiru hu on 1/31/24.
//

#pragma once

#include <cfloat>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

#include "mathlib.h"
#include "json_parser.h"
#include "constants.h"
#include "transform.h"
#include "mesh.h"
#include "camera.h"
#include "driver.h"








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
        // file_path in the form scene/[folder]/scene.s72
        const JsonList jsonList = parseScene(SCENE_PATH+file_path);
        folder_path = file_path.substr(0, file_path.find("/")+1);
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
    std::string folder_path;
    

    JsonList parseScene(const std::string& file_path) {
        JsonParser parser;
        std::string output;
        parser.load(file_path, output);

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
                LoadInfo{
                    folder_path+pos["src"]->as_str().value(),
                    static_cast<int>(pos["offset"]->as_num().value()),
                    static_cast<int>(pos["stride"]->as_num().value())
                },
                LoadInfo{
                    folder_path+normal["src"]->as_str().value(),
                    static_cast<int>(normal["offset"]->as_num().value()),
                    static_cast<int>(normal["stride"]->as_num().value())
                },
                LoadInfo{
                    folder_path+color["src"]->as_str().value(),
                    static_cast<int>(color["offset"]->as_num().value()),
                    static_cast<int>(color["stride"]->as_num().value())
                }
        );
            mesh_ptr->loadMesh();
            meshs.push_back(mesh_ptr);
            return mesh_ptr;
        };

        auto load_transform = [=](JsonObject& jmap){
            std::shared_ptr<Transform> trans_ptr = std::make_shared<Transform>(
                jmap["name"]->as_str().value(),
                vec3(jmap["translation"]->as_array().value()),
                qua(jmap["rotation"]->as_array().value()),
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
                    driver_ptr->values4d.push_back(qua(vec[i],vec[i+1],vec[i+2],vec[i+3]));
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
