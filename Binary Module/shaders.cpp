#include "shaders.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

Vector3 TransformToWorld(const float& x, const float& y, const float& z, const Vector3& normal)
{
	// Find an axis that is not parallel to normal
	Vector3 majorAxis;
	if (abs(normal.x) < 0.57735026919f /* 1 / sqrt(3) */) {
		majorAxis = Vector3(1, 0, 0);
	} else if (abs(normal.y) < 0.57735026919f /* 1 / sqrt(3) */) {
		majorAxis = Vector3(0, 1, 0);
	} else {
		majorAxis = Vector3(0, 0, 1);
	}

	// Use majorAxis to create a coordinate system relative to world space
	Vector3 u = normal.cross(majorAxis).getNormalised();
	Vector3 v = normal.cross(u);
	Vector3 w = normal;

	// Transform from local coordinates to world coordinates
	return u * x + v * y + w * z;
}

Vector3 fresnelApprox(const double& cosT, const Vector3& fColour)
{
	return fColour + (1. - fColour) * pow(1 - cosT, 5);
}



#pragma region Lambert BRDF
LambertBRDF::LambertBRDF() {}

Vector3 LambertBRDF::sample(const Vector3& outputDirection, const Vector3& normal, const double& r1, const double& r2) const
{
	double r = sqrt(r1);
	double theta = r2 * 2. * M_PI;

	double x = r * cos(theta);
	double y = r * sin(theta);

	// Project z up to the unit hemisphere
	double z = sqrt(1.0f - x * x - y * y);

	return TransformToWorld(x, y, z, normal).getNormalised();
}

double LambertBRDF::pdf(const Vector3& inputDirection, const Vector3& normal) const
{
	return inputDirection.dot(normal) * M_1_PI;
}

Vector3 LambertBRDF::evaluate(const Vector3& albedo, const Vector3& inputDirection, const Vector3& normal) const
{
	return albedo * M_1_PI * inputDirection.dot(normal);
}
#pragma endregion

#pragma region Cook-Torrance BRDF
Vector3 CookTorranceBRDF::reflect(const Vector3& I, const Vector3& N) const
{
	return I - 2 * N.dot(I) * N;
}

double CookTorranceBRDF::thetaFromVec(const Vector3& vec) const
{
	return atan(sqrt(vec.x * vec.x + vec.y * vec.y) / vec.z);
}

double CookTorranceBRDF::chi(double val) const
{
	return val > 0 ? 1 : 0;
}

double CookTorranceBRDF::distribution(const Vector3& m, const Vector3& n, const double& alpha) const
{
	double a2 = alpha * alpha;
	double NdotM = n.dot(m);
	double NdotM2 = NdotM * NdotM;
	double tanT = tan(acos(m.dot(n)));
	return (chi(NdotM) / (M_PI * a2 * NdotM2 * NdotM2)) * exp(-(tanT * tanT) / a2);
}

double CookTorranceBRDF::geometry(const Vector3& v, const Vector3& m, const Vector3& n, const double& alpha) const
{
	double a = 1 / (alpha * tan(cos(v.dot(m))));
	double chiVal = chi(v.dot(m) / v.dot(n));

	if (chiVal == 0) return 0;
	if (a >= 1.6) return chiVal;

	return chiVal * (3.535 * a + 2.181 * a * a) / (1 + 2.276 * a + 2.577 * a * a);
}

CookTorranceBRDF::CookTorranceBRDF() {}

Vector3 CookTorranceBRDF::sample(const Vector3& outputDirection, const Vector3& normal, const double& r1, const double& r2, const Material* mat, Vector3& m) const
{
	double theta = atan(sqrt(-(mat->roughness * mat->roughness) * log(1 - r1)));
	double phiM = 2 * M_PI * r2;
	m = TransformToWorld(sin(theta) * cos(phiM), sin(theta) * sin(phiM), cos(theta), normal).getNormalised();
	return (2 * abs(outputDirection.dot(m)) * m - outputDirection).getNormalised();
}

double CookTorranceBRDF::pdf(const Vector3& inputDirection, const Vector3& m, const Vector3& n, const Material* mat) const
{
	return distribution(m, n, mat->roughness) * abs(m.dot(n)) / (4 * abs(inputDirection.dot(m)));
}

Vector3 CookTorranceBRDF::evaluate(
	const Vector3& albedo,
	const Vector3& inputDirection, const Vector3& outputDirection, const Vector3& normal, const Vector3& microfacetNormal, const Material* mat
) const
{
	double denominator = 4 * abs(outputDirection.dot(normal)) * abs(inputDirection.dot(normal));
	return albedo * geometry(outputDirection, microfacetNormal, normal, mat->roughness) * geometry(inputDirection, microfacetNormal, normal, mat->roughness) * distribution(microfacetNormal, normal, mat->roughness) / denominator;
}
#pragma endregion