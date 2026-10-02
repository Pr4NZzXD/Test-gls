#include "modules.h"

namespace modules {
    void InitAll() {
        player_mods::Init();
        speedhack::Init();
        auto_farm::Init();
        aim::Init();
        Logger::Init();
        macro::Init();
    }

    void UpdateAll() {
        esp_enemies::Update();
        esp_items::Update();
        speedhack::Update();
        flat_textures::Update();
        player_mods::Update();
        game_unlocks::Update();
        auto_farm::Update();
        reshade::Update();
        scene_explorer::Update();
        aim::Update();
        macro::Update();
    }

    void DrawOverlays() {
        esp_enemies::DrawOverlay();
    }

    void ResetOnSceneChange() {
        player_mods::ClearCache();
        aim::ClearCache(); // <--- ДОБАВЬ
        esp_enemies::ClearCache();
        esp_items::ClearCache();
        speedhack::ClearCache();
        game_unlocks::ClearCache();
        auto_farm::ClearCache();
        scene_explorer::ClearCache();
        macro::ClearCache();
    }
}