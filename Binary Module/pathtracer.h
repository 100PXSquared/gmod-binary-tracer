#pragma once

#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <random>
#include <map>
#include <time.h>
#include <mutex>

#include "image.h"
#include "vistrace.h"
#include "vector.h"

#include "light.h"
#include "constants.h"

#define GI

constexpr double BIAS = 0.001;

constexpr int MAXDEPTH = 5;
constexpr int MAXREFLECT = 5;
constexpr int MAXTRANSMIT = 5;

const std::string TEXTURE_PATH = "D:\\Programming\\GMod\\raytracer_materials\\";

class Camera
{
public:
    int resX;
    int resY;
	double viewplaneDist;
	double fov;
	Vector3 pos;
	std::vector<std::vector<double>> ang;
	int samples;
	double hdriBrightness;
	double viewportHDRIBrightness;

    Camera(Vector3 _pos, const int _res[2], double _viewplaneDist, double _fov);

	void calcViewplane(const int&, const int&, Vector3&, Vector3&) const;
};

Vector3 getPixel(const double& u, const double& v, const std::string& texture, const std::map<std::string, Image::PPM>& textures);

double sign(const double& num);
Vector3 uniformSampleHemisphere(const double& r1, const double& r2);
void createCoordinateSystem(const Vector3& N, Vector3& Nt, Vector3& Nb);
Vector3 refract(const Vector3& I, const Vector3& N, const double& ior);
double fresnel(const Vector3& v, const Vector3& h, const double& f0);
double getPitch(const Vector3& N);
double getYaw(const Vector3& N);
Vector3 getHDRIFromVec(const Vector3& v, const double brightness, const Image::PFM* img = nullptr);

Vector3 tracePath(
	const Vector3& origin, const Vector3& direction,
	const int& depth, const int& samples,
	const std::vector<Entity*>& ents, const std::vector<Light*>& lights, const std::map<std::string, Image::PPM>& textures,
	const Image::PFM* hdri, const double& hdriBrightness, const double& viewHDRIBrightness,
	Vector3& albedo, std::pair<double, unsigned int>& depthAndObjID, Vector3& normal,
	Vector3& bounceAlbedo,
	const bool& bCameraRay = false
);
void renderScene(
	const Camera& cam, const std::vector<Entity*>& ents, const std::vector<Light*>& lights,
	std::vector<std::vector<Vector3>>& out,
	std::mutex& m, int& nextCol, int& finishedPixels,
	const Image::PFM* hdri,
	std::vector<std::vector<Vector3>>& albedo, std::vector<std::vector<std::pair<double, unsigned int>>>& depthAndObjID, std::vector<std::vector<Vector3>>& normal
);

void denoise(
	std::vector<std::vector<Vector3>>& out,
	const std::vector<std::vector<Vector3>>& scene,
	const std::vector<std::vector<Vector3>>& albedo,
	const std::vector<std::vector<Vector3>>& normal,
	const std::vector<std::vector<std::pair<double, unsigned int>>>& depthAndOBJ,
	std::mutex& m, int& nextCol, int& finishedPixels,
	const int& kernelSize, const double& normalThresh, const double& albedoThresh, const double& depthThresh
);