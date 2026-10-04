#pragma once
#include <string>
#include <vector>

namespace speedrun {
    // Enum & Struktur untuk RNG Spawn
    enum class EntityType {
        GRANNY,
        GRANDPA
    };

    struct EntitySpawnPos {
        std::string name;
        int index;
    };

    // Variabel state menu
    extern bool autoUnlockShop;
    extern bool restartButton;
    extern float restartPosX;
    extern float restartPosY;
    
    extern int ratChoice;
    extern int momRoute;
    extern int vaseChoice;
    extern int vaseCount;
    extern bool extraTraps;
    extern bool lavaMode;

    // Variabel state RNG Entity
    extern int grannySpawnChoice;
    extern int grandpaSpawnChoice;

    const std::vector<EntitySpawnPos>& GetAvailableSpawnPos(EntityType type);

    void Init();
    void ClearCache();
    void Update();
    void DrawMenu(float colW, float colH, float colGap);
    void DrawRestartButton();
    void RestartRun(bool allowWhilePaused);
    
    bool IsGameplay();
    bool IsPaused();
}
