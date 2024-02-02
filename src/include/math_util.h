//
//  math_util.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//

#ifndef math_util_h
#define math_util_h

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
mat4 rotate(mat4 transform, float angle, vec3 axis){
    //TODO
    return mat4::Zero;
}

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
    // mat4 m(
    //     x[0], y[0], -z[0], 0.0f,
    //     x[1], y[1], -z[1], 0.0f,
    //     x[2], y[2], -z[2], 0.0f,
    //     -dot(x,eye), -dot(y,eye), dot(z,eye), 1.0f
    // );

    return mat4::transpose(m);
}

/**
Return perspective matrix with
- @vfov: vertical field of view (in radians)
- @aspect: aspect ratio = width / height 
- @near: near clipping plane distance
- @far: far clipping plane distance
*/
mat4 perspective(const float vfov, const float aspect, const float near, const float far){
    float tanHalfFovInv = 1.0f / std::tan(vfov/2.0f);
    // return mat4(tanHalfFovInv/aspect, 0.0f, 0.0f, 0.0f,
    //             0.0f, -tanHalfFovInv, 0.0f,0.0f,
    //             0.0f, 0.0f, near/(far-near), near*far/(far-near),
    //             0.0f, 0.0f, -1, 0.0f);
    //Referring to glm::perspectiveRH_ZO
    return mat4::transpose(mat4(tanHalfFovInv/aspect, 0.0f, 0.0f, 0.0f,
                0.0f, tanHalfFovInv, 0.0f,0.0f,
                0.0f, 0.0f, far/(near-far), -1.0f,
                0.0f, 0.0f, -far*near/(far-near), 0.0f));
    // return mat4(tanHalfFovInv/aspect, 0.0f, 0.0f, 0.0f,
    //             0.0f, tanHalfFovInv, 0.0f,0.0f,
    //             0.0f, 0.0f, - (far + near) / (far - near), - 1.0f,
    //             0.0f, 0.0f, -(2.0f * far * near) / (far - near), 0.0f);       
}

// Convert degree to radians
float degToRad(float degree) {
    return degree * 2 * M_PI / 360.0f;
}

#endif /* math_util_h */
