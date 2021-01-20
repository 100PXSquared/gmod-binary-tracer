#pragma once

#include "vector.h"
#include "entity.h"

Vector3 TransformToWorld(const float& x, const float& y, const float& z, const Vector3& normal);
Vector3 fresnelApprox(const double& cosT, const Vector3& fColour);

class LambertBRDF
{
public:
	LambertBRDF();
	Vector3 sample(const Vector3& outputDirection, const Vector3& normal, const double& r1, const double& r2) const;
	double pdf(const Vector3& inputDirection, const Vector3& normal) const;
	Vector3 evaluate(const Vector3& albedo, const Vector3& inputDirection, const Vector3& normal) const;
};

class CookTorranceBRDF
{
private:
	Vector3 reflect(const Vector3& I, const Vector3& N) const;
	double thetaFromVec(const Vector3& vec) const;
	double chi(double val) const;
	double distribution(const Vector3& m, const Vector3& n, const double& alpha) const;
	double geometry(const Vector3& v, const Vector3& m, const Vector3& n, const double& alpha) const;

public:
	CookTorranceBRDF();

	Vector3 sample(const Vector3& outputDirection, const Vector3& normal, const double& r1, const double& r2, const Material* mat, Vector3& m) const;
	double pdf(const Vector3& inputDirection, const Vector3& m, const Vector3& n, const Material* mat) const;
	Vector3 evaluate(
		const Vector3& albedo,
		const Vector3& inputDirection, const Vector3& outputDirection, const Vector3& normal, const Vector3& microfacetNormal, const Material* mat
	) const;
};