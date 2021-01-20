#include "entity.h"

// Computes the entity's AABB from its meshes
void Entity::calcAABB()
{
	bounds = AABB{ Vector3(0), Vector3(0) };

	for (int i = 0; i < meshes.size(); i++) {
		for (int j = 0; j < meshes[i].verts.size(); j++) {
			Vector3 vert = meshes[i].verts[j].pos.getRotated(ang);

			if (vert.x < bounds.min.x) { bounds.min.x = vert.x; }
			if (vert.y < bounds.min.y) { bounds.min.y = vert.y; }
			if (vert.z < bounds.min.z) { bounds.min.z = vert.z; }

			if (vert.x > bounds.max.x) { bounds.max.x = vert.x; }
			if (vert.y > bounds.max.y) { bounds.max.y = vert.y; }
			if (vert.z > bounds.max.z) { bounds.max.z = vert.z; }
		}
	}

	bounds.max += pos;
	bounds.min += pos;
}

Entity::Entity(
	const Vector3 _pos,
	std::vector<std::vector<double>> _ang, std::vector<std::vector<double>> _invAng,
	const std::vector<Mesh> _meshes, const int _id
) : pos(_pos), ang(_ang), invAng(_invAng), meshes(_meshes), id(_id) {}