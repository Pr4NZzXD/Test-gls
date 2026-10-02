#pragma once
#include <vector>
#include <string>
#include <unordered_map>

namespace game_unlocks {
    struct UnlockOption {
        std::string name;
        void* gameObject;
        bool active;
    };

    extern bool enabled;
    extern bool showSettings;
    extern std::vector<UnlockOption> optionsList;
    extern std::unordered_map<std::string, bool> savedToggles;

    void ScanSecondPart();
    void Update();
    void DrawMenu();
    void ClearCache();
}