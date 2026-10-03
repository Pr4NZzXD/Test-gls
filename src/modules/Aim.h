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
    extern float itemAimDistance;   // aim item hanya aktif jika jarak pemain-item <= nilai ini (meter)

    struct LiveItemTarget {
        std::string name;
        void* transform;
        bool selected;
        std::string baseName;   // nama tanpa "(Clone)" dipakai oleh filter item
    };
    extern std::vector<LiveItemTarget> liveItems;

    void Init();
    void ClearCache();
    void Update();
    void DrawMenu();
    void ScanLiveItems();
    void ExportItemsToTxt();

    // Filter jenis item (centang = item itu boleh di-aim)
    std::vector<std::string> ItemTypeNames();
    bool* ItemTypeEnabledPtr(const std::string& name);
    void SetItemTypeEnabled(const std::string& name, bool enabled);
    void SetAllItemTypes(bool enabled);
    std::string PrettyItemName(const std::string& name);
}