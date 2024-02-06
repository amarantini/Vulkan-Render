//
//  math_util.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//  Referenced VulkanCookbook

#pragma once

#include "mat.h"
#include "vec.h"

vec3 cross(vec3 l, vec3 r) {
	return vec3(l[1] * r[2] - l[2] * r[1], l[2] * r[0] - l[0] * r[2], l[0] * r[1] - l[1] * r[0]);
}

template<typename T, uint32_t size>
T dot(vec<T,size> l, vec<T,size> r) {
    T result = 0;
    for(size_t i=0; i<size; i++){
        result += l[i]*r[i];
    }
    return result;
}

template float dot(vec3 l, vec3 r);

/**
Rotate the current transformation (@transform) to the rotation angle (@angle) 
by the rotation axis (@axis)
*/ 
// mat4 rotate(mat4 transform, float angle, vec3 axis){
//     //TODO
//     return mat4::Zero;
// }

/**
Return transformation for viewing the scene from eye position (@eye)
looking at the position (@center) with the up axis (@up)
*/
mat4 lookAt(const vec3 eye, const vec3 center, const vec3 up){
    //Set up axes
    vec3 x,y,z;
    z = eye - center;
    z.normalize();
    y = up;
    x = cross(y, z);

    y = cross(z, x);

    x.normalize();
    y.normalize();

    mat4 m(
        x[0], y[0], z[0], 0.0f,
        x[1], y[1], z[1], 0.0f,
        x[2], y[2], z[2], 0.0f,
        -dot(x,eye), -dot(y,eye), -dot(z,eye), 1.0f
    );

    return m;
}

/**
Return perspective matrix with
- @vfov: vertical field of view (in radians)
- @aspect: aspect ratio = width / height 
- @near: near clipping plane distance
- @far: far clipping plane distance
*/
mat4 perspective(const float vfov, const float aspect, const float near, const float far){
    //TODO: infinite perspective matrix
    float tanHalfFovInv = 1.0f / std::tan(vfov/2.0f);
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

// Prepare a translation matrix
mat4 translationMat(vec3 t) {
    return mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        t[0], t[1], t[2], 1.0f
    );
}

// Prepare a scale matrix
mat4 scaleMat(vec3 s) {
    return mat4(
        s[0], 0.0f, 0.0f, 0.0f,
        0.0f, s[1], 0.0f, 0.0f,
        0.0f, 0.0f, s[2], 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    );
}

// Prepare a rotation matrix (from a quaternion)
mat4 rotationMat(vec4 r) {
    // float x = rotation[0];
    // float y = rotation[1];
    // float z = rotation[2];
    // float w = rotation[3];
    // return mat4::transpose(mat4(
    //     2*(x*x+y*y)-1, 2*(y*z-x*w), 2*(y*w+x*z), 0.0f,
    //     2*(y*z+x*w), 2*(x*x+z*z)-1, 2*(z*w-x*y), 0.0f,
    //     2*(y*w-x*z), 2*(z*w+x*y), 2*(x*x-w*w)-1, 0.0f,
    //     0.0f, 0.0f, 0.0f, 1.0f
    // ));

    mat4 m = mat4::I;
    float qxx(r[0] * r[0]);
    float qyy(r[1] * r[1]);
    float qzz(r[2] * r[2]);
    float qxz(r[0] * r[2]);
    float qxy(r[0] * r[1]);
    float qyz(r[1] * r[2]);
    float qwx(r[3] * r[0]);
    float qwy(r[3] * r[1]);
    float qwz(r[3] * r[2]);

    m[0][0] = 1.0f - 2.0f * (qyy +  qzz);
    m[0][1] = 2.0f * (qxy + qwz);
    m[0][2] = 2.0f * (qxz - qwy);

    m[1][0] = 2.0f * (qxy - qwz);
    m[1][1] = 1.0f - 2.0f * (qxx +  qzz);
    m[1][2] = 2.0f * (qyz + qwx);

    m[2][0] = 2.0f * (qxz + qwy);
    m[2][1] = 2.0f * (qyz - qwx);
    m[2][2] = 1.0f - 2.0f * (qxx +  qyy);
    return m;
}

vec4 quaternionInv(const vec4& q){
    // conjungate
    vec4 r = -q;
    r[3] *= -1;

    //unit
    r.normalize();
    return r;
}

/* math_util_h */
