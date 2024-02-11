#pragma once

#include "vec.h"
#include <cassert>
#include <cfloat>

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

    qua(const std::vector<double> v){
        assert(v.size()==4);
        data[0] = v[0];
        data[1] = v[1];
        data[2] = v[2];
        data[3] = v[3];
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

    vec4 toVec(){
        return data;
    }

    qua inv() const{
        // conjungate
        vec4 r = -data;
        r[3] *= -1;

        //unit
        r.normalize();
        return qua(r);
    }

    // Quaternion multiplation
    qua operator*(const qua r) {
        return qua(data[1] * r[2] - data[2] * r[1] + data[0] * r[3] + data[3] * r[0], 
                    data[2] * r[0] - data[0] * r[2] + data[1] * r[3] + data[3] * r[1],
                    data[0] * r[1] - data[1] * r[0] + data[2] * r[3] + data[3] * r[2], 
                    data[3] * r[3] - data[0] * r[0] - data[1] * r[1] - data[2] * r[2]);
    }

    float pitch()
	{
		float const y = 2.0f * (data[1] * data[2] + data[3] * data[0]);
		float const x = data[3] * data[3] - data[0] * data[0] - data[1] * data[1] + data[2] * data[2];

		if(std::abs(x)<FLT_EPSILON && std::abs(y)<FLT_EPSILON)
			return 2.0f * std::atan2(data[0], data[3]);

		return std::atan2(y, x);
	}

    float yaw()
	{
		return std::asin(std::clamp(-2.0f * (data[0] * data[2] - data[3] * data[1]), -1.0f, 1.0f));
	}

    float roll(){
        float x = data[3] * data[3] + data[0] * data[0] - data[2] * data[2] - data[2] * data[2];
        float y = 2.0f * (data[0] * data[1] + data[3] * data[2]);

		if(std::abs(x)<FLT_EPSILON && std::abs(y)<FLT_EPSILON)
			return 0;

		return std::atan2(y, x);
    }

    vec3 toEuler() {
        return vec3(pitch(), yaw(), roll());
    }
};

inline qua operator*(float s, qua data) {
	qua r;
    for(size_t i=0; i<4; i++){
        r[i] = s * data[i];
    }
	return r;
}

inline qua operator*(qua data, float s) {
	qua r;
    for(size_t i=0; i<4; i++){
        r[i] = s * data[i];
    }
	return r;
}