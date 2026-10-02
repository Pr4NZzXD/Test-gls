#pragma once
#include <vector>

namespace esp_enemies {
    extern bool enabled;

    extern bool targetGranny;
    extern bool targetGrandpa;
    extern bool targetSpiderMom;

    extern bool drawBox;
    extern int  boxType;
    extern bool boxFill;
    extern bool drawName;
    extern bool drawDistance;
    extern bool drawTracers;
    extern bool enableChams;

    // Калибровка под реальные габариты Granny
    extern float enemyHeight;
    extern float enemyWidthMul;
    extern float yOffset;
    extern float spiderHeight;
    extern float spiderWidthMul;

    extern float outlineWidth;
    extern float color[4];
    extern float tracerColor[4];
    extern bool showSettings;

    void ClearCache();
    void Update();
    void DrawOverlay();
    void DrawMenuCard();
}