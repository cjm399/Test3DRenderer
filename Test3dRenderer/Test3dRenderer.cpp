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

struct Mesh
{
	mat4x4 matRotZ;
	mat4x4 matRotX;
	std::vector<Triangle> triangles;

	bool LoadMeshFromFile(std::string sFileName)
	{
		matRotZ = {};
		matRotX = {};
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


// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable bool GlobalRunning;
global_variable float GameRunTime;
global_variable Vector3 camera = {0, 0, -8.0f};
global_variable uint32_t renderThreadCount;
global_variable std::thread** threads;
global_variable Pair** preAllocatedPairs;
global_variable mat4x4 matProj;
global_variable mat4x4 matView;

global_variable std::mutex frameMutex;
global_variable std::condition_variable frameCond;
global_variable std::atomic<bool>* threadGoFlags;
global_variable std::atomic<bool>* threadDoneFlags;
global_variable bool rendering = true;

global_variable LARGE_INTEGER frequency;
global_variable LARGE_INTEGER GameStartTime;
global_variable float DeltaTime;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
internal void Win32CopyBufferToWindow(HDC deviceContext,
	int windowWidth, int windowHeight,
	win32_offscreen_buffer *buffer);
internal void DrawMesh(Mesh *_mesh, win32_offscreen_buffer *buffer);
internal void MultiplyMatrixVector(Vector3& input, Vector3& output, mat4x4& m);
internal void DrawLine(Pair *p1, Pair *p2, uint32_t color = 0xFF0000);
internal void DrawTriangle(Pair *p1, Pair *p2, Pair *p3, uint32_t color = 0xFF0000);




internal inline double DiffTimeMS(LARGE_INTEGER _start, LARGE_INTEGER _end)
{
	return 1000.0 * (float)(_end.QuadPart - _start.QuadPart) / (float)frequency.QuadPart;
}

internal void DrawTriangleFromMesh(Triangle& tri, mat4x4 &matRotX, mat4x4 &matRotZ, win32_offscreen_buffer* buffer, int threadContext)
{
	Triangle triProjected, triTranslated, triRotatedZ, triRotatedZX;

	MultiplyMatrixVector(tri.points[0], triRotatedZ.points[0], matRotZ);
	MultiplyMatrixVector(tri.points[1], triRotatedZ.points[1], matRotZ);
	MultiplyMatrixVector(tri.points[2], triRotatedZ.points[2], matRotZ);

	MultiplyMatrixVector(triRotatedZ.points[0], triRotatedZX.points[0], matRotX);
	MultiplyMatrixVector(triRotatedZ.points[1], triRotatedZX.points[1], matRotX);
	MultiplyMatrixVector(triRotatedZ.points[2], triRotatedZX.points[2], matRotX);

	MultiplyMatrixVector(triRotatedZX.points[0], triTranslated.points[0], matView);
	MultiplyMatrixVector(triRotatedZX.points[1], triTranslated.points[1], matView);
	MultiplyMatrixVector(triRotatedZX.points[2], triTranslated.points[2], matView);

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
		normal.x*(triTranslated.points[0].x -camera.x) +
		normal.y* (triTranslated.points[0].y - camera.y) + 
		normal.z* (triTranslated.points[0].z - camera.z);
	
	//char buff[1024];
	//sprintf_s(buff, "fDot %f\n", fDot);
	//OutputDebugStringA(buff);
	int triColor = 0xFF0000;
	//if (fDot >= 0.0f)
	//	triColor = 0x0000FF;
	//triColor = ((int)(fDot*2.0) + 255);
	if (true || fDot < 0.0f)
	{
		MultiplyMatrixVector(triTranslated.points[0], triProjected.points[0], matProj);
		MultiplyMatrixVector(triTranslated.points[1], triProjected.points[1], matProj);
		MultiplyMatrixVector(triTranslated.points[2], triProjected.points[2], matProj);

		//Scale into view
		triProjected.points[0].x += 1.0f; triProjected.points[0].y += 1.0f;
		triProjected.points[1].x += 1.0f; triProjected.points[1].y += 1.0f;
		triProjected.points[2].x += 1.0f; triProjected.points[2].y += 1.0f;

		triProjected.points[0].x *= .5f * buffer->width;
		triProjected.points[0].y *= .5f * buffer->height;
		triProjected.points[1].x *= .5f * buffer->width;
		triProjected.points[1].y *= .5f * buffer->height;
		triProjected.points[2].x *= .5f * buffer->width;
		triProjected.points[2].y *= .5f * buffer->height;

		Pair* p1 = preAllocatedPairs[(threadContext * 3) + 0];
		Pair* p2 = preAllocatedPairs[(threadContext * 3) + 1];
		Pair* p3 = preAllocatedPairs[(threadContext * 3) + 2];

		p1->x = triProjected.points[0].x;
		p1->y = triProjected.points[0].y;

		p2->x = triProjected.points[1].x;
		p2->y = triProjected.points[1].y;

		p3->x = triProjected.points[2].x;
		p3->y = triProjected.points[2].y;

		DrawTriangle(p1, p2, p3, triColor);
	}
}

internal void FillScreen(uint32_t color = 0x000000)
{
	uint8_t *row = (uint8_t *)GlobalBackBuffer.memory;
	for (int Y = 0; Y < GlobalBackBuffer.height; Y++)
	{
		uint32_t *pixel = (uint32_t *)row;
		for (int X = 0; X < GlobalBackBuffer.width; X++)
		{
			*pixel++ = color;
		}
		row += GlobalBackBuffer.pitch;
	}
}

internal void DrawLine(Pair* p0, Pair* p1, uint32_t color)
{
	int x0 = p0->x, y0 = p0->y;
	int x1 = p1->x, y1 = p1->y;

	int dx = abs(x1 - x0);
	int dy = abs(y1 - y0);

	int sx = (x0 < x1) ? 1 : -1;
	int sy = (y0 < y1) ? 1 : -1;

	int err = dx - dy;

	uint8_t* row = (uint8_t*)GlobalBackBuffer.memory;
	int bytesPerPixel = sizeof(uint32_t);

	while (true)
	{
		// Clamp to screen bounds (skip if out-of-bounds)
		if (x0 >= 0 && x0 < GlobalBackBuffer.width && y0 >= 0 && y0 < GlobalBackBuffer.height)
		{
			int pixelIndex = y0 * GlobalBackBuffer.pitch + x0 * bytesPerPixel;
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

/*internal void DrawLine(Pair* p1, Pair* p2, uint32_t color)
{
	//y = mx+b
	if (p1 ->x >= 0 && p1->x < GlobalBackBuffer.width && p1->y >= 0 && p1->y < GlobalBackBuffer.height &&
		p2->x >= 0 && p2->x < GlobalBackBuffer.width && p2->y >= 0 && p2->y < GlobalBackBuffer.height)
	{
		float fSlope = 1;
		if(p1->x-p2->x != 0)
			fSlope = ((float)(p1->y - p2->y)) / ((float)(p1->x - p2->x));
		float b = (-fSlope * p1->x) + p1->y;

		int minX = min(p1->x, p2->x);
		int minY = min(p1->y, p2->y);
		int maxX = max(p1->x, p2->x);
		int maxY = max(p1->y, p2->y);

		uint8_t *row = (uint8_t *)GlobalBackBuffer.memory;
		int bytesPerPixel = sizeof(uint32_t);
		std::vector<Pair> pixelCoords;

		for (int x = minX; x <= maxX; ++x)
		{
			int y = fSlope * x + b;

			int pixelIndex = (GlobalBackBuffer.pitch*y) + (bytesPerPixel*x);
			uint32_t *pixelLocation = (uint32_t *)(row + pixelIndex);
			*pixelLocation = color;
		}
	}
}*/

internal inline void DrawTriangle(Pair *p1, Pair *p2, Pair *p3, uint32_t color)
{
	DrawLine(p1, p2, color);
	DrawLine(p2, p3, color);
	DrawLine(p3, p1, color);
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
Win32ResizeDIBSection(win32_offscreen_buffer *buffer,
	int _width, int _height)
{
	buffer->height = _height;
	buffer->width = _width;
	int bytesPerPixel = 4;

	if (buffer->memory)
	{
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->Info.bmiHeader.biSize = sizeof(buffer->Info.bmiHeader);
	buffer->Info.bmiHeader.biWidth = _width;
	buffer->Info.bmiHeader.biHeight = -_height;
	buffer->Info.bmiHeader.biPlanes = 1;
	buffer->Info.bmiHeader.biBitCount = 32;
	buffer->Info.bmiHeader.biCompression = BI_RGB;

	SIZE_T bufferSize = bytesPerPixel * (_width * _height);
	buffer->memory = VirtualAlloc(0, bufferSize, MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = _width * bytesPerPixel;
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

internal void DrawMesh(Mesh *_mesh, win32_offscreen_buffer *buffer)
{
	local_persist float fTheta = 0;
	local_persist float rotateSpeed = (1.0f / 4.0f);
	
	fTheta += rotateSpeed * DeltaTime;

	// Rotation matrices
	_mesh->matRotZ.m[0][0] = cosf(fTheta);
	_mesh->matRotZ.m[0][1] = sinf(fTheta);
	_mesh->matRotZ.m[1][0] = -sinf(fTheta);
	_mesh->matRotZ.m[1][1] = cosf(fTheta);
	_mesh->matRotZ.m[2][2] = 1;
	_mesh->matRotZ.m[3][3] = 1;

	_mesh->matRotX.m[0][0] = 1;
	_mesh->matRotX.m[1][1] = cosf(fTheta * 0.5f);
	_mesh->matRotX.m[1][2] = sinf(fTheta * 0.5f);
	_mesh->matRotX.m[2][1] = -sinf(fTheta * 0.5f);
	_mesh->matRotX.m[2][2] = cosf(fTheta * 0.5f);
	_mesh->matRotX.m[3][3] = 1;

	{
		std::unique_lock<std::mutex> lock(frameMutex);
		for (int i = 0; i < renderThreadCount; ++i)
		{
			threadGoFlags[i] = true;
			threadDoneFlags[i] = false;
		}
	}
	frameCond.notify_all();

	// Wait for all threads to finish
	bool allDone = false;
	while (!allDone)
	{
		allDone = true;
		for (int i = 0; i < renderThreadCount; ++i)
		{
			if (!threadDoneFlags[i]) {
				allDone = false;
				break;
			}
		}
	}
}

int WINAPI WinMain(HINSTANCE instance,
	HINSTANCE previousInstance,
	LPSTR commandLine,
	int showCode)

{
	Mesh testMesh = Mesh();
	testMesh.LoadMeshFromFile("Astronaut2.obj");
	WNDCLASSEXA windowClass = {};

	//win32_window_dimension dimensions = Win32GetWindowDimension(window);
	Win32ResizeDIBSection(&GlobalBackBuffer, 1920, 1080);

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


		matView.m[0][0] = 1.0f;
		matView.m[1][1] = 1.0f;
		matView.m[2][2] = 1.0f;
		matView.m[3][3] = 1.0f;
		matView.m[3][0] = -camera.x;
		matView.m[3][1] = -camera.y;
		matView.m[3][2] = -camera.z;

		uint32_t pcThreadCount = std::thread::hardware_concurrency();
		renderThreadCount = 1;
		renderThreadCount = pcThreadCount - 1;
		if (renderThreadCount <= 0)
		{
			renderThreadCount = 1;
		}
		float oneOverRenderThreadCount = 1.0f / renderThreadCount;

		preAllocatedPairs = new Pair * [renderThreadCount * 3];
		for (int i = 0; i < renderThreadCount * 3; ++i)
		{
			preAllocatedPairs[i] = new Pair();
		}

		threads = new std::thread * [renderThreadCount];

		threadGoFlags = new std::atomic<bool>[renderThreadCount];
		threadDoneFlags = new std::atomic<bool>[renderThreadCount];
		for (int i = 0; i < renderThreadCount; ++i)
		{
			threadGoFlags[i] = false;
			threadDoneFlags[i] = false;

			threads[i] = new std::thread([&, i]()
			{
				while (rendering)
				{
					std::unique_lock<std::mutex> lock(frameMutex);
					frameCond.wait(lock, [&] { return threadGoFlags[i].load() || !rendering; });
					lock.unlock();

					if (!rendering) break;

					// Work slice for this thread
					int start = i * testMesh.triangles.size() * oneOverRenderThreadCount;
					int end = (i + 1) * testMesh.triangles.size() * oneOverRenderThreadCount;

					for (int j = start; j < end; ++j)
					{
						DrawTriangleFromMesh(testMesh.triangles[j], testMesh.matRotX, testMesh.matRotZ, &GlobalBackBuffer, i);
					}

					threadGoFlags[i] = false;
					threadDoneFlags[i] = true;
				}
			});
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
				DeltaTime = 33.0 / 1000.0f;
				QueryPerformanceCounter(&frameStart);
				GameRunTime = DiffTimeMS(GameStartTime, frameStart);

				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&message);
					DispatchMessageA(&message);
				}

				//Projection Matrix
				float fNear = 0.1f;
				float fFar = 1000.0f;
				float fov = 90.0f;
				float aspectRation = (float)GlobalBackBuffer.height / (float)GlobalBackBuffer.width;
				float fovRad = 1.0f / tanf(fov * .5f / 180.0f * 3.14159f);

				matProj.m[0][0] = aspectRation * fovRad;
				matProj.m[1][1] = fovRad;
				matProj.m[2][2] = fFar / (fFar - fNear);
				matProj.m[3][2] = (-fFar * fNear) / (fFar - fNear);
				matProj.m[2][3] = 1.0f;
				matProj.m[3][3] = 0.0f;

				//camera.z += DeltaTime/5.0f;
				//camera.x -= DeltaTime;
				matView.m[3][0] = -camera.x;
				matView.m[3][1] = -camera.y;
				matView.m[3][2] = -camera.z;

				//Redraw the screen black!
				FillScreen();
				DrawMesh(&testMesh, &GlobalBackBuffer);
				
				if (xOffset >= 1278) xOffset = 0;
				if (yOffset >= 718) yOffset = 0;

				xOffset += 1;
				yOffset += 1;
				win32_window_dimension dimensions = Win32GetWindowDimension(window);
				
				QueryPerformanceCounter(&framePreRenderEnd);
				
				double frameTime = DiffTimeMS(frameStart, framePreRenderEnd);
				if (frameTime < 33)
				{

					double excessFrameTime = 33 - DiffTimeMS(frameStart, framePreRenderEnd);
					sprintf_s(buffer, sizeof(buffer), "Excess frame time: %.02f ms/f s\n", excessFrameTime);
					OutputDebugStringA(buffer);

					Sleep((33 - frameTime));
				}
				Win32CopyBufferToWindow(deviceContext,
					dimensions.width, dimensions.height,
					&GlobalBackBuffer);

				QueryPerformanceCounter(&frameEnd);

				//double msPerFrame = DiffTimeMS(frameStart, frameEnd);
				//sprintf_s(buffer, sizeof(buffer), "Elapsed time: %.02f ms/f s\n", msPerFrame);
				//OutputDebugStringA(buffer);

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
	win32_offscreen_buffer *buffer)
{
	StretchDIBits(deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, buffer->width, buffer->height,
		buffer->memory,
		&buffer->Info,
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
		switch (wParam)
		{
		case VK_UP:
			camera.z += 0.1f;  // Move forward
			break;
		case VK_DOWN:
			camera.z -= 0.1f;  // Move backward
			break;
		case VK_LEFT:
			camera.x -= 0.1f;  // Strafe left
			break;
		case VK_RIGHT:
			camera.x += 0.1f;  // Strafe right
			break;
		case 'Q':
			camera.y += 0.1f;  // Move up
			break;
		case 'E':
			camera.y -= 0.1f;  // Move down
			break;
		}
		break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
			int x = ps.rcPaint.left;
			int y = ps.rcPaint.right;
			int height = ps.rcPaint.bottom - ps.rcPaint.top;
			int width = ps.rcPaint.right - ps.rcPaint.left;

			win32_window_dimension dimensions = Win32GetWindowDimension(hWnd);
			Win32CopyBufferToWindow(hdc, width, height, &GlobalBackBuffer);

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