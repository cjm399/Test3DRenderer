// Test3dRenderer.cpp : Defines the entry point for the application.
//

#include "Test3dRenderer.h"
#include <fstream>
#include <strstream>
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")

#define MAX_LOADSTRING 100
#define global_variable static
#define internal static
#define local_persist static


struct win32_window_dimension
{
	int height;
	int width;
};

struct win32_offscreen_buffer
{
	BITMAPINFO Info;
	void *memory;
	int width;
	int height;
	int pitch;
};

struct Pair
{
	int x, y;
};

struct Vector3
{
public:
	float x, y, z;
};

struct Triangle
{
public:
	Vector3 points[3];
};

struct mat4x4
{
	float m[4][4] = { 0 };
};

inline mat4x4 MakeIdentity()
{
	mat4x4 m = {};
	m.m[0][0] = 1;
	m.m[1][1] = 1;
	m.m[2][2] = 1;
	m.m[3][3] = 1;
	return m;
}

inline mat4x4 MakeRotationZ(float angle)
{
	mat4x4 m = MakeIdentity();
	float c = cosf(angle);
	float s = sinf(angle);

	m.m[0][0] = c;
	m.m[0][1] = s;
	m.m[1][0] = -s;
	m.m[1][1] = c;
	return m;
}

inline mat4x4 MakeRotationX(float angle)
{
	mat4x4 m = MakeIdentity();
	float c = cosf(angle);
	float s = sinf(angle);

	m.m[1][1] = c;
	m.m[1][2] = s;
	m.m[2][1] = -s;
	m.m[2][2] = c;
	return m;
}

inline mat4x4 Multiply(const mat4x4& a, const mat4x4& b)
{
	mat4x4 r = {};

	for (int c = 0; c < 4; c++)
		for (int r2 = 0; r2 < 4; r2++)
			r.m[r2][c] =
			a.m[r2][0] * b.m[0][c] +
			a.m[r2][1] * b.m[1][c] +
			a.m[r2][2] * b.m[2][c] +
			a.m[r2][3] * b.m[3][c];

	return r;
}

struct Mesh
{
	std::vector<Vector3> positions;
	std::vector<Triangle> triangles; //TODO:Delete and swap to positions and indicies only
	std::vector<uint32_t> indicies;

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

struct Renderer {
	win32_offscreen_buffer* backbuffer;
	mat4x4 view;
	mat4x4 proj;
	Vector3 cameraPosition;
};

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

struct input_state {
	bool keys[256];
};

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

global_variable bool GlobalRunning;
global_variable float GameRunTime;
global_variable Pair** preAllocatedPairs;
global_variable input_state InputState;
global_variable input_state PreviousInputState;

global_variable LARGE_INTEGER frequency;
global_variable LARGE_INTEGER GameStartTime;
global_variable float DeltaTime;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
internal void Win32CopyBufferToWindow(HDC deviceContext,
	int windowWidth, int windowHeight,
	Renderer *renderContext);

internal void DrawMesh(Renderer *renderContext, Mesh* _mesh, mat4x4 _model);
internal void MultiplyMatrixVector(Vector3& input, Vector3& output, mat4x4& m);
internal void DrawLine(Renderer* renderContext, Pair *p1, Pair *p2, uint32_t color = 0xFF0000);
internal void DrawTriangle(Renderer* renderContext, Pair *p1, Pair *p2, Pair *p3, uint32_t color = 0xFF0000);


internal inline float DiffTimeMS(LARGE_INTEGER _start, LARGE_INTEGER _end)
{
	return 1000.0f * (float)(_end.QuadPart - _start.QuadPart) / (float)frequency.QuadPart;
}

internal void DrawTriangleFromMesh(
	Renderer* renderContext, 
	Triangle& tri, 
	mat4x4 &matRotX, 
	mat4x4 &matRotZ, 
	int threadContext
)
{
	Triangle triProjected, triTranslated, triRotatedZ, triRotatedZX;

	MultiplyMatrixVector(tri.points[0], triRotatedZ.points[0], matRotZ);
	MultiplyMatrixVector(tri.points[1], triRotatedZ.points[1], matRotZ);
	MultiplyMatrixVector(tri.points[2], triRotatedZ.points[2], matRotZ);

	MultiplyMatrixVector(triRotatedZ.points[0], triRotatedZX.points[0], matRotX);
	MultiplyMatrixVector(triRotatedZ.points[1], triRotatedZX.points[1], matRotX);
	MultiplyMatrixVector(triRotatedZ.points[2], triRotatedZX.points[2], matRotX);

	MultiplyMatrixVector(triRotatedZX.points[0], triTranslated.points[0], renderContext->view);
	MultiplyMatrixVector(triRotatedZX.points[1], triTranslated.points[1], renderContext->view);
	MultiplyMatrixVector(triRotatedZX.points[2], triTranslated.points[2], renderContext->view);

	if (triTranslated.points[0].z <= 0.1f ||
		triTranslated.points[1].z <= 0.1f ||
		triTranslated.points[2].z <= 0.1f)
	{
		return;
	}

	Vector3 normal, line1, line2;
	line1.x = triTranslated.points[1].x - triTranslated.points[0].x;
	line1.y = triTranslated.points[1].y - triTranslated.points[0].y;
	line1.z = triTranslated.points[1].z - triTranslated.points[0].z;

	line2.x = triTranslated.points[2].x - triTranslated.points[0].x;
	line2.y = triTranslated.points[2].y - triTranslated.points[0].y;
	line2.z = triTranslated.points[2].z - triTranslated.points[0].z;

	float fDot = -1.0;
	
	normal.x = line1.y * line2.z - line1.z * line2.y;
	normal.y = line1.z * line2.x - line1.x * line2.z;
	normal.z = line1.x * line2.y - line1.y * line2.x;
	float l = sqrtf(normal.x*normal.x + normal.y*normal.y + normal.z*normal.z);
	normal.x /= l; normal.y /= l; normal.z /= l;
	fDot =
		normal.x*(triTranslated.points[0].x - renderContext->cameraPosition.x) +
		normal.y* (triTranslated.points[0].y - renderContext->cameraPosition.y) +
		normal.z* (triTranslated.points[0].z - renderContext->cameraPosition.z);
	
	//char buff[1024];
	//sprintf_s(buff, "fDot %f\n", fDot);
	//OutputDebugStringA(buff);
	int triColor = 0xFF0000;
	//if (fDot >= 0.0f)
	//	triColor = 0x0000FF;
	//triColor = ((int)(fDot*2.0) + 255);
	if (true || fDot < 0.0f)
	{
		MultiplyMatrixVector(triTranslated.points[0], triProjected.points[0], renderContext->proj);
		MultiplyMatrixVector(triTranslated.points[1], triProjected.points[1], renderContext->proj);
		MultiplyMatrixVector(triTranslated.points[2], triProjected.points[2], renderContext->proj);

		//Scale into view
		triProjected.points[0].x += 1.0f; triProjected.points[0].y += 1.0f;
		triProjected.points[1].x += 1.0f; triProjected.points[1].y += 1.0f;
		triProjected.points[2].x += 1.0f; triProjected.points[2].y += 1.0f;

		triProjected.points[0].x *= .5f * renderContext->backbuffer->width;
		triProjected.points[0].y *= .5f * renderContext->backbuffer->height;
		triProjected.points[1].x *= .5f * renderContext->backbuffer->width;
		triProjected.points[1].y *= .5f * renderContext->backbuffer->height;
		triProjected.points[2].x *= .5f * renderContext->backbuffer->width;
		triProjected.points[2].y *= .5f * renderContext->backbuffer->height;

		Pair* p1 = preAllocatedPairs[(threadContext * 3) + 0];
		Pair* p2 = preAllocatedPairs[(threadContext * 3) + 1];
		Pair* p3 = preAllocatedPairs[(threadContext * 3) + 2];

		p1->x = triProjected.points[0].x;
		p1->y = triProjected.points[0].y;

		p2->x = triProjected.points[1].x;
		p2->y = triProjected.points[1].y;

		p3->x = triProjected.points[2].x;
		p3->y = triProjected.points[2].y;

		DrawTriangle(renderContext, p1, p2, p3, triColor);
	}
}

internal void FillScreen(Renderer* renderContext, uint32_t color = 0x000000)
{
	uint8_t *row = (uint8_t *)renderContext->backbuffer->memory;
	for (int Y = 0; Y < renderContext->backbuffer->height; Y++)
	{
		uint32_t *pixel = (uint32_t *)row;
		for (int X = 0; X < renderContext->backbuffer->width; X++)
		{
			*pixel++ = color;
		}
		row += renderContext->backbuffer->pitch;
	}
}

internal void DrawLine(Renderer* renderContext, Pair* p0, Pair* p1, uint32_t color)
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

internal inline void DrawTriangle(Renderer* renderContext, Pair *p1, Pair *p2, Pair *p3, uint32_t color)
{
	DrawLine(renderContext, p1, p2, color);
	DrawLine(renderContext, p2, p3, color);
	DrawLine(renderContext, p3, p1, color);
}

internal inline void MultiplyMatrixVector(Vector3 &input, Vector3 &output, mat4x4 &m)
{
	output.x = input.x * m.m[0][0] + input.y * m.m[1][0] + input.z * m.m[2][0] + m.m[3][0];
	output.y = input.x * m.m[0][1] + input.y * m.m[1][1] + input.z * m.m[2][1] + m.m[3][1];
	output.z = input.x * m.m[0][2] + input.y * m.m[1][2] + input.z * m.m[2][2] + m.m[3][2];
	float w = input.x * m.m[0][3] + input.y * m.m[1][3] + input.z * m.m[2][3] + m.m[3][3];

	if (w != 0)
	{
		output.x /= w;
		output.y /= w;
		output.z /= w;
	}
}

internal win32_window_dimension
Win32GetWindowDimension(HWND window)
{
	win32_window_dimension result;

	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;

	return(result);
}

internal void
Win32ResizeDIBSection(Renderer *renderContext,
	int _width, int _height)
{
	renderContext->backbuffer->height = _height;
	renderContext->backbuffer->width = _width;
	int bytesPerPixel = 4;

	if (renderContext->backbuffer->memory)
	{
		VirtualFree(renderContext->backbuffer->memory, 0, MEM_RELEASE);
	}

	renderContext->backbuffer->Info.bmiHeader.biSize = sizeof(renderContext->backbuffer->Info.bmiHeader);
	renderContext->backbuffer->Info.bmiHeader.biWidth = _width;
	renderContext->backbuffer->Info.bmiHeader.biHeight = -_height;
	renderContext->backbuffer->Info.bmiHeader.biPlanes = 1;
	renderContext->backbuffer->Info.bmiHeader.biBitCount = 32;
	renderContext->backbuffer->Info.bmiHeader.biCompression = BI_RGB;

	SIZE_T bufferSize = bytesPerPixel * (_width * _height);
	renderContext->backbuffer->memory = VirtualAlloc(0, bufferSize, MEM_COMMIT, PAGE_READWRITE);
	renderContext->backbuffer->pitch = _width * bytesPerPixel;
}


internal void WindowGradient(win32_offscreen_buffer *buffer, uint8_t xOffset, uint8_t yOffset)
{
	uint8_t *row = (uint8_t *)buffer->memory;
	for (int Y = 0; Y < buffer->height; Y++)
	{
		uint32_t *pixel = (uint32_t *)row;
		for (int X = 0; X < buffer->width; X++)
		{
			uint8_t red = 255;
			uint8_t green = X + xOffset;
			uint8_t blue = Y + yOffset;
			*pixel++ = (((red << 16) | (green << 8)) | blue);
		}
		row += buffer->pitch;
	}
}

internal void DrawMesh(Renderer* _renderContext, Mesh *_mesh, mat4x4 _model)
{

	for (int i = 0; i < _mesh->triangles.size(); ++i)
	{
		DrawTriangleFromMesh(_renderContext, _mesh->triangles[i], _model, _model, 0);
	}
}


void Renderer_Execute(
	Renderer* _renderer,
	RenderCommandBuffer* _buffer)
{
	uint8_t* loc = _buffer->data;

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

int WINAPI WinMain(HINSTANCE instance,
	HINSTANCE previousInstance,
	LPSTR commandLine,
	int showCode)

{
	Renderer renderContext = Renderer{};
	RenderCommandBuffer renderCommandBuffer = RenderCommandBuffer{};
	renderCommandBuffer.data = (uint8_t*) malloc((1024 * 1024));
	renderCommandBuffer.capaity = 1024 * 1024;
	renderContext.backbuffer = new win32_offscreen_buffer();
	renderContext.cameraPosition = { 0, 0, -8.0f };

	InputState = {};
	PreviousInputState = {};

	Mesh testMesh = Mesh();
	testMesh.LoadMeshFromFile("Astronaut2.obj");
	WNDCLASSEXA windowClass = {};

	//win32_window_dimension dimensions = Win32GetWindowDimension(window);
	Win32ResizeDIBSection(&renderContext, 1920, 1080);

	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = WndProc;
	windowClass.hInstance = instance;
	//windowClass.hIcon;
	windowClass.lpszClassName = "RendererWindowClass";

	ATOM uniqueClassId = RegisterClassExA(&windowClass);
	if (uniqueClassId)
	{
		//create window and look at the queue
		HWND window = CreateWindowExA(
			0,
			"RendererWindowClass",
			"Test 3D Renderer",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			instance,
			0);


		char buffer[1024];

		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&GameStartTime);


		renderContext.view.m[0][0] = 1.0f;
		renderContext.view.m[1][1] = 1.0f;
		renderContext.view.m[2][2] = 1.0f;
		renderContext.view.m[3][3] = 1.0f;
		renderContext.view.m[3][0] = -renderContext.cameraPosition.x;
		renderContext.view.m[3][1] = -renderContext.cameraPosition.y;
		renderContext.view.m[3][2] = -renderContext.cameraPosition.z;

		preAllocatedPairs = new Pair * [3];
		for (int i = 0; i < 3; ++i)
		{
			preAllocatedPairs[i] = new Pair();
		}

		char logBuffer[1024];

		if (window)
		{
			HDC deviceContext = GetDC(window);
			MSG message;
			(timeBeginPeriod(1) == TIMERR_NOERROR);

			GlobalRunning = true;

			int xOffset = 1, yOffset = 1;
			LARGE_INTEGER frameStart;
			LARGE_INTEGER framePreRenderEnd;
			LARGE_INTEGER frameEnd;
			while (GlobalRunning)
			{
				DeltaTime = 33.0f / 1000.0f;
				QueryPerformanceCounter(&frameStart);
				GameRunTime = DiffTimeMS(GameStartTime, frameStart);

				PreviousInputState = InputState;
				InputState = {};
				//memset(renderCommandBuffer.data, 0, renderCommandBuffer.capaity);
				renderCommandBuffer.size = 0;

				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&message);
					DispatchMessageA(&message);
				}
				
				if (InputState.keys[VK_UP])
				{
					renderContext.cameraPosition.z += 0.1f;
				}
				if (InputState.keys[VK_DOWN])
				{
					renderContext.cameraPosition.z -= 0.1f;
				}
				if (InputState.keys[VK_LEFT])
				{
					renderContext.cameraPosition.x -= 0.1f;
				}
				if (InputState.keys[VK_RIGHT])
				{
					renderContext.cameraPosition.x += 0.1f;
				}
				if (InputState.keys['Q'])
				{
					renderContext.cameraPosition.y += 0.1f;
				}
				if (InputState.keys['E'])
				{
					renderContext.cameraPosition.y -= 0.1f;
				}
					
				//Projection Matrix
				float fNear = 0.1f;
				float fFar = 1000.0f;
				float fov = 90.0f;
				float aspectRation = (float)renderContext.backbuffer->height / (float)renderContext.backbuffer->width;
				float fovRad = 1.0f / tanf(fov * .5f / 180.0f * 3.14159f);

				renderContext.proj.m[0][0] = aspectRation * fovRad;
				renderContext.proj.m[1][1] = fovRad;
				renderContext.proj.m[2][2] = fFar / (fFar - fNear);
				renderContext.proj.m[3][2] = (-fFar * fNear) / (fFar - fNear);
				renderContext.proj.m[2][3] = 1.0f;
				renderContext.proj.m[3][3] = 0.0f;

				renderContext.view.m[3][0] = -renderContext.cameraPosition.x;
				renderContext.view.m[3][1] = -renderContext.cameraPosition.y;
				renderContext.view.m[3][2] = -renderContext.cameraPosition.z;

				static float fTheta = 0.0f;
				fTheta += DeltaTime * 0.25f;

				mat4x4 rotZ = MakeRotationZ(fTheta);
				mat4x4 rotX = MakeRotationX(fTheta * 0.5f);
				mat4x4 model = Multiply(rotZ, rotX);

				PushClear(&renderCommandBuffer, 0x000000);
				PushMesh(&renderCommandBuffer, &testMesh, model);
				Renderer_Execute(&renderContext, &renderCommandBuffer);
				
				if (xOffset >= 1278) xOffset = 0;
				if (yOffset >= 718) yOffset = 0;

				xOffset += 1;
				yOffset += 1;
				win32_window_dimension dimensions = Win32GetWindowDimension(window);
				
				QueryPerformanceCounter(&framePreRenderEnd);
				
				float frameTime = DiffTimeMS(frameStart, framePreRenderEnd);
				if (frameTime < 33.0f)
				{

					float excessFrameTime = 33.0f - DiffTimeMS(frameStart, framePreRenderEnd);
					sprintf_s(buffer, sizeof(buffer), "Excess frame time: %.02f ms s\n", excessFrameTime);
					//OutputDebugStringA(buffer);

					Sleep(excessFrameTime);
				}
				QueryPerformanceCounter(&frameEnd);

				float msPerFrame = DiffTimeMS(frameStart, frameEnd);
				sprintf_s(buffer, sizeof(buffer), "Elapsed time: %.02f ms/f s\n", msPerFrame);
				OutputDebugStringA(buffer);

				Win32CopyBufferToWindow(deviceContext,
					dimensions.width, dimensions.height,
					&renderContext);

			}
		}
		else
		{
			//TODO: Loggin error info
		}

	}
	else
	{
		uniqueClassId;
		//TODO: Logging error info
	}

	return(0);
}


internal void 
Win32CopyBufferToWindow(HDC deviceContext,
	int windowWidth, int windowHeight,
	Renderer *renderContext)
{
	StretchDIBits(deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, renderContext->backbuffer->width, renderContext->backbuffer->height,
		renderContext->backbuffer->memory,
		&renderContext->backbuffer->Info,
		DIB_RGB_COLORS, SRCCOPY);
}


//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
	case WM_KEYDOWN:
		InputState.keys[wParam] = true;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
			int x = ps.rcPaint.left;
			int y = ps.rcPaint.right;
			int height = ps.rcPaint.bottom - ps.rcPaint.top;
			int width = ps.rcPaint.right - ps.rcPaint.left;

			win32_window_dimension dimensions = Win32GetWindowDimension(hWnd);
			//Win32CopyBufferToWindow(hdc, width, height, &renderContext);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
		GlobalRunning = false;
        break;
	case WM_QUIT:
		GlobalRunning = false;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}