#pragma once
#include <stdint.h>

struct backbuffer;

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

struct platform_file_result
{
	void* data;
	uint64_t size;
};

typedef platform_file_result platform_read_entire_file(const char* _filename);
typedef platform_file_result platform_read_entire_local_file(const char* _relativePath);
typedef void platform_free_file_memory(void* _memory);


struct platform_api
{
	platform_read_entire_file* ReadEntireFile;
	platform_read_entire_local_file* ReadEntireLocalFile;
	platform_free_file_memory* FreeFileMemory;
};