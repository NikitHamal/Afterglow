// Afterglow engine — small column-major math library (GL/glTF conventions, column vectors).
#pragma once
#include <cmath>
#include <cstring>
#include <algorithm>

namespace ag {

constexpr float kPi = 3.14159265358979323846f;

struct V2 { float x = 0, y = 0; };
struct V3 {
    float x = 0, y = 0, z = 0;
    V3() = default;
    constexpr V3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    V3 operator+(const V3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    V3 operator-(const V3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    V3 operator-() const { return {-x, -y, -z}; }
    V3 operator*(float s) const { return {x * s, y * s, z * s}; }
    V3 operator/(float s) const { return {x / s, y / s, z / s}; }
    V3 operator*(const V3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    V3& operator+=(const V3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    V3& operator-=(const V3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    V3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};
struct V4 {
    float x = 0, y = 0, z = 0, w = 0;
    V4() = default;
    constexpr V4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
};

inline float dot(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(const V3& a, const V3& b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(const V3& a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(const V3& a) { float l = length(a); return l > 1e-12f ? a / l : V3{0, 0, 0}; }
inline V3 lerp(const V3& a, const V3& b, float t) { return a + (b - a) * t; }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float smoothstep(float e0, float e1, float x) { float t = clampf((x - e0) / (e1 - e0), 0.f, 1.f); return t * t * (3.f - 2.f * t); }
inline float damp(float rate, float dt) { return 1.f - std::exp(-rate * dt); }   // frame-rate independent lerp factor

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;
    Quat() = default;
    constexpr Quat(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
};
inline Quat qmul(const Quat& a, const Quat& b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
inline Quat qconj(const Quat& q) { return {-q.x, -q.y, -q.z, q.w}; }
inline float qdot(const Quat& a, const Quat& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }
inline Quat qnormalize(const Quat& q) {
    float l = std::sqrt(qdot(q, q));
    return l > 1e-12f ? Quat{q.x / l, q.y / l, q.z / l, q.w / l} : Quat{};
}
inline Quat qaxis(const V3& axis, float angle) {
    V3 a = normalize(axis); float s = std::sin(angle * 0.5f);
    return {a.x * s, a.y * s, a.z * s, std::cos(angle * 0.5f)};
}
inline V3 qrotate(const Quat& q, const V3& v) {
    V3 u{q.x, q.y, q.z}; float s = q.w;
    return u * (2.f * dot(u, v)) + v * (s * s - dot(u, u)) + cross(u, v) * (2.f * s);
}
inline Quat qnlerp(const Quat& a, Quat b, float t) {
    if (qdot(a, b) < 0) b = {-b.x, -b.y, -b.z, -b.w};
    return qnormalize({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
}
inline Quat qslerp(const Quat& a, Quat b, float t) {
    float c = qdot(a, b);
    if (c < 0) { b = {-b.x, -b.y, -b.z, -b.w}; c = -c; }
    if (c > 0.9995f) return qnlerp(a, b, t);
    float th = std::acos(clampf(c, -1.f, 1.f)), s = std::sin(th);
    float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
    return {a.x * wa + b.x * wb, a.y * wa + b.y * wb, a.z * wa + b.z * wb, a.w * wa + b.w * wb};
}
// shortest rotation taking unit vector a onto unit vector b
inline Quat qfromto(const V3& a, const V3& b) {
    float c = dot(a, b);
    if (c < -0.999999f) {
        V3 ax = cross(V3{1, 0, 0}, a);
        if (length(ax) < 1e-6f) ax = cross(V3{0, 1, 0}, a);
        return qaxis(ax, kPi);
    }
    V3 ax = cross(a, b);
    return qnormalize({ax.x, ax.y, ax.z, 1.f + c});
}
// limit a rotation's angle
inline Quat qclampAngle(const Quat& q, float maxAngle) {
    Quat n = q.w < 0 ? Quat{-q.x, -q.y, -q.z, -q.w} : q;
    float ang = 2.f * std::acos(clampf(n.w, -1.f, 1.f));
    if (ang <= maxAngle || ang < 1e-6f) return n;
    V3 ax{n.x, n.y, n.z};
    return qaxis(ax, maxAngle);
}

struct M4 {
    float m[16];   // column-major: m[col * 4 + row]
    static M4 identity() { M4 r; std::memset(r.m, 0, sizeof r.m); r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.f; return r; }
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
    V3 translation() const { return {m[12], m[13], m[14]}; }
    V3 col3(int c) const { return {m[c * 4], m[c * 4 + 1], m[c * 4 + 2]}; }
};
inline M4 operator*(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr)
            r.m[c * 4 + rr] = a.m[0 * 4 + rr] * b.m[c * 4 + 0] + a.m[1 * 4 + rr] * b.m[c * 4 + 1] +
                              a.m[2 * 4 + rr] * b.m[c * 4 + 2] + a.m[3 * 4 + rr] * b.m[c * 4 + 3];
    return r;
}
inline V3 xformPoint(const M4& a, const V3& p) {
    return {a.m[0] * p.x + a.m[4] * p.y + a.m[8] * p.z + a.m[12],
            a.m[1] * p.x + a.m[5] * p.y + a.m[9] * p.z + a.m[13],
            a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14]};
}
inline V3 xformDir(const M4& a, const V3& d) {
    return {a.m[0] * d.x + a.m[4] * d.y + a.m[8] * d.z,
            a.m[1] * d.x + a.m[5] * d.y + a.m[9] * d.z,
            a.m[2] * d.x + a.m[6] * d.y + a.m[10] * d.z};
}
inline M4 m4translate(const V3& t) { M4 r = M4::identity(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
inline M4 m4scale(const V3& s) { M4 r = M4::identity(); r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; return r; }
inline M4 m4rotate(const Quat& q) {
    M4 r = M4::identity();
    float x = q.x, y = q.y, z = q.z, w = q.w;
    r.m[0] = 1 - 2 * (y * y + z * z); r.m[1] = 2 * (x * y + z * w);     r.m[2] = 2 * (x * z - y * w);
    r.m[4] = 2 * (x * y - z * w);     r.m[5] = 1 - 2 * (x * x + z * z); r.m[6] = 2 * (y * z + x * w);
    r.m[8] = 2 * (x * z + y * w);     r.m[9] = 2 * (y * z - x * w);     r.m[10] = 1 - 2 * (x * x + y * y);
    return r;
}
inline M4 m4trs(const V3& t, const Quat& q, const V3& s) {
    M4 r = m4rotate(q);
    r.m[0] *= s.x; r.m[1] *= s.x; r.m[2] *= s.x;
    r.m[4] *= s.y; r.m[5] *= s.y; r.m[6] *= s.y;
    r.m[8] *= s.z; r.m[9] *= s.z; r.m[10] *= s.z;
    r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
    return r;
}
inline Quat quatFromM3(const M4& a) {   // a must be orthonormal in its upper 3x3
    float tr = a.at(0, 0) + a.at(1, 1) + a.at(2, 2);
    Quat q;
    if (tr > 0) {
        float s = std::sqrt(tr + 1.f) * 2.f;
        q.w = 0.25f * s; q.x = (a.at(2, 1) - a.at(1, 2)) / s; q.y = (a.at(0, 2) - a.at(2, 0)) / s; q.z = (a.at(1, 0) - a.at(0, 1)) / s;
    } else if (a.at(0, 0) > a.at(1, 1) && a.at(0, 0) > a.at(2, 2)) {
        float s = std::sqrt(1.f + a.at(0, 0) - a.at(1, 1) - a.at(2, 2)) * 2.f;
        q.w = (a.at(2, 1) - a.at(1, 2)) / s; q.x = 0.25f * s; q.y = (a.at(0, 1) + a.at(1, 0)) / s; q.z = (a.at(0, 2) + a.at(2, 0)) / s;
    } else if (a.at(1, 1) > a.at(2, 2)) {
        float s = std::sqrt(1.f + a.at(1, 1) - a.at(0, 0) - a.at(2, 2)) * 2.f;
        q.w = (a.at(0, 2) - a.at(2, 0)) / s; q.x = (a.at(0, 1) + a.at(1, 0)) / s; q.y = 0.25f * s; q.z = (a.at(1, 2) + a.at(2, 1)) / s;
    } else {
        float s = std::sqrt(1.f + a.at(2, 2) - a.at(0, 0) - a.at(1, 1)) * 2.f;
        q.w = (a.at(1, 0) - a.at(0, 1)) / s; q.x = (a.at(0, 2) + a.at(2, 0)) / s; q.y = (a.at(1, 2) + a.at(2, 1)) / s; q.z = 0.25f * s;
    }
    return qnormalize(q);
}
inline void decomposeTRS(const M4& a, V3& t, Quat& q, V3& s) {
    t = a.translation();
    s = {length(a.col3(0)), length(a.col3(1)), length(a.col3(2))};
    M4 r = M4::identity();
    for (int c = 0; c < 3; ++c) {
        float inv = 1.f / (c == 0 ? s.x : (c == 1 ? s.y : s.z));
        for (int rr = 0; rr < 3; ++rr) r.m[c * 4 + rr] = a.m[c * 4 + rr] * inv;
    }
    q = quatFromM3(r);
}
inline M4 inverse(const M4& mm) {
    const float* m = mm.m; M4 r; float* inv = r.m;
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-20f) return M4::identity();
    det = 1.f / det;
    for (int i = 0; i < 16; ++i) inv[i] *= det;
    return r;
}
inline M4 perspective(float fovyRad, float aspect, float zn, float zf) {
    M4 r; std::memset(r.m, 0, sizeof r.m);
    float f = 1.f / std::tan(fovyRad * 0.5f);
    r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (zf + zn) / (zn - zf); r.m[11] = -1.f; r.m[14] = 2.f * zf * zn / (zn - zf);
    return r;
}
inline M4 lookAt(const V3& eye, const V3& target, const V3& up) {
    V3 f = normalize(target - eye);
    V3 s = normalize(cross(f, up));
    if (length(s) < 1e-6f) s = normalize(cross(f, V3{0, 0, 1}));
    V3 u = cross(s, f);
    M4 r = M4::identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z;
    r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dot(s, eye); r.m[13] = -dot(u, eye); r.m[14] = dot(f, eye);
    return r;
}

// deterministic hash noise (camera sway, saccades, flicker)
inline float hash1(float n) { float s = std::sin(n) * 43758.5453123f; return s - std::floor(s); }
inline float vnoise1(float x) {
    float i = std::floor(x), f = x - i; float u = f * f * (3.f - 2.f * f);
    return lerpf(hash1(i), hash1(i + 1.f), u) * 2.f - 1.f;
}

}  // namespace ag
