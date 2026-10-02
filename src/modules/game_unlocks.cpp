#include "game_unlocks.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <algorithm>
#include <vector>
#include <string>

namespace game_unlocks {
    bool enabled = false;
    bool showSettings = false;
    static bool s_LastEnabled = false;

    std::vector<UnlockOption> optionsList;
    std::unordered_map<std::string, bool> savedToggles;
    static char s_Filter[64] = "";
    static bool s_FoundUI = false;

    void ClearCache() {
        optionsList.clear();
        s_FoundUI = false;
    }

    void ScanSecondPart() {
        optionsList.clear();
        s_FoundUI = false;
        if (!IsSceneReady() || !oGameObjectFind || !il2cpp_string_new) return;

        const char* paths[] = {
            "UI/SecondPart",
            "Canvas/SecondPart",
            "SecondPart",
            "UI/Canvas/SecondPart"
        };
        void* secondPartGO = nullptr;

        for (const char* p : paths) {
            void* str = il2cpp_string_new(p);
            if (str) {
                void* go = oGameObjectFind(str);
                if (go && IsNativeObjectAlive(go)) {
                    secondPartGO = go;
                    break;
                }
            }
        }

        if (!secondPartGO || !oGameObjectGetTransform) return;
        void* spTr = oGameObjectGetTransform(secondPartGO);
        if (!spTr || !IsNativeObjectAlive(spTr)) return;

        s_FoundUI = true;
        int childCount = SafeGetChildCount(spTr);
        for (int i = 0; i < childCount; i++) {
            void* childTr = SafeGetChild(spTr, i);
            if (!childTr || !IsNativeObjectAlive(childTr)) continue;

            void* childGO = oComponentGetGameObject ? oComponentGetGameObject(childTr) : nullptr;
            if (!childGO || !IsNativeObjectAlive(childGO)) continue;

            std::string name = GetUnityObjectName(childGO);
            if (name.empty()) name = "Option_" + std::to_string(i);

            bool isAct = oGetGameObjectActive ? oGetGameObjectActive(childGO) : true;
            if (savedToggles.find(name) != savedToggles.end()) {
                isAct = savedToggles[name];
                if (enabled && oSetGameObjectActive) {
                    oSetGameObjectActive(childGO, isAct);
                }
            }

            optionsList.push_back({ name, childGO, isAct });
        }
    }

    void Update() {
        if (!IsSceneReady()) {
            ClearCache();
            return;
        }

        if (optionsList.empty()) {
            ScanSecondPart();
        }

        if (enabled != s_LastEnabled) {
            for (auto& opt : optionsList) {
                if (opt.gameObject && IsNativeObjectAlive(opt.gameObject) && oSetGameObjectActive) {
                    oSetGameObjectActive(opt.gameObject, enabled ? opt.active : false);
                }
            }
            s_LastEnabled = enabled;
        }
    }

    void DrawMenu() {
        DrawFeatureCardWithGear("game_unlocks",
            LOC("Разблокировка скрытых и вырезанных функций", "Unlock Hidden & Cut Settings"),
            LOC("Анлокает скрытые, устаревшие и недоступные функции", "Unlocks hidden, outdated & cut features"),
            &enabled, &showSettings);

        if (BeginSubSettingsAnim("game_unlocks", showSettings || enabled, 260.0f * g_UiScale)) {
            if (!s_FoundUI || optionsList.empty()) {
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOC("⚠️ Сначала зайдите в главное меню для сканирования UI/SecondPart!", "⚠️ Enter the main menu first to scan UI/SecondPart!"));
                ImGui::Spacing();
                if (ImGui::Button(LOC(" [↻] Повторить сканирование ", " [↻] Retry Scan Now "), ImVec2(-1, 32.0f * g_UiScale))) {
                    ScanSecondPart();
                }
            } else {
                ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s (%zu):", LOC("ДОСТУПНЫЕ МОДИФИКАТОРЫ", "AVAILABLE MODIFIERS"), optionsList.size());
                ImGui::Spacing();

                float availWidth = ImGui::GetContentRegionAvail().x;
                float btnW = (availWidth - 10.0f * g_UiScale) * 0.5f;

                if (ImGui::Button(LOC(" [✓] Включить все ", " [✓] Enable All "), ImVec2(btnW, 28.0f * g_UiScale))) {
                    for (auto& opt : optionsList) {
                        opt.active = true;
                        savedToggles[opt.name] = true;
                        if (enabled && opt.gameObject && IsNativeObjectAlive(opt.gameObject) && oSetGameObjectActive) {
                            oSetGameObjectActive(opt.gameObject, true);
                        }
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button(LOC(" [X] Выключить все ", " [X] Disable All "), ImVec2(btnW, 28.0f * g_UiScale))) {
                    for (auto& opt : optionsList) {
                        opt.active = false;
                        savedToggles[opt.name] = false;
                        if (opt.gameObject && IsNativeObjectAlive(opt.gameObject) && oSetGameObjectActive) {
                            oSetGameObjectActive(opt.gameObject, false);
                        }
                    }
                }

                ImGui::Spacing();
                ImGui::SetNextItemWidth(-1);
                ImGui::InputTextWithHint("##filterUnlocks", LOC("Поиск модификатора...", "Search modifier..."), s_Filter, sizeof(s_Filter));
                ImGui::Spacing();

                ImGui::BeginChild("##unlocksListChild", ImVec2(0, 130.0f * g_UiScale), true);
                for (size_t i = 0; i < optionsList.size(); i++) {
                    auto& opt = optionsList[i];

                    if (s_Filter[0] != '\0') {
                        std::string lName = opt.name;
                        std::string lFlt = s_Filter;
                        std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);
                        std::transform(lFlt.begin(), lFlt.end(), lFlt.begin(), ::tolower);
                        if (lName.find(lFlt) == std::string::npos) continue;
                    }

                    ImGui::PushID((int)i);
                    if (ImGui::Checkbox(opt.name.c_str(), &opt.active)) {
                        savedToggles[opt.name] = opt.active;
                        if (enabled && opt.gameObject && IsNativeObjectAlive(opt.gameObject) && oSetGameObjectActive) {
                            oSetGameObjectActive(opt.gameObject, opt.active);
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
            }

            EndSubSettingsAnim("game_unlocks");
        }
    }
}