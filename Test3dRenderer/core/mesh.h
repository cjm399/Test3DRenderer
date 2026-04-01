#pragma once

#include <vector> //TODO(chris): Replace this with your own version.
#include "../platform/platform.h" 
#include "sp_intrinsics.h"

struct Mesh
{
	std::vector<Vector3> positions;
	std::vector<Triangle> triangles; //TODO:Delete and swap to positions and indicies only
	std::vector<uint32_t> indicies;
};