#include "Aim.h"
#include "player_mods.h"
#include "il2cpp_api.h"
#include "logger.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <cstdio>

namespace aim {
    // Бабка
    bool grannyAimEnabled = true;
    bool onlyWithWeapon = false;
    float grannySmoothness = 30.0f;
    bool showGrannySettings = false;

    // Предметы
    bool itemAimEnabled = false;
    float itemSmoothness = 30.0f;
    bool showItemSettings = false;
    std::vector<LiveItemTarget> liveItems;
    static void* s_LockedItemTr = nullptr;
    static void* s_HandHoldTr = nullptr;
    static char s_ItemFilter[64] = "";
    static std::string s_Status = "";
    static float s_StatusTimer = 0.0f;

    static void* s_CachedGrannyHeadTr = nullptr;
    static void* s_CachedMobileFPS = nullptr;
    static void* s_MobileFPSClass = nullptr;

    void ClearCache() {
        s_CachedGrannyHeadTr = nullptr;
        s_CachedMobileFPS = nullptr;
        s_MobileFPSClass = nullptr;
        s_LockedItemTr = nullptr;
        s_HandHoldTr = nullptr;
        liveItems.clear();
    }

    void Init() {
        ClearCache();
        Logger::Log("AIM", LOG_OK, "aim::Init() -> Aim module initialized.");
    }

    // ПОИСК ОБЪЕКТА ПО ПУТИ
    void* FindTransformByPath(const char* fullPath) {
        if (!fullPath || !oGameObjectFind || !il2cpp_string_new) return nullptr;

        std::string pathStr = fullPath;
        std::vector<std::string> parts;
        size_t start = 0, end = 0;
        while ((end = pathStr.find('/', start)) != std::string::npos) {
            parts.push_back(pathStr.substr(start, end - start));
            start = end + 1;
        }
        parts.push_back(pathStr.substr(start));

        if (parts.empty()) return nullptr;

        void* rootStr = il2cpp_string_new(parts[0].c_str());
        if (!rootStr) return nullptr;
        void* curGO = oGameObjectFind(rootStr);
        if (!curGO || !IsNativeObjectAlive(curGO) || !oGameObjectGetTransform) return nullptr;
        void* curTr = oGameObjectGetTransform(curGO);

        for (size_t i = 1; i < parts.size(); i++) {
            if (!curTr || !IsNativeObjectAlive(curTr)) return nullptr;
            int childCount = SafeGetChildCount(curTr);
            void* nextTr = nullptr;
            for (int c = 0; c < childCount; c++) {
                void* childTr = SafeGetChild(curTr, c);
                if (!childTr || !IsNativeObjectAlive(childTr)) continue;
                std::string cName = GetUnityObjectName(childTr);
                if (cName == parts[i]) {
                    nextTr = childTr;
                    break;
                }
            }
            if (!nextTr) return nullptr;
            curTr = nextTr;
        }
        return curTr;
    }

    void* GetActiveMobileFPS() {
        if (s_CachedMobileFPS && IsNativeObjectAlive(s_CachedMobileFPS)) return s_CachedMobileFPS;
        void* playerTr = player_mods::GetPlayerRootTransform();
        if (playerTr && g_GetComponentMethod) {
            void* playerGO = oComponentGetGameObject ? oComponentGetGameObject(playerTr) : nullptr;
            if (playerGO) {
                if (!s_MobileFPSClass) s_MobileFPSClass = FindClass("", "MobileFPS");
                if (s_MobileFPSClass && il2cpp_class_get_type && il2cpp_type_get_object) {
                    void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(s_MobileFPSClass));
                    if (typeObj) {
                        void* args[1] = { typeObj };
                        void* exc = nullptr;
                        s_CachedMobileFPS = il2cpp_runtime_invoke(g_GetComponentMethod, playerGO, args, &exc);
                        return s_CachedMobileFPS;
                    }
                }
            }
        }
        return nullptr;
    }

    bool IsPlayerHoldingWeapon() {
        const char* weaponPaths[] = {
            "PlayerStuff/HandHoldObjects/CrossbowHand",
            "PlayerStuff/HandHoldObjects/OldShotgunHand",
            "PlayerStuff/HandHoldObjects/PepperSprayHand",
            "PlayerStuff/HandHoldObjects/ShotgunHand"
        };

        for (const char* path : weaponPaths) {
            void* tr = FindTransformByPath(path);
            if (tr && IsNativeObjectAlive(tr)) {
                void* go = oComponentGetGameObject ? oComponentGetGameObject(tr) : nullptr;
                if (go && IsNativeObjectAlive(go) && oGetGameObjectActive && oGetGameObjectActive(go)) {
                    return true;
                }
            }
        }
        return false;
    }

    // Cek apakah pemain sedang memegang item/senjata apa pun di tangan
    bool IsPlayerHoldingAnyItem() {
        if (!s_HandHoldTr || !IsNativeObjectAlive(s_HandHoldTr)) {
            s_HandHoldTr = FindTransformByPath("PlayerStuff/HandHoldObjects");
        }
        if (!s_HandHoldTr || !IsNativeObjectAlive(s_HandHoldTr)) return false;

        int childCount = SafeGetChildCount(s_HandHoldTr);
        for (int i = 0; i < childCount; i++) {
            void* childTr = SafeGetChild(s_HandHoldTr, i);
            if (!childTr || !IsNativeObjectAlive(childTr)) continue;
            void* go = oComponentGetGameObject ? oComponentGetGameObject(childTr) : nullptr;
            if (go && IsNativeObjectAlive(go) && oGetGameObjectActive && oGetGameObjectActive(go)) {
                return true;
            }
        }
        return false;
    }

    void* GetGrannyHeadTransform() {
        if (s_CachedGrannyHeadTr && IsNativeObjectAlive(s_CachedGrannyHeadTr)) {
            return s_CachedGrannyHeadTr;
        }

        void* headTr = FindTransformByPath("Entities/Granny-Stuff/GrannyParent/GrannyHeadPos");
        if (headTr && IsNativeObjectAlive(headTr)) {
            s_CachedGrannyHeadTr = headTr;
            Logger::Log("AIM", LOG_OK, "GrannyHeadPos resolved via path: 0x%lx", (uintptr_t)headTr);
            return s_CachedGrannyHeadTr;
        }

        if (oGameObjectFind && il2cpp_string_new && oGameObjectGetTransform) {
            void* str = il2cpp_string_new("GrannyHeadPos");
            if (str) {
                void* go = oGameObjectFind(str);
                if (go && IsNativeObjectAlive(go)) {
                    s_CachedGrannyHeadTr = oGameObjectGetTransform(go);
                    Logger::Log("AIM", LOG_OK, "GrannyHeadPos resolved via short name: 0x%lx", (uintptr_t)s_CachedGrannyHeadTr);
                    return s_CachedGrannyHeadTr;
                }
            }
        }

        return nullptr;
    }

    // ==============================================================
    // СКАНИРОВАНИЕ ВСЕХ ЖИВЫХ ПРЕДМЕТОВ В СЦЕНЕ
    // ==============================================================
    void ScanLiveItems() {
        liveItems.clear();
        if (!IsSceneReady()) return;

        std::unordered_set<void*> addedTransforms;

        // 1. Objects/MainItemSelection/MapItems
        if (oGameObjectFind && il2cpp_string_new) {
            const char* containerPaths[] = {
                "Objects/MainItemSelection/MapItems",
                "Objects/MainItemSelection",
                "Objects/MapItems"
            };

            for (const char* path : containerPaths) {
                void* str = il2cpp_string_new(path);
                if (!str) continue;

                void* containerGO = oGameObjectFind(str);
                if (containerGO && IsNativeObjectAlive(containerGO) && oGameObjectGetTransform) {
                    void* containerTr = oGameObjectGetTransform(containerGO);
                    int childCount = SafeGetChildCount(containerTr);

                    for (int i = 0; i < childCount; i++) {
                        void* cTr = SafeGetChild(containerTr, i);
                        if (!cTr || !IsNativeObjectAlive(cTr)) continue;

                        void* cGO = oComponentGetGameObject ? oComponentGetGameObject(cTr) : nullptr;
                        bool isAct = (cGO && oGetGameObjectActive) ? oGetGameObjectActive(cGO) : true;

                        if (isAct) {
                            std::string cName = GetUnityObjectName(cTr);
                            if (!cName.empty() && addedTransforms.find(cTr) == addedTransforms.end()) {
                                addedTransforms.insert(cTr);
                                liveItems.push_back({ cName, cTr, true });
                            }
                        }
                    }
                }
            }
        }

        // 2. Клоны и выпавшие предметы
        if (oGameObjectFind && il2cpp_string_new) {
            const char* itemNames[] = {
                "Crossbow", "Shotgun", "Hammer", "PadlockKey", "PassCode", "Pliers", "SafeKey",
                "ExitKey", "MasterKey", "CuttingPliers", "Battery", "DoorKey", "CarKey", "SparkPlug",
                "CarBattery", "Gasoline", "Wrench", "EnginePart", "CellarKey", "Fuse", "WinchHandle",
                "Cogwheel", "SpecialKey", "PlayhouseKey", "WeaponKey", "RemoteControl", "BirdSeed", "Meat",
                "Passcode", "PadlockCode", "Watermelon", "Vase", "Vase_01"
            };

            for (const char* baseName : itemNames) {
                std::string cloneName = std::string(baseName) + "(Clone)";
                const char* toFind[2] = { baseName, cloneName.c_str() };

                for (int f = 0; f < 2; f++) {
                    void* str = il2cpp_string_new(toFind[f]);
                    if (!str) continue;

                    void* go = oGameObjectFind(str);
                    if (go && IsNativeObjectAlive(go)) {
                        bool isAct = oGetGameObjectActive ? oGetGameObjectActive(go) : true;
                        if (isAct) {
                            void* tr = oGameObjectGetTransform ? oGameObjectGetTransform(go) : nullptr;
                            if (tr && IsNativeObjectAlive(tr) && addedTransforms.find(tr) == addedTransforms.end()) {
                                addedTransforms.insert(tr);
                                liveItems.push_back({ toFind[f], tr, true });
                            }
                        }
                    }
                }
            }
        }
    }

    void ExportItemsToTxt() {
        std::string exportText = "# Kahanium Map Items List:\n";
        int count = 0;

        for (auto& itm : liveItems) {
            if (itm.selected) {
                exportText += itm.name + "\n";
                count++;
            }
        }

        CopyToAndroidClipboard(exportText.c_str());

        std::string folder = "/sdcard/Kahanium";
        std::string path = folder + "/aim_items.txt";
        FILE* f = fopen(path.c_str(), "w");
        if (!f) {
            folder = "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files";
            path = folder + "/aim_items.txt";
            f = fopen(path.c_str(), "w");
        }

        if (f) {
            fprintf(f, "%s", exportText.c_str());
            fclose(f);
        }

        s_Status = "✓ " + std::to_string(count) + " предм. скопировано в буфер!";
        s_StatusTimer = 3.5f;
    }

    // ==============================================================
    // ТОЧНЫЙ ВЫЗОВ TRANSFORM.LOOKAT (ИЗ ТВОЕГО РАБОЧЕГО КОДА)
    // ==============================================================
    void SafeLookAt(void* tr, void* targetTransform, Vector3 targetWorldPos) {
        if (!tr || !IsNativeObjectAlive(tr)) return;

        void* trClass = FindClass("UnityEngine", "Transform");
        if (!trClass) return;

        // 1. Пробуем Transform.LookAt(Transform target)
        if (targetTransform && IsNativeObjectAlive(targetTransform)) {
            void* lookAtTrMethod = il2cpp_class_get_method_from_name(trClass, "LookAt", 1);
            if (lookAtTrMethod) {
                void* args[1] = { targetTransform };
                void* exc = nullptr;
                il2cpp_runtime_invoke(lookAtTrMethod, tr, args, &exc);
                if (!exc) return;
            }
        }

        // 2. Резерв: Transform.LookAt(Vector3 worldPosition, Vector3 worldUp)
        void* lookAtVecMethod = il2cpp_class_get_method_from_name(trClass, "LookAt", 2);
        if (lookAtVecMethod) {
            Vector3 up{ 0, 1, 0 };
            void* args[2] = { &targetWorldPos, &up };
            void* exc = nullptr;
            il2cpp_runtime_invoke(lookAtVecMethod, tr, args, &exc);
            if (!exc) return;
        }
    }

    // НАВЕДЕНИЕ НА ЛЮБОЙ ТРАНСФОРМ ЧЕРЕЗ SAFELOOKAT
    void AimAtTarget(void* targetTr, void* playerTr, void* camTr, void* camPivotTr, void* mfps) {
        if (!targetTr || !IsNativeObjectAlive(targetTr)) return;

        Vector3 camPos{ 0, 0, 0 };
        Vector3 targetPos{ 0, 0, 0 };
        Vector3 playerPos{ 0, 0, 0 };

        if (oTransformGetPosition) {
            oTransformGetPosition(camTr, &camPos);
            oTransformGetPosition(targetTr, &targetPos);
            oTransformGetPosition(playerTr, &playerPos);
        }

        Vector3 delta = { targetPos.x - camPos.x, targetPos.y - camPos.y, targetPos.z - camPos.z };
        float distXZ = sqrtf(delta.x * delta.x + delta.z * delta.z);
        if (distXZ < 0.05f) return;

        float targetPitch = -atan2f(delta.y, distXZ) * 57.2957795f;
        targetPitch = std::clamp(targetPitch, -82.0f, 82.0f);

        // 1. Поворот тела игрока по горизонтали ровно на цель
        Vector3 flatTarget = { targetPos.x, playerPos.y, targetPos.z };
        SafeLookAt(playerTr, nullptr, flatTarget);

        // 2. Поворот головы и камеры на объект цели
        if (camPivotTr && IsNativeObjectAlive(camPivotTr)) {
            SafeLookAt(camPivotTr, targetTr, targetPos);
        }
        SafeLookAt(camTr, targetTr, targetPos);

        // 3. Синхронизируем MobileFPS
        if (mfps && s_MobileFPSClass && IsNativeObjectAlive(mfps)) {
            SetFieldFloat(mfps, s_MobileFPSClass, "rotationX", targetPitch);
            SetFieldFloat(mfps, s_MobileFPSClass, "lookDelta", 0.0f);
        }
    }

    // ==============================================================
    // ГЛАВНЫЙ ЦИКЛ ОБНОВЛЕНИЯ (КАЖДЫЙ КАДР)
    // ==============================================================
    void Update() {
        if (!IsSceneReady()) return;

        void* playerTr = player_mods::GetPlayerRootTransform();
        void* camTr = player_mods::GetPlayerCameraTransform();
        void* camPivotTr = player_mods::GetPlayerCamPivotTransform();
        void* mfps = GetActiveMobileFPS();

        if (!playerTr || !IsNativeObjectAlive(playerTr) || !camTr || !IsNativeObjectAlive(camTr)) {
            return;
        }

        // 1. АИМБОТ НА ГОЛОВУ БАБКИ
        if (grannyAimEnabled) {
            bool canAimGranny = true;
            if (onlyWithWeapon && !IsPlayerHoldingWeapon()) {
                canAimGranny = false;
            }

            if (canAimGranny) {
                void* headTr = GetGrannyHeadTransform();
                if (headTr && IsNativeObjectAlive(headTr)) {
                    AimAtTarget(headTr, playerTr, camTr, camPivotTr, mfps);
                    return;
                }
            }
        }

        // 2. АИМБОТ НА ПРЕДМЕТЫ
        // Jika item sudah dipegang pemain, lepas kunci dan biarkan kamera bebas
        if ((itemAimEnabled || s_LockedItemTr) && IsPlayerHoldingAnyItem()) {
            s_LockedItemTr = nullptr;
            return;
        }

        if (s_LockedItemTr && IsNativeObjectAlive(s_LockedItemTr)) {
            void* itemGO = oComponentGetGameObject ? oComponentGetGameObject(s_LockedItemTr) : nullptr;
            if (itemGO && oGetGameObjectActive && !oGetGameObjectActive(itemGO)) {
                s_LockedItemTr = nullptr; // Сброс при подборе
            } else {
                AimAtTarget(s_LockedItemTr, playerTr, camTr, camPivotTr, mfps);
                return;
            }
        }

        if (itemAimEnabled) {
            if (liveItems.empty()) {
                ScanLiveItems();
            }

            Vector3 camPos{ 0, 0, 0 };
            if (oTransformGetPosition) oTransformGetPosition(camTr, &camPos);

            void* bestTarget = nullptr;
            float bestDistSq = 99999.0f;

            for (auto& itm : liveItems) {
                if (!itm.selected || !itm.transform || !IsNativeObjectAlive(itm.transform)) continue;

                void* itemGO = oComponentGetGameObject ? oComponentGetGameObject(itm.transform) : nullptr;
                if (itemGO && oGetGameObjectActive && !oGetGameObjectActive(itemGO)) continue;

                Vector3 iPos{ 0, 0, 0 };
                if (oTransformGetPosition) oTransformGetPosition(itm.transform, &iPos);

                float d = (iPos.x - camPos.x)*(iPos.x - camPos.x) + (iPos.y - camPos.y)*(iPos.y - camPos.y) + (iPos.z - camPos.z)*(iPos.z - camPos.z);
                if (d < bestDistSq) {
                    bestDistSq = d;
                    bestTarget = itm.transform;
                }
            }

            if (bestTarget) {
                AimAtTarget(bestTarget, playerTr, camTr, camPivotTr, mfps);
            }
        }
    }

    void DrawMenu() {
        // КАРТОЧКА АИМА НА БАБКУ
        DrawFeatureCardWithGear("granny_head_aim",
            LOC("Аимбот на Бабку (Granny Aimlock)", "Granny Aimlock (Head Track)"),
            LOC("Непрерывно наводит прицел в голову Бабки каждый кадр", "Locks camera directly on Granny's head every frame"),
            &grannyAimEnabled, &showGrannySettings);

        if (BeginSubSettingsAnim("granny_head_aim", showGrannySettings || grannyAimEnabled, 120.0f * g_UiScale)) {
            ImGui::Checkbox(LOC("Наводиться только при оружии в руках", "Only when holding weapon"), &onlyWithWeapon);

            ImGui::Text("%s:", LOC("Скорость / Плавность доводки", "Aim Speed & Smoothness"));
            DrawSliderWithInput("##aimSmoothGranny", &grannySmoothness, 5.0f, 50.0f, "%.0f");

            void* head = GetGrannyHeadTransform();
            if (head && IsNativeObjectAlive(head)) {
                Vector3 hPos{ 0, 0, 0 };
                if (oTransformGetPosition) oTransformGetPosition(head, &hPos);
                void* camTr = player_mods::GetPlayerCameraTransform();
                Vector3 cPos{ 0, 0, 0 };
                if (camTr && oTransformGetPosition) oTransformGetPosition(camTr, &cPos);
                float dist = sqrtf((hPos.x - cPos.x)*(hPos.x - cPos.x) + (hPos.y - cPos.y)*(hPos.y - cPos.y) + (hPos.z - cPos.z)*(hPos.z - cPos.z));

                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s (%.1f м)", LOC("🎯 Голова Бабки захвачена", "🎯 Granny Head Locked"), dist);
            }

            EndSubSettingsAnim("granny_head_aim");
        }

        // КАРТОЧКА АИМА НА ПРЕДМЕТЫ
        DrawFeatureCardWithGear("item_aimbot_card",
            LOC("Аимбот на предметы (Auto-Look)", "Item Aimbot (Auto-Look)"),
            LOC("Плавная наводка камеры на выбранные предметы карты", "Smoothly aims camera towards selected live items"),
            &itemAimEnabled, &showItemSettings);

        if (BeginSubSettingsAnim("item_aimbot_card", showItemSettings || itemAimEnabled, 340.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.4f, 1.0f), "%s", LOC("НАСТРОЙКА НАВОДКИ НА ПРЕДМЕТЫ:", "ITEM AIM ACTIONS:"));
            DrawSliderWithInput("##itemAimSmooth", &itemSmoothness, 5.0f, 50.0f, "%.0f");

            ImGui::Spacing();
            float availWidth = ImGui::GetContentRegionAvail().x;
            float btnW = (availWidth - 16.0f * g_UiScale) / 3.0f;

            if (ImGui::Button(LOC("Выбрать все", "Select All"), ImVec2(btnW, 26 * g_UiScale))) {
                for (auto& itm : liveItems) itm.selected = true;
            }
            ImGui::SameLine(0, 8.0f * g_UiScale);
            if (ImGui::Button(LOC("Снять все", "Unselect"), ImVec2(btnW, 26 * g_UiScale))) {
                for (auto& itm : liveItems) itm.selected = false;
                s_LockedItemTr = nullptr;
            }
            ImGui::SameLine(0, 8.0f * g_UiScale);
            if (ImGui::Button(LOC("Обновить", "Rescan"), ImVec2(btnW, 26 * g_UiScale))) {
                ScanLiveItems();
            }

            ImGui::Spacing();
            if (ImGui::Button(LOC(" 📁 Экспорт списка в буфер и .txt ", " 📁 Export Selected List to .txt & Clipboard "), ImVec2(-1, 30.0f * g_UiScale))) {
                ExportItemsToTxt();
            }

            if (s_StatusTimer > 0.0f) {
                s_StatusTimer -= ImGui::GetIO().DeltaTime;
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", s_Status.c_str());
            }

            ImGui::Spacing();
            ImGui::SetNextItemWidth(-1);
            ImGui::InputTextWithHint("##filterItemsAim", LOC("Поиск предмета...", "Filter items..."), s_ItemFilter, sizeof(s_ItemFilter));
            ImGui::Spacing();

            ImGui::BeginChild("##aimLiveItemsList", ImVec2(0, 140.0f * g_UiScale), true);
            float listAvailW = ImGui::GetContentRegionAvail().x;
            float lookBtnW = 75.0f * g_UiScale;
            float nameMaxW = listAvailW - lookBtnW - 40.0f * g_UiScale;

            for (size_t idx = 0; idx < liveItems.size(); idx++) {
                auto& itm = liveItems[idx];

                if (s_ItemFilter[0] != '\0') {
                    std::string lName = itm.name;
                    std::string lFlt = s_ItemFilter;
                    std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);
                    std::transform(lFlt.begin(), lFlt.end(), lFlt.begin(), ::tolower);
                    if (lName.find(lFlt) == std::string::npos) continue;
                }

                ImGui::PushID((int)idx);
                ImGui::Checkbox("##aimTglItem", &itm.selected);
                ImGui::SameLine(0, 6.0f * g_UiScale);

                ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + nameMaxW);
                ImGui::TextColored(itm.selected ? ImVec4(0.3f, 1.0f, 0.4f, 1.0f) : ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", itm.name.c_str());
                ImGui::PopTextWrapPos();

                ImGui::SameLine(listAvailW - lookBtnW - 6.0f * g_UiScale);
                bool isLocked = (s_LockedItemTr == itm.transform);
                if (isLocked) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.55f, 0.25f, 1.0f));
                }

                if (ImGui::Button(isLocked ? LOC("🎯 Держит", "🎯 Lock") : LOC("👁 Навести", "👁 Look"), ImVec2(lookBtnW, 22 * g_UiScale))) {
                    if (isLocked) {
                        s_LockedItemTr = nullptr;
                    } else {
                        s_LockedItemTr = itm.transform;
                    }
                }

                if (isLocked) {
                    ImGui::PopStyleColor();
                }

                ImGui::PopID();
            }

            if (liveItems.empty()) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LOC("Нажмите [Обновить] для поиска предметов в MapItems.", "Click [Rescan] to find items."));
            }

            ImGui::EndChild();

            EndSubSettingsAnim("item_aimbot_card");
        }
    }
}