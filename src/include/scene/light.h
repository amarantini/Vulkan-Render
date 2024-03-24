#pragma once

#include "mathlib.h"
#include "transform.h"
#include <cfloat>
#include <memory>
#include <variant>

struct SphereLight { //sphere
    // a light that emits light in all directions equally
    // has location
    vec4 pos; //world space
    vec4 color; //tint * power
    vec4 others; //radius, limit, *, *
    // float radius;
    // float limit = -1;//no limit
};

struct SpotLight { //spot
    // a light emits light in a cone shape
    // has position
    mat4 lightVP = mat4(0);
    vec4 pos; //world space, calculate using model * vec4(0,0,0,1)
    vec4 direction;  //world space, calculate using model * vec4(0,0,-1,0)
    vec4 color; //tint * power
    vec4 others; //radius, limit, outter, inner
    vec4 shadow = vec4(0,0,0,0); //shadow_res, shadow map index, 0, 0
    // float radius;
    // float outter;//fov/2
    // float inner;//fov*(1-blend)/2
    // float limit = -1; //no limit
};

struct DirectionalLight { //sun 
    // a light infinitely far away and emits light in one direction only
    vec4 direction; 
    vec4 color; //tint * power
    vec4 others; //angle, *, *, *
    // float angle;
};

struct Light {
    enum Type {SPOT, DIRECTIONAL, POINT};

    std::string name;
    std::variant<SphereLight, DirectionalLight, SpotLight> light;
    Type type;
    std::shared_ptr<Transform> transform;
    int shadow_res = 0; //the side length of the shadow map

    Light() = default;

    Light(decltype(light) light_, Type type_) : light(std::move(light_)), type(type_) { }

    SpotLight spotLight(){
        SpotLight l = std::get<SpotLight>(light);
        mat4 m = transform->localToWorld();
        l.pos = m * vec4(0,0,0,1);
        l.direction = m * vec4(0,0,-1,0);
        return l;
    }

    SphereLight sphereLight(){
        SphereLight l = std::get<SphereLight>(light);
        mat4 m = transform->localToWorld();
        l.pos = m * vec4(0,0,0,1);
        return l;
    }

    DirectionalLight directionalLight(){
        DirectionalLight l = std::get<DirectionalLight>(light);
        mat4 m = transform->localToWorld();
        l.direction = m * vec4(0,0,-1,0);
        return l;
    }
};