#pragma once

#include <string>
#include <vector>

#include "GarrysMod/Lua/Interface.h"
#include "vector.h"

// Material
struct Material
{
	std::string texture;
	std::string normalTexture;
	Vector3 colour;
	double alpha = 1;
	double roughness = 0;
	double ior = 1.5;
	Vector3 emission = Vector3(0);
	double metalness = 0;
};

// Axis aligned bounding box
struct AABB
{
	Vector3 min;
	Vector3 max;
};

// Implementation of the MeshVertex struct
struct MeshVertex
{
	Vector3 pos;
	Vector3 normal;
	double u, v;
	Vector3 tangent;
	Vector3 binormal;
};

// Mesh representation as a material path and vertices
struct Mesh
{
	Material material;
	std::vector<MeshVertex> verts;
};

// Entity class to represent game entities to trace
class Entity
{
public:
	Vector3 pos;
	std::vector<std::vector<double>> ang;
	std::vector<std::vector<double>> invAng;
	AABB bounds;
	std::vector<Mesh> meshes;
	int id;

	Entity(
		const Vector3 _pos,
		std::vector<std::vector<double>> _ang, std::vector<std::vector<double>> _invAng,
		const std::vector<Mesh> _meshes, const int _id
	);

	void calcAABB();
};