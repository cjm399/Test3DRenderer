#include "win32_platform.h"
#include <timeapi.h>

#include "../core/sp_intrinsics.h"
#include "../core/mesh.h"
#include "../renderer/renderer.h"
#include "../core/types.h"
#include "../game/game.h"

#pragma comment(lib, "winmm.lib")


// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name

global_variable bool GlobalRunning;
global_variable float GameRunTime;
global_variable platform_input InputState;
global_variable platform_input PreviousInputState;
global_variable win32_offscreen_buffer GlobalBackBuffer;

global_variable LARGE_INTEGER frequency;
global_variable LARGE_INTEGER GameStartTime;
global_variable float DeltaTime;

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
internal void Win32CopyBufferToWindow(HDC deviceContext,
	int windowWidth, int windowHeight,
	win32_offscreen_buffer* renderContext);

internal inline float DiffTimeMS(LARGE_INTEGER _start, LARGE_INTEGER _end)
{
	return 1000.0f * (float)(_end.QuadPart - _start.QuadPart) / (float)frequency.QuadPart;
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
Win32ResizeDIBSection(win32_offscreen_buffer* _win32bb,
	int _width, int _height)
{
	_win32bb->buffer.height = _height;
	_win32bb->buffer.width = _width;
	int bytesPerPixel = 4;

	if (_win32bb->buffer.memory)
	{
		VirtualFree(_win32bb->buffer.memory, 0, MEM_RELEASE);
	}

	_win32bb->info.bmiHeader.biSize = sizeof(_win32bb->info.bmiHeader);
	_win32bb->info.bmiHeader.biWidth = _width;
	_win32bb->info.bmiHeader.biHeight = -_height;
	_win32bb->info.bmiHeader.biPlanes = 1;
	_win32bb->info.bmiHeader.biBitCount = 32;
	_win32bb->info.bmiHeader.biCompression = BI_RGB;

	SIZE_T bufferSize = bytesPerPixel * (_width * _height);
	_win32bb->buffer.memory = VirtualAlloc(0, bufferSize, MEM_COMMIT, PAGE_READWRITE);
	_win32bb->buffer.pitch= _width * bytesPerPixel;
}


/*internal void WindowGradient(win32_offscreen_buffer* buffer, uint8_t xOffset, uint8_t yOffset)
{
	uint8_t* row = (uint8_t*)buffer->memory;
	for (int Y = 0; Y < buffer->height; Y++)
	{
		uint32_t* pixel = (uint32_t*)row;
		for (int X = 0; X < buffer->width; X++)
		{
			uint8_t red = 255;
			uint8_t green = X + xOffset;
			uint8_t blue = Y + yOffset;
			*pixel++ = (((red << 16) | (green << 8)) | blue);
		}
		row += buffer->pitch;
	}
}*/

int WINAPI WinMain(HINSTANCE instance,
	HINSTANCE previousInstance,
	LPSTR commandLine,
	int showCode)

{	
	GlobalBackBuffer = win32_offscreen_buffer{};
	GlobalBackBuffer.buffer = backbuffer{};
	InputState = {};
	PreviousInputState = {};

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

			platform_data platformData = platform_data{};
			platformData.backBuffer = &GlobalBackBuffer.buffer;
			platformData.platformInput = &InputState;

			Init(platformData);

			while (GlobalRunning)
			{
				DeltaTime = 33.0f / 1000.0f;
				QueryPerformanceCounter(&frameStart);
				GameRunTime = DiffTimeMS(GameStartTime, frameStart);

				PreviousInputState = InputState;
				InputState = {};

				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&message);
					DispatchMessageA(&message);
				}

				platformData.platformInput = &InputState;
				platformData.delta_time = DeltaTime;

				game_update_and_render(platformData);

				win32_window_dimension dimensions = Win32GetWindowDimension(window);

				QueryPerformanceCounter(&framePreRenderEnd);

				float frameTime = DiffTimeMS(frameStart, framePreRenderEnd);
				if (frameTime < 33.0f)
				{

					float excessFrameTime = 33.0f - DiffTimeMS(frameStart, framePreRenderEnd);
					sprintf_s(buffer, sizeof(buffer), "Excess frame time: %.02f ms s\n", excessFrameTime);
					OutputDebugStringA(buffer);

					Sleep(excessFrameTime);
				}
				QueryPerformanceCounter(&frameEnd);

				float msPerFrame = DiffTimeMS(frameStart, frameEnd);
				//sprintf_s(buffer, sizeof(buffer), "Elapsed time: %.02f ms/f s\n", msPerFrame);
				//OutputDebugStringA(buffer);

				Win32CopyBufferToWindow(deviceContext,
					dimensions.width, dimensions.height,
					&GlobalBackBuffer);
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
	win32_offscreen_buffer * _win32bb)
{
	StretchDIBits(deviceContext,
		0, 0, windowWidth, windowHeight,
		0, 0, _win32bb->buffer.width, _win32bb->buffer.height,
		_win32bb->buffer.memory,
		&_win32bb->info,
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