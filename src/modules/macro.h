#pragma once
#include <string>
#include <vector>

namespace macro {
    enum MacroState {
        STATE_IDLE,
        STATE_ARMED_RECORD, // Ожидание спавна игрока для старта записи
        STATE_RECORDING,    // Активная запись на 0.20x
        STATE_ARMED_PLAY,   // Ожидание спавна игрока для старта воспроизведения
        STATE_PLAYING       // Активное воспроизведение на 1.00x
    };

    extern MacroState currentState;
    extern int targetTPS;           // 60, 120, 240 TPS
    extern float recordingTimeScale;// 0.20x
    extern float playbackTimeScale; // 1.00x

    void Init();
    void ClearCache();
    void Update();
    void DrawMenu();

    void ArmRecording();
    void StopRecording();
    void ArmPlayback();
    void StopPlayback();

    void SaveMacroToFile(const char* name);
    void LoadMacroFromFile(const char* name);
    void RefreshMacroList();
}