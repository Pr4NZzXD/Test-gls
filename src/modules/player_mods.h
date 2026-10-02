#pragma once

namespace player_mods {
    extern bool godmode;
    extern bool noFallDamage;
    extern bool fovChanger;
    extern float customFov;

    // Оружие
    extern bool infiniteAmmo;
    extern bool electricDarts;
    extern bool explosiveShotgun;
    extern bool showWeaponSettings;

    void* GetPlayerRootTransform();
    void* GetPlayerCameraTransform();
    void* GetPlayerCamPivotTransform();
    void Init();
    void ClearCache();
    void Update();
    void DrawMenu();
}