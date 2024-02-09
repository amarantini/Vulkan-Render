#pragma once

#include "vertex.hpp"
#include "mathlib.h"
#include <cfloat>

class Bbox { //Axis aligned bounding box 
public:
    vec3 min, max; 
    Bbox() : min(FLT_MAX), max(-FLT_MAX) {}

    void enclose(const vec3& r);
};


bool within(float target, float left, float right);


// Referenced https://bruop.github.io/frustum_culling/
// Has false negatives!
bool frustum_cull_test(const mat4& mvp, const Bbox& bbox);