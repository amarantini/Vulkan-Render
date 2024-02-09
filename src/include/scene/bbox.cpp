#include "bbox.h"

void Bbox::enclose(const vec3& r) {
    min = vmin(min, r);
    max = vmax(max, r);
}

bool within(float target, float left, float right){
    return target >= left && target <= right;
}

// Referenced https://bruop.github.io/frustum_culling/
// Has false negatives!
bool frustum_cull_test(const mat4& mvp, const Bbox& bbox){
    vec4 corners[8] = {
        // near plane
        {bbox.min[0], bbox.min[1], bbox.min[2], 1.0}, //x y z bottom left
        {bbox.max[0], bbox.min[1], bbox.min[2], 1.0}, //X y z bottom right
        {bbox.min[0], bbox.max[1], bbox.min[2], 1.0}, //x Y z top left
        {bbox.max[0], bbox.max[1], bbox.min[2], 1.0}, //X Y z top right
        // far plane
        {bbox.min[0], bbox.min[1], bbox.max[2], 1.0}, //x y Z bottom left
        {bbox.max[0], bbox.min[1], bbox.max[2], 1.0}, //X y Z bottom right
        {bbox.min[0], bbox.max[1], bbox.max[2], 1.0}, //x Y Z top left
        {bbox.max[0], bbox.max[1], bbox.max[2], 1.0}, //X Y Z top right
    };

    bool inside = false;
    // checking if all 8 AABB clip space vertices are outside each of the 6 frustum planes 
    bool leftSideTest = true;
    bool rightSideTest = true;
    bool nearSideTest = true;
    bool farSideTest = true;
    bool bottomSideTest = true;
    bool topSideTest = true;
    for(size_t i=0; i<8; i++){
        vec4 corner = mvp * corners[i]; // in clip space
        float w = corner[3];
        leftSideTest = leftSideTest && corner[0]<-w;
        rightSideTest = rightSideTest && corner[0]>w;
        nearSideTest = nearSideTest && corner[2]<0;
        farSideTest = farSideTest && corner[2]>w;
        bottomSideTest = bottomSideTest && corner[1]<-w;
        topSideTest = topSideTest && corner[1]>w;
        inside = inside || 
                (within(corner[0], -w, w) &&
                within(corner[1], -w, w) &&
                within(corner[2], 0, w));
    }

    if(inside)
        return inside;
    else if (leftSideTest || rightSideTest
            || nearSideTest || farSideTest 
            || bottomSideTest | topSideTest) {
        return false;
    } else
        return true;
}

