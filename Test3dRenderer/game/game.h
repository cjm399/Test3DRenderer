#pragma once

#include "../renderer/renderer.h"
#include "../platform/platform.h"

void Init(platform_data& _platformData);

void game_update_and_render(platform_data& _platformData);