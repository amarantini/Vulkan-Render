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
#include "material.h"
#include "light.h"

// s72 Json name
static const std::string S72_NAME = "name";
static const std::string S72_TYPE = "type";
static const std::string S72_NODE_TYPE = "NODE";
static const std::string S72_SCENE_TYPE = "SCENE";
static const std::string S72_MESH_TYPE = "MESH";
static const std::string S72_CAMERA_TYPE = "camera";
static const std::string S72_DRIVER_TYPE = "DRIVER";
static const std::string S72_MATERIAL_TYPE = "MATERIAL";
static const std::string S72_ENVIRONMENT_TYPE = "ENVIRONMENT";
static const std::string S72_LIGHT_TYPE = "LIGHT";

// Mesh
static const std::string S72_TOPOLOGY = "topology";
static const std::string S72_COUNT = "count";
static const std::string S72_ATTRIBUTES = "attributes";
static const std::string S72_POSITION = "POSITION";
static const std::string S72_NORMAL = "NORMAL";
static const std::string S72_TANGENT = "TANGENT";
static const std::string S72_TEXCOORD = "TEXCOORD";
static const std::string S72_COLOR = "COLOR";
static const std::string S72_SRC = "src";
static const std::string S72_OFFSET = "offset";
static const std::string S72_STRIDE = "stride";
static const std::string S72_FORMAT = "format";
static const std::string S72_MATERIAL = "material";

// Transform
static const std::string S72_TRANSLATION = "translation";
static const std::string S72_ROTATION = "rotation";
static const std::string S72_SCALE = "scale";
static const std::string S72_CHILDREN = "children";
static const std::string S72_CAMERA = "camera";
static const std::string S72_MESH = "mesh";
static const std::string S72_ENVIRONMENT = "environment";
static const std::string S72_LIGHT = "light";

// Camera
static const std::string S72_PERSPECTIVE = "perspective";
static const std::string S72_ASPECT = "aspect";
static const std::string S72_VFOV = "vfov";
static const std::string S72_NEAR = "near";
static const std::string S72_FAR = "far";

// Driver
static const std::string S72_NODE = "node";
static const std::string S72_CHANNEL = "channel";
static const std::string S72_TIMES = "times";
static const std::string S72_VALUES = "values";
static const std::string S72_INTERPOLATION = "interpolation";

// Material
static const std::string S72_NORMAL_MAP = "normalMap";
static const std::string S72_DISPLACEMENT_MAP = "displacementMap";
static const std::string S72_PBR = "pbr";
static const std::string S72_ALBEDO = "albedo";
static const std::string S72_ROUGHNESS = "roughness";
static const std::string S72_METALNESS = "metalness";
static const std::string S72_LAMBERTIAN = "lambertian";
static const std::string S72_MIRROR = "mirror";
static const std::string S72_SIMPLE = "simple";

// Environemnt
static const std::string S72_RADIANCE = "radiance";

// Light
static const std::string S72_POINT_LIGHT = "sphere";
static const std::string S72_RADIUS = "radius";
static const std::string S72_DIRECTIONAL_LIGHT = "sun";
static const std::string S72_STRENGTH = "strength";
static const std::string S72_SOLID_ANGLE = "angle";
static const std::string S72_LIMIT = "limit";
static const std::string S72_SPOT_LIGHT = "spot";
static const std::string S72_TINT = "tint";
static const std::string S72_POWER = "power";
static const std::string S72_FOV = "fov";
static const std::string S72_BLEND = "blend";

static const std::string S72_SHADOW = "shadow";

struct ModelInfo {
    std::shared_ptr<Transform> transform;
    std::shared_ptr<Mesh> mesh;

    ModelInfo(std::shared_ptr<Transform> transform_, std::shared_ptr<Mesh> mesh_):
    transform(transform_), mesh(mesh_) {}
};

struct ModelInfoList {
    std::vector<std::shared_ptr<ModelInfo> > simple_models;
    std::vector<std::shared_ptr<ModelInfo> > env_models;
    std::vector<std::shared_ptr<ModelInfo> > mirror_models;
    std::vector<std::shared_ptr<ModelInfo> > pbr_models;
    std::vector<std::shared_ptr<ModelInfo> > lamber_models;
};

struct EnvironmentLightingInfo {
    Texture texture;
    std::shared_ptr<Transform> transform;
    bool exist = false;
};

struct LightInfoList {
    std::vector<std::shared_ptr<Light> > sphere_light_infos;
    std::vector<std::shared_ptr<Light> > spot_light_infos;
    std::vector<std::shared_ptr<Light> > directional_light_infos;
    std::vector<SphereLight> sphere_lights;
    std::vector<SpotLight> spot_lights;
    std::vector<DirectionalLight> directional_lights;

    bool shadow_mapping = false;

    std::vector<SphereLight> putSphereLights() {
        for(size_t i=0; i<sphere_light_infos.size(); i++) {
            sphere_lights.push_back(sphere_light_infos[i]->sphereLight());
        }
        return sphere_lights;
    }

    std::vector<SpotLight> putSpotLights(){
        for(size_t i=0; i<spot_light_infos.size(); i++) {
            spot_lights.push_back(spot_light_infos[i]->spotLight());
            if(spot_light_infos[i]->shadow_res>0){
                shadow_mapping = true;
                spot_lights.back().shadow[0] = spot_light_infos[i]->shadow_res;
            }
        }
        return spot_lights;
    }

    std::vector<DirectionalLight> putDirectionalLights(){
        for(size_t i=0; i<directional_light_infos.size(); i++) {
            directional_lights.push_back(directional_light_infos[i]->directionalLight());
        }
        return directional_lights;
    }

    void init() {
        putSphereLights();
        putSpotLights();
        putDirectionalLights();
    }

    void update() {
        for(size_t i=0; i<sphere_light_infos.size(); i++) {
            mat4 m = sphere_light_infos[i]->transform->localToWorld();
            sphere_lights[i].pos = m * vec4(0,0,0,1);
        }
        for(size_t i=0; i<spot_light_infos.size(); i++) {
            mat4 m = spot_light_infos[i]->transform->localToWorld();
            spot_lights[i].pos = m * vec4(0,0,0,1);
            spot_lights[i].direction = m * vec4(0,0,-1,0);
        }
        for(size_t i=0; i<directional_light_infos.size(); i++) {
            mat4 m = directional_light_infos[i]->transform->localToWorld();
            directional_lights[i].direction = m * vec4(0,0,-1,0);
        }
    }
};

class Scene {
public:

    void init(const std::string& file_path) {
        // file_path in the form scene/[folder]/scene.s72
        const JsonList jsonList = parseScene(SCENE_PATH+file_path);
        folder_path = file_path.substr(0, file_path.find("/")+1);
        loadScene(jsonList);
    }

    std::unordered_map<std::string, std::shared_ptr<Camera> > getAllCameras(){
        return cameras;
    }

    const ModelInfoList getModelInfos() const {
        ModelInfoList model_info_list;

        for(auto info: modelInfos){
            if(info->mesh->material->type == Material::Type::SIMPLE) {
                model_info_list.simple_models.push_back(info);
            } else if (info->mesh->material->type == Material::Type::PBR) {
                model_info_list.pbr_models.push_back(info);
            } else if (info->mesh->material->type == Material::Type::LAMBERTIAN) {
                model_info_list.lamber_models.push_back(info);
            } else if (info->mesh->material->type == Material::Type::MIRROR) {
                model_info_list.mirror_models.push_back(info);
            } else if (info->mesh->material->type == Material::Type::ENVIRONMENT) {
                model_info_list.env_models.push_back(info);
            }
        }

        std::cout<<"simple_models: "<<model_info_list.simple_models.size()<<"\n";
        std::cout<<"env_models: "<<model_info_list.env_models.size()<<"\n";
        std::cout<<"pbr_models: "<<model_info_list.pbr_models.size()<<"\n";
        std::cout<<"mirror_models: "<<model_info_list.mirror_models.size()<<"\n";
        std::cout<<"lamber_models: "<<model_info_list.lamber_models.size()<<"\n";

        return model_info_list;
    }

    const std::vector<std::shared_ptr<Driver> > getDrivers() const {
        return drivers;
    }

    const EnvironmentLightingInfo getEnvironment() const {
        return environment;
    }

    const LightInfoList getLightInfos() const {
        LightInfoList info_list;
        for(auto l: lights) {
            if(l->type == Light::Type::POINT) {
                info_list.sphere_light_infos.push_back(l);
            } else if(l->type == Light::Type::SPOT) {
                info_list.spot_light_infos.push_back(l);
            } else if (l->type == Light::Type::DIRECTIONAL) {
                info_list.directional_light_infos.push_back(l);
            }
        }
        if(info_list.sphere_light_infos.size()>MAX_LIGHT_COUNT || info_list.spot_light_infos.size()>MAX_LIGHT_COUNT || info_list.directional_light_infos.size()>MAX_LIGHT_COUNT){
            throw std::runtime_error("Number of lights exceed maximum light limit!");
        }
        std::cout<<"Sphere light count: "<<info_list.sphere_light_infos.size()<<"\n";
        std::cout<<"Spot light count: "<<info_list.spot_light_infos.size()<<"\n";
        std::cout<<"Directional light count: "<<info_list.directional_light_infos.size()<<"\n";
        info_list.init();
        return info_list;
    }

private:
    std::string name;
    std::vector<std::shared_ptr<ModelInfo>> modelInfos;
    std::vector<std::shared_ptr<Driver> > drivers;
    std::vector<std::shared_ptr<Transform> > roots;
    std::vector<std::shared_ptr<Transform> > transforms;
    std::vector<std::shared_ptr<Mesh> > meshs;
    std::unordered_map<std::string, std::shared_ptr<Camera> > cameras;
    std::vector<std::shared_ptr<Material> > materials;
    std::vector<std::shared_ptr<Light> > lights;
    std::string folder_path;
    EnvironmentLightingInfo environment;
    

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
        std::unordered_map<uint32_t,std::shared_ptr<Material> > idx_to_material;
        std::unordered_map<uint32_t,std::shared_ptr<Light> > idx_to_light;

        std::shared_ptr<Material> simple_material = std::make_shared<Material>();
        simple_material->type = Material::Type::SIMPLE;

        std::shared_ptr<Transform> default_transform = std::make_shared<Transform>(
               "Default environment transform",
                vec3(0,0,0),
                qua(0,0,0,1),
                vec3(1,1,1));

        environment.transform = default_transform;

        enum Type {Me, Trans, Cam, Dri, Mat, Li};

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
            JsonObject attr = jmap[S72_ATTRIBUTES]->as_obj().value();
            JsonObject pos = attr[S72_POSITION]->as_obj().value();
            JsonObject normal = attr[S72_NORMAL]->as_obj().value();
            JsonObject color = attr[S72_COLOR]->as_obj().value();
            JsonObject tan;
            JsonObject tex;
            bool simple = true;
            if(attr.count(S72_TANGENT)) {
                tan = attr[S72_TANGENT]->as_obj().value();
                simple = false;
            } else {
                tan = color;
            }
            if(attr.count(S72_TEXCOORD)) {
                tex = attr[S72_TEXCOORD]->as_obj().value();
            } else {
                tex = color;
            }
            std::shared_ptr<Mesh> mesh_ptr = std::make_shared<Mesh>(
                jmap[S72_NAME]->as_str().value(),
                jmap[S72_TOPOLOGY]->as_str().value(),
                jmap[S72_COUNT]->as_num().value(),
                LoadInfo{
                    folder_path+pos[S72_SRC]->as_str().value(),
                    static_cast<int>(pos[S72_OFFSET]->as_num().value()),
                    static_cast<int>(pos[S72_STRIDE]->as_num().value())
                },
                LoadInfo{
                    folder_path+normal[S72_SRC]->as_str().value(),
                    static_cast<int>(normal[S72_OFFSET]->as_num().value()),
                    static_cast<int>(normal[S72_STRIDE]->as_num().value())
                },
                LoadInfo{
                    folder_path+color[S72_SRC]->as_str().value(),
                    static_cast<int>(color[S72_OFFSET]->as_num().value()),
                    static_cast<int>(color[S72_STRIDE]->as_num().value())
                },
                LoadInfo{
                    folder_path+tan[S72_SRC]->as_str().value(),
                    static_cast<int>(tan[S72_OFFSET]->as_num().value()),
                    static_cast<int>(tan[S72_STRIDE]->as_num().value())
                },
                LoadInfo{
                    folder_path+tex[S72_SRC]->as_str().value(),
                    static_cast<int>(tex[S72_OFFSET]->as_num().value()),
                    static_cast<int>(tex[S72_STRIDE]->as_num().value())
                },
                simple
        );
            mesh_ptr->loadMesh();
            meshs.push_back(mesh_ptr);
            return mesh_ptr;
        };

        auto load_transform = [=](JsonObject& jmap){
            vec3 translation = vec3();
            qua rotation = qua(0,0,0,1);
            vec3 scale = vec3(1,1,1);
            if(jmap.count(S72_TRANSLATION)) {
                translation =  vec3(jmap[S72_TRANSLATION]->as_array().value());
            } 
            if(jmap.count(S72_ROTATION)) {
                rotation =  qua(jmap[S72_ROTATION]->as_array().value());
            } 
            if(jmap.count(S72_SCALE)) {
                scale =  vec3(jmap[S72_SCALE]->as_array().value());
            } 
            std::shared_ptr<Transform> trans_ptr = std::make_shared<Transform>(
                jmap[S72_NAME]->as_str().value(),
                translation,
                rotation,
                scale
            );
            transforms.push_back(trans_ptr);
            return trans_ptr;
        };

        auto load_camera = [=](JsonObject& jmap){
            JsonObject perspective = jmap[S72_PERSPECTIVE]->as_obj().value();
            std::shared_ptr<Camera> cam_ptr = std::make_shared<Camera>(
                perspective[S72_ASPECT]->as_num().value(),
                perspective[S72_VFOV]->as_num().value(),
                perspective[S72_NEAR]->as_num().value(),
                perspective[S72_FAR]->as_num().value()
        );
            cameras.insert(std::make_pair(jmap[S72_NAME]->as_str().value(),cam_ptr));
            return cam_ptr;
        };

        auto load_driver = [=](JsonObject& jmap){
            
            std::shared_ptr<Driver> driver_ptr = std::make_shared<Driver>(
                jmap[S72_NAME]->as_str().value(),
                jmap[S72_CHANNEL]->as_str().value(),
                jmap[S72_TIMES]->as_array().value(),
                jmap[S72_INTERPOLATION]->as_str().value()
            );  
            const std::vector<double> vec = jmap[S72_VALUES]->as_array().value();
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

        auto load_texture = [=](JsonObject jmap){
            Texture t = {};
            t.src = jmap[S72_SRC]->as_str().value(); // must give full texture path in s72
            if(jmap.count(S72_TYPE)){
                if(jmap[S72_TYPE]->as_str().value()=="cube"){
                    t.type = Texture::Type::CUBE;
                } else {
                    t.type = Texture::Type::TWO_D;
                }
            }
            if(jmap.count(S72_FORMAT)){
                if(jmap[S72_FORMAT]->as_str().value()=="rgbe"){
                    t.format = Texture::Format::RGBE;
                } else {
                    t.format = Texture::Format::LINEAR;
                }
            }
            return t;
        };

        auto load_material = [=](JsonObject& jmap) {
            std::shared_ptr<Material> mat_ptr;
            if(jmap.count(S72_PBR)){
                Pbr m = {};
                JsonObject pbr = jmap[S72_PBR]->as_obj().value();
                auto albedo = pbr[S72_ALBEDO]->as_array();
                if(albedo.has_value()){
                    m.albedo = vec3(albedo.value());
                } else {
                    m.albedo_texture = load_texture(pbr[S72_ALBEDO]->as_obj().value());
                }
                auto roughness = pbr[S72_ROUGHNESS]->as_num();
                if(roughness.has_value()){
                    m.roughness = roughness.value();
                } else {
                    m.roughness_texture = load_texture(pbr[S72_ROUGHNESS]->as_obj().value());
                }
                auto metalness = pbr[S72_METALNESS]->as_num();
                if(metalness.has_value()){
                    m.metalness = metalness.value();
                } else {
                    m.metalness_texture = load_texture(pbr[S72_METALNESS]->as_obj().value());
                }

                mat_ptr = std::make_shared<Material>(m, Material::Type::PBR);
            } else if (jmap.count(S72_LAMBERTIAN)) {
                Lambertian m = {};
                JsonObject lambertian = jmap[S72_LAMBERTIAN]->as_obj().value();
                auto albedo = lambertian[S72_ALBEDO]->as_array();
                if(albedo.has_value()){
                    m.albedo = vec3(albedo.value());
                } else {
                    m.albedo_texture = load_texture(lambertian[S72_ALBEDO]->as_obj().value());
                }

                mat_ptr = std::make_shared<Material>(m, Material::Type::LAMBERTIAN);
            } else if (jmap.count(S72_MIRROR)) {
                mat_ptr = std::make_shared<Material>(Mirror{}, Material::Type::MIRROR);
            } else if (jmap.count(S72_ENVIRONMENT)) {
                mat_ptr = std::make_shared<Material>(Environment{}, Material::Type::ENVIRONMENT);
            } else if (jmap.count(S72_SIMPLE)) {
                mat_ptr = std::make_shared<Material>(Simple{}, Material::Type::SIMPLE);
            }
            
            mat_ptr->name = jmap[S72_NAME]->as_str().value();
            if(jmap.count(S72_NORMAL_MAP)){
                JsonObject normalMap = jmap[S72_NORMAL_MAP]->as_obj().value();
                mat_ptr->normal_map = load_texture(normalMap);
            }
            if(jmap.count(S72_DISPLACEMENT_MAP)){
                JsonObject displacementMap = jmap[S72_DISPLACEMENT_MAP]->as_obj().value();
                mat_ptr->displacement_map = load_texture(displacementMap);
            }
            return mat_ptr;
        };

        auto load_light = [=](JsonObject& jmap) {
            std::shared_ptr<Light> light_ptr;
            //TODO
            vec4 tint = vec4(1);
            if(jmap.count(S72_TINT) != 0) {
                tint = vec4(vec3(jmap[S72_TINT]->as_array().value()),0);
            }
            if(jmap.count(S72_POINT_LIGHT)){
                SphereLight l = {};
                JsonObject sphere_light = jmap[S72_POINT_LIGHT]->as_obj().value();
                float radius = sphere_light[S72_RADIUS]->as_num().value();
                l.others = vec4(radius,-1,0,0);
                l.color = tint * sphere_light[S72_POWER]->as_num().value();
                if(sphere_light.count(S72_LIMIT) != 0) {
                    l.others[1] = sphere_light[S72_LIMIT]->as_num().value();
                }
                light_ptr = std::make_shared<Light>(l, Light::Type::POINT);
            } else if (jmap.count(S72_DIRECTIONAL_LIGHT)) {
                DirectionalLight l = {};
                JsonObject dir_light = jmap[S72_DIRECTIONAL_LIGHT]->as_obj().value();
                l.others = vec4(dir_light[S72_SOLID_ANGLE]->as_num().value(),0,0,0);
                l.color = tint * dir_light[S72_STRENGTH]->as_num().value();
                light_ptr = std::make_shared<Light>(l, Light::Type::DIRECTIONAL);
            } else if (jmap.count(S72_SPOT_LIGHT)) {
                SpotLight l = {};
                JsonObject spot_light = jmap[S72_SPOT_LIGHT]->as_obj().value();
                float fov = spot_light[S72_FOV]->as_num().value();
                float blend = spot_light[S72_BLEND]->as_num().value();
                float outter = fov / 2.0f;
                float inner = fov * (1.0f - blend) / 2.0f;
                float radius = spot_light[S72_RADIUS]->as_num().value();
                l.others = vec4(radius, -1, outter, inner);
                l.color = tint * spot_light[S72_POWER]->as_num().value();
                if(spot_light.count(S72_LIMIT) != 0) {
                    l.others[1] = spot_light[S72_LIMIT]->as_num().value();
                }
                light_ptr = std::make_shared<Light>(l, Light::Type::SPOT);
            }
            light_ptr->name = jmap[S72_NAME]->as_str().value();
            if(jmap.count(S72_SHADOW) != 0) {
                light_ptr->shadow_res = jmap[S72_SHADOW]->as_num().value();
            }
            light_ptr->transform = default_transform;
            
            return light_ptr;
        };

        
        std::vector<int> root_idxes;

        for(size_t i=1; i<jsonList.size(); i++){
            const auto& jsonPtr = jsonList[i];
            JsonObject jmap = jsonPtr->as_obj().value();
            std::string type = jmap[S72_TYPE]->as_str().value();
            std::cout<<"load type: "<<type<<"\n";
            if(type == S72_MESH_TYPE) {
                std::shared_ptr<Mesh> mesh_ptr = load_mesh(jmap);
                idx_to_mesh[i] = mesh_ptr;
                if(jmap.count(S72_MATERIAL)) {
                    int material_id=jmap[S72_MATERIAL]->as_num().value();
                    references.push_back(Reference(material_id, Type::Mat, i, Type::Me));
                } else {
                    mesh_ptr->material = simple_material;
                }
                
            } else if (type == S72_NODE_TYPE) {
                std::shared_ptr<Transform> trans_ptr = load_transform(jmap);
                if(jmap.count(S72_MESH)){
                    int mesh_id=jmap[S72_MESH]->as_num().value();
                    references.push_back(Reference(mesh_id, Type::Me, i, Type::Mat));
                }
                if(jmap.count(S72_CAMERA)){
                    int cam_id=jmap[S72_CAMERA]->as_num().value();
                    references.push_back(Reference(cam_id, Type::Cam, i, Type::Trans));
                }
                if(jmap.count(S72_CHILDREN)) {
                    std::vector<double> children = jmap[S72_CHILDREN]->as_array().value();
                    for(auto n: children){
                        references.emplace_back(i, Type::Trans, static_cast<int>(n), Type::Trans);
                    }
                }
                if(jmap.count(S72_ENVIRONMENT)) {
                    environment.transform = trans_ptr;
                }
                if(jmap.count("light")){
                    int light_id=jmap["light"]->as_num().value();
                    references.push_back(Reference(light_id, Type::Li, i, Type::Trans));
                }
                idx_to_trans[i] = trans_ptr;
            } else if (type == "CAMERA") {
                std::shared_ptr<Camera> cam_ptr = load_camera(jmap);
                idx_to_cam[i] = cam_ptr;
            } else if (type == S72_DRIVER_TYPE) {
                std::shared_ptr<Driver> driver_ptr = load_driver(jmap);
                idx_to_dri[i] = driver_ptr;
                int tran_id=jmap[S72_NODE]->as_num().value();
                references.push_back(Reference(i, Type::Dri, tran_id, Type::Trans));
            } else if (type == S72_SCENE_TYPE) {
                name = jmap[S72_NAME]->as_str().value();
                std::vector<double> vec = jmap["roots"]->as_array().value();
                for(size_t i=0; i<vec.size(); i++){
                    root_idxes.push_back(static_cast<int>(vec[i]));
                }
            } else if (type == S72_MATERIAL_TYPE) {
                std::shared_ptr<Material> mat_ptr = load_material(jmap);
                idx_to_material[i] = mat_ptr;
            } else if (type == S72_ENVIRONMENT_TYPE) {
                environment.texture = load_texture(jmap[S72_RADIANCE]->as_obj().value());
                environment.exist = true;
            } else if (type == S72_LIGHT_TYPE) {
                std::shared_ptr<Light> light_ptr = load_light(jmap);
                idx_to_light[i] = light_ptr;
                lights.push_back(light_ptr);
            }
        }
        
        // Map references between 
        // - driver to transform
        // - camera to transform
        // - mesh to transform
        // - transform to parent transform
        // - mesh to material
        
        for(Reference& r: references){
            if(r.fromType == Type::Dri) {
                idx_to_dri[r.from]->transform = idx_to_trans[r.to];
            } else if (r.fromType == Type::Cam) {
                idx_to_cam[r.from]->transform = idx_to_trans[r.to];
            } else if (r.fromType == Type::Me) {
                modelInfos.push_back(std::make_shared<ModelInfo>(idx_to_trans[r.to], idx_to_mesh[r.from]));
            } else if (r.fromType == Type::Trans) {
                idx_to_trans[r.from]->children.push_back(idx_to_trans[r.to]);
                idx_to_trans[r.to]->parent = idx_to_trans[r.from];
            } else if (r.fromType == Type::Mat) {
                idx_to_mesh[r.to]->material = idx_to_material[r.from];
            } else if (r.fromType == Type::Li) {
                idx_to_light[r.from]->transform = idx_to_trans[r.to];
            }
        }
        
        for(int n: root_idxes){
            roots.push_back(idx_to_trans[n]);
        }
    }
};

 /* scene_h */
