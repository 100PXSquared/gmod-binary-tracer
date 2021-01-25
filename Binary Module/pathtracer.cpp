#include "pathtracer.h"
#include "shaders.h"

using namespace Image;
using namespace Vistrace;
using namespace std::chrono_literals;

Camera::Camera(Vector3 _pos, const int _res[2], double _viewplaneDist, double _fov)
{
	pos = _pos;
	resX = _res[0];
	resY = _res[1];
	viewplaneDist = _viewplaneDist;
	fov = _fov;
	ang = {
		{1, 0, 0},
		{0, 1, 0},
		{0, 0, 1}
	};
	samples = 64;

	hdriBrightness = 0.005;
	viewportHDRIBrightness = 0.05;
}

void Camera::calcViewplane(const int& x, const int& y, Vector3& origin, Vector3& direction) const
{
	double coeff = viewplaneDist * tan((fov / 2) * (M_PI / 180)) * 2;
	origin = Vector3(
		viewplaneDist,
		(static_cast<double>(resX - x) / static_cast<double>(resX - 1) - 0.5) * coeff,
		(coeff / static_cast<double>(resX)) * static_cast<double>(resY - y) - 0.5 * (coeff / static_cast<double>(resX)) * static_cast<double>(resY - 1)
	).getRotated(ang);
	direction = origin.getNormalised();
	origin += pos;
}

Vector3 getPixel(const double& u, const double& v, const std::string& texture, const std::map<std::string, PPM>& textures)
{
	if (textures.find(texture) == textures.end()) {
		int x = (int)floor((u - floor(u)) * textures.at("missingTexture").width);
		int y = (int)floor((v - floor(v)) * textures.at("missingTexture").height);
		return textures.at("missingTexture").getPixel(x, y);
	}

	int x = (int)floor((u - floor(u)) * textures.at(texture).width);
	int y = (int)floor((v - floor(v)) * textures.at(texture).height);
	return textures.at(texture).getPixel(x, y);
}

#pragma region Utility
double sign(const double& num)
{
	if (num < 0.) return -1.;
	return 1.;
}

Vector3 uniformSampleHemisphere(const double& r1, const double& r2)
{
	double sinTheta = sqrt(1 - r1 * r1);
	double phi = 2 * M_PI * r2;
	return Vector3(sinTheta * cos(phi), r1, sinTheta * sin(phi));
}

void createCoordinateSystem(const Vector3& N, Vector3& Nt, Vector3& Nb)
{
	if (abs(N.x) > abs(N.y))
		Nt = Vector3(N.z, 0, -N.x) / sqrt(N.x * N.x + N.z * N.z);
	else
		Nt = Vector3(0, -N.z, N.y) / sqrt(N.y * N.y + N.z * N.z);
	Nb = N.cross(Nt);
}

Vector3 refract(const Vector3& I, const Vector3& N, const double& ior)
{
	double cosi = std::clamp(I.dot(N), -1., 1.);
	double etai = 1, etat = ior;
	Vector3 n = N;

	if (cosi < 0) {
		cosi = -cosi;
	} else {
		std::swap(etai, etat);
		n = -N;
	}

	double eta = etai / etat;
	double k = 1 - eta * eta * (1 - cosi * cosi);
	return k < 0 ? 0 : eta * I + (eta * cosi - sqrt(k)) * n;
}

Vector3 fresnel(const Vector3& v, const Vector3& h, const Vector3& f0)
{
	return f0 + (1. - f0) * pow((1. - v.dot(h)), 5);
}

double getPitch(const Vector3& N)
{
	if (N.x == 0 && N.y == 0) return PI / 2 * sign(N.z);
	return asin(abs(N.z)) * sign(N.z);
}

double getYaw(const Vector3& N)
{
	if (N.y == 0) return N.x >= 0 ? 0 : PI;
	return acos((N * Vector3(1, 1, 0)).getNormalised().dot(Vector3(1, 0, 0))) * sign(N.y);
}

Vector3 getHDRIFromVec(const Vector3& v, const double brightness, const PFM* img)
{
	if (img == nullptr) return Vector3(0.f);

	double x = img->width - (1 + getYaw(v) / M_PI) * img->width / 2;
	double y = (1 - getPitch(v) * M_2_PI) * img->height / 2;

	x -= img->width * floor(x / img->width);
	y -= img->height * floor(y / img->height);

	Vector3 colour = img->getPixel(floor(x), floor(y));
	colour *= colour * brightness;
	return colour;
}
#pragma endregion

std::default_random_engine engine;
std::uniform_real_distribution<double> distribution(0, 1);

// Right now just use a static shader (eventually there will be multiple types, for now though, just lambertian)
LambertBRDF lambert = LambertBRDF();
CookTorranceBRDF cook = CookTorranceBRDF();

Vector3 tracePath(
	const Vector3& origin, const Vector3& direction,
	const int& depth, const int& samples,
	const std::vector<Entity*>& ents, const std::vector<Light*>& lights, const std::map<std::string, PPM>& textures,
	const PFM* hdri, const double& hdriBrightness, const double& viewHDRIBrightness,
	Vector3& albedo, std::pair<double, unsigned int>& depthAndObjID, Vector3& normal,
	Vector3& bounceAlbedo,
	const bool& bCameraRay
)
{
	if (depth > MAXDEPTH) return Vector3(0);

	HitData hit;
	if (!trace(origin, direction, hit, ents, false, textures))
		return bounceAlbedo * getHDRIFromVec(direction, bCameraRay ? viewHDRIBrightness : hdriBrightness, hdri);

	Vector3 biased = hit.pos + hit.normal * BIAS;

	Vector3 brdfColour = Vector3(0.);
	Vector3 transmission = Vector3(0.);
	//double kr = 0.;//fresnel(direction, hit.normal, hit.mat->ior); TRANSMISSION DISABLED TEMPORARILY

	Vector3 diffuseAlbedo = getPixel(hit.u, hit.v, hit.mat->texture, textures) * hit.mat->colour;

	if (hit.mat->alpha != 0.) {
		// Direct lighting
		Vector3 directLighting = Vector3(0.);
		int count = 0;
		for (int i = 0; i < lights.size(); i++) {
			Vector3 lightDir, lightIntensity;
			double dist;
			lights[i]->illuminate(hit.pos, lightDir, lightIntensity, dist);

			HitData hitShadow;
			bool vis = !trace(biased, -lightDir, hitShadow, ents, true);
			if (!vis && hitShadow.distance > dist) vis = true;
			directLighting += vis * lightIntensity * std::max(0., hit.normal.dot(-lightDir));
			count++;
		}
		directLighting += hit.mat->emission;

		// Indirect lighting
		Vector3 indirectLighting = Vector3(0);

#ifdef GI
		if (depth + 1 <= MAXDEPTH) {
			Vector3 outDir = -direction;

			// Compute f0 for fresnel
			Vector3 f0 = abs((1.0 - hit.mat->ior) / (1.0 + hit.mat->ior));
			f0 *= f0;
			f0 = f0.lerp(hit.mat->colour, hit.mat->metalness);

			for (int n = 0; n < samples; n++) {
				Vector3 bounceColour = bounceAlbedo;

				// russian roulette diffuse or specular using roughness
				if (distribution(engine) < hit.mat->roughness) {
					Vector3 sample = lambert.sample(outDir, hit.normal, distribution(engine), distribution(engine));

					indirectLighting += lambert.evaluate(tracePath(
						biased, sample,
						depth + 1, 1,
						ents, lights, textures,
						hdri, hdriBrightness, viewHDRIBrightness,
						albedo, depthAndObjID, normal, bounceColour
					), sample, hit.normal) / lambert.pdf(sample, hit.normal);
				} else {
					Vector3 m;
					Vector3 sample = cook.sample(outDir, hit.normal, distribution(engine), distribution(engine), hit.mat, m);

					indirectLighting += fresnel(outDir, m, f0) * cook.evaluate(tracePath(
						biased, sample,
						depth + 1, samples,
						ents, lights, textures,
						hdri, hdriBrightness, viewHDRIBrightness,
						albedo, depthAndObjID, normal, bounceColour
					), sample, outDir, hit.normal, m, hit.mat) / cook.pdf(sample, m, hit.normal, hit.mat);
				}
			}

			// Calculate final colour and average fresnel
			indirectLighting /= samples;
		}
#endif
		bounceAlbedo *= diffuseAlbedo;
		brdfColour = (directLighting / M_PI + 2. * indirectLighting) * bounceAlbedo;
	}
	// TRANSMISSION IS TEMPORARILY DISABLED UNTIL I GET A METHOD THAT WORKS BETTER WITH COLOURED FRESNEL
	/*if (hit.mat->alpha != 1.) {
		if (kr < 1.) { // Not total internal reflection
			Vector3 transmitBounceAlbedo = Vector3(1.);
			transmission = tracePath(
				(direction.dot(hit.normal) < 0.) ? biased - 2 * BIAS * hit.normal : biased,
				refract(direction, hit.normal, hit.mat->ior),
				depth, samples,
				ents, lights, textures, hdri, hdriBrightness, viewHDRIBrightness,
				albedo, depthAndObjID, normal, transmitBounceAlbedo
			);
		}
	}*/

	// if this is a camera ray, output to the denoiser channels
	if (bCameraRay) {
		albedo = diffuseAlbedo;
		depthAndObjID = std::pair<double, unsigned int>(hit.distance, hit.ent);
		normal = hit.normal;
	}

	Vector3 colour = brdfColour * hit.mat->alpha + transmission * (1. - hit.mat->alpha);
	return colour;
}

void renderScene(
	const Camera& cam, const std::vector<Entity*>& ents, const std::vector<Light*>& lights,
	std::vector<std::vector<Vector3>>& out,
	std::mutex& m, int& nextCol, int& finishedPixels,
	const PFM* hdri,
	std::vector<std::vector<Vector3>>& albedo, std::vector<std::vector<std::pair<double, unsigned int>>>& depthAndObjID, std::vector<std::vector<Vector3>>& normal
)
{
	// Build texture cache
	std::map<std::string, PPM> textures;

	PPM missingTexture((TEXTURE_PATH + "missingtexture.ppm").c_str());
	if (!missingTexture.read()) {
		return;
	}
	textures.insert(std::pair<std::string, PPM>("missingTexture", missingTexture));

	for (int i = 0; i < ents.size(); i++) {
		for (int j = 0; j < ents[i]->meshes.size(); j++) {
			if (textures.find(ents[i]->meshes[j].material.texture) != textures.end()) continue;

			PPM texture((TEXTURE_PATH + ents[i]->meshes[j].material.texture).c_str());
			if (texture.read()) {
				textures.insert(std::pair<std::string, PPM>(ents[i]->meshes[j].material.texture, texture));
			}

			if (
				ents[i]->meshes[j].material.normalTexture == "" || textures.find(ents[i]->meshes[j].material.normalTexture) != textures.end()
			) continue;

			PPM textureNorm((TEXTURE_PATH + ents[i]->meshes[j].material.normalTexture).c_str());
			if (textureNorm.read()) {
				textures.insert(std::pair<std::string, PPM>(ents[i]->meshes[j].material.normalTexture, textureNorm));
			}
		}
	}

	int curPixels = 0;
	while (true) {
		m.lock();
		if (nextCol < 1) {
			m.unlock();
			break;
		}

		finishedPixels += curPixels;
		curPixels = 0;

		int x = --nextCol;
		m.unlock();

		for (int y = 0; y < cam.resY; y++) {
			Vector3 albedoP, normalP;
			std::pair<double, unsigned int> depthAndObjIDP = std::pair<double, unsigned int>(MAX_DOUBLE, 0);
			Vector3 origin, direction;
			cam.calcViewplane(x + 1, y + 1, origin, direction);

			Vector3 bounceAlbedo = Vector3(1.);
			Vector3 colour = tracePath(
				origin, direction,
				0,
				cam.samples,
				ents, lights, textures,
				hdri, cam.hdriBrightness, cam.viewportHDRIBrightness,
				albedoP, depthAndObjIDP, normalP,
				bounceAlbedo, true
			);

			out[x][y] = Vector3(
				std::min(sqrt(colour.x), 1.),
				std::min(sqrt(colour.y), 1.),
				std::min(sqrt(colour.z), 1.)
			);

			albedo[x][y] = albedoP;
			depthAndObjID[x][y] = depthAndObjIDP;
			normal[x][y] = normalP;

			curPixels++;
		}
	}
}

void denoise(
	std::vector<std::vector<Vector3>>& out,
	const std::vector<std::vector<Vector3>>& scene,
	const std::vector<std::vector<Vector3>>& albedo,
	const std::vector<std::vector<Vector3>>& normal,
	const std::vector<std::vector<std::pair<double, unsigned int>>>& depthAndOBJ,
	std::mutex& m, int& nextCol, int& finishedPixels,
	const int& kernelSize, const double& normalThresh, const double& albedoThresh, const double& depthThresh
)
{
	unsigned int width = scene.size(), height = scene[0].size();
	int curPixels = 0;
	while (true) {
		m.lock();
		if (nextCol < 1) {
			m.unlock();
			break;
		}

		finishedPixels += curPixels;
		curPixels = 0;

		int x = --nextCol;
		m.unlock();

		for (int y = 0; y < height; y++) {
			std::vector<Vector3> valid = { scene[x][y] };
			for (int xK = x - kernelSize; xK <= x + kernelSize; xK++) {
				for (int yK = y - kernelSize; yK <= y + kernelSize; yK++) {
					if (xK < 0 || yK < 0 || xK >= width || yK >= height) continue;

					// check obj id matches
					if (depthAndOBJ[xK][yK].second != depthAndOBJ[x][y].second) continue;

					// check depth is within threshold
					if (abs(depthAndOBJ[xK][yK].first - depthAndOBJ[x][y].first) > depthThresh) continue;

					// check albedo is within threshold
					if (albedo[xK][yK].getDistance(albedo[x][y]) > albedoThresh) continue;

					// check dot of normals is positive
					double dot = normal[xK][yK].dot(normal[x][y]);
					if (dot < 0.) continue;

					// check angle between normals is within threshold
					if (acos(dot) > normalThresh) continue;

					valid.push_back(scene[xK][yK]);
				}
			}

			Vector3 sum = Vector3(0.);
			for (int i = 0; i < valid.size(); i++) sum += valid[i];
			out[x][y] = sum / valid.size();
			curPixels++;
		}
	}
}