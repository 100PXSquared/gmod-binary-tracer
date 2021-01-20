#pragma once

#include "vector.h"
#include "constants.h"

class Light
{
public:
	Light(const Vector3& t, const Vector3& c = 1, const double& i = 1);
	virtual ~Light() {};
	virtual void illuminate(const Vector3& P, Vector3&, Vector3&, double&) const = 0;
	Vector3 colour;
	float intensity;
	Vector3 transform;
};

class DistantLight : public Light
{
	Vector3 dir;
public:
	DistantLight(const Vector3& t, const Vector3& c = 1, const double& i = 1);
	void illuminate(const Vector3& P, Vector3& lightDir, Vector3& lightIntensity, double& distance) const;
};

class PointLight : public Light
{
	Vector3 pos;
public:
	PointLight(const Vector3& position, const Vector3& colour = 1, const double& intensity = 1);
	// P: is the shaded point
	void illuminate(const Vector3& P, Vector3& lightDir, Vector3& lightIntensity, double& distance) const;
};