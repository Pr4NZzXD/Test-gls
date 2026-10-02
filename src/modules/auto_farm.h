#pragma once
#include <string>

namespace auto_farm {
    extern bool enabled;
    extern bool showSettings;
    extern int escapeMethod; // 0 = Door, 1 = Car, 2 = Cellar, 3 = Robo
    extern float fastForwardSpeed;

    // НАСТРОЙКИ СКОРОСТИ ЦИКЛА
    extern float menuDelay;      // Задержка в меню (сек)
    extern float cutsceneDelay;  // Задержка ускоренной победы (сек)

    extern int totalWins;

    void Init();
    void Update();
    void DrawMenu();
    void ClearCache();
    bool TriggerInstantWin();
    void LoadSceneByName(const char* sceneName);
    std::string GetCurrentSceneName();
}