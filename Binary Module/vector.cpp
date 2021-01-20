#include <math.h>
#include <algorithm>

#include "vector.h"

#pragma region Vector3

#pragma region Constructors
Vector3::Vector3()
{
	x = 0;
	y = 0;
	z = 0;
}
Vector3::Vector3(const Vector& original)
{
	x = original.x;
	y = original.y;
	z = original.z;
}
Vector3::Vector3(const Vector3& original)
{
	x = original.x;
	y = original.y;
	z = original.z;
}
Vector3::Vector3(const double xyz)
{
	x = xyz;
	y = xyz;
	z = xyz;
}
Vector3::Vector3(const double _x, const double _y, const double _z)
{
	x = _x;
	y = _y;
	z = _z;
}
#pragma endregion

#pragma region Methods
// Calculates cross product between this and a vector
Vector3 Vector3::cross(const Vector3& b) const
{
	return Vector3(
		y * b.z - z * b.y,
		z * b.x - x * b.z,
		x * b.y - y * b.x
	);
}

// Calculates dot product between this and a vector
double Vector3::dot(const Vector3& b) const
{
	return x * b.x + y * b.y + z * b.z;
}

// Returns this vector rotated by a rotation matrix
Vector3 Vector3::getRotated(const std::vector<std::vector<double>>& matrix) const
{
	return Vector3(
		x * matrix[0][0] + y * matrix[0][1] + z * matrix[0][2],
		x * matrix[1][0] + y * matrix[1][1] + z * matrix[1][2],
		x * matrix[2][0] + y * matrix[2][1] + z * matrix[2][2]
	);
}

// Returns a normalised version of the vector
Vector3 Vector3::getNormalised() const
{
	return *this / sqrt(x * x + y * y + z * z);
}

// Gets the distance between this vector and another
double Vector3::getDistance(const Vector3& b) const
{
	return sqrt((b.x - x) * (b.x - x) + (b.y - y) * (b.y - y) + (b.z - z) * (b.z - z));
}

// Lerps between two vectors
Vector3 Vector3::lerp(const Vector3& b, const double& t) const
{
	return Vector3(
		x + t * (b.x - x),
		y + t * (b.y - y),
		z + t * (b.z - z)
	);
}

void Vector3::clamp(const double min, const double max)
{
	x = std::clamp(x, min, max);
	y = std::clamp(y, min, max);
	z = std::clamp(z, min, max);
}
#pragma endregion

#pragma region Operators
Vector3 operator+(const Vector3& a, const Vector3& b) { return Vector3(a.x + b.x, a.y + b.y, a.z + b.z); }
Vector3 operator-(const Vector3& a, const Vector3& b) { return Vector3(a.x - b.x, a.y - b.y, a.z - b.z); }
Vector3 operator/(const Vector3& a, const Vector3& b) { return Vector3(a.x / b.x, a.y / b.y, a.z / b.z); }
Vector3 operator*(const Vector3& a, const Vector3& b) { return Vector3(a.x * b.x, a.y * b.y, a.z * b.z); }

Vector3& operator+=(Vector3& a, const Vector3& b)
{
	a.x += b.x;
	a.y += b.y;
	a.z += b.z;
	return a;
}
Vector3& operator-=(Vector3& a, const Vector3& b)
{
	a.x -= b.x;
	a.y -= b.y;
	a.z -= b.z;
	return a;
}
Vector3& operator/=(Vector3& a, const Vector3& b)
{
	a.x /= b.x;
	a.y /= b.y;
	a.z /= b.z;
	return a;
}
Vector3& operator*=(Vector3& a, const Vector3& b)
{
	a.x *= b.x;
	a.y *= b.y;
	a.z *= b.z;
	return a;
}

Vector3 Vector3::operator-() const
{
	return Vector3(-x, -y, -z);
}
#pragma endregion

#pragma endregion