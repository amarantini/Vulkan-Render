//
//  math_util.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//

#ifndef math_util_h
#define math_util_h

#include "mat.h"

/**
Rotate the current transformation (@transform) to the rotation angle (@angle) 
by the rotation axis (@axis)
*/ 
mat4 rotate(mat4 transform, float angle, vec3 axis){
    //TODO
    return mat4::Zero;
}

/**
Return transformation for viewing the scene from eye position (@eye_pos)
looking at the position (@at) with the up axis (@up)
*/
mat4 look_at(vec3 eye_pos, vec3 at, vec3 up){
    //TODO
    return mat4::Zero;
}

/**
Return perspective matrix with
- @vfov: vertical field of view (in radians)
- @aspect: aspect ratio = width / height 
- @near: near clipping plane distance
- @far: far clipping plane distance
*/
mat4 perspective(float vfov, float aspect, float near, float far){
    float tanHalfFovInv = 1.0f / std::tan(vfov/2.0f);
    // return mat4(tanHalfFovInv/aspect, 0.0f, 0.0f, 0.0f,
    //             0.0f, -tanHalfFovInv, 0.0f,0.0f,
    //             0.0f, 0.0f, near/(far-near), near*far/(far-near),
    //             0.0f, 0.0f, -1, 0.0f);
    //Referring to glm::perspectiveRH_ZO
    return mat4(tanHalfFovInv/aspect, 0.0f, 0.0f, 0.0f,
                0.0f, tanHalfFovInv, 0.0f,0.0f,
                0.0f, 0.0f, far/(near-far), -1.0f,
                0.0f, 0.0f, -far*near/(far-near), 0.0f);
}

// Convert degree to radians
float degToRad(float degree) {
    return degree * 2 * M_PI / 360.0f;
}

#endif /* math_util_h */
