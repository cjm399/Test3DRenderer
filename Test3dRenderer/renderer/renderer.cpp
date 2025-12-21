#include "renderer.h"

void PushClear(RenderCommandBuffer* _buffer, uint32_t _color)
{
	RenderCommandClear* cmd = (RenderCommandClear*)(_buffer->data + _buffer->size);
	cmd->header.type = RenderCommand_Clear;
	cmd->color = _color;

	_buffer->size += sizeof(*cmd); // TODO: each struct should know how big they are from the get-go.
	//NOTE(chris): Do I want them different sizes?
}

void PushMesh(RenderCommandBuffer* _buffer, Mesh* _mesh, mat4x4 _model)
{
	RenderCommandMesh* cmd = (RenderCommandMesh*)(_buffer->data + _buffer->size);
	cmd->header.type = RenderCommand_Mesh;
	cmd->mesh = _mesh;
	cmd->model = _model;

	_buffer->size += sizeof(*cmd);
}

void Init(Renderer* _renderContext)
{
	_renderContext->cameraPosition = Vector3{ 0, 0, -8 };
	_renderContext->preAllocatedPairs = new Pair * [3];
	for (int i = 0; i < 3; ++i)
	{
		_renderContext->preAllocatedPairs[i] = new Pair();
	}
}

void Renderer_Execute(
	Renderer* _renderer,
	RenderCommandBuffer* _buffer)
{
	uint8_t* loc = _buffer->data;

	_renderer->view.m[0][0] = 1.0f;
	_renderer->view.m[1][1] = 1.0f;
	_renderer->view.m[2][2] = 1.0f;
	_renderer->view.m[3][3] = 1.0f;
	_renderer->view.m[3][0] = -_renderer->cameraPosition.x;
	_renderer->view.m[3][1] = -_renderer->cameraPosition.y;
	_renderer->view.m[3][2] = -_renderer->cameraPosition.z;

	while (loc < _buffer->data + _buffer->size)
	{
		RenderCommandHeader* header = (RenderCommandHeader*)loc;

		switch (header->type)
		{
		case RenderCommand_Clear:
		{
			RenderCommandClear* cmd = (RenderCommandClear*)loc;
			FillScreen(_renderer, cmd->color);
			loc += sizeof(*cmd);
		} break;
		case RenderCommand_Mesh:
		{
			RenderCommandMesh* cmd = (RenderCommandMesh*)loc;
			DrawMesh(_renderer, cmd->mesh, cmd->model);
			loc += sizeof(*cmd);
		} break;
		}
	}
}

void DrawMesh(Renderer* _renderContext, Mesh* _mesh, mat4x4 _model)
{

	for (int i = 0; i < _mesh->triangles.size(); ++i)
	{
		DrawTriangleFromMesh(_renderContext, _mesh->triangles[i], _model);
	}
}

void DrawTriangleFromMesh(
	Renderer* renderContext,
	Triangle& tri,
	mat4x4& model
)
{
	Triangle triTranslated, triView, triProjected;

	//model -> world
	MultiplyMatrixVector(tri.points[0], triTranslated.points[0], model);
	MultiplyMatrixVector(tri.points[1], triTranslated.points[1], model);
	MultiplyMatrixVector(tri.points[2], triTranslated.points[2], model);

	// world -> view
	MultiplyMatrixVector(triTranslated.points[0], triView.points[0], renderContext->view);
	MultiplyMatrixVector(triTranslated.points[1], triView.points[1], renderContext->view);
	MultiplyMatrixVector(triTranslated.points[2], triView.points[2], renderContext->view);

	// view -> projection
	MultiplyMatrixVector(triView.points[0], triProjected.points[0], renderContext->proj);
	MultiplyMatrixVector(triView.points[1], triProjected.points[1], renderContext->proj);
	MultiplyMatrixVector(triView.points[2], triProjected.points[2], renderContext->proj);

	if (triView.points[0].z <= 0.1f ||
		triView.points[1].z <= 0.1f ||
		triView.points[2].z <= 0.1f)
	{
		return;
	}

	Vector3 line1 = {
		triView.points[1].x - triView.points[0].x,
		triView.points[1].y - triView.points[0].y,
		triView.points[1].z - triView.points[0].z
	};

	Vector3 line2 = {
		triView.points[2].x - triView.points[0].x,
		triView.points[2].y - triView.points[0].y,
		triView.points[2].z - triView.points[0].z
	};

	Vector3 normal = {
		line1.y * line2.z - line1.z * line2.y,
		line1.z * line2.x - line1.x * line2.z,
		line1.x * line2.y - line1.y * line2.x
	};

	float len = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
	normal.x /= len;
	normal.y /= len;
	normal.z /= len;

	if (normal.z >= 0.0f)
	{
		return;
	}

	MultiplyMatrixVector(triView.points[0], triProjected.points[0], renderContext->proj);
	MultiplyMatrixVector(triView.points[1], triProjected.points[1], renderContext->proj);
	MultiplyMatrixVector(triView.points[2], triProjected.points[2], renderContext->proj);

	int triColor = 0xFF0000;


	for (int i = 0; i < 3; i++)
	{
		triProjected.points[i].x = (triProjected.points[i].x + 1.0f) * 0.5f * renderContext->backbuffer->width;
		triProjected.points[i].y = (triProjected.points[i].y + 1.0f) * 0.5f * renderContext->backbuffer->height;
	}

	Pair* p1 = renderContext->preAllocatedPairs[0];
	Pair* p2 = renderContext->preAllocatedPairs[1];
	Pair* p3 = renderContext->preAllocatedPairs[2];

	p1->x = (int)triProjected.points[0].x;
	p1->y = (int)triProjected.points[0].y;
	p2->x = (int)triProjected.points[1].x;
	p2->y = (int)triProjected.points[1].y;
	p3->x = (int)triProjected.points[2].x;
	p3->y = (int)triProjected.points[2].y;

	DrawTriangle(renderContext, p1, p2, p3, triColor);
}

void FillScreen(Renderer* _renderContext, uint32_t color)
{
	uint8_t* row = (uint8_t*)_renderContext->backbuffer->memory;
	for (int Y = 0; Y < _renderContext->backbuffer->height; Y++)
	{
		uint32_t* pixel = (uint32_t*)row;
		for (int X = 0; X < _renderContext->backbuffer->width; X++)
		{
			*pixel++ = color;
		}
		row += _renderContext->backbuffer->pitch;
	}
}

void DrawLine(Renderer* renderContext, Pair* p0, Pair* p1, uint32_t color)
{
	int x0 = p0->x, y0 = p0->y;
	int x1 = p1->x, y1 = p1->y;

	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	uint8_t* row = (uint8_t*)renderContext->backbuffer->memory;
	int bytesPerPixel = sizeof(uint32_t);

	while (true)
	{
		// Clamp to screen bounds (skip if out-of-bounds)
		if (x0 >= 0 && x0 < renderContext->backbuffer->width && y0 >= 0 && y0 < renderContext->backbuffer->height)
		{
			int pixelIndex = y0 * renderContext->backbuffer->pitch + x0 * bytesPerPixel;
			uint32_t* pixelLocation = (uint32_t*)(row + pixelIndex);
			*pixelLocation = color;
		}

		if (x0 == x1 && y0 == y1)
			break;

		int e2 = 2 * err;
		if (e2 > -dy) { err -= dy; x0 += sx; }
		if (e2 < dx) { err += dx; y0 += sy; }
	}
}

inline void DrawTriangle(Renderer* renderContext, Pair* p1, Pair* p2, Pair* p3, uint32_t color)
{
	DrawLine(renderContext, p1, p2, color);
	DrawLine(renderContext, p2, p3, color);
	DrawLine(renderContext, p3, p1, color);
}
