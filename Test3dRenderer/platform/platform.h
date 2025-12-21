#pragma once

#include <stdint.h>

struct platform_window
{
	uint32_t width;
	uint32_t height;
	void* handle;
};

struct platform_input
{
	bool keys[256];
};

struct platform_data
{
	platform_input* platformInput;
	platform_window* platfprmWindow;
	backbuffer* backBuffer;
	float delta_time;
};