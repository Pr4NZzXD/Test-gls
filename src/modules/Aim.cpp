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
#include <cstdint>

namespace Modules {

bool Aim::grannyAimEnabled = true;
bool Aim::onlyWithWeapon = false;
float Aim::grannySmoothness = 30.0f;
bool Aim::showGrannySettings = false;

bool Aim::aim_item_enabled = false;
bool Aim::auto_aim_on_distance = false;
float Aim::aim_item_distance = 15.0f;
float Aim::itemSmoothness = 30.0f;
bool Aim::showItemSettings = false;

std::unordered_set<std::string> Aim::target_item_list = {
    "Crossbow", "Shotgun", "Hammer", "PadlockKey", "PassCode", "Pliers", 
    "SafeKey", "ExitKey", "MasterKey", "CuttingPliers", "Battery", "DoorKey", 
    "CarKey", "SparkPlug", "CarBattery", "Gasoline", "Wrench", "EnginePart", 
    "CellarKey", "Fuse", "WinchHandle", "Cogwheel", "SpecialKey", "PlayhouseKey", 
    "WeaponKey", "RemoteControl", "BirdSeed", "Meat", "Passcode", "PadlockCode", 
    "Watermelon", "Vase", "Vase_01"
};

std::vector<Aim::LiveItemTarget> Aim::liveItems;

static void* s_LockedItemTr = nullptr;
static void* s_HandHoldTr = nullptr;
static int s_HandBaseline = 1000;
static int s_LastHandLog = -2;
static void* s_PrevAutoTarget = nullptr;
static float s_ItemCooldown = 0.0f;
static char s_ItemFilter[64] = "";
static std::string s_Status = "";
static float s_StatusTimer = 0.0f;
static void* s_CachedGrannyHeadTr = nullptr;
static void* s_CachedMobileFPS = nullptr;
static void* s_MobileFPSClass = nullptr;

void Aim::ClearCache() {
    s_CachedGrannyHeadTr = s_CachedMobileFPS = s_MobileFPSClass = nullptr;
    s_LockedItemTr = s_HandHoldTr = s_PrevAutoTarget = nullptr;
    s_HandBaseline = 1000;
    s_LastHandLog = -2;
    s_ItemCooldown = 0.0f;
    liveItems.clear();
}

void Aim::Init() {
    ClearCache();
    Logger::Log("AIM", LOG_OK, "Modules::Aim::Init() -> Aim module initialized.");
}

void* Aim::FindTransformByPath(const char* fullPath) {
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
    if (!curTr) return nullptr;

    for (size_t i = 1; i < parts.size(); i++) {
        if (!curTr || !IsNativeObjectAlive(curTr)) return nullptr;
        int childCount = SafeGetChildCount(curTr);
        void* nextTr = nullptr;

        for (int c = 0; c < childCount; c++) {
            void* childTr = SafeGetChild(curTr, c);
            if (!childTr || !IsNativeObjectAlive(childTr)) continue;
            if (GetUnityObjectName(childTr) == parts[i]) {
                nextTr = childTr;
                break;
            }
        }
        if (!nextTr) return nullptr;
        curTr = nextTr;
    }
    return curTr;
}

void* Aim::GetActiveMobileFPS() {
    if (s_CachedMobileFPS && IsNativeObjectAlive(s_CachedMobileFPS)) return s_CachedMobileFPS;

    void* playerTr = player_mods::GetPlayerRootTransform();
    if (!playerTr || !g_GetComponentMethod) return nullptr;

    void* playerGO = oComponentGetGameObject ? oComponentGetGameObject(playerTr) : nullptr;
    if (!playerGO) return nullptr;

    if (!s_MobileFPSClass) s_MobileFPSClass = FindClass("", "MobileFPS");
    if (!s_MobileFPSClass || !il2cpp_class_get_type || !il2cpp_type_get_object) return nullptr;

    void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(s_MobileFPSClass));
    if (!typeObj) return nullptr;

    void* args[1] = { typeObj };
    void* exc = nullptr;
    s_CachedMobileFPS = il2cpp_runtime_invoke(g_GetComponentMethod, playerGO, args, &exc);
    if (exc) s_CachedMobileFPS = nullptr;

    return s_CachedMobileFPS;
}

bool Aim::IsPlayerHoldingWeapon() {
    const char* weaponPaths[] = {
        "PlayerStuff/HandHoldObjects/CrossbowHand",
        "PlayerStuff/HandHoldObjects/OldShotgunHand",
        "PlayerStuff/HandHoldObjects/PepperSprayHand",
        "PlayerStuff/HandHoldObjects/ShotgunHand"
    };

    for (const char* path : weaponPaths) {
        void* tr = FindTransformByPath(path);
        if (!tr || !IsNativeObjectAlive(tr)) continue;

        void* go = oComponentGetGameObject ? oComponentGetGameObject(tr) : nullptr;
        if (go && IsNativeObjectAlive(go) && oGetGameObjectActive && oGetGameObjectActive(go)) {
            return true;
        }
    }
    return false;
}

bool Aim::IsPlayerHoldingAnyItem() {
    if (!s_HandHoldTr || !IsNativeObjectAlive(s_HandHoldTr)) {
        s_HandHoldTr = FindTransformByPath("PlayerStuff/HandHoldObjects");
    }
    if (!s_HandHoldTr || !IsNativeObjectAlive(s_HandHoldTr)) return false;

    int childCount = SafeGetChildCount(s_HandHoldTr);
    int active = 0;
    std::string names;

    for (int i = 0; i < childCount; i++) {
        void* childTr = SafeGetChild(s_HandHoldTr, i);
        if (!childTr || !IsNativeObjectAlive(childTr)) continue;

        void* go = oComponentGetGameObject ? oComponentGetGameObject(childTr) : nullptr;
        if (go && IsNativeObjectAlive(go) && oGetGameObjectActive && oGetGameObjectActive(go)) {
            active++;
            if (names.size() < 160) {
                names += GetUnityObjectName(childTr) + " ";
            }
        }
    }

    if (active < s_HandBaseline) s_HandBaseline = active;
    if (active != s_LastHandLog) {
        s_LastHandLog = active;
        Logger::Log("AIM", LOG_OK, "HandHold aktif=%d baseline=%d : %s", active, s_HandBaseline, names.c_str());
    }
    return active > s_HandBaseline;
}

void* Aim::GetGrannyHeadTransform() {
    if (s_CachedGrannyHeadTr && IsNativeObjectAlive(s_CachedGrannyHeadTr)) return s_CachedGrannyHeadTr;

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

bool Aim::IsItemInList(const std::string& itemName) {
    if (target_item_list.empty()) return true;
    return target_item_list.find(itemName) != target_item_list.end();
}

void Aim::ScanLiveItems() {
    liveItems.clear();
    if (!IsSceneReady()) return;

    std::unordered_set<void*> addedTransforms;
    if (!oGameObjectFind || !il2cpp_string_new) return;

    const char* containerPaths[] = {
        "Objects/MainItemSelection/MapItems", "Objects/MainItemSelection", "Objects/MapItems"
    };

    for (const char* path : containerPaths) {
        void* str = il2cpp_string_new(path);
        if (!str) continue;
        void* containerGO = oGameObjectFind(str);
        if (!containerGO || !IsNativeObjectAlive(containerGO) || !oGameObjectGetTransform) continue;

        void* containerTr = oGameObjectGetTransform(containerGO);
        if (!containerTr) continue;

        int childCount = SafeGetChildCount(containerTr);
        for (int i = 0; i < childCount; i++) {
            void* cTr = SafeGetChild(containerTr, i);
            if (!cTr || !IsNativeObjectAlive(cTr)) continue;

            void* cGO = oComponentGetGameObject ? oComponentGetGameObject(cTr) : nullptr;
            bool isActive = (cGO && oGetGameObjectActive) ? oGetGameObjectActive(cGO) : true;
            if (!isActive) continue;

            std::string name = GetUnityObjectName(cTr);
            if (name.empty() || addedTransforms.count(cTr)) continue;

            addedTransforms.insert(cTr);
            liveItems.push_back({name, cTr, true});
        }
    }
}

void Aim::ExportItemsToTxt() {
    std::string exportText = "# Kahanium Map Items List:\n";
    int count = 0;

    for (auto& item : liveItems) {
        if (!item.selected) continue;
        exportText += item.name + "\n";
        count++;
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

    s_Status = "Exported " + std::to_string(count) + " items";
    s_StatusTimer = 3.5f;
}

void Aim::SafeLookAt(void* tr, void* targetTransform, Vector3 targetWorldPos) {
    if (!tr || !IsNativeObjectAlive(tr)) return;

    void* trClass = FindClass("UnityEngine", "Transform");
    if (!trClass) return;

    if (targetTransform && IsNativeObjectAlive(targetTransform)) {
        void* method = il2cpp_class_get_method_from_name(trClass, "LookAt", 1);
        if (method) {
            void* args[1] = { targetTransform };
            void* exc = nullptr;
            il2cpp_runtime_invoke(method, tr, args, &exc);
            if (!exc) return;
        }
    }

    void* method = il2cpp_class_get_method_from_name(trClass, "LookAt", 2);
    if (!method) return;

    Vector3 up = { 0.0f, 1.0f, 0.0f };
    void* args[2] = { &targetWorldPos, &up };
    void* exc = nullptr;
    il2cpp_runtime_invoke(method, tr, args, &exc);
}

void Aim::AimAtTarget(void* targetTr, void* playerTr, void* camTr, void* camPivotTr, void* mfps) {
    if (!targetTr || !IsNativeObjectAlive(targetTr) || !oTransformGetPosition) return;

    Vector3 camPos{0}, targetPos{0}, playerPos{0};
    oTransformGetPosition(camTr, &camPos);
    oTransformGetPosition(targetTr, &targetPos);
    oTransformGetPosition(playerTr, &playerPos);

    Vector3 delta = { targetPos.x - camPos.x, targetPos.y - camPos.y, targetPos.z - camPos.z };
    float distXZ = sqrtf(delta.x * delta.x + delta.z * delta.z);
    if (distXZ < 0.05f) return;

    float targetPitch = std::clamp(-atan2f(delta.y, distXZ) * 57.2957795f, -82.0f, 82.0f);
    Vector3 flatTarget = { targetPos.x, playerPos.y, targetPos.z };

    SafeLookAt(playerTr, nullptr, flatTarget);
    if (camPivotTr && IsNativeObjectAlive(camPivotTr)) SafeLookAt(camPivotTr, targetTr, targetPos);
    SafeLookAt(camTr, targetTr, targetPos);

    if (mfps && s_MobileFPSClass && IsNativeObjectAlive(mfps)) {
        SetFieldFloat(mfps, s_MobileFPSClass, "rotationX", targetPitch);
        SetFieldFloat(mfps, s_MobileFPSClass, "lookDelta", 0.0f);
    }
}

void Aim::ProcessGrannyAim() {
    if (!grannyAimEnabled || (onlyWithWeapon && !IsPlayerHoldingWeapon())) return;

    void* headTr = GetGrannyHeadTransform();
    if (!headTr || !IsNativeObjectAlive(headTr)) return;

    void* playerTr = player_mods::GetPlayerRootTransform();
    void* camTr = player_mods::GetPlayerCameraTransform();
    void* camPivotTr = player_mods::GetPlayerCamPivotTransform();
    void* mfps = GetActiveMobileFPS();

    if (!playerTr || !camTr || !IsNativeObjectAlive(playerTr) || !IsNativeObjectAlive(camTr)) return;

    AimAtTarget(headTr, playerTr, camTr, camPivotTr, mfps);
}

void Aim::ProcessItemAim() {
    if (!aim_item_enabled || !IsSceneReady()) return;

    void* playerTr = player_mods::GetPlayerRootTransform();
    void* camTr = player_mods::GetPlayerCameraTransform();
    void* camPivotTr = player_mods::GetPlayerCamPivotTransform();
    void* mfps = GetActiveMobileFPS();

    if (!playerTr || !camTr || !IsNativeObjectAlive(playerTr) || !IsNativeObjectAlive(camTr)) return;

    if (IsPlayerHoldingAnyItem()) {
        s_LockedItemTr = nullptr;
        return;
    }
}

} // namespace Modules
