#pragma once

#include "../core/core.h"
#include "../core/mesh.h"

enum RenderCommandType
{
	RenderCommand_Clear,
	RenderCommand_Mesh,
};

struct RenderCommandHeader
{
	RenderCommandType type;
};

struct RenderCommandClear {
	RenderCommandHeader header;
	uint32_t color;
};

struct RenderCommandMesh {
	RenderCommandHeader header;
	Mesh* mesh;
	mat4x4 model;
};

struct RenderCommandBuffer
{
	uint8_t* data;
	uint32_t size;
	uint32_t capaity;
};
