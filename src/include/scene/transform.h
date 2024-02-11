#pragma once

#include "mathlib.h"
#include <string>

struct Transform {
    std::string name;
    vec3 translation;
    qua rotation;
    vec3 scale;
    std::shared_ptr<Transform> parent;
    std::vector<std::shared_ptr<Transform> > children;

    Transform(const std::string _name, const vec3 _translation, const qua _rotation, const vec3 _scale)
        : name(_name), translation(_translation), rotation(_rotation), scale(_scale) {}

    mat4 localToParent() const {
        return translationMat(translation) * rotationMat(rotation) * scaleMat(scale);
    }

    mat4 parentToLocal() const {
        return scaleMat(1.0f / scale) * rotationMat(rotation.inv()) * translationMat(-translation);
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