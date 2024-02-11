#pragma once

#include "vec.h"

class qua {
private:
    vec4 data;

public:
    qua(const qua&) = default;
    qua& operator=(const qua&) = default;
    ~qua() = default;

    qua() {
        data[0] = 0;
        data[1] = 0;
        data[2] = 0;
        data[3] = 0;
    }

    qua(const float x, const float y, const float z, const float w){
        data[0] = x;
        data[1] = y;
        data[2] = z;
        data[3] = w;
    }

    qua(const vec4& v){
        data[0] = v[0];
        data[1] = v[1];
        data[2] = v[2];
        data[3] = v[3];
    }

    qua(float x, const vec3 v){
        data[0] = x;
        data[1] = v[0];
        data[2] = v[1];
        data[3] = v[2];
    }

    qua(const vec3 v, float w){
        data[0] = v[0];
        data[1] = v[1];
        data[2] = v[2];
        data[3] = w;
    }

    qua operator*(float s) {
        return qua(data*s);
    }

    qua operator+(const qua q) {
        qua r;
        for(size_t i=0; i<4; i++) {
            r[i] = data[i] + q[i];
        }
        return r;
    }

    float operator[](uint32_t idx) const {
        assert(idx < 4);
        return data[idx];
    }

    float& operator[](uint32_t idx) {
        assert(idx < 4);
        return data[idx];
    }

    qua inv(){
        // conjungate
        vec4 r = -data;
        r[3] *= -1;

        //unit
        r.normalize();
        return qua(r);
    }

    /** Return a quaternion given
    - @angle: rotation angle
    - @dir: rotation axis
    */
    qua angleAxis(const float& angle, const vec3& dir){
        float s = std::sin(angle * 0.5f);
        return qua(dir * s, std::cos(angle * 0.5f));
    }

    /** Quaternion multiplation
    */
    qua operator*(const qua r) {
        return qua(data[1] * r[2] - data[2] * r[1] + data[0] * r[3] + data[3] * r[0], 
                    data[2] * r[0] - data[0] * r[2] + data[1] * r[3] + data[3] * r[1],
                    data[0] * r[1] - data[1] * r[0] + data[2] * r[3] + data[3] * r[2], 
                    data[3] * r[3] - data[0] * r[0] - data[1] * r[1] - data[2] * r[2]);
    }
};

inline qua operator*(float s, qua q) {
	qua r;
    for(size_t i=0; i<4; i++){
        r[i] = s * q[i];
    }
	return r;
}

inline qua operator*(qua q, float s) {
	qua r;
    for(size_t i=0; i<4; i++){
        r[i] = s * q[i];
    }
	return r;
}

// /** Convert euler angle to quaternion */
// qua eulerToQua(vec3 euler){ //pitch, yaw, roll
// //roll (x), pitch (y), yaw (z)
//     if (euler == vec3(0.0f, 0.0f, M_PI/2.0f) || euler == vec3(M_PI/2.0f, 0.0f, 0.0f))
//             return vec4(0.0f, 0.0f, -1.0f, 0.0f);
//         float c1 = std::cos(euler[2] * 0.5f);
//         float c2 = std::cos(euler[1] * 0.5f);
//         float c3 = std::cos(euler[0] * 0.5f);
//         float s1 = std::sin(euler[2] * 0.5f);
//         float s2 = std::sin(euler[1] * 0.5f);
//         float s3 = std::sin(euler[0] * 0.5f);
//         float x = c1 * c2 * s3 - s1 * s2 * c3;
//         float y = c1 * s2 * c3 + s1 * c2 * s3;
//         float z = s1 * c2 * c3 - c1 * s2 * s3;
//         float w = c1 * c2 * c3 + s1 * s2 * s3;
//         return qua(x, y, z, w);
// }

// qua quaLerp(const qua qStart, const qua qEnd, float t) {
//     return qStart * (1.0f - t) + qEnd * t;
// }