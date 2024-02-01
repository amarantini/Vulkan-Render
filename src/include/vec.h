//
//  vec.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//

#ifndef vec_h
#define vec_h

#include <array>
#include <cassert>
#include <functional>
#include <sys/_types/_sigaltstack.h>

template<typename T, uint32_t size>
class vec {
private:
    typedef T DATA_TYPE;
    typedef vec<T,size> VEC_TYPE;
    uint32_t SIZE = size;
    std::array<T, size> data;

    static T _add_op(const T& a, const T& b){
        return a + b;
    }

    static T _sub_op(const T& a, const T& b){
        return a - b;
    }

    static T _mul_op(const T& a, const T& b){
        return a * b;
    }

    static T _div_op(const T& a, const T& b){
        return a / b;
    }

    void _apply_op(const vec<T,size>& v, std::function<T(T, T)> op){
        for (size_t i = 0; i < SIZE; ++i) {
            data[i] = op(data[i], v[i]);
        }
    }

    void _apply_op(T s, std::function<T(T, T)> op){
        for (size_t i = 0; i < SIZE; ++i) {
            data[i] = op(data[i], s);
        }
    }

public:
    vec() {
        for(size_t i=0; i<SIZE; i++){
            data[i] = T(0);
        }
    }

    vec(T _x){
        for(size_t i=0; i<SIZE; i++){
            data[i] = _x;
        }
    }

    vec(T _x, T _y){
        assert(SIZE == 2);
        data[0] = _x;
        data[1] = _y;
    }

    vec(T _x, T _y, T _z){
        assert(SIZE == 3);
        data[0] = _x;
        data[1] = _y;
        data[2] = _z;
    }

    vec(T _x, T _y, T _z, T _w){
        assert(SIZE == 4);
        data[0] = _x;
        data[1] = _y;
        data[2] = _z;
        data[3] = _w;
    }

    vec(vec<T,2> v, T _z){
        assert(SIZE==3);
        data[0] = v[0];
        data[1] = v[1];
        data[2] = _z;
    }

    vec(T _x, vec<T,2> v){
        assert(SIZE==3);
        data[0] = _x;
        data[1] = v[0];
        data[2] = v[1];
    }

    vec(vec<T,3> v, T _z){
        assert(SIZE==4);
        data[0] = v[0];
        data[1] = v[1];
        data[2] = v[2];
        data[3] = _z;
    }

    vec(T _x, vec<T,3> v){
        assert(SIZE==4);
        data[0] = _x;
        data[1] = v[0];
        data[2] = v[1];
        data[3] = v[2];
    }

    vec(std::vector<T> vec){
        assert(SIZE==vec.size());
        for(size_t i=0; i<SIZE; i++){
            data[i] = vec[i];
        }
    }

    vec(std::vector<double> vec){
        assert(SIZE==vec.size());
        for(size_t i=0; i<SIZE; i++){
            data[i] = static_cast<T>(vec[i]);
        }
    }

    vec(const vec&) = default;
    vec& operator=(const vec&) = default;
    ~vec() = default;

    T operator[](uint32_t idx) const {
        assert(idx < SIZE);
        return data[idx];
    }

    T& operator[](uint32_t idx) {
        assert(idx < SIZE);
        return data[idx];
    }

    void operator+=(const vec<T,size>& v) {
        _apply_op(v, _add_op);
    }

    void operator-=(const vec<T,size>& v) {
        _apply_op(v, _sub_op);
    }

    void operator*=(const vec<T,size>& v) {
        _apply_op(v, _mul_op);
    }

    void operator/=(const vec<T,size>& v) {
        _apply_op(v, _div_op);
    }

    void operator+=(T s) {
        _apply_op(s, _add_op);
    }

    void operator-=(T s) {
        _apply_op(s, _sub_op);
    }

    void operator*=(T s) {
        _apply_op(s, _mul_op);
    }

    void operator/=(T s) {
        _apply_op(s, _div_op);
    }

    VEC_TYPE operator+(VEC_TYPE v) const {
        vec<T,size> result = *this;
        result += v;
        return result;
    }
    
    VEC_TYPE operator-(VEC_TYPE v) const {
        vec<T,size> result = *this;
        result -= v;
        return result;
    }
    
    VEC_TYPE operator*(VEC_TYPE v) const {
        vec<T,size> result = *this;
        result *= v;
        return result;
    }
    
    VEC_TYPE operator/(VEC_TYPE v) const {
        vec<T,size> result = *this;
        result /= v;
        return result;
    }

    VEC_TYPE operator+(float s) const {
        vec<T,size> result = *this;
        result += s;
        return result;
    }
    
    VEC_TYPE operator-(float s) const {
        vec<T,size> result = *this;
        result -= s;
        return result;
    }
    
    VEC_TYPE operator*(float s) const {
        vec<T,size> result = *this;
        result *= s;
        return result;
    }
    
    VEC_TYPE operator/(float s) const {
        vec<T,size> result = *this;
        result /= s;
        return result;
    }
    
    std::string as_string() const {
        std::string str = "";
        for(size_t i=0; i<SIZE; ++i){
            str += std::to_string(data[i]); 
            if(i!=SIZE-1)
                str += ", "; 
        }
        str += "\n";
        return str;
    }

    friend std::ostream &operator<<(std::ostream &os, vec const &v) { 
        return os << v.as_string();
    }
};

typedef vec<float,2> vec2;
typedef vec<float,3> vec3;
typedef vec<float,4> vec4;

#endif /* vec_h */
