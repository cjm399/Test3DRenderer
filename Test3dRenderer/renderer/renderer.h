#pragma once

#include "render_commands.h"
#include "../core/types.h"


struct backbuffer
{
	void* memory;
	int width;
	int height;
	int pitch;
};

struct Renderer {
	backbuffer* backbuffer;
	mat4x4 view;
	mat4x4 proj;
	Vector3 cameraPosition;
	Pair** preAllocatedPairs;
};

void Init(Renderer* _renderContext);

void PushClear(RenderCommandBuffer* _buffer, uint32_t _color);

void PushMesh(RenderCommandBuffer* _buffer, Mesh* _mesh, mat4x4 _model);

void Renderer_Execute(
	Renderer* _renderer,
	RenderCommandBuffer* _buffer);

void DrawMesh(Renderer* _renderContext, Mesh* _mesh, mat4x4 _model);

void DrawTriangleFromMesh(
	Renderer* renderContext,
	Triangle& tri,
	mat4x4& model
);

void FillScreen(Renderer* _renderContext, uint32_t color = 0x000000);

void DrawLine(Renderer* renderContext, Pair* p0, Pair* p1, uint32_t color);

inline void DrawTriangle(Renderer* renderContext, Pair* p1, Pair* p2, Pair* p3, uint32_t color);
