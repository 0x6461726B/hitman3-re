#pragma once
#include <cmath>

struct Vector {
    float X{}, Y{}, Z{}, W{};

    Vector() = default;

    Vector(float x, float y, float z) : X(x), Y(y), Z(z), W(0.f) {}

    Vector(float x, float y, float z, float w) : X(x), Y(y), Z(z), W(w) {}

    Vector operator+(const Vector &a) const { return Vector(X + a.X, Y + a.Y, Z + a.Z); }
    Vector operator+(float a) const { return Vector(X + a, Y + a, Z + a); }
    Vector operator-(const Vector &a) const { return Vector(X - a.X, Y - a.Y, Z - a.Z); }
    Vector operator-(float a) const { return Vector(X - a, Y - a, Z - a); }
    Vector operator*(const Vector &a) const { return Vector(X * a.X, Y * a.Y, Z * a.Z); }
    Vector operator*(float a) const { return Vector(X * a, Y * a, Z * a); }
    Vector operator/(const Vector &a) const { return Vector(X / a.X, Y / a.Y, Z / a.Z); }
    Vector operator/(float a) const { return Vector(X / a, Y / a, Z / a); }

    float dot(const Vector &v) const { return X * v.X + Y * v.Y + Z * v.Z; }
    float lengthSquared() const { return X * X + Y * Y + Z * Z; }
    float length() const { return sqrtf(lengthSquared()); }
    float distTo(const Vector &a) const { return Vector(X - a.X, Y - a.Y, Z - a.Z).length(); }

    Vector normalize() const {
        float len = length();
        if (len == 0.f)
            return *this;
        float inv = 1.f / len;
        return Vector(X * inv, Y * inv, Z * inv);
    }

    Vector limit(float maxLen) const {
        float len = length();
        if (len == 0.f)
            return *this;
        float inv = maxLen / len;
        return Vector(X * inv, Y * inv, Z * inv);
    }

    Vector projectOnto(const Vector &other) const {
        float denom = other.dot(other);
        if (denom == 0.f)
            return Vector(0, 0, 0);
        return other * (dot(other) / denom);
    }

    Vector perpendicularTo() const { return Vector(Z, Y, -X); }

    static Vector pointAlongLine(const Vector &start, const Vector &dir, float range) {
        Vector n = dir.normalize();
        return Vector(start.X + n.X * range, start.Y + n.Y * range, start.Z + n.Z * range);
    }
};