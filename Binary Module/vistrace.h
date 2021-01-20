#pragma once

#include <string>
#include <map>

#include "GarrysMod/Lua/Interface.h"
#include "entity.h"
#include "vector.h"
#include "image.h"
#include "constants.h"

namespace Vistrace
{

	struct HitData
	{
		Vector3 pos;
		Vector3 normal;
		double distance;
		double u = 0;
		double v = 0;
		const Material* mat;
		unsigned int ent;
	};

	bool aabbIntersect(const AABB& bounds, const Vector3& m, const Vector3& n);
	bool traceEnt(const Vector3& origin, const Vector3& direction, HitData& out, const Entity* ent, double& tMin, double& uMin, double& vMin, int hitVert[3]);
	bool trace(const Vector3& origin, const Vector3& direction, HitData& out, const std::vector<Entity*>& entList, const bool& shadow = false, const std::map<std::string, Image::PPM>& textures = {});
}