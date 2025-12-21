#pragma once

#include <vector> //TODO(chris): Replace this with your own version.
#include <fstream> //TODO(chris): Take this out.
#include <strstream> //TODO(chris): Take this out.
#include <iostream> //TODO(chris): Take this out.
#include "sp_intrinsics.h"

struct Mesh
{
	std::vector<Vector3> positions;
	std::vector<Triangle> triangles; //TODO:Delete and swap to positions and indicies only
	std::vector<uint32_t> indicies;

	//TODO(chris): Pull this out too.
	bool LoadMeshFromFile(std::string sFileName)
	{
		std::ifstream myFile(sFileName);
		if (!myFile.is_open())
		{
			return false;
		}

		std::vector<Vector3> verts;

		while (!myFile.eof())
		{
			char line[128];
			myFile.getline(line, 128);

			std::strstream s;
			s << line;
			char tempChar;

			if (line[0] == 'v')
			{
				Vector3 v;
				s >> tempChar >> v.x >> v.y >> v.z;
				verts.push_back(v);
			}

			if (line[0] == 'f')
			{
				int f[3];
				s >> tempChar >> f[0] >> f[1] >> f[2];
				triangles.push_back({
					verts[f[0] - 1],
					verts[f[1] - 1],
					verts[f[2] - 1]
					});
			}
		}

		return true;
	}
};