#include <vector>
#include <cstdlib>
#include <cstring>
#include "obj_loader.h"

bool LoadMeshFromObjMemory(void* _memory, uint64_t _size, Mesh* _mesh)
{
	if (!_memory || _size == 0 || !_mesh)
		return false;

	std::vector<Vector3> verts;

	char* frameStart = (char*)_memory;
	char* bufferEnd = (char*)_memory + _size;

	while (frameStart < bufferEnd)
	{
		char line[256];
		char* frameEnd = frameStart;
		int index = 0;
		while (frameEnd < bufferEnd && *frameEnd != '\n' && *frameEnd != '\r' && index < (int)sizeof(line) - 1)
		{
			line[index++] = *frameEnd;
			++frameEnd;
		}
		line[index] = '\0';

		while (frameEnd < bufferEnd && (*frameEnd == '\n' || *frameEnd == '\r'))
			++frameEnd;
		frameStart = frameEnd;

		if (line[0] == 'v' && (line[1] == ' ' || line[1] == '\t'))
		{
			Vector3 v = {};
			if (sscanf_s(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3)
            {
                verts.push_back(v);
            }
		}
		else if (line[0] == 'f' && (line[1] == ' ' || line[1] == '\t'))
		{
            int f [3];
            if (sscanf_s(line + 2, "%d %d %d", &f[0], &f[1], &f[2]) == 3)
            {
                _mesh->triangles.push_back({
                    verts[f[0] - 1],
                    verts[f[1] - 1],
                    verts[f[2] - 1],
                });
            }
		}
	}
	return true;
}
