#pragma once

namespace reshade {
    extern bool disableDarkerFog;
    extern bool disableLightmaps;

    void Update();
    void DrawMenu();
    void ApplyNoLightmaps(bool enable);
}