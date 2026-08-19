#ifndef havok_math
#define havok_math

#include <iostream>
#include <Windows.h>
#include <vector>

#include "ida_defs.hpp"
#include "../../utils/utils.h"
#include <chrono>

#define RVA(offset) (void*)(utils::memory::image_base + offset)

#define RVA_CALL(call, offset) call = reinterpret_cast<decltype(call)>(RVA(offset));

#define STR_MERGE_IMPL(a, b) a##b
#define STR_MERGE(a, b) STR_MERGE_IMPL(a, b)
#define MAKE_PAD(size) STR_MERGE(_pad, __COUNTER__)[size]
#define MEMBER(type, name, offset) struct {unsigned char MAKE_PAD(offset); type name;}

#define DEFINE_MEMBER(type, name, offset) \
	__forceinline auto name( ) const noexcept -> type { if ( !this ) return {}; return *reinterpret_cast< type* >( reinterpret_cast< uintptr_t >( this ) + offset ); } \
	__forceinline auto name( ) -> type&               { type clown{}; if ( !this ) return clown; return *reinterpret_cast< type* >( reinterpret_cast< uintptr_t >( this ) + offset ); }



class ubiQuaternion {
public:
    constexpr ubiQuaternion(float w = 1.f, float x = 0.f, float y = 0.f, float z = 0.f) noexcept
        : w(w), x(x), y(y), z(z) {
    }

    constexpr ubiQuaternion operator+(const ubiQuaternion& other) const noexcept {
        return ubiQuaternion(w + other.w, x + other.x, y + other.y, z + other.z);
    }

    constexpr ubiQuaternion operator-(const ubiQuaternion& other) const noexcept {
        return ubiQuaternion(w - other.w, x - other.x, y - other.y, z - other.z);
    }

    constexpr ubiQuaternion operator*(float factor) const noexcept {
        return ubiQuaternion(w * factor, x * factor, y * factor, z * factor);
    }

    constexpr ubiQuaternion operator*(const ubiQuaternion& other) const noexcept {
        return ubiQuaternion(
            w * other.w - x * other.x - y * other.y - z * other.z,
            w * other.x + x * other.w + y * other.z - z * other.y,
            w * other.y - x * other.z + y * other.w + z * other.x,
            w * other.z + x * other.y - y * other.x + z * other.w
        );
    }

    ubiQuaternion& operator+=(const ubiQuaternion& other) noexcept {
        w += other.w; x += other.x; y += other.y; z += other.z;
        return *this;
    }

    ubiQuaternion& operator-=(const ubiQuaternion& other) noexcept {
        w -= other.w; x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    ubiQuaternion& operator*=(float factor) noexcept {
        w *= factor; x *= factor; y *= factor; z *= factor;
        return *this;
    }

    ubiQuaternion& operator*=(const ubiQuaternion& other) noexcept {
        *this = *this * other;
        return *this;
    }

    float w, x, y, z;
};

class ubiVector2 {
public:
    constexpr ubiVector2(float x = 0.f, float y = 0.f) noexcept
        : x(x), y(y) {
    }

    constexpr ubiVector2 operator-(const ubiVector2& other) const noexcept {
        return ubiVector2(x - other.x, y - other.y);
    }

    constexpr ubiVector2 operator+(const ubiVector2& other) const noexcept {
        return ubiVector2(x + other.x, y + other.y);
    }

    constexpr ubiVector2 operator/(float factor) const noexcept {
        return ubiVector2(x / factor, y / factor);
    }

    constexpr ubiVector2 operator*(float factor) const noexcept {
        return ubiVector2(x * factor, y * factor);
    }

    ubiVector2& operator+=(const ubiVector2& other) noexcept {
        x += other.x; y += other.y;
        return *this;
    }

    ubiVector2& operator-=(const ubiVector2& other) noexcept {
        x -= other.x; y -= other.y;
        return *this;
    }

    ubiVector2& operator*=(float factor) noexcept {
        x *= factor; y *= factor;
        return *this;
    }

    ubiVector2& operator/=(float factor) noexcept {
        x /= factor; y /= factor;
        return *this;
    }

    float x, y;
};


class ubiVector3 {
public:
    float x, y, z;

    constexpr ubiVector3(float x = 0.f, float y = 0.f, float z = 0.f) noexcept
        : x(x), y(y), z(z) {
    }

    // Unary minus
    constexpr ubiVector3 operator-() const noexcept {
        return { -x, -y, -z };
    }

    // Arithmetic
    constexpr ubiVector3 operator+(const ubiVector3& other) const noexcept {
        return { x + other.x, y + other.y, z + other.z };
    }

    constexpr ubiVector3 operator-(const ubiVector3& other) const noexcept {
        return { x - other.x, y - other.y, z - other.z };
    }

    constexpr ubiVector3 operator*(float scalar) const noexcept {
        return { x * scalar, y * scalar, z * scalar };
    }

    constexpr ubiVector3 operator/(float scalar) const noexcept {
        return { x / scalar, y / scalar, z / scalar };
    }

    // Compound assignment
    ubiVector3& operator+=(const ubiVector3& other) noexcept {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }

    ubiVector3& operator-=(const ubiVector3& other) noexcept {
        x -= other.x; y -= other.y; z -= other.z;
        return *this;
    }

    ubiVector3& operator*=(float scalar) noexcept {
        x *= scalar; y *= scalar; z *= scalar;
        return *this;
    }

    ubiVector3& operator/=(float scalar) noexcept {
        x /= scalar; y /= scalar; z /= scalar;
        return *this;
    }

    // Comparison
    constexpr bool operator==(const ubiVector3& other) const noexcept {
        return x == other.x && y == other.y && z == other.z;
    }

    // Math functions
    float Dot(const ubiVector3& other) const noexcept {
        return x * other.x + y * other.y + z * other.z;
    }

    float Length() const noexcept {
        return std::sqrt(x * x + y * y + z * z);
    }

    void Normalize() noexcept {
        float len = Length();
        if (len > 0.0f) {
            x /= len;
            y /= len;
            z /= len;
        }
    }

    void Normalize2()
    {
        float magnitude = std::sqrt(x * x + y * y + z * z);
        if (magnitude != 0.0f)
        {
            x /= magnitude;
            y /= magnitude;
            z /= magnitude;
        }
    }
};

class ubiVector4 {
public:
    constexpr ubiVector4(float x = 0.f, float y = 0.f, float z = 0.f, float w = 0.f) noexcept
        : x(x), y(y), z(z), w(w) {
    }

    constexpr ubiVector4 operator-(const ubiVector4& other) const noexcept {
        return ubiVector4(x - other.x, y - other.y, z - other.z, w - other.w);
    }

    constexpr ubiVector4 operator+(const ubiVector4& other) const noexcept {
        return ubiVector4(x + other.x, y + other.y, z + other.z, w + other.w);
    }

    constexpr ubiVector4 operator/(float factor) const noexcept {
        return ubiVector4(x / factor, y / factor, z / factor, w / factor);
    }

    constexpr ubiVector4 operator*(float factor) const noexcept {
        return ubiVector4(x * factor, y * factor, z * factor, w * factor);
    }

    ubiVector4& operator+=(const ubiVector4& other) noexcept {
        x += other.x; y += other.y; z += other.z; w += other.w;
        return *this;
    }

    ubiVector4& operator-=(const ubiVector4& other) noexcept {
        x -= other.x; y -= other.y; z -= other.z; w -= other.w;
        return *this;
    }

    ubiVector4& operator*=(float factor) noexcept {
        x *= factor; y *= factor; z *= factor; w *= factor;
        return *this;
    }

    ubiVector4& operator/=(float factor) noexcept {
        x /= factor; y /= factor; z /= factor; w /= factor;
        return *this;
    }



    ubiVector4 operator*(const  ubiVector4& other) const
    {
        ubiVector4 result;

        result.w = w * other.w - x * other.x - y * other.y - z * other.z;
        result.x = w * other.x + x * other.w + y * other.z - z * other.y;
        result.y = w * other.y - x * other.z + y * other.w + z * other.x;
        result.z = w * other.z + x * other.y - y * other.x + z * other.w;

        return result;
    }


    bool empty() const {
        return x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f;
    }

    void Normalize()
    {
        float magnitude = std::sqrt(x * x + y * y + z * z + w * w);
        if (magnitude != 0.0f)
        {
            x /= magnitude;
            y /= magnitude;
            z /= magnitude;
            w /= magnitude;
        }
    }

    inline float Length()
    {
        return std::sqrt((x * x) + (y * y) + (z * z) + (w * w));
    }

    float x, y, z, w;

};


struct ubiViewMatrix {
    ubiViewMatrix() noexcept
        : m_data() {
    }

    float* operator[](int index) noexcept {
        return m_data[index];
    }

    const float* operator[](int index) const noexcept {
        return m_data[index];
    }

    float m_data[4][4];
};

inline unsigned char F32_TO_INT8_SATURATED(float x)
{
    if (x < 0.0f) return 0;
    if (x > 1.0f) return 255;
    return static_cast<unsigned char>(x * 255.0f + 0.5f);
}

__forceinline float Saturate(float f)
{
    return (f < 0.0f) ? 0.0f : (f > 1.0f) ? 1.0f : f;
}

class IColor
{
public:
    union
    {
        /* 0x0000 */ unsigned long m_Color;
        struct
        {
            /* 0x0000 */ unsigned char m_A;
            /* 0x0001 */ unsigned char m_R;
            /* 0x0002 */ unsigned char m_G;
            /* 0x0003 */ unsigned char m_B;
        } /* size: 0x0004 */ m_Channels;
    }; /* size: 0x0004 */

    IColor() = default;

    IColor(float r, float g, float b, float a)
    {
        m_Color = ((unsigned long)F32_TO_INT8_SATURATED(b)) << 0;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(g)) << 8;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(r)) << 16;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(a)) << 24;
    }

    IColor(float val[4])
    {
        m_Color = ((unsigned long)F32_TO_INT8_SATURATED(val[2])) << 0;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(val[1])) << 8;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(val[0])) << 16;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(val[3])) << 24;
    }

    void Update(float val[4])
    {
        m_Color = ((unsigned long)F32_TO_INT8_SATURATED(val[2])) << 0;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(val[1])) << 8;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(val[0])) << 16;
        m_Color |= ((unsigned long)F32_TO_INT8_SATURATED(val[3])) << 24;
    }
}; /* size: 0x0004 */


struct ColorRGB {
    float r, g, b;
};

// h: 0.0–1.0, s: 0.0–1.0, v: 0.0–1.0
inline ColorRGB HSVtoRGB(float h, float s, float v) {
    h = fmodf(h, 1.0f);  // wrap around if h > 1
    float c = v * s;
    float x = c * (1 - fabsf(fmodf(h * 6.0f, 2.0f) - 1));
    float m = v - c;

    float r = 0, g = 0, b = 0;

    if (h < 1.0f / 6.0f) { r = c; g = x; b = 0; }
    else if (h < 2.0f / 6.0f) { r = x; g = c; b = 0; }
    else if (h < 3.0f / 6.0f) { r = 0; g = c; b = x; }
    else if (h < 4.0f / 6.0f) { r = 0; g = x; b = c; }
    else if (h < 5.0f / 6.0f) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    r += m;
    g += m;
    b += m;

    return { r, g, b };
}

namespace math {

    inline int calculate_distance(const ubiVector4& p1, const ubiVector4& p2)
    {
        int distance = std::sqrt(std::pow(p2.x - p1.x, 2) + std::pow(p2.y - p1.y, 2) + std::pow(p2.z - p1.z, 2));
        return distance;
    }
}


#endif // !havok_math