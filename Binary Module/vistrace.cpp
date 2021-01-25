#include <stdlib.h>
#include <algorithm>
#include <iostream>

#include "vistrace.h"
#include "vector.h"

#define BACKFACE_CULLING

namespace Vistrace
{
	Vector3 getPixelNormal(const double& u, const double& v, const std::string& texture, const std::map<std::string, Image::PPM>& textures)
	{
		int x = (int)floor((u - floor(u)) * textures.at(texture).width);
		int y = (int)floor((v - floor(v)) * textures.at(texture).height);
		return textures.at(texture).getPixel(x, y) * 2 - 1;
	}

	// Determins whether a ray intersects an Axis-Alligned Bounding Box using precomputed m and n (see the shader toy code thing from that ray intersection website)
	bool aabbIntersect(const AABB& bounds, const Vector3& m, const Vector3& n)
	{
		Vector3 nLocal = n - m * (bounds.min + bounds.max) / 2;

		Vector3 k = Vector3(abs(m.x), abs(m.y), abs(m.z)) * (bounds.max - bounds.min) / 2;
		Vector3 t1 = -nLocal - k;
		Vector3 t2 = -nLocal + k;

		double tNear = std::max(std::max(t1.x, t1.y), t1.z);
		double tFar = std::min(std::min(t2.x, t2.y), t2.z);

		return !(tNear > tFar || tFar < 0);
	}

	// Moller Trumbore triangle intersection algorithm
	/*bool triIntersect(
		const Vector3& origin, const Vector3& direction,
		const Vector3& v0, const Vector3& v1, const Vector3& v2,
		double& t, double& u, double& v
	)
	{
		Vector3 v0v1 = v1 - v0;
		Vector3 v0v2 = v2 - v0;
		Vector3 pvec = direction.cross(v0v2);
		double det = v0v1.dot(pvec);

#ifdef BACKFACE_CULLING
		if (det > -EPSILON) return false;
#else
		if (fabs(det) < EPSILON) return false;
#endif

		double invDet = 1 / det;

		Vector3 tvec = origin - v0;
		u = tvec.dot(pvec) * invDet;
		if (u < 0 || u > 1) return false;

		Vector3 qvec = tvec.cross(v0v1);
		v = direction.dot(qvec) * invDet;
		if (v < 0 || u + v > 1) return false;

		t = v0v2.dot(qvec) * invDet;

		return true;
	}*/

	// Moller Trumbore triangle intersection algorithm
	bool triIntersect(
		const Vector3& origin, const Vector3& direction,
		const Vector3& v0, const Vector3& v1, const Vector3& v2,
		double& t, double& u, double& v
	)
	{
		const Vector3 edge1 = v1 - v0;
		const Vector3 edge2 = v2 - v0;

		const Vector3 h = direction.cross(edge2);
		const double a = edge1.dot(h);
		if (a > -EPSILON && a < EPSILON) return false;

		const double f = 1 / a;
		const Vector3 s = origin - v0;
		u = f * s.dot(h);
		if (u < 0 || u > 1) return false;

		const Vector3 q = s.cross(edge1);
		v = f * direction.dot(q);
		if (v < 0 || u + v > 1) return false;

		t = f * edge2.dot(q);
		if (t > EPSILON) return true;
		return false;
	}

	// Traces an entity
	bool traceEnt(const Vector3& origin, const Vector3& direction, HitData& out, const Entity* ent, double& tMin, double& uMin, double& vMin, int hitVert[3])
	{
		// localise ray to entity's transform
		Vector3 adjOrig = (origin - ent->pos).getRotated(ent->invAng);
		Vector3 adjDir = direction.getRotated(ent->invAng);

		bool hit = false;
		double t = MAX_DOUBLE, u, v;
		
		// iterate sub meshes
		for (int i = 0; i < ent->meshes.size(); i++) {
			const Mesh* mesh = &ent->meshes[i];

			// iterate tris
			for (int j = 0; j < mesh->verts.size(); j += 3) {
				if (triIntersect(
					adjOrig, adjDir,
					mesh->verts[j].pos,
					mesh->verts[j + 1].pos,
					mesh->verts[j + 2].pos,
					t, u, v
				) && t < tMin) {
					// assign tri hit if it's closest
					tMin = t;
					uMin = u;
					vMin = v;
					hitVert[1] = i;
					hitVert[2] = j;
					hit = true;
				}
			}
		}

		return hit;
	}

	// Performs a trace of an entity list
	bool trace(const Vector3& origin, const Vector3& direction, HitData& out, const std::vector<Entity*>& entList, const bool& shadow, const std::map<std::string, Image::PPM>& textures)
	{
		Vector3 m = 1 / direction;
		Vector3 n = m * origin;

		double t = MAX_DOUBLE, u, v;
		int hitVert[3] = {0, 0, 0};
		bool hit = false;

		for (int i = 0; i < entList.size(); i++) {
			if (aabbIntersect(entList[i]->bounds, m, n)) {
				if (traceEnt(origin, direction, out, entList[i], t, u, v, hitVert)) {
					hitVert[0] = i;
					hit = true;
				}
			}
		}

		if (!hit) return false;

		if (!shadow) {
			const MeshVertex v0 = entList[hitVert[0]]->meshes[hitVert[1]].verts[hitVert[2]];
			const MeshVertex v1 = entList[hitVert[0]]->meshes[hitVert[1]].verts[static_cast<size_t>(hitVert[2]) + 1];
			const MeshVertex v2 = entList[hitVert[0]]->meshes[hitVert[1]].verts[static_cast<size_t>(hitVert[2]) + 2];

			out.normal = (1 - u - v) * v0.normal + u * v1.normal + v * v2.normal;;
			out.u = (1 - u - v) * v0.u + u * v1.u + v * v2.u;
			out.v = (1 - u - v) * v0.v + u * v1.v + v * v2.v;

			if (
				entList[hitVert[0]]->meshes[hitVert[1]].material.normalTexture != "" &&
				textures.find(entList[hitVert[0]]->meshes[hitVert[1]].material.normalTexture) != textures.end()
			) {
				// compute the interpolated normal, binormal, and tangent
				Vector3 tangent = (1 - u - v) * v0.tangent + u * v1.tangent + v * v2.tangent;
				Vector3 binormal = (1 - u - v) * v0.binormal + u * v1.binormal + v * v2.binormal;

				// transform the tangent normal into model space
				out.normal = getPixelNormal(out.u, out.v, entList[hitVert[0]]->meshes[hitVert[1]].material.normalTexture, textures).getRotated({
					{tangent.x, binormal.x, out.normal.x},
					{tangent.y, binormal.y, out.normal.y},
					{tangent.z, binormal.z, out.normal.z}
				});
			}

			out.normal = out.normal.getRotated(entList[hitVert[0]]->ang).getNormalised();
			out.pos = ((1 - u - v) * v0.pos + u * v1.pos + v * v2.pos).getRotated(entList[hitVert[0]]->ang) + entList[hitVert[0]]->pos;
			out.mat = &entList[hitVert[0]]->meshes[hitVert[1]].material;
			out.ent = entList[hitVert[0]]->id;
		}
		
		out.distance = t;

		return true;
	}
}