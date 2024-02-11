#pragma once

#include <iostream>

#include "transform.h"
#include "mathlib.h"

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
};