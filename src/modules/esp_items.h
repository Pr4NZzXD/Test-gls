#pragma once
#include <vector>
#include <string>

namespace esp_items {
    extern bool enabled;
    extern float width;
    extern float color[4];
    extern bool showSettings;

    struct ItemData {
        std::string name;
        void* gameObject;
        void* transform;
        bool customHighlight;
    };
    extern std::vector<ItemData> itemsList;

    void ClearCache();
    void Update();
    void DrawMenu();
    void TeleportItemToPlayer(void* itemTransform);
}