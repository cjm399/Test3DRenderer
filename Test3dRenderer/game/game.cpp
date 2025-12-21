#include "game.h"
#include "../renderer/render_commands.h"

Renderer ScreenRenderer;
RenderCommandBuffer renderCommandBuffer;
Mesh testMesh;

void Init(platform_data& _platformData)
{
	renderCommandBuffer = RenderCommandBuffer{};
	renderCommandBuffer.data = (uint8_t*)malloc((1024 * 1024));
	renderCommandBuffer.capaity = 1024 * 1024;

	ScreenRenderer = Renderer{};
	ScreenRenderer.backbuffer = _platformData.backBuffer;
	Init(&ScreenRenderer);

	testMesh = Mesh();
	testMesh.LoadMeshFromFile("Astronaut2.obj");
}

void game_update_and_render(platform_data& _platformData)
{
	renderCommandBuffer.size = 0;
	if (_platformData.platformInput->keys[0x26])
	{
		ScreenRenderer.cameraPosition.z += 0.1f;
	}
	if (_platformData.platformInput->keys[0x28])
	{
		ScreenRenderer.cameraPosition.z -= 0.1f;
	}
	if (_platformData.platformInput->keys[0x25])
	{
		ScreenRenderer.cameraPosition.x -= 0.1f;
	}
	if (_platformData.platformInput->keys[0x27])
	{
		ScreenRenderer.cameraPosition.x += 0.1f;
	}
	if (_platformData.platformInput->keys['Q'])
	{
		ScreenRenderer.cameraPosition.y += 0.1f;
	}
	if (_platformData.platformInput->keys['E'])
	{
		ScreenRenderer.cameraPosition.y -= 0.1f;
	}

	//Projection Matrix
	float fNear = 0.1f;
	float fFar = 1000.0f;
	float fov = 90.0f;
	float aspectRation = (float)ScreenRenderer.backbuffer->height / (float)ScreenRenderer.backbuffer->width;
	float fovRad = 1.0f / tanf(fov * .5f / 180.0f * 3.14159f);

	ScreenRenderer.proj.m[0][0] = aspectRation * fovRad;
	ScreenRenderer.proj.m[1][1] = fovRad;
	ScreenRenderer.proj.m[2][2] = fFar / (fFar - fNear);
	ScreenRenderer.proj.m[3][2] = (-fFar * fNear) / (fFar - fNear);
	ScreenRenderer.proj.m[2][3] = 1.0f;
	ScreenRenderer.proj.m[3][3] = 0.0f;

	ScreenRenderer.view.m[3][0] = -ScreenRenderer.cameraPosition.x;
	ScreenRenderer.view.m[3][1] = -ScreenRenderer.cameraPosition.y;
	ScreenRenderer.view.m[3][2] = -ScreenRenderer.cameraPosition.z;

	static float fTheta = 0.0f;
	fTheta += _platformData.delta_time * 0.25f;

	mat4x4 rotZ = MakeRotationZ(fTheta);
	mat4x4 rotX = MakeRotationX(fTheta * 0.5f);
	mat4x4 model = Multiply(rotZ, rotX);

	PushClear(&renderCommandBuffer, 0x000000);
	PushMesh(&renderCommandBuffer, &testMesh, model);
	Renderer_Execute(&ScreenRenderer, &renderCommandBuffer);
}