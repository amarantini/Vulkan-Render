//
//  mat.h
//  VulkanTesting
//
//  Created by qiru hu on 1/28/24.
//

#ifndef mat_h
#define mat_h

#include "vec.h"
#include <cassert>
#include <cstddef>

template<typename T, uint32_t row_size, uint32_t col_size>
class mat {
    typedef vec<T,row_size> COL_TYPE;
    typedef mat<T,row_size,col_size> MAT_TYPE;

private:
    std::array<COL_TYPE,col_size> data;

    static COL_TYPE _add_op(const COL_TYPE& a, const COL_TYPE& b){
        return a + b;
    }

    static COL_TYPE _sub_op(const COL_TYPE& a, const COL_TYPE& b){
        return a - b;
    }

    static COL_TYPE _mul_op(const COL_TYPE& a, const COL_TYPE& b){
        return a * b;
    }

    static COL_TYPE _div_op(const COL_TYPE& a, const COL_TYPE& b){
        return a / b;
    }

    void _apply_op(const MAT_TYPE& m, std::function<COL_TYPE(COL_TYPE, COL_TYPE)> op){
        for (size_t i = 0; i < col_size; ++i) {
            data[i] = op(data[i], m[i]);
        }
    }

    void _apply_op(T s, std::function<COL_TYPE(COL_TYPE, COL_TYPE)> op){
        COL_TYPE r = COL_TYPE(s);
        for (size_t i = 0; i < col_size; ++i) {
            data[i] = op(data[i], r);
        }
    }

public:
    static const MAT_TYPE I;
    static const MAT_TYPE Zero;

    static mat<T,col_size,row_size> transpose(const MAT_TYPE& m);
    // static mat<T,col_size,row_size> inverse(const MAT_TYPE& m);

    mat() {
        for(size_t r=0; r<row_size; r++){
            for(size_t c=0; c<col_size; c++){
                data[r][c] = T(0);
            }
        }
    }

    mat(T s){
        for(size_t r=0; r<row_size; r++){
            for(size_t c=0; c<col_size; c++){
                data[r][c] = s;
            }
        }
    }

    mat(const COL_TYPE v1,
        const COL_TYPE v2) {
        assert(col_size == 2);
        data[0] = v1;
        data[1] = v2;
    }

    mat(const COL_TYPE v1,
        const COL_TYPE v2,
        const COL_TYPE v3) {
        assert(col_size == 3);
        data[0] = v1;
        data[1] = v2;
        data[2] = v3;
    }

    mat(const COL_TYPE v1,
        const COL_TYPE v2,
        const COL_TYPE v3,
        const COL_TYPE v4) {
        assert(col_size == 4);
        data[0] = v1;
        data[1] = v2;
        data[2] = v3;
        data[3] = v4;
    }

    mat(
        float m00, float m10, float m20, float m30,
        float m01, float m11, float m21, float m31,
        float m02, float m12, float m22, float m32,
        float m03, float m13, float m23, float m33 ) {
        assert(row_size==4 && col_size==4);
        data[0] = vec4(m00, m10, m20, m30);
        data[1] = vec4(m01, m11, m21, m31);
        data[2] = vec4(m02, m12, m22, m32);
        data[3] = vec4(m03, m13, m23, m33);
    }

    mat(
        float m00, float m10, float m20,
        float m01, float m11, float m21,
        float m02, float m12, float m22) {
        assert(row_size==3 && col_size==3);
        data[0] = vec3(m00, m10, m20);
        data[1] = vec3(m01, m11, m21);
        data[2] = vec3(m02, m12, m22);
    }

    mat(
        float m00, float m10,
        float m01, float m11) {
        assert(row_size==2 && col_size==2);
        data[0] = vec2(m00, m10);
        data[1] = vec2(m01, m11);
    }

    mat(const mat&) = default;
    mat& operator=(const mat&) = default;
    ~mat() = default;

    COL_TYPE& operator[](uint32_t idx) {
        assert(idx < col_size);
        return data[idx];
    }

    COL_TYPE operator[](uint32_t idx) const {
        assert(idx < col_size);
        return data[idx];
    }

    void operator+=(const MAT_TYPE& m) {
        _apply_op(m, _add_op);
    }

    void operator-=(const MAT_TYPE& m) {
        _apply_op(m, _sub_op);
    }

    void operator*=(const MAT_TYPE& m) {
        _apply_op(m, _mul_op);
    }

    void operator/=(const MAT_TYPE& m) {
        _apply_op(m, _div_op);
    }

    void operator+=(const T& s) {
        _apply_op(s, _add_op);
    }

    void operator-=(const T& s) {
        _apply_op(s, _sub_op);
    }

    void operator*=(const T& s) {
        _apply_op(s, _mul_op);
    }

    void operator/=(const T& s) {
        _apply_op(s, _div_op);
    }

    MAT_TYPE operator+(const MAT_TYPE& m) const {
        MAT_TYPE r = *this;
        r += m;
        return r;
    }

    MAT_TYPE operator-(const MAT_TYPE& m) const {
        mat r = *this;
        r -= m;
        return r;
    }

    template<uint32_t m_row_size, uint32_t m_col_size>
    mat<T,row_size,m_col_size> operator*(const mat<T,m_row_size,m_col_size> m) const {
        assert(col_size==m_row_size);
        mat r = mat<T,row_size,m_col_size>::Zero;
        for(size_t i=0; i<row_size; ++i){
            for(size_t j=0; j<m_col_size; ++j){
                for(size_t k=0; k<col_size; ++k) {
                    r[j][i] += data[k][i] * m[j][k];
                }
            }
        }
        return r;
    }

    vec<T,row_size> operator*(const vec<T,col_size> v) const {
        vec<T,row_size> r = vec<T,row_size>();
        for(size_t i=0; i<row_size; ++i){
            for(size_t j=0; j<col_size; ++j){
                r[i] += data[j][i] * v[j];
            }
        }
        return r;
    }

    std::string as_string() const {
        std::string str = "";
        for(size_t i=0; i<col_size; ++i){
            for(size_t j=0; j<row_size; ++j){
                str += std::to_string(data[i][j]) + ", ";
            }
            str += "\n";
        }
        return str;
    }

    friend std::ostream &operator<<(std::ostream &os, mat const &m) { 
        return os << m.as_string();
    }

};

template <typename T, uint32_t row_size, uint32_t col_size>
const mat<T, row_size, col_size> mat<T, row_size, col_size>::I = []() {
    mat<T, row_size, col_size> identityMatrix = mat<T, row_size, col_size>();
    for (size_t i = 0; i < row_size && i < col_size; ++i) {
        identityMatrix[i][i] = T(1);
    }
    return identityMatrix;
}();

template<typename T, uint32_t row_size, uint32_t col_size>
const mat<T, row_size, col_size> mat<T, row_size, col_size>::Zero = mat<T, row_size, col_size>();

template<typename T, uint32_t row_size, uint32_t col_size>
mat<T, col_size,row_size> mat<T, row_size, col_size>::transpose(const mat<T, row_size, col_size> &m){
    mat<T, col_size,row_size> r = mat<T, col_size,row_size>();
    for(size_t i=0; i<row_size; ++i){
        for(size_t j=0; j<col_size; ++j){
                r[j][i] = m[i][j];
        }
    }
    return r;
}

typedef mat<float,4,4> mat4;
typedef mat<float,3,3> mat3;
typedef mat<float,2,2> mat2;

// class mat4_64 {
//     float data[16];

//     mat4_64(mat4 m){
//         size_t i = 0;
//         for(size_t r=0; r<4; r++){
//             for(size_t c=0; c<4; c++){
//                 data[i++] = m[r][c];
//             }
//         }
//     }
// }


#endif /* mat_h */
