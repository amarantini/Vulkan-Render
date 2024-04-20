
#include <cmath>
#include <algorithm>
#include <cfloat>
#include <iostream>
#include "math_util.h"

float lerp(const float start, const float end, float t /* a fraction of 1*/){
    return start + (end - start) * t;
}

/* ----------------- Vec -----------------*/

vec3 cross(vec3 l, vec3 r) {
	return vec3(l[1] * r[2] - l[2] * r[1], l[2] * r[0] - l[0] * r[2], l[0] * r[1] - l[1] * r[0]);
}

template<typename T, uint32_t size>
T dot(const vec<T,size> l, const vec<T,size> r) {
    T result = 0;
    for(size_t i=0; i<size; i++){
        result += l[i]*r[i];
    }
    return result;
}

template float dot(vec3 l, vec3 r);

template<typename T, uint32_t size>
vec<T,size> vmin(const vec<T,size>& l, const vec<T,size>& r) {
    vec<T,size> v;
    for(size_t i=0; i<size; i++){
        v[i] = std::min(l[i], r[i]);
    }
    return v;
}

template vec3 vmin(const vec3& l, const vec3& r);

template<typename T, uint32_t size>
vec<T,size> vmax(const vec<T,size>& l, const vec<T,size>& r) {
    vec<T,size> v;
    for(size_t i=0; i<size; i++){
        v[i] = std::max(l[i], r[i]);
    }
    return v;
}
template vec3 vmax(const vec3& l, const vec3& r);

// Convert degree to radians
float degToRad(float degree) {
    return degree * 2 * M_PI / 360.0f;
}

// Lerp
vec3 lerp(const vec3 start, const vec3 end, float t /* a fraction of 1*/){
    return start + (end - start) * t;
}

/* ---------------- Matrix ---------------- */

// reference compute_inverse<4, 4, T, Q, Aligned>
mat4 inverse(const mat4 m) {
    float Coef00 = m[2][2] * m[3][3] - m[3][2] * m[2][3];
    float Coef02 = m[1][2] * m[3][3] - m[3][2] * m[1][3];
    float Coef03 = m[1][2] * m[2][3] - m[2][2] * m[1][3];

    float Coef04 = m[2][1] * m[3][3] - m[3][1] * m[2][3];
    float Coef06 = m[1][1] * m[3][3] - m[3][1] * m[1][3];
    float Coef07 = m[1][1] * m[2][3] - m[2][1] * m[1][3];

    float Coef08 = m[2][1] * m[3][2] - m[3][1] * m[2][2];
    float Coef10 = m[1][1] * m[3][2] - m[3][1] * m[1][2];
    float Coef11 = m[1][1] * m[2][2] - m[2][1] * m[1][2];

    float Coef12 = m[2][0] * m[3][3] - m[3][0] * m[2][3];
    float Coef14 = m[1][0] * m[3][3] - m[3][0] * m[1][3];
    float Coef15 = m[1][0] * m[2][3] - m[2][0] * m[1][3];

    float Coef16 = m[2][0] * m[3][2] - m[3][0] * m[2][2];
    float Coef18 = m[1][0] * m[3][2] - m[3][0] * m[1][2];
    float Coef19 = m[1][0] * m[2][2] - m[2][0] * m[1][2];

    float Coef20 = m[2][0] * m[3][1] - m[3][0] * m[2][1];
    float Coef22 = m[1][0] * m[3][1] - m[3][0] * m[1][1];
    float Coef23 = m[1][0] * m[2][1] - m[2][0] * m[1][1];

    vec4 Fac0(Coef00, Coef00, Coef02, Coef03);
    vec4 Fac1(Coef04, Coef04, Coef06, Coef07);
    vec4 Fac2(Coef08, Coef08, Coef10, Coef11);
    vec4 Fac3(Coef12, Coef12, Coef14, Coef15);
    vec4 Fac4(Coef16, Coef16, Coef18, Coef19);
    vec4 Fac5(Coef20, Coef20, Coef22, Coef23);

    vec4 Vec0(m[1][0], m[0][0], m[0][0], m[0][0]);
    vec4 Vec1(m[1][1], m[0][1], m[0][1], m[0][1]);
    vec4 Vec2(m[1][2], m[0][2], m[0][2], m[0][2]);
    vec4 Vec3(m[1][3], m[0][3], m[0][3], m[0][3]);

    vec4 Inv0(Vec1 * Fac0 - Vec2 * Fac1 + Vec3 * Fac2);
    vec4 Inv1(Vec0 * Fac0 - Vec2 * Fac3 + Vec3 * Fac4);
    vec4 Inv2(Vec0 * Fac1 - Vec1 * Fac3 + Vec3 * Fac5);
    vec4 Inv3(Vec0 * Fac2 - Vec1 * Fac4 + Vec2 * Fac5);

    vec4 SignA(+1, -1, +1, -1);
    vec4 SignB(-1, +1, -1, +1);
    mat4 Inverse(Inv0 * SignA, Inv1 * SignB, Inv2 * SignA, Inv3 * SignB);

    vec4 Row0(Inverse[0][0], Inverse[1][0], Inverse[2][0], Inverse[3][0]);

    vec4 Dot0(m[0] * Row0);
    float Dot1 = (Dot0[0] + Dot0[1]) + (Dot0[2] + Dot0[3]);

    float OneOverDeterminant = 1.0f / Dot1;

    return Inverse * OneOverDeterminant;
}

/*
Reference https://github.com/g-truc/glm/blob/4137519418a933e5863eea7c3ac53890ae7faf9d/glm/ext/matrix_transform.inl#L17
Rotate matrix @m for an angle @angle around axis @v
*/
mat4 rotate(const mat4 m, float angle, vec3 v) {
    float cos_a = cos(angle);
    float sin_a = sin(angle);

    vec3 axis = v.normalized();
    vec3 temp = (1 - cos_a) * axis;

    mat4 r;
    r[0][0] = cos_a + temp[0] * axis[0];
    r[0][1] = temp[0] * axis[1] + sin_a * axis[2];
    r[0][2] = temp[0] * axis[2] - sin_a * axis[1];

    r[1][0] = temp[1] * axis[0] - sin_a * axis[2];
    r[1][1] = cos_a + temp[1] * axis[1];
    r[1][2] = temp[1] * axis[2] + sin_a * axis[0];

    r[2][0] = temp[2] * axis[0] + sin_a * axis[1];
    r[2][1] = temp[2] * axis[1] - sin_a * axis[0];
    r[2][2] = cos_a + temp[2] * axis[2];

    mat4 result;
    result[0] = m[0] * r[0][0] + m[1] * r[0][1] + m[2] * r[0][2];
    result[1] = m[0] * r[1][0] + m[1] * r[1][1] + m[2] * r[1][2];
    result[2] = m[0] * r[2][0] + m[1] * r[2][1] + m[2] * r[2][2];
    result[3] = m[3];
    return result;
}

/* ---------------- Transform ---------------- */

/**
Return transformation for viewing the scene from eye position (@eye)
looking at the position (@center) with the up axis (@up)
Reference glm: https://github.com/g-truc/glm/blob/ab913bbdd0bd10462114a17bcb65cf5a368c1f32/glm/ext/matrix_transform.inl
*/
mat4 lookAt(const vec3 eye, const vec3 center, const vec3 up){
    vec3 const f = (center - eye).normalized();
    vec3 const s = (cross(f, up)).normalized();
    vec3 const u = (cross(s, f)).normalized();

    mat4 m(0);
    m[0][0] = s[0];
    m[1][0] = s[1];
    m[2][0] = s[2];
    m[0][1] = u[0];
    m[1][1] = u[1];
    m[2][1] = u[2];
    m[0][2] =-f[0];
    m[1][2] =-f[1];
    m[2][2] =-f[2];
    m[3][0] =-dot(s, eye);
    m[3][1] =-dot(u, eye);
    m[3][2] = dot(f, eye);
    m[3][3] = 1;
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
    //Reference: perspectiveRH_ZO
    return mat4(tanHalfFovInv/aspect, 0.0f, 0.0f, 0.0f,
                0.0f, tanHalfFovInv, 0.0f,0.0f,
                0.0f, 0.0f, far/(near-far), -1.0f,
                0.0f, 0.0f, -far*near/(far-near), 0.0f);      
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
mat4 rotationMat(qua r) {
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


/* ------------------ Quaternion ------------------ */
/** Convert euler angle to quaternion */
qua eulerToQua(vec3 euler){ //pitch, yaw, roll
//roll (x), pitch (y), yaw (z)
    if (euler == vec3(0.0f, 0.0f, M_PI/2.0f) || euler == vec3(M_PI/2.0f, 0.0f, 0.0f))
            return vec4(0.0f, 0.0f, -1.0f, 0.0f);
        float c1 = std::cos(euler[2] * 0.5f);
        float c2 = std::cos(euler[1] * 0.5f);
        float c3 = std::cos(euler[0] * 0.5f);
        float s1 = std::sin(euler[2] * 0.5f);
        float s2 = std::sin(euler[1] * 0.5f);
        float s3 = std::sin(euler[0] * 0.5f);
        float x = c1 * c2 * s3 - s1 * s2 * c3;
        float y = c1 * s2 * c3 + s1 * c2 * s3;
        float z = s1 * c2 * c3 - c1 * s2 * s3;
        float w = c1 * c2 * c3 + s1 * s2 * s3;
        return qua(x, y, z, w);
}

qua quaLerp(const qua qStart, const qua qEnd, float t) {
    return qStart * (1.0f - t) + qEnd * t;
}

// Slerp for quaternions
// Referenced Scotter3D (https://github.com/CMU-Graphics/Scotty3D)
qua slerp(qua qStart, qua qEnd, float t /* a fraction of 1*/){
    float cosHalfTheta = dot(qStart.toVec(), qEnd.toVec());
    qua q = cosHalfTheta < 0 ? -qStart : qStart;
    if (std::abs(cosHalfTheta) >= 1.0f - FLT_EPSILON) {
		return (1.0f - t) * q + t * qEnd;
	}

	float halfTheta = std::acos(std::abs(cosHalfTheta));
	return (std::sin((1.0f - t) * halfTheta) * q + std::sin(t * halfTheta) * qEnd) * (1.0f / std::sin(halfTheta));
}

/** Return a quaternion given
- @angle: rotation angle
- @dir: rotation axis
*/
qua angleAxis(const float& angle, const vec3& dir){
    float s = std::sin(angle * 0.5f);
    return qua(dir * s, std::cos(angle * 0.5f));
}

/* ------------------- Cubemap ------------------- */
// Referenced from https://github.com/ixchow/15-466-ibl
vec3 rgbe_to_float(u8vec4 col) {
	//avoid decoding zero to a denormalized value:
	if (col == u8vec4(0,0,0,0)) return vec3(0.0f);

	int exp = int(col[3]) - 128;
	return vec3(
		std::ldexp(((float)col[0] + 0.5f) / 256.0f, exp),
		std::ldexp(((float)col[1] + 0.5f) / 256.0f, exp),
		std::ldexp(((float)col[2]+ 0.5f) / 256.0f, exp)
	);
}

u8vec4 float_to_rgbe(vec3 col) {

	float d = std::max(col[0], std::max(col[1], col[2]));

	//1e-32 is from the radiance code, and is probably larger than strictly necessary:
	if (d <= 1e-32f) {
		return u8vec4(0,0,0,0);
	}

	int e;
	float fac = 255.999f * (std::frexp(d, &e) / d);

	//value is too large to represent, clamp to bright white:
	if (e > 127) {
		return u8vec4(0xff, 0xff, 0xff, 0xff);
	}

	//scale and store:
	return u8vec4(
		std::max(0, int32_t(col[0] * fac)),
		std::max(0, int32_t(col[1] * fac)),
		std::max(0, int32_t(col[2] * fac)),
		e + 128
	);
}