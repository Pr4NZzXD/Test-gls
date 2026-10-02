#pragma once
#include <vector>
#include <string>

namespace aim {
    // Аимбот на Бабку
    extern bool grannyAimEnabled;
    extern bool onlyWithWeapon;
    extern float grannySmoothness;
    extern bool showGrannySettings;

    // Аимбот на предметы
    extern bool itemAimEnabled;
    extern float itemSmoothness;
    extern bool showItemSettings;

    struct LiveItemTarget {
        std::string name;
        void* transform;
        bool selected;
    };
    extern std::vector<LiveItemTarget> liveItems;

    void Init();
    void ClearCache();
    void Update();
    void DrawMenu();
    void ScanLiveItems();
    void ExportItemsToTxt();
}