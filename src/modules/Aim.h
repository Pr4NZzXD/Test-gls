#pragma once

#include <vector>
#include <string>
#include <unordered_set>

namespace Modules {

    class Aim {
    public:
        static bool grannyAimEnabled;
        static bool onlyWithWeapon;
        static float grannySmoothness;
        static bool showGrannySettings;

        static bool aim_item_enabled;
        static bool auto_aim_on_distance;
        static float aim_item_distance;
        static float itemSmoothness;
        static bool showItemSettings;

        static std::unordered_set<std::string> target_item_list;

        struct LiveItemTarget {
            std::string name;
            void* transform;
            bool selected;
        };

        static std::vector<LiveItemTarget> liveItems;

        static void Update();
        static void ScanLiveItems();
        static void ClearCache();
        static bool IsItemInList(const std::string& itemName);
        static void ExportItemsToTxt();
        static void DrawMenu();
        static void Init();

    private:
        static void ProcessGrannyAim();
        static void ProcessItemAim();
    };
}