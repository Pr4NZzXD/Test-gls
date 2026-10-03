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
    // Бабка
    bool grannyAimEnabled = true;
    bool onlyWithWeapon = false;
    float grannySmoothness = 30.0f;
    bool showGrannySettings = false;

    // Предметы
    bool itemAimEnabled = false;
    float itemSmoothness = 30.0f;
    bool showItemSettings = false;
    float itemAimDistance = 3.0f;
    std::vector<LiveItemTarget> liveItems;

    // ---- filter jenis item (tidak dihapus saat ClearCache, disimpan di config) ----
    struct CiLess {
        bool operator()(const std::string& a, const std::string& b) const {
            return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end(),
                [](unsigned char x, unsigned char y) { return std::tolower(x) < std::tolower(y); });
        }
    };
    static std::map<std::string, bool, CiLess> s_ItemTypes;
    static float s_RescanTimer = 0.0f;
    static size_t s_LastLiveCount = (size_t)-1;

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
    static void EnsureItemType(const std::string& n) { s_ItemTypes.emplace(n, true); }

    static bool IsItemTypeEnabled(const std::string& n) {
        auto it = s_ItemTypes.find(n);
        if (it == s_ItemTypes.end()) { s_ItemTypes.emplace(n, true); return true; }
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
    void SetItemTypeEnabled(const std::string& name, bool enabled) {
        s_ItemTypes[name] = enabled;
    }
    void SetAllItemTypes(bool enabled) {
        EnsureKnownItems();
        for (auto& kv : s_ItemTypes) kv.second = enabled;
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
    static int s_HandBaseline = 1000;       // jumlah objek tangan aktif paling sedikit yang pernah terlihat (tangan kosong)
    static int s_LastHandLog = -2;
    static void* s_PrevAutoTarget = nullptr; // target item terakhir yang di-aim otomatis
    static float s_ItemCooldown = 0.0f;      // jeda aim item setelah item diambil
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
        s_HandBaseline = 1000;
        s_LastHandLog = -2;
        s_PrevAutoTarget = nullptr;
        s_ItemCooldown = 0.0f;
        s_RescanTimer = 0.0f;
        s_LastLiveCount = (size_t)-1;
        liveItems.clear();
    }

    void Init() {
        ClearCache();
        EnsureKnownItems();
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

    // Cek apakah pemain sedang memegang sesuatu di tangan.
    // Tidak bergantung pada nama objek: dihitung dari jumlah anak HandHoldObjects yang aktif,
    // dibandingkan dengan jumlah aktif terkecil yang pernah terlihat (tangan kosong).
    bool IsPlayerHoldingAnyItem() {
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
                if (names.size() < 160) names += GetUnityObjectName(childTr) + " ";
            }
        }

        if (active < s_HandBaseline) s_HandBaseline = active;

        // Catat ke tab Debugger setiap kali jumlah objek aktif berubah (untuk pengecekan)
        if (active != s_LastHandLog) {
            s_LastHandLog = active;
            Logger::Log("AIM", LOG_OK, "HandHold aktif=%d baseline=%d : %s", active, s_HandBaseline, names.c_str());
        }

        return active > s_HandBaseline;
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
    // SCAN SEMUA ITEM YANG ADA DI SCENE (termasuk item yang di-drop pemain)
    // ==============================================================
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
        liveItems.clear();
        if (!IsSceneReady()) return;

        std::unordered_set<void*> addedTransforms;

        auto addLive = [&](const std::string& rawName, void* tr) {
            if (!tr || !IsNativeObjectAlive(tr)) return;
            if (addedTransforms.find(tr) != addedTransforms.end()) return;
            if (IsHeldByPlayer(tr)) return;            // item yang sedang dipegang bukan target
            std::string base = NormalizeItemName(rawName);
            if (base.empty()) return;
            addedTransforms.insert(tr);
            EnsureItemType(base);
            liveItems.push_back({ rawName, tr, true, base });
        };

        // 1. Container item di map
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

        // 2. Klon dan item yang di-drop (dicari lewat nama)
        if (oGameObjectFind && il2cpp_string_new) {
            static const char* kFindAliases[] = { "Passcode" };   // ejaan lain yang dipakai game
            std::vector<const char*> findNames(std::begin(kKnownItems), std::end(kKnownItems));
            for (const char* al : kFindAliases) findNames.push_back(al);

            for (const char* baseName : findNames) {
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
                            addLive(toFind[f], tr);
                        }
                    }
                }
            }
        }

        // Catat ke Debugger saat jumlah item berubah (membantu pengecekan item drop)
        if (liveItems.size() != s_LastLiveCount) {
            s_LastLiveCount = liveItems.size();
            std::string names;
            for (size_t i = 0; i < liveItems.size() && i < 10; i++) names += liveItems[i].name + " ";
            Logger::Log("AIM", LOG_OK, "Live items: %d : %s", (int)liveItems.size(), names.c_str());
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

    // Aim halus: arah kamera bergerak bertahap ke item, kecepatan diatur "Item Aim Speed"
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

        Vector3 N = D;
        if (oTransformGetForward) {
            Vector3 F{ 0, 0, 0 };
            oTransformGetForward(camTr, &F);
            float fl = sqrtf(F.x * F.x + F.y * F.y + F.z * F.z);
            if (fl > 0.001f) {
                F = { F.x / fl, F.y / fl, F.z / fl };
                float dot = F.x * D.x + F.y * D.y + F.z * D.z;
                if (dot < 0.99999f) {
                    float dt = ImGui::GetIO().DeltaTime;
                    float k = 1.0f - expf(-speed * 0.35f * dt);
                    k = std::clamp(k, 0.02f, 1.0f);
                    Vector3 M = { F.x + (D.x - F.x) * k, F.y + (D.y - F.y) * k, F.z + (D.z - F.z) * k };
                    float ml = sqrtf(M.x * M.x + M.y * M.y + M.z * M.z);
                    if (ml > 0.001f) N = { M.x / ml, M.y / ml, M.z / ml };
                }
            }
        }

        float horiz = sqrtf(N.x * N.x + N.z * N.z);
        if (horiz < 0.001f) return;

        float aimDist = std::max(dist, 1.0f);
        Vector3 point = { camPos.x + N.x * aimDist, camPos.y + N.y * aimDist, camPos.z + N.z * aimDist };
        float pitch = -atan2f(N.y, horiz) * 57.2957795f;
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
                s_ItemCooldown = 2.5f;    // item baru diambil: kamera bebas sejenak
            } else {
                AimAtTarget(s_LockedItemTr, playerTr, camTr, camPivotTr, mfps);
                return;
            }
        }

        if (itemAimEnabled) {
            // Kalau target sebelumnya hilang/nonaktif berarti item baru diambil -> beri jeda
            if (s_PrevAutoTarget) {
                bool gone = !IsNativeObjectAlive(s_PrevAutoTarget);
                if (!gone) {
                    void* prevGO = oComponentGetGameObject ? oComponentGetGameObject(s_PrevAutoTarget) : nullptr;
                    if (prevGO && oGetGameObjectActive && !oGetGameObjectActive(prevGO)) gone = true;
                }
                if (gone) {
                    s_PrevAutoTarget = nullptr;
                    s_ItemCooldown = 2.5f;
                }
            }
            if (s_ItemCooldown > 0.0f) {
                s_ItemCooldown -= ImGui::GetIO().DeltaTime;
                return;
            }

            // Scan ulang tiap 1 detik agar item yang di-drop ikut terdeteksi
            s_RescanTimer += ImGui::GetIO().DeltaTime;
            if (liveItems.empty() || s_RescanTimer >= 1.0f) {
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
                if (!IsItemTypeEnabled(itm.baseName)) continue;   // filter jenis item

                void* itemGO = oComponentGetGameObject ? oComponentGetGameObject(itm.transform) : nullptr;
                if (itemGO && oGetGameObjectActive && !oGetGameObjectActive(itemGO)) continue;

                Vector3 iPos{ 0, 0, 0 };
                if (oTransformGetPosition) oTransformGetPosition(itm.transform, &iPos);

                float d = (iPos.x - pPos.x)*(iPos.x - pPos.x) + (iPos.y - pPos.y)*(iPos.y - pPos.y) + (iPos.z - pPos.z)*(iPos.z - pPos.z);
                if (d <= bestDistSq) {      // hanya item dalam jarak yang diatur
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