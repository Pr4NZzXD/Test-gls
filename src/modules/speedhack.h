#pragma once

namespace speedhack {
    extern bool enabled;
    extern float multiplier;

    void Init();
    void Update();
    void DrawMenu();
    void ClearCache();
}