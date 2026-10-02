#pragma once

namespace flat_textures {
    extern bool flatEnabled;
    extern float flatColor[4];

    extern bool pixelateEnabled;
    extern int pixelateLevel;

    extern int randomizedTexturesCount;

    void Update();
    void DrawMenu();
    void ApplyRandomTextures();
    void RestoreOriginalTextures();
}