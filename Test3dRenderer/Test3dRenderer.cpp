// Test3dRenderer.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "Test3dRenderer.h"
#include <fstream>
#include <strstream>
#include <iostream>
#include <time.h>
#include <vector>

#define MAX_LOADSTRING 100
#define global_variable static
#define internal static
#define local_persist static

//using namespace std;

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

struct Mesh
{
	std::vector<Triangle> triangles;

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

struct mat4x4
{
	float m[4][4] = { 0 };
};


// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
Mesh testMesh{};
mat4x4 matProj;

global_variable win32_offscreen_buffer GlobalBackBuffer;
global_variable bool GlobalRunning;
global_variable time_t GlobalStartTime;
global_variable float GlobalDeltaTime;

// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


internal void Win32CopyBufferToWindow(HDC deviceContext,
	int windowWidth, int windowHeight,
	win32_offscreen_buffer *buffer);

internal void DrawMesh(Mesh _mesh, win32_offscreen_buffer *buffer);

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

internal void DrawLine(Pair p1, Pair p2, uint32_t color = 0xFF0000)
{
	//y = mx+b
	float fSlope = 1;
	if(p1.x-p2.x != 0)
		fSlope = ((float)(p1.y - p2.y)) / ((float)(p1.x - p2.x));
	float b = (-fSlope * p1.x) + p1.y;

	int minX = min(p1.x, p2.x);
	int minY = min(p1.y, p2.y);
	int maxX = max(p1.x, p2.x);
	int maxY = max(p1.y, p2.y);

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


	/*for (auto coord : pixelCoords)
	{
		int pixelIndex = (GlobalBackBuffer.pitch*coord.y) + (bytesPerPixel*coord.x);
		uint32_t *pixelLocation = (uint32_t *)(row + pixelIndex);
		*pixelLocation = color;
	}*/

	/*uint8_t *row = (uint8_t *)GlobalBackBuffer.memory;
	for (int Y = 0; Y < GlobalBackBuffer.height; Y++)
	{
		uint32_t *pixel = (uint32_t *)row;
		for (int X = 0; X < GlobalBackBuffer.width; X++)
		{
			Pair tempCoord{ X,Y };
			for (auto coord : pixelCoords)
			{
				if (tempCoord.x == coord.x && tempCoord.y == coord.y)
				{
					*pixel = color;
					break;
				}
			}
			/*if (*pixel != color)
			{
				*pixel = 0xFFFFFF;
			}
			//*pixel = color;
			*pixel++;
		}
		row += GlobalBackBuffer.pitch;
	}*/
}

internal void DrawTriangle(Pair p1, Pair p2, Pair p3, uint32_t color = 0xFF0000)
{
	DrawLine(p1, p2, color);
	DrawLine(p2, p3, color);
	DrawLine(p3, p1, color);
}

void MultiplyMatrixVector(Vector3 &input, Vector3 &output, mat4x4 &m)
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

internal void DrawMesh(Mesh _mesh, win32_offscreen_buffer *buffer)
{

	mat4x4 matRotZ, matRotX;
	float fTheta = GlobalDeltaTime;

	//Z rotation
	matRotZ.m[0][0] = cosf(fTheta);
	matRotZ.m[0][1] = sinf(fTheta);
	matRotZ.m[1][0] = -sinf(fTheta);
	matRotZ.m[1][1] = cosf(fTheta);
	matRotZ.m[2][2] = 1;
	matRotZ.m[3][3] = 1;

	//X rotation
	matRotX.m[0][0] = 1;
	matRotX.m[1][1] = cosf(fTheta * .5f);
	matRotX.m[1][2] = sinf(fTheta * .5f);
	matRotX.m[2][1] = -sinf(fTheta * .5f);
	matRotX.m[2][2] = cosf(fTheta * .5f);
	matRotX.m[3][3] = 1;

	//Draw Triangles
	for (auto tri : testMesh.triangles)
	{
		Triangle triProjected, triTranslated, triRotatedZ, triRotatedZX;

		MultiplyMatrixVector(tri.points[0], triRotatedZ.points[0], matRotZ);
		MultiplyMatrixVector(tri.points[1], triRotatedZ.points[1], matRotZ);
		MultiplyMatrixVector(tri.points[2], triRotatedZ.points[2], matRotZ);
		
		MultiplyMatrixVector(triRotatedZ.points[0], triRotatedZX.points[0], matRotX);
		MultiplyMatrixVector(triRotatedZ.points[1], triRotatedZX.points[1], matRotX);
		MultiplyMatrixVector(triRotatedZ.points[2], triRotatedZX.points[2], matRotX);

		triTranslated = triRotatedZX;
		triTranslated.points[0].z = triRotatedZX.points[0].z + 8.0f;
		triTranslated.points[1].z = triRotatedZX.points[1].z + 8.0f;
		triTranslated.points[2].z = triRotatedZX.points[2].z + 8.0f;

		MultiplyMatrixVector(triTranslated.points[0], triProjected.points[0], matProj);
		MultiplyMatrixVector(triTranslated.points[1], triProjected.points[1], matProj);
		MultiplyMatrixVector(triTranslated.points[2], triProjected.points[2], matProj);

		//Scale into view
		triProjected.points[0].x += 1.0f; triProjected.points[0].y += 1.0f;
		triProjected.points[1].x += 1.0f; triProjected.points[1].y += 1.0f;
		triProjected.points[2].x += 1.0f; triProjected.points[2].y += 1.0f;

		triProjected.points[0].x *= .5f *buffer->width;
		triProjected.points[0].y *= .5f *buffer->height;
		triProjected.points[1].x *= .5f *buffer->width;
		triProjected.points[1].y *= .5f *buffer->height;
		triProjected.points[2].x *= .5f *buffer->width;
		triProjected.points[2].y *= .5f *buffer->height;

		Pair p1{ triProjected.points[0].x, triProjected.points[0].y };
		Pair p2{ triProjected.points[1].x, triProjected.points[1].y };
		Pair p3{ triProjected.points[2].x, triProjected.points[2].y };

		DrawTriangle(p1, p2, p3, 0xFF0000);
	}
}

/*int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{*/
int WINAPI WinMain(HINSTANCE instance,
	HINSTANCE previousInstance,
	LPSTR commandLine,
	int showCode)

{
	GlobalStartTime = time(0);
	testMesh.LoadMeshFromFile("Astronaut2.obj");
	WNDCLASSEXA windowClass = {};

	//win32_window_dimension dimensions = Win32GetWindowDimension(window);
	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

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
			"10",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			instance,
			0);

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


		if (window)
		{
			HDC deviceContext = GetDC(window);
			MSG message;

			GlobalRunning = true;

			int xOffset = 1, yOffset = 1;
			while (GlobalRunning)
			{
				GlobalDeltaTime = (float)difftime(time(0), GlobalStartTime);

				while (PeekMessage(&message, window, 0, 0, PM_REMOVE))
				{
					if (message.message == WM_QUIT)
					{
						GlobalRunning = false;
					}
					TranslateMessage(&message);
					DispatchMessageA(&message);

				}

				//Redraw the screen black!
				FillScreen();

				//Draw Mesh
				//DrawTriangle(Pair{ 50, 100 }, Pair{ 700, 250 }, Pair{ 300, 500 }, 0xFF0000);
				DrawMesh(testMesh, &GlobalBackBuffer);
				//DrawLine(Pair{ (int)abs(sin(xOffset)*500), (int)abs(sin(yOffset)*500) }, Pair{ 1+xOffset, 1+yOffset}, 0xFF0000);
				//WindowGradient(&GlobalBackBuffer, xOffset, yOffset);
				if (xOffset >= 1278) xOffset = 0;
				if (yOffset >= 718) yOffset = 0;

				xOffset += 2;
				yOffset += 1;
				win32_window_dimension dimensions = Win32GetWindowDimension(window);
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



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TEST3DRENDERER));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_TEST3DRENDERER);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

internal void 
Win32CopyBufferToWindow(HDC deviceContext,
	int windowWidth, int windowHeight,
	win32_offscreen_buffer *buffer)
{
	StretchDIBits(deviceContext,
		/*_x, _y, _width, _height,
		  _x, _y, _width, _height,*/
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
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
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

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
