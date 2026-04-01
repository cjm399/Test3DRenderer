#pragma once

#include "../platform/platform.h"
#include "../renderer/renderer.h"

void Init(platform_data& _platformData, platform_api _platformAPI);

void game_update_and_render(platform_data& _platformData);