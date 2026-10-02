#include "esp_items.h"
#include "esp_enemies.h"
#include "player_mods.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>

namespace esp_items
{
    bool enabled = false;
    float width = 4.0f;
    float color[4] = { 0.0f, 1.0f, 0.2f, 1.0f };
    bool showSettings = false;

    std::vector<ItemData> itemsList;
    static std::unordered_map<std::string, bool> g_SavedItemToggles;
    static char itemFilter[64] = "";

    void ClearCache() {
        itemsList.clear();
    }

    void TeleportItemToPlayer(void* itemTransform) {
        if (!itemTransform || !IsNativeObjectAlive(itemTransform)) return;

        void* playerTr = player_mods::GetPlayerRootTransform();
        if (!playerTr || !IsNativeObjectAlive(playerTr) || !oTransformGetPosition || !oTransformSetPosition) return;

        Vector3 pPos{ 0, 0, 0 };
        oTransformGetPosition(playerTr, &pPos);

        void* mainCam = GetCurrentCamera();
        Vector3 fwd{ 0, 0, 1 };
        if (mainCam && IsNativeObjectAlive(mainCam) && oComponentGetTransform) {
            void* cTr = oComponentGetTransform(mainCam);
            if (cTr && oTransformGetForward) oTransformGetForward(cTr, &fwd);
        }

        Vector3 targetPos = {
            pPos.x + fwd.x * 0.8f,
            pPos.y + 0.2f,
            pPos.z + fwd.z * 0.8f
        };

        oTransformSetPosition(itemTransform, &targetPos);
    }

    void ScanActiveSeedManagerItems() {
        if (!IsSceneReady() || !g_FindObjectsOfTypeMethod) return;

        const char* mgrNames[] = {
            "SeedSystemManager_Normal",
            "SeedSystemManager_More",
            "SeedSystemManager_1.7",
            "SeedSystemManager_1.6",
            "SeedSystemManager_1.5",
            "SeedSystemManager",
            "SeedManager"
        };

        void* activeSeedMgrGO = nullptr;
        void* activeSeedMgrClass = nullptr;

        for (const char* name : mgrNames) {
            void* klass = FindClass("", name);
            if (!klass) continue;
            void* type = il2cpp_class_get_type(klass);
            if (!type) continue;
            void* typeObj = il2cpp_type_get_object(type);
            if (!typeObj) continue;

            std::vector<void*> comps = FindObjectsOfUnityType(typeObj);
            for (void* comp : comps) {
                if (!comp || !IsNativeObjectAlive(comp)) continue;
                void* go = oComponentGetGameObject ? oComponentGetGameObject(comp) : nullptr;
                if (!go || !IsNativeObjectAlive(go)) continue;

                bool isActive = oGetGameObjectActive ? oGetGameObjectActive(go) : true;
                if (isActive) {
                    activeSeedMgrGO = go;
                    activeSeedMgrClass = klass;
                    break;
                }
            }
            if (activeSeedMgrGO) break;
        }

        if (!activeSeedMgrGO || !activeSeedMgrClass) return;

        void* type = il2cpp_type_get_object(il2cpp_class_get_type(activeSeedMgrClass));
        void* exc = nullptr;
        void* args[1] = { type };
        void* mgrComp = il2cpp_runtime_invoke(g_GetComponentMethod, activeSeedMgrGO, args, &exc);
        if (!mgrComp || !IsNativeObjectAlive(mgrComp)) return;

        MonoList* allItemsList = nullptr;
        void* field = il2cpp_class_get_field_from_name(activeSeedMgrClass, "allItems");
        if (field) {
            size_t offset = il2cpp_field_get_offset(field);
            allItemsList = *(MonoList**)((uintptr_t)mgrComp + offset);
        } else {
            allItemsList = *(MonoList**)((uintptr_t)mgrComp + 0x20);
        }

        if (!allItemsList || !allItemsList->items) return;

        MonoArray* itemsArray = allItemsList->items;
        int32_t count = allItemsList->size;

        itemsList.clear();
        for (int32_t i = 0; i < count; i++) {
            void* itemGO = itemsArray->vector[i];
            if (!itemGO || !IsNativeObjectAlive(itemGO)) continue;

            void* itemTr = oGameObjectGetTransform ? oGameObjectGetTransform(itemGO) : nullptr;
            std::string name = GetUnityObjectName(itemGO);
            if (name.empty()) name = "Item_" + std::to_string(i);

            bool highlightState = true;
            if (g_SavedItemToggles.find(name) != g_SavedItemToggles.end()) {
                highlightState = g_SavedItemToggles[name];
            }

            itemsList.push_back({ name, itemGO, itemTr, highlightState });
        }
    }

    void Update() {
        if (!IsSceneReady()) {
            ClearCache();
            return;
        }

        // Проверяем живы ли предметы прошлой сцены
        bool hasDeadItems = false;
        for (auto& item : itemsList) {
            if (!item.gameObject || !IsNativeObjectAlive(item.gameObject)) {
                hasDeadItems = true;
                break;
            }
        }

        if (hasDeadItems || itemsList.empty()) {
            ClearCache();
            ScanActiveSeedManagerItems();
        }

        for (auto& item : itemsList) {
            if (item.gameObject && IsNativeObjectAlive(item.gameObject)) {
                bool shouldHighlight = enabled && item.customHighlight;
                SetOutlineOnObject(item.gameObject, shouldHighlight, color, width);
            }
        }
    }

    void DrawMenu()
    {
        DrawFeatureCardWithGear("esp_items",
            LOC("Валлхак на предметы (ESP)", "Items ESP"),
            LOC("Подсветка предметов сида и сбор лута", "Wallhack Chams on seed items"),
            &enabled, &showSettings);

        if (BeginSubSettingsAnim("esp_items", showSettings, 340.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), "%s", LOC("НАСТРОЙКИ КОНТУРА:", "ITEMS OUTLINE SETTINGS:"));
            DrawSliderWithInput("##itemsWidth", &width, 1.0f, 15.0f, "%.1f px");
            ImGui::ColorEdit4("Color##items", color, ImGuiColorEditFlags_NoInputs);

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s (%zu):", LOC("АКТИВНЫЕ ПРЕДМЕТЫ", "ACTIVE ITEMS"), itemsList.size());

            float availWidth = ImGui::GetContentRegionAvail().x;
            float btnW = (availWidth - 16.0f * g_UiScale) / 3.0f;

            if (ImGui::Button(LOC("Выбрать все", "Select All"), ImVec2(btnW, 28 * g_UiScale))) {
                for (auto& itm : itemsList) {
                    itm.customHighlight = true;
                    g_SavedItemToggles[itm.name] = true;
                }
            }
            ImGui::SameLine(0, 8.0f * g_UiScale);
            if (ImGui::Button(LOC("Снять все", "Unselect"), ImVec2(btnW, 28 * g_UiScale))) {
                for (auto& itm : itemsList) {
                    itm.customHighlight = false;
                    g_SavedItemToggles[itm.name] = false;
                }
            }
            ImGui::SameLine(0, 8.0f * g_UiScale);
            if (ImGui::Button("[TP ALL]", ImVec2(btnW, 28 * g_UiScale))) {
                for (auto& itm : itemsList) {
                    if (itm.transform) TeleportItemToPlayer(itm.transform);
                }
            }

            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##filterItems", LOC("Поиск предмета...", "Search item..."), itemFilter, sizeof(itemFilter));
            ImGui::Spacing();

            ImGui::BeginChild("##itemsScrollList", ImVec2(0, 140.0f * g_UiScale), true);
            float listAvailW = ImGui::GetContentRegionAvail().x;
            float tpBtnW = 55.0f * g_UiScale;
            float nameMaxW = listAvailW - tpBtnW - 40.0f * g_UiScale;

            for (size_t idx = 0; idx < itemsList.size(); idx++) {
                auto& itm = itemsList[idx];

                if (itemFilter[0] != '\0') {
                    std::string lName = itm.name;
                    std::string lFlt = itemFilter;
                    std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);
                    std::transform(lFlt.begin(), lFlt.end(), lFlt.begin(), ::tolower);
                    if (lName.find(lFlt) == std::string::npos) continue;
                }

                ImGui::PushID((int)idx);
                if (ImGui::Checkbox("##itmTgl", &itm.customHighlight)) {
                    g_SavedItemToggles[itm.name] = itm.customHighlight;
                }
                ImGui::SameLine(0, 8.0f * g_UiScale);

                ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + nameMaxW);
                ImGui::TextColored(itm.customHighlight ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", itm.name.c_str());
                ImGui::PopTextWrapPos();

                ImGui::SameLine(listAvailW - tpBtnW - 6.0f * g_UiScale);
                if (ImGui::Button("TP", ImVec2(tpBtnW, 22 * g_UiScale))) {
                    if (itm.transform) TeleportItemToPlayer(itm.transform);
                }
                ImGui::PopID();
            }
            ImGui::EndChild();

            EndSubSettingsAnim("esp_items");
        }
    }
}