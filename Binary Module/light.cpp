#include "light.h"

Light::Light(const Vector3& t, const Vector3& c, const double& i) : transform(t), colour(c), intensity(i) {}

DistantLight::DistantLight(const Vector3& t, const Vector3& c, const double& i) : Light(t, c, i)
{
   dir = t.getNormalised();
}
void DistantLight::illuminate(const Vector3& P, Vector3& lightDir, Vector3& lightIntensity, double& distance) const
{
	lightDir = dir;
	lightIntensity = colour * intensity;
	distance = std::numeric_limits<double>().max();
}

PointLight::PointLight(const Vector3& position, const Vector3& colour, const double& intensity) : Light(position, colour, intensity)
{
	pos = position;
}
void PointLight::illuminate(const Vector3& P, Vector3& lightDir, Vector3& lightIntensity, double& distance) const
{
	lightDir = (P - pos);
	double r2 = lightDir.x * lightDir.x + lightDir.y * lightDir.y + lightDir.z * lightDir.z;
	distance = sqrt(r2);
	lightDir /= distance;

	// avoid division by 0
	if (r2 == 0) r2 = EPSILON;
	lightIntensity = colour * intensity / (4 * PI * distance);
}