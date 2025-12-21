#pragma once
#include "../renderer/renderer.h"
#include <windows.h>

#define MAX_LOADSTRING 100

struct win32_offscreen_buffer
{
    backbuffer buffer; // <-- shared layout
    BITMAPINFO info;
};

struct win32_window_dimension
{
	int height;
	int width;
};
