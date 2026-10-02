#pragma once
#include "esp_enemies.h"
#include "esp_items.h"
#include "speedhack.h"
#include "flat_textures.h"
#include "player_mods.h"
#include "game_unlocks.h"
#include "auto_farm.h"
#include "reshade.h"
#include "scene_explorer.h"
#include "logger.h"
#include "Aim.h"
#include "macro.h"

namespace modules {
    void InitAll();
    void UpdateAll();
    void DrawOverlays();
    void ResetOnSceneChange();
}