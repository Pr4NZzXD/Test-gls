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
#include <map>
#include <cctype>
#include <cstdio>

namespace aim {
    bool grannyAimEnabled = false;
    bool onlyWithWeapon = false;
    float grannySmoothness = 30.0f;
    bool showGrannySettings = false;

    bool itemAimEnabled = false;
    float itemSmoothness = 30.0f;
    bool showItemSettings = false;
    float itemAimDistance = 3.0f;
    std::vector<LiveItemTarget> liveItems;

    struct CiLess {
        bool operator()(const std::string& a, const std::string& b) const {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
        }
    };
    static std::map<std::string, bool, CiLess> s_ItemTypes;
    static float s_RescanTimer = 0.0f;
    static bool s_DefaultNewItems = true;

    static const char* kKnownItems[] = {
        "Crossbow", "Shotgun", "Hammer", "PadlockKey", "PassCode", "Pliers", "SafeKey",
        "ExitKey", "MasterKey", "CuttingPliers", "Battery", "DoorKey", "CarKey", "SparkPlug",
        "CarBattery", "Gasoline", "Wrench", "EnginePart", "CellarKey", "Fuse", "WinchHandle",
        "Cogwheel", "SpecialKey", "PlayhouseKey", "WeaponKey", "RemoteControl", "BirdSeed", "Meat",
        "PadlockCode", "Watermelon", "Vase", "Vase_01"
    };

    static void EnsureKnownItems() {
        for (const char* n : kKnownItems) s_ItemTypes.emplace(n, true);
    }

    static bool IsItemTypeEnabled(const std::string& n) {
        auto it = s_ItemTypes.find(n);
        if (it == s_ItemTypes.end()) { 
            s_ItemTypes.emplace(n, s_DefaultNewItems); 
            return s_DefaultNewItems; 
        }
        return it->second;
    }

    static std::string NormalizeItemName(std::string n) {
        size_t p = n.find("(Clone)");
        if (p != std::string::npos) n.erase(p);
        while (!n.empty() && n.back() == ' ') n.pop_back();
        if (!n.empty() && n.back() == ')') {
            size_t o = n.rfind('(');
            if (o != std::string::npos && o + 2 < n.size()) {
                bool digits = true;
                for (size_t i = o + 1; i + 1 < n.size(); i++) if (!std::isdigit((unsigned char)n[i])) { digits = false; break; }
                if (digits) { n.erase(o); while (!n.empty() && n.back() == ' ') n.pop_back(); }
            }
        }
        return n;
    }

    std::vector<std::string> ItemTypeNames() {
        EnsureKnownItems();
        std::vector<std::string> v;
        for (auto& kv : s_ItemTypes) v.push_back(kv.first);
        return v;
    }
    bool* ItemTypeEnabledPtr(const std::string& name) {
        return &s_ItemTypes.emplace(name, true).first->second;
    }
    void SetAllItemTypes(bool enabled) {
        EnsureKnownItems();
        for (auto& kv : s_ItemTypes) kv.second = enabled;
        s_DefaultNewItems = enabled;
    }
    std::string PrettyItemName(const std::string& name) {
        std::string out;
        for (size_t i = 0; i < name.size(); i++) {
            char ch = name[i];
            if (ch == '_') { out += ' '; continue; }
            if (i > 0 && std::isupper((unsigned char)ch) && std::islower((unsigned char)name[i - 1])) out += ' ';
            out += ch;
        }
        return out;
    }

    static void* s_LockedItemTr = nullptr;
    static void* s_HandHoldTr = nullptr;
    static int s_HandBaseline = 1000;       
    static void* s_PrevAutoTarget = nullptr; 
    static float s_ItemCooldown = 0.0f;      

    static void* s_CachedGrannyHeadTr = nullptr;
    static void* s_CachedMobileFPS = nullptr;
    static void* s_MobileFPSClass = nullptr;

    void ClearCache() {
        s_CachedGrannyHeadTr = nullptr;
        s_CachedMobileFPS = nullptr;
        s_MobileFPSClass = nullptr;
        s_LockedItemTr = nullptr;
        s_HandHoldTr = nullptr;
        s_HandBaseline = 1000;
        s_PrevAutoTarget = nullptr;
        s_ItemCooldown = 0.0f;
        s_RescanTimer = 0.0f;
        liveItems.clear();
    }

    void Init() {
        ClearCache();
        EnsureKnownItems();
    }

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
                if (cName == parts[i]) { nextTr = childTr; break; }
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
                if (go && IsNativeObjectAlive(go) && oGetGameObjectActive && oGetGameObjectActive(go)) return true;
            }
        }
        return false;
    }

    // [PERBAIKAN UTAMA] Deteksi akurat apakah pemain sedang memegang item apa pun di tangan
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
                return true; // Ada item aktif di tangan pemain -> Aim harus mati
            }
        }
        return false;
    }

    void* GetGrannyHeadTransform() {
        if (s_CachedGrannyHeadTr && IsNativeObjectAlive(s_CachedGrannyHeadTr)) return s_CachedGrannyHeadTr;
        void* headTr = FindTransformByPath("Entities/Granny-Stuff/GrannyParent/GrannyHeadPos");
        if (headTr && IsNativeObjectAlive(headTr)) {
            s_CachedGrannyHeadTr = headTr;
            return s_CachedGrannyHeadTr;
        }
        return nullptr;
    }

    static bool IsHeldByPlayer(void* tr) {
        void* p = SafeGetParentTransform(tr);
        for (int i = 0; i < 6 && p; i++) {
            std::string n = GetUnityObjectName(p);
            if (n == "HandHoldObjects" || n == "PlayerStuff") return true;
            p = SafeGetParentTransform(p);
        }
        return false;
    }

    void ScanLiveItems() {
        std::vector<LiveItemTarget> oldLive = liveItems;
        liveItems.clear();
        if (!IsSceneReady()) return;

        std::unordered_set<void*> addedTransforms;

        auto addLive = [&](const std::string& rawName, void* tr) {
            if (!tr || !IsNativeObjectAlive(tr)) return;
            if (addedTransforms.find(tr) != addedTransforms.end()) return;
            if (IsHeldByPlayer(tr)) return;            
            std::string base = NormalizeItemName(rawName);
            if (base.empty()) return;
            addedTransforms.insert(tr);

            bool isSelected = s_DefaultNewItems;
            for (auto& old : oldLive) {
                if (old.transform == tr) {
                    isSelected = old.selected;
                    break;
                }
            }
            if (s_ItemTypes.find(base) == s_ItemTypes.end()) {
                s_ItemTypes.emplace(base, s_DefaultNewItems); 
            }
            liveItems.push_back({ rawName, tr, isSelected, base });
        };

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
                        if (!isAct) continue;
                        std::string cName = GetUnityObjectName(cTr);
                        if (!cName.empty()) addLive(cName, cTr);
                    }
                }
            }
        }
    }

    void SafeLookAt(void* tr, void* targetTransform, Vector3 targetWorldPos) {
        if (!tr || !IsNativeObjectAlive(tr)) return;
        void* trClass = FindClass("UnityEngine", "Transform");
        if (!trClass) return;
        if (targetTransform && IsNativeObjectAlive(targetTransform)) {
            void* lookAtTrMethod = il2cpp_class_get_method_from_name(trClass, "LookAt", 1);
            if (lookAtTrMethod) {
                void* args[1] = { targetTransform };
                void* exc = nullptr;
                il2cpp_runtime_invoke(lookAtTrMethod, tr, args, &exc);
                if (!exc) return;
            }
        }
        void* lookAtVecMethod = il2cpp_class_get_method_from_name(trClass, "LookAt", 2);
        if (lookAtVecMethod) {
            Vector3 up{ 0, 1, 0 };
            void* args[2] = { &targetWorldPos, &up };
            void* exc = nullptr;
            il2cpp_runtime_invoke(lookAtVecMethod, tr, args, &exc);
        }
    }

    void AimAtTarget(void* targetTr, void* playerTr, void* camTr, void* camPivotTr, void* mfps) {
        if (!targetTr || !IsNativeObjectAlive(targetTr)) return;
        Vector3 camPos{ 0, 0, 0 }, targetPos{ 0, 0, 0 }, playerPos{ 0, 0, 0 };
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

        Vector3 flatTarget = { targetPos.x, playerPos.y, targetPos.z };
        SafeLookAt(playerTr, nullptr, flatTarget);
        if (camPivotTr && IsNativeObjectAlive(camPivotTr)) SafeLookAt(camPivotTr, targetTr, targetPos);
        SafeLookAt(camTr, targetTr, targetPos);

        if (mfps && s_MobileFPSClass && IsNativeObjectAlive(mfps)) {
            SetFieldFloat(mfps, s_MobileFPSClass, "rotationX", targetPitch);
            SetFieldFloat(mfps, s_MobileFPSClass, "lookDelta", 0.0f);
        }
    }

    void AimAtTargetSmooth(void* targetTr, void* playerTr, void* camTr, void* camPivotTr, void* mfps, float speed) {
        if (!targetTr || !IsNativeObjectAlive(targetTr) || !oTransformGetPosition) return;
        Vector3 camPos{ 0, 0, 0 }, tPos{ 0, 0, 0 }, pPos{ 0, 0, 0 };
        oTransformGetPosition(camTr, &camPos);
        oTransformGetPosition(targetTr, &tPos);
        oTransformGetPosition(playerTr, &pPos);

        Vector3 d = { tPos.x - camPos.x, tPos.y - camPos.y, tPos.z - camPos.z };
        float dist = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        if (dist < 0.05f) return;
        Vector3 D = { d.x / dist, d.y / dist, d.z / dist };
        float horiz = sqrtf(D.x * D.x + D.z * D.z);
        if (horiz < 0.001f) return;

        float aimDist = std::max(dist, 1.0f);
        Vector3 point = { camPos.x + D.x * aimDist, camPos.y + D.y * aimDist, camPos.z + D.z * aimDist };
        float pitch = -atan2f(D.y, horiz) * 57.2957795f;
        pitch = std::clamp(pitch, -82.0f, 82.0f);

        Vector3 flatTarget = { point.x, pPos.y, point.z };
        SafeLookAt(playerTr, nullptr, flatTarget);
        if (camPivotTr && IsNativeObjectAlive(camPivotTr)) SafeLookAt(camPivotTr, nullptr, point);
        SafeLookAt(camTr, nullptr, point);

        if (mfps && s_MobileFPSClass && IsNativeObjectAlive(mfps)) {
            SetFieldFloat(mfps, s_MobileFPSClass, "rotationX", pitch);
            SetFieldFloat(mfps, s_MobileFPSClass, "lookDelta", 0.0f);
        }
    }

    void Update() {
        if (!IsSceneReady()) return;
        if (!itemAimEnabled) s_LockedItemTr = nullptr;

        void* playerTr = player_mods::GetPlayerRootTransform();
        void* camTr = player_mods::GetPlayerCameraTransform();
        void* camPivotTr = player_mods::GetPlayerCamPivotTransform();
        void* mfps = GetActiveMobileFPS();
        if (!playerTr || !IsNativeObjectAlive(playerTr) || !camTr || !IsNativeObjectAlive(camTr)) return;

        if (grannyAimEnabled) {
            bool canAimGranny = true;
            if (onlyWithWeapon) canAimGranny = IsPlayerHoldingWeapon();
            if (canAimGranny) {
                void* headTr = GetGrannyHeadTransform();
                if (headTr && IsNativeObjectAlive(headTr)) {
                    AimAtTarget(headTr, playerTr, camTr, camPivotTr, mfps);
                    return;
                }
            }
        }

        // [PERBAIKAN UTAMA] Jika pemain sedang memegang item di tangan, matikan aim item secara mutlak!
        if (itemAimEnabled && IsPlayerHoldingAnyItem()) {
            return; 
        }

        if (itemAimEnabled) {
            if (s_ItemCooldown > 0.0f) {
                s_ItemCooldown -= ImGui::GetIO().DeltaTime;
                return;
            }

            s_RescanTimer += ImGui::GetIO().DeltaTime;
            if (liveItems.empty() || s_RescanTimer >= 2.0f) {
                s_RescanTimer = 0.0f;
                ScanLiveItems();
            }

            Vector3 pPos{ 0, 0, 0 };
            if (oTransformGetPosition) oTransformGetPosition(playerTr, &pPos);
            float maxDistSq = itemAimDistance * itemAimDistance;
            void* bestTarget = nullptr;
            float bestDistSq = maxDistSq;

            for (auto& itm : liveItems) {
                if (!itm.transform || !IsNativeObjectAlive(itm.transform)) continue;
                if (!itm.selected) continue; // Pastikan hanya membidik item yang dicentang di checkbox
                
                void* itemGO = oComponentGetGameObject ? oComponentGetGameObject(itm.transform) : nullptr;
                if (itemGO && oGetGameObjectActive && !oGetGameObjectActive(itemGO)) continue;

                Vector3 iPos{ 0, 0, 0 };
                if (oTransformGetPosition) oTransformGetPosition(itm.transform, &iPos);

                float d = (iPos.x - pPos.x)*(iPos.x - pPos.x) + (iPos.y - pPos.y)*(iPos.y - pPos.y) + (iPos.z - pPos.z)*(iPos.z - pPos.z);
                if (d <= bestDistSq) {      
                    bestDistSq = d;
                    bestTarget = itm.transform;
                }
            }

            if (bestTarget) {
                s_PrevAutoTarget = bestTarget;
                AimAtTargetSmooth(bestTarget, playerTr, camTr, camPivotTr, mfps, itemSmoothness);
            } else {
                s_PrevAutoTarget = nullptr;
            }
        }
    }

    void DrawMenu() {
        DrawFeatureCardWithGear("granny_head_aim", LOC("Аимбот на Бабку", "Granny Aimlock"), LOC("Наводит прицел в голову Бабки", "Locks camera on Granny's head"), &grannyAimEnabled, &showGrannySettings);
        if (BeginSubSettingsAnim("granny_head_aim", showGrannySettings || grannyAimEnabled, 120.0f * g_UiScale)) {
            ImGui::Checkbox(LOC("Только с оружием", "Only when holding weapon"), &onlyWithWeapon);
            DrawSliderWithInput("##aimSmoothGranny", &grannySmoothness, 5.0f, 50.0f, "%.0f");
            EndSubSettingsAnim("granny_head_aim");
        }

        DrawFeatureCardWithGear("item_aimbot_card", LOC("Аимбот на предметы", "Item Aimbot"), LOC("Наводит камеру на предметы", "Aims camera towards items"), &itemAimEnabled, &showItemSettings);
        if (BeginSubSettingsAnim("item_aimbot_card", showItemSettings || itemAimEnabled, 320.0f * g_UiScale)) {
            DrawSliderWithInput("##itemAimSmooth", &itemSmoothness, 5.0f, 50.0f, "%.0f");
            DrawSliderWithInput("##itemAimDist", &itemAimDistance, 1.0f, 20.0f, "%.1f m");
            ImGui::Spacing();
            
            float availWidth = ImGui::GetContentRegionAvail().x;
            float btnW = (availWidth - 16.0f * g_UiScale) / 3.0f;

            if (ImGui::Button(LOC("Выбрать все", "Select All"), ImVec2(btnW, 26 * g_UiScale))) {
                for (auto& itm : liveItems) itm.selected = true;
                s_DefaultNewItems = true;
            }
            ImGui::SameLine(0, 8.0f * g_UiScale);
            if (ImGui::Button(LOC("Снять все", "Unselect"), ImVec2(btnW, 26 * g_UiScale))) {
                for (auto& itm : liveItems) itm.selected = false;
                s_DefaultNewItems = false; 
            }
            ImGui::SameLine(0, 8.0f * g_UiScale);
            if (ImGui::Button(LOC("Обновить", "Rescan"), ImVec2(btnW, 26 * g_UiScale))) { ScanLiveItems(); }

            ImGui::Spacing();
            ImGui::BeginChild("##itemTypeList", ImVec2(-1, 140.0f * g_UiScale), true);
            for (auto& itm : liveItems) {
                ImGui::Checkbox(itm.name.c_str(), &itm.selected);
            }
            ImGui::EndChild();

            EndSubSettingsAnim("item_aimbot_card");
        }
    }
}
