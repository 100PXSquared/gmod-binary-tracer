#pragma once

#include <vector>

#include "GarrysMod/Lua/SourceCompat.h"

class Vector3
{
public:
	double x, y, z;

	// Constructors
	Vector3();
	Vector3(const Vector&);
	Vector3(const Vector3&);
	Vector3(const double);
	Vector3(const double, const double, const double);

	// Methods
	Vector3 cross(const Vector3&) const;
	double dot(const Vector3&) const;

	Vector3 getRotated(const std::vector<std::vector<double>>& matrix) const;
	Vector3 getNormalised() const;
	double getDistance(const Vector3&) const;
	Vector3 lerp(const Vector3& b, const double& t) const;

	void clamp(const double min, const double max);

	// Standard operators
	friend Vector3 operator+(const Vector3&, const Vector3&);
	friend Vector3 operator-(const Vector3&, const Vector3&);
	friend Vector3 operator/(const Vector3&, const Vector3&);
	friend Vector3 operator*(const Vector3&, const Vector3&);

	// Assignment operators
	friend Vector3& operator+=(Vector3&, const Vector3&);
	friend Vector3& operator-=(Vector3&, const Vector3&);
	friend Vector3& operator/=(Vector3&, const Vector3&);
	friend Vector3& operator*=(Vector3&, const Vector3&);

	// Unary operators
	Vector3 operator-() const;
};

Vector3 operator+(const Vector3&, const Vector3&);
Vector3 operator-(const Vector3&, const Vector3&);
Vector3 operator/(const Vector3&, const Vector3&);
Vector3 operator*(const Vector3&, const Vector3&);

Vector3& operator+=(Vector3&, const Vector3&);
Vector3& operator-=(Vector3&, const Vector3&);
Vector3& operator/=(Vector3&, const Vector3&);
Vector3& operator*=(Vector3&, const Vector3&);

class Quaternion
{
public:
	double x, y, z, w;

	Quaternion();
	Quaternion(const double _x, const double _y, const double _z, const double _w);

	friend Quaternion operator*(const Quaternion& a, const Quaternion& b);
};

Quaternion operator*(const Quaternion& a, const Quaternion& b);