//
//  math_util.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//  Referenced VulkanCookbook

#pragma once

#include "mat.h"
#include "vec.h"
#include "qua.h"

/* ----------------- Vec -----------------*/
// Return the cross product of 2 vec3
vec3 cross(const vec3 l, const vec3 r);

// Return the dot product of 2 vec
template<typename T, uint32_t size>
T dot(const vec<T,size> l, const vec<T,size> r);

// Return a vec whose elements are minimum of both vec
template<typename T, uint32_t size>
vec<T,size> vmin(const vec<T,size>& l, const vec<T,size>& r);

// Return a vec whose elements are maximum of both vec
template<typename T, uint32_t size>
vec<T,size> vmax(const vec<T,size>& l, const vec<T,size>& r);

// Convert degree to radians
float degToRad(float degree);

// Liner interpolation between 2 vec3
vec3 lerp(const vec3 start, const vec3 end, float t /* a fraction of 1*/);

/* ---------------- Matrix ---------------- */
mat4 inverse(const mat4);

mat4 rotate(const mat4 m, float angle, vec3 v);

/* ---------------- Transform ---------------- */

/**
Return transformation for viewing the scene from eye position (@eye)
looking at the position (@center) with the up axis (@up)
*/
mat4 lookAt(const vec3 eye, const vec3 center, const vec3 up);

/**
Return perspective matrix with
- @vfov: vertical field of view (in radians)
- @aspect: aspect ratio = width / height 
- @near: near clipping plane distance
- @far: far clipping plane distance
*/
mat4 perspective(const float vfov, const float aspect, const float near, const float far);

// Prepare a translation matrix
mat4 translationMat(vec3 t);

// Prepare a scale matrix
mat4 scaleMat(vec3 s);

// Prepare a rotation matrix (from a quaternion)
mat4 rotationMat(qua r);


/* ------------------ Quaternion ------------------ */
// Convert euler angle to quaternion
qua eulerToQua(vec3 euler);
// Linear interpolation between 2 quaternions
qua quaLerp(const qua qStart, const qua qEnd, float t);
// Spherical interpolation between 2 quaternons 
qua slerp(const qua qStart, const qua qEnd, float t /* a fraction of 1*/);
// Return a quaternion from an angle and a axis
qua angleAxis(const float& angle, const vec3& dir);

/* ------------------- Cubemap ------------------- */
vec3 rgbe_to_float(u8vec4 col);
u8vec4 float_to_rgbe(vec3 col);
