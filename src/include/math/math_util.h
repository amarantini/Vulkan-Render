//
//  math_util.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//  Referenced VulkanCookbook

#pragma once

#include "mat.h"
#include "vec.h"

/* ----------------- Vec -----------------*/

vec3 cross(const vec3 l, const vec3 r);

template<typename T, uint32_t size>
T dot(const vec<T,size> l, const vec<T,size> r);

template<typename T, uint32_t size>
vec<T,size> vmin(const vec<T,size>& l, const vec<T,size>& r);

template<typename T, uint32_t size>
vec<T,size> vmax(const vec<T,size>& l, const vec<T,size>& r);

// Convert degree to radians
float degToRad(float degree);

// Lerp
vec3 lerp(const vec3 start, const vec3 end, float t /* a fraction of 1*/);

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
mat4 rotationMat(vec4 r);

/* ---------------- Quad ---------------- */

vec4 quaInv(const vec4& q);

/** Return a quaternion given
- @angle: rotation angle
- @dir: rotation axis
*/
vec4 angleAxis(const float& angle, const vec3& dir);

/** Quaternion multiplation
*/

vec4 quaMul(const vec4 l, const vec4 r);

/** Convert euler angle to quaternion */
vec4 eulerToQua(vec3 euler);

vec4 quaLerp(const vec4 qStart, const vec4 qEnd, float t);

/* math_util_h */
