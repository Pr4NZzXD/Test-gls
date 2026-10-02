#include "macro.h"
#include "player_mods.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <dirent.h>

namespace macro {
    MacroState currentState = STATE_IDLE;
    int targetTPS = 120;
    float recordingTimeScale = 0.20f;
    float playbackTimeScale = 1.00f;

    struct Quat4 {
        float x, y, z, w;
    };

    enum MacroAction : uint8_t {
        ACT_NONE     = 0,
        ACT_INTERACT = (1 << 0), // Дверь / Подбор / Использование (разовый импульс)
        ACT_DROP     = (1 << 1)  // Сброс предмета (разовый импульс)
    };

    struct MacroFrame {
        Vector3 pos;
        Quat4 playerWorldRot;
        Quat4 headLocalRot;
        Quat4 camWorldRot;
        float pitchX;
        uint8_t actionFlags;
        uint8_t pad[3];
    };
    static std::vector<MacroFrame> s_Frames;

    static size_t s_CurrentPlayIndex = 0;
    static float s_GameTimeAccumulator = 0.0f;
    static char s_MacroFileName[64] = "speedrun_run";
    static std::vector<std::string> s_FoundMacros;
    static std::string s_StatusMessage = "";
    static float s_StatusTimer = 0.0f;

    static int s_RecordedInteracts = 0;
    static int s_RecordedDrops = 0;

    static void* s_CachedMobileFPS = nullptr;
    static void* s_MobileFPSClass = nullptr;
    static void* s_CachedPickRayComp = nullptr;
    static void* s_PickRayClass = nullptr;
    static void* s_CachedDoorRayComp = nullptr;
    static void* s_DoorRayClass = nullptr;
    static void* s_CachedInventoryComp = nullptr;
    static void* s_InventoryClass = nullptr;

    // Динамические смещения полей
    static size_t s_Off_PickRay_CanDropDis = (size_t)-1;
    static size_t s_Off_PickRay_buttonClicked = (size_t)-1;
    static size_t s_Off_PickRay_buttonClicked2 = (size_t)-1;
    static size_t s_Off_PickRay_RaycastDis = (size_t)-1;
    static size_t s_Off_PickRay_RaycastCheckItemDis = (size_t)-1;
    static size_t s_Off_PickRay_Inventory = (size_t)-1;

    static size_t s_Off_DoorRay_buttonClicked = (size_t)-1;
    static size_t s_Off_DoorRay_RaycastDis = (size_t)-1;
    static size_t s_Off_MobileFPS_rotationX = (size_t)-1;

    // Фильтр тапов (отделение клика от свайпа камеры)
    static bool s_TouchWasDown = false;
    static Vector3 s_TouchStartPos{ 0, 0, 0 };
    static float s_TouchMaxDrag = 0.0f;
    static bool s_PendingInteract = false;
    static bool s_PendingDrop = false;

    size_t GetFieldOffsetSafe(void* klass, const char* fieldName) {
        if (!klass || !fieldName || !il2cpp_class_get_field_from_name || !il2cpp_field_get_offset) return (size_t)-1;
        void* f = il2cpp_class_get_field_from_name(klass, fieldName);
        if (!f) return (size_t)-1;
        return il2cpp_field_get_offset(f);
    }

    void ResolveDynamicOffsets() {
        if (!s_PickRayClass) s_PickRayClass = FindClass("", "PickRay");
        if (s_PickRayClass) {
            s_Off_PickRay_CanDropDis = GetFieldOffsetSafe(s_PickRayClass, "CanDropDis");
            s_Off_PickRay_buttonClicked = GetFieldOffsetSafe(s_PickRayClass, "buttonClicked");
            s_Off_PickRay_buttonClicked2 = GetFieldOffsetSafe(s_PickRayClass, "buttonClicked2");
            s_Off_PickRay_RaycastDis = GetFieldOffsetSafe(s_PickRayClass, "RaycastDis");
            s_Off_PickRay_RaycastCheckItemDis = GetFieldOffsetSafe(s_PickRayClass, "RaycastCheckItemDis");
            s_Off_PickRay_Inventory = GetFieldOffsetSafe(s_PickRayClass, "Inventory");
        }

        if (!s_DoorRayClass) s_DoorRayClass = FindClass("", "DoorRay");
        if (s_DoorRayClass) {
            s_Off_DoorRay_buttonClicked = GetFieldOffsetSafe(s_DoorRayClass, "buttonClicked");
            s_Off_DoorRay_RaycastDis = GetFieldOffsetSafe(s_DoorRayClass, "RaycastDis");
        }

        if (!s_MobileFPSClass) s_MobileFPSClass = FindClass("", "MobileFPS");
        if (s_MobileFPSClass) {
            s_Off_MobileFPS_rotationX = GetFieldOffsetSafe(s_MobileFPSClass, "rotationX");
        }
    }

    void* GetActivePickRay() {
        if (s_CachedPickRayComp && IsNativeObjectAlive(s_CachedPickRayComp)) return s_CachedPickRayComp;

        if (oGameObjectFind && il2cpp_string_new && g_GetComponentMethod) {
            void* prStr = il2cpp_string_new("PlayerStuff/Player/CameraShakeAnim/CameraPivot/Main Camera/PickRay");
            if (prStr) {
                void* go = oGameObjectFind(prStr);
                if (go && IsNativeObjectAlive(go)) {
                    if (!s_PickRayClass) s_PickRayClass = FindClass("", "PickRay");
                    if (s_PickRayClass && il2cpp_class_get_type && il2cpp_type_get_object) {
                        void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(s_PickRayClass));
                        if (typeObj) {
                            void* args[1] = { typeObj };
                            void* exc = nullptr;
                            s_CachedPickRayComp = il2cpp_runtime_invoke(g_GetComponentMethod, go, args, &exc);
                            if (s_CachedPickRayComp) return s_CachedPickRayComp;
                        }
                    }
                }
            }
        }

        if (!s_PickRayClass) s_PickRayClass = FindClass("", "PickRay");
        if (s_PickRayClass && il2cpp_class_get_type && il2cpp_type_get_object) {
            void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(s_PickRayClass));
            if (typeObj) {
                std::vector<void*> list = FindObjectsOfUnityType(typeObj);
                if (!list.empty() && list[0] && IsNativeObjectAlive(list[0])) {
                    s_CachedPickRayComp = list[0];
                    return s_CachedPickRayComp;
                }
            }
        }
        return nullptr;
    }

    void* GetActiveDoorRay() {
        if (s_CachedDoorRayComp && IsNativeObjectAlive(s_CachedDoorRayComp)) return s_CachedDoorRayComp;

        if (oGameObjectFind && il2cpp_string_new && g_GetComponentMethod) {
            void* drStr = il2cpp_string_new("PlayerStuff/Player/CameraShakeAnim/CameraPivot/Main Camera/DoorRay");
            if (drStr) {
                void* go = oGameObjectFind(drStr);
                if (go && IsNativeObjectAlive(go)) {
                    if (!s_DoorRayClass) s_DoorRayClass = FindClass("", "DoorRay");
                    if (s_DoorRayClass && il2cpp_class_get_type && il2cpp_type_get_object) {
                        void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(s_DoorRayClass));
                        if (typeObj) {
                            void* args[1] = { typeObj };
                            void* exc = nullptr;
                            s_CachedDoorRayComp = il2cpp_runtime_invoke(g_GetComponentMethod, go, args, &exc);
                            if (s_CachedDoorRayComp) return s_CachedDoorRayComp;
                        }
                    }
                }
            }
        }

        if (!s_DoorRayClass) s_DoorRayClass = FindClass("", "DoorRay");
        if (s_DoorRayClass && il2cpp_class_get_type && il2cpp_type_get_object) {
            void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(s_DoorRayClass));
            if (typeObj) {
                std::vector<void*> list = FindObjectsOfUnityType(typeObj);
                if (!list.empty() && list[0] && IsNativeObjectAlive(list[0])) {
                    s_CachedDoorRayComp = list[0];
                    return s_CachedDoorRayComp;
                }
            }
        }
        return nullptr;
    }

    void* GetActiveInventory() {
        if (s_CachedInventoryComp && IsNativeObjectAlive(s_CachedInventoryComp)) return s_CachedInventoryComp;
        void* pickRay = GetActivePickRay();
        if (pickRay && IsNativeObjectAlive(pickRay) && s_Off_PickRay_Inventory != (size_t)-1) {
            s_CachedInventoryComp = *(void**)((uintptr_t)pickRay + s_Off_PickRay_Inventory);
            if (!s_InventoryClass) s_InventoryClass = FindClass("", "Inventory");
            return s_CachedInventoryComp;
        }
        return nullptr;
    }

    void* GetActivePlayerGO() {
        if (!oGameObjectFind || !il2cpp_string_new) return nullptr;
        void* pStr = il2cpp_string_new("PlayerStuff/Player");
        if (!pStr) return nullptr;
        void* go = oGameObjectFind(pStr);
        if (go && IsNativeObjectAlive(go) && oGetGameObjectActive && oGetGameObjectActive(go)) {
            return go;
        }
        return nullptr;
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

    bool IsHoldingItemDirect(void* pickRay) {
        if (!pickRay || !IsNativeObjectAlive(pickRay)) return false;
        if (s_Off_PickRay_CanDropDis != (size_t)-1) {
            return *(bool*)((uintptr_t)pickRay + s_Off_PickRay_CanDropDis);
        }
        return false;
    }

    Quat4 GetTransformRotation(void* tr) {
        Quat4 q{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (!tr || !IsNativeObjectAlive(tr)) return q;
        void* trClass = FindClass("UnityEngine", "Transform");
        if (trClass) {
            void* m = il2cpp_class_get_method_from_name(trClass, "get_rotation", 0);
            if (m) {
                void* exc = nullptr;
                void* res = il2cpp_runtime_invoke(m, tr, nullptr, &exc);
                if (res && !exc) q = *(Quat4*)((uintptr_t)res + 0x10);
            }
        }
        return q;
    }

    void SetTransformRotation(void* tr, Quat4 q) {
        if (!tr || !IsNativeObjectAlive(tr)) return;
        void* trClass = FindClass("UnityEngine", "Transform");
        if (trClass) {
            void* m = il2cpp_class_get_method_from_name(trClass, "set_rotation", 1);
            if (m) {
                void* args[1] = { &q };
                void* exc = nullptr;
                il2cpp_runtime_invoke(m, tr, args, &exc);
            }
        }
    }

    Quat4 GetTransformLocalRotation(void* tr) {
        Quat4 q{ 0.0f, 0.0f, 0.0f, 1.0f };
        if (!tr || !IsNativeObjectAlive(tr)) return q;
        void* trClass = FindClass("UnityEngine", "Transform");
        if (trClass) {
            void* m = il2cpp_class_get_method_from_name(trClass, "get_localRotation", 0);
            if (m) {
                void* exc = nullptr;
                void* res = il2cpp_runtime_invoke(m, tr, nullptr, &exc);
                if (res && !exc) q = *(Quat4*)((uintptr_t)res + 0x10);
            }
        }
        return q;
    }

    void SetTransformLocalRotation(void* tr, Quat4 q) {
        if (!tr || !IsNativeObjectAlive(tr)) return;
        void* trClass = FindClass("UnityEngine", "Transform");
        if (trClass) {
            void* m = il2cpp_class_get_method_from_name(trClass, "set_localRotation", 1);
            if (m) {
                void* args[1] = { &q };
                void* exc = nullptr;
                il2cpp_runtime_invoke(m, tr, args, &exc);
            }
        }
    }

    void ClearCache() {
        s_CachedMobileFPS = nullptr;
        s_MobileFPSClass = nullptr;
        s_CachedPickRayComp = nullptr;
        s_PickRayClass = nullptr;
        s_CachedDoorRayComp = nullptr;
        s_DoorRayClass = nullptr;
        s_CachedInventoryComp = nullptr;
        s_InventoryClass = nullptr;

        s_TouchWasDown = false;
        s_TouchMaxDrag = 0.0f;
        s_PendingInteract = false;
        s_PendingDrop = false;

        if (currentState != STATE_IDLE) {
            StopRecording();
            StopPlayback();
        }
    }

    void Init() {
        ClearCache();
        ResolveDynamicOffsets();
        RefreshMacroList();
    }

    // ==============================================================
    // 1. ЗАПИСЬ ТИКА
    // ==============================================================
    void RecordTick(void* playerGO) {
        void* playerTr = oGameObjectGetTransform ? oGameObjectGetTransform(playerGO) : nullptr;
        void* camPivotTr = player_mods::GetPlayerCamPivotTransform();
        void* camTr = player_mods::GetPlayerCameraTransform();
        void* mfps = GetActiveMobileFPS();
        void* pickRay = GetActivePickRay();
        void* doorRay = GetActiveDoorRay();

        if (!playerTr || !IsNativeObjectAlive(playerTr)) return;

        Vector3 pPos{ 0, 0, 0 };
        if (oTransformGetPosition) oTransformGetPosition(playerTr, &pPos);

        Quat4 playerWorldRot = GetTransformRotation(playerTr);
        Quat4 headLocalRot = (camPivotTr && IsNativeObjectAlive(camPivotTr)) ? GetTransformLocalRotation(camPivotTr) : Quat4{ 0, 0, 0, 1 };
        Quat4 camWorldRot = (camTr && IsNativeObjectAlive(camTr)) ? GetTransformRotation(camTr) : Quat4{ 0, 0, 0, 1 };

        float pitchX = 0.0f;
        if (mfps && s_Off_MobileFPS_rotationX != (size_t)-1 && IsNativeObjectAlive(mfps)) {
            pitchX = *(float*)((uintptr_t)mfps + s_Off_MobileFPS_rotationX);
        }

        uint8_t actionFlags = ACT_NONE;

        // Забираем разовые флаги тапа
        if (s_PendingInteract) {
            actionFlags |= ACT_INTERACT;
            s_PendingInteract = false;
            s_RecordedInteracts++;
        }
        if (s_PendingDrop) {
            actionFlags |= ACT_DROP;
            s_PendingDrop = false;
            s_RecordedDrops++;
        }

        // Ловушка флагов клика самой игры
        if (doorRay && s_Off_DoorRay_buttonClicked != (size_t)-1) {
            if (*(bool*)((uintptr_t)doorRay + s_Off_DoorRay_buttonClicked)) {
                actionFlags |= ACT_INTERACT;
            }
        }
        if (pickRay && s_Off_PickRay_buttonClicked != (size_t)-1) {
            if (*(bool*)((uintptr_t)pickRay + s_Off_PickRay_buttonClicked)) {
                actionFlags |= ACT_INTERACT;
            }
        }

        MacroFrame frame;
        frame.pos = pPos;
        frame.playerWorldRot = playerWorldRot;
        frame.headLocalRot = headLocalRot;
        frame.camWorldRot = camWorldRot;
        frame.pitchX = pitchX;
        frame.actionFlags = actionFlags;
        frame.pad[0] = frame.pad[1] = frame.pad[2] = 0;

        s_Frames.push_back(frame);
    }

    // ==============================================================
    // 2. ВОСПРОИЗВЕДЕНИЕ ТИКА
    // ==============================================================
    void PlaybackTick(void* playerGO) {
        if (s_CurrentPlayIndex >= s_Frames.size()) {
            StopPlayback();
            s_StatusMessage = "✓ Воспроизведение завершено!";
            s_StatusTimer = 3.0f;
            return;
        }

        const auto& frame = s_Frames[s_CurrentPlayIndex++];

        void* playerTr = oGameObjectGetTransform ? oGameObjectGetTransform(playerGO) : nullptr;
        void* camPivotTr = player_mods::GetPlayerCamPivotTransform();
        void* camTr = player_mods::GetPlayerCameraTransform();
        void* mfps = GetActiveMobileFPS();
        void* pickRay = GetActivePickRay();
        void* doorRay = GetActiveDoorRay();
        void* inventory = GetActiveInventory();

        if (!playerTr || !IsNativeObjectAlive(playerTr)) return;

        // Позиция
        if (oTransformSetPosition) {
            Vector3 pos = frame.pos;
            oTransformSetPosition(playerTr, &pos);
        }

        // Вращения
        SetTransformRotation(playerTr, frame.playerWorldRot);
        if (camPivotTr) SetTransformLocalRotation(camPivotTr, frame.headLocalRot);
        if (camTr) SetTransformRotation(camTr, frame.camWorldRot);

        // MobileFPS rotationX
        if (mfps && s_Off_MobileFPS_rotationX != (size_t)-1 && IsNativeObjectAlive(mfps)) {
            *(float*)((uintptr_t)mfps + s_Off_MobileFPS_rotationX) = frame.pitchX;
        }

        // РАЗОВОЕ ВЗАИМОДЕЙСТВИЕ (ДВЕРЬ / ПОДБОР / ПРИМЕНЕНИЕ)
        if (frame.actionFlags & ACT_INTERACT) {
            // А) Дверь
            if (doorRay && IsNativeObjectAlive(doorRay)) {
                if (s_Off_DoorRay_RaycastDis != (size_t)-1) *(float*)((uintptr_t)doorRay + s_Off_DoorRay_RaycastDis) = 7.0f;
                if (s_Off_DoorRay_buttonClicked != (size_t)-1) *(bool*)((uintptr_t)doorRay + s_Off_DoorRay_buttonClicked) = true;

                void* btnDoor = il2cpp_class_get_method_from_name(s_DoorRayClass, "HandleButtonClick", 0);
                if (btnDoor) {
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(btnDoor, doorRay, nullptr, &exc);
                }
            }

            // Б) Подбор предмета
            if (pickRay && IsNativeObjectAlive(pickRay)) {
                if (s_Off_PickRay_RaycastDis != (size_t)-1) *(float*)((uintptr_t)pickRay + s_Off_PickRay_RaycastDis) = 7.0f;
                if (s_Off_PickRay_RaycastCheckItemDis != (size_t)-1) *(float*)((uintptr_t)pickRay + s_Off_PickRay_RaycastCheckItemDis) = 7.0f;
                if (s_Off_PickRay_buttonClicked != (size_t)-1) *(bool*)((uintptr_t)pickRay + s_Off_PickRay_buttonClicked) = true;

                void* btnPick = il2cpp_class_get_method_from_name(s_PickRayClass, "HandleButtonClick1", 0);
                if (btnPick) {
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(btnPick, pickRay, nullptr, &exc);
                }

                // В) Действие / выстрел / применение ключа
                if (s_Off_PickRay_buttonClicked2 != (size_t)-1) *(bool*)((uintptr_t)pickRay + s_Off_PickRay_buttonClicked2) = true;
                void* btnAction = il2cpp_class_get_method_from_name(s_PickRayClass, "HandleButtonClick", 0);
                if (btnAction) {
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(btnAction, pickRay, nullptr, &exc);
                }
            }
        }

        // РАЗОВЫЙ СБРОС ПРЕДМЕТА
        if (frame.actionFlags & ACT_DROP) {
            if (pickRay && IsNativeObjectAlive(pickRay)) {
                void* dropMethod = il2cpp_class_get_method_from_name(s_PickRayClass, "CheckItemDropping", 0);
                if (dropMethod) {
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(dropMethod, pickRay, nullptr, &exc);
                }
            }
            if (inventory && IsNativeObjectAlive(inventory)) {
                void* dropLogicMethod = il2cpp_class_get_method_from_name(s_InventoryClass, "DropLogic", 0);
                if (dropLogicMethod) {
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(dropLogicMethod, inventory, nullptr, &exc);
                }
            }
        }
    }

    void ArmRecording() {
        s_Frames.clear();
        s_CurrentPlayIndex = 0;
        s_GameTimeAccumulator = 0.0f;
        s_TouchWasDown = false;
        s_TouchMaxDrag = 0.0f;
        s_PendingInteract = false;
        s_PendingDrop = false;
        s_RecordedInteracts = 0;
        s_RecordedDrops = 0;

        ResolveDynamicOffsets();

        void* playerGO = GetActivePlayerGO();
        if (playerGO) {
            currentState = STATE_RECORDING;
            if (oSetTimeScale) oSetTimeScale(recordingTimeScale);
            if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f * recordingTimeScale);
            s_StatusMessage = "⏺ Запись активна (0.20x)!";
        } else {
            currentState = STATE_ARMED_RECORD;
            s_StatusMessage = "⏳ Жду спавна игрока для старта записи...";
        }
        s_StatusTimer = 3.5f;
    }

    void StopRecording() {
        currentState = STATE_IDLE;
        if (oSetTimeScale) oSetTimeScale(1.0f);
        if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f);

        s_StatusMessage = "⏹ Запись завершена! Кадров: " + std::to_string(s_Frames.size());
        s_StatusTimer = 3.0f;
    }

    void ArmPlayback() {
        if (s_Frames.empty()) {
            s_StatusMessage = "⚠️ Нет сохранённых кадров!";
            s_StatusTimer = 3.0f;
            return;
        }

        s_CurrentPlayIndex = 0;
        s_GameTimeAccumulator = 0.0f;

        ResolveDynamicOffsets();

        void* playerGO = GetActivePlayerGO();
        if (playerGO) {
            currentState = STATE_PLAYING;
            if (oSetTimeScale) oSetTimeScale(playbackTimeScale);
            if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f * playbackTimeScale);
            s_StatusMessage = "▶ Воспроизведение запущено (1.00x)!";
        } else {
            currentState = STATE_ARMED_PLAY;
            s_StatusMessage = "⏳ Жду старта рана для запуска макроса...";
        }
        s_StatusTimer = 3.5f;
    }

    void StopPlayback() {
        currentState = STATE_IDLE;
        if (oSetTimeScale) oSetTimeScale(1.0f);
        if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f);

        s_StatusMessage = "⏹ Воспроизведение остановлено";
        s_StatusTimer = 2.0f;
    }

    // ==============================================================
    // ЦИКЛ ОБНОВЛЕНИЯ С ТОЧНЫМ ДЕТЕКТОРОМ ТАПОВ
    // ==============================================================
    void Update() {
        if (currentState == STATE_IDLE) return;

        void* playerGO = GetActivePlayerGO();

        if (currentState == STATE_ARMED_RECORD) {
            if (playerGO) {
                currentState = STATE_RECORDING;
                s_TouchWasDown = false;
                s_TouchMaxDrag = 0.0f;
                s_PendingInteract = false;
                s_PendingDrop = false;
                ResolveDynamicOffsets();

                if (oSetTimeScale) oSetTimeScale(recordingTimeScale);
                if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f * recordingTimeScale);
                s_StatusMessage = "⏺ Игрок появился! Запись пошла (0.20x)...";
                s_StatusTimer = 2.5f;
            }
            return;
        }
        else if (currentState == STATE_ARMED_PLAY) {
            if (playerGO) {
                currentState = STATE_PLAYING;
                ResolveDynamicOffsets();
                if (oSetTimeScale) oSetTimeScale(playbackTimeScale);
                if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f * playbackTimeScale);
                s_StatusMessage = "▶ Игрок появился! Воспроизведение пошло (1.00x)...";
                s_StatusTimer = 2.5f;
            }
            return;
        }

        if (!playerGO) {
            if (currentState == STATE_RECORDING && !s_Frames.empty()) {
                StopRecording();
            } else if (currentState == STATE_PLAYING) {
                StopPlayback();
            }
            return;
        }

        // ДЕТЕКТОР ТАПА (КАЖДЫЙ КАДР EGL)
        if (currentState == STATE_RECORDING && !g_ShowMenu) {
            bool isDown = false;
            if (oGetMouseButton) isDown = oGetMouseButton(0);

            Vector3 mPos{ 0, 0, 0 };
            if (oGetMousePosition) oGetMousePosition(&mPos);

            ImGuiIO& io = ImGui::GetIO();
            float screenW = io.DisplaySize.x > 0 ? io.DisplaySize.x : 1920.0f;
            float screenH = io.DisplaySize.y > 0 ? io.DisplaySize.y : 1080.0f;

            if (isDown && !s_TouchWasDown) {
                s_TouchStartPos = mPos;
                s_TouchMaxDrag = 0.0f;
            }
            else if (isDown && s_TouchWasDown) {
                float dx = mPos.x - s_TouchStartPos.x;
                float dy = mPos.y - s_TouchStartPos.y;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist > s_TouchMaxDrag) s_TouchMaxDrag = dist;
            }
            else if (!isDown && s_TouchWasDown) {
                // Если сдвиг меньше 30 пикселей — ЭТО ТАП
                if (s_TouchMaxDrag < 30.0f * g_UiScale && s_TouchStartPos.y > 60.0f * g_UiScale) {
                    void* pickRay = GetActivePickRay();
                    bool hasItem = IsHoldingItemDirect(pickRay);

                    // Дроп (только при тапе в нижней центральной зоне И когда CanDropDis == true)
                    if (hasItem && s_TouchStartPos.y < screenH * 0.28f && s_TouchStartPos.x > screenW * 0.30f && s_TouchStartPos.x < screenW * 0.70f) {
                        s_PendingDrop = true;
                    }
                    // Взаимодействие (правая половина экрана вне джойстика)
                    else if (s_TouchStartPos.x > screenW * 0.38f) {
                        s_PendingInteract = true;
                    }
                }
            }
            s_TouchWasDown = isDown;
        }

        float realDt = ImGui::GetIO().DeltaTime;
        if (realDt <= 0.0f || realDt > 0.5f) realDt = 0.016f;

        float activeTimeScale = (currentState == STATE_RECORDING) ? recordingTimeScale : playbackTimeScale;
        float inGameDt = realDt * activeTimeScale;

        s_GameTimeAccumulator += inGameDt;
        float tickInterval = 1.0f / (float)targetTPS;

        while (s_GameTimeAccumulator >= tickInterval) {
            s_GameTimeAccumulator -= tickInterval;

            if (currentState == STATE_RECORDING) {
                RecordTick(playerGO);
            }
            else if (currentState == STATE_PLAYING) {
                PlaybackTick(playerGO);
            }
        }
    }

    void RefreshMacroList() {
        s_FoundMacros.clear();
        std::string folder = "/sdcard/Kahanium";
        DIR* dir = opendir(folder.c_str());
        if (!dir) {
            folder = "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files";
            dir = opendir(folder.c_str());
        }

        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string fName = entry->d_name;
                if (fName.length() > 4 && fName.rfind(".tas") == fName.length() - 4) {
                    s_FoundMacros.push_back(fName);
                }
            }
            closedir(dir);
        }
    }

    void SaveMacroToFile(const char* name) {
        if (s_Frames.empty()) {
            s_StatusMessage = "⚠️ Нечего сохранять!";
            s_StatusTimer = 2.0f;
            return;
        }

        std::string fName = name && strlen(name) > 0 ? name : s_MacroFileName;
        if (fName.find(".tas") == std::string::npos) fName += ".tas";

        std::string fullPath = "/sdcard/Kahanium/" + fName;
        FILE* f = fopen(fullPath.c_str(), "wb");
        if (!f) {
            fullPath = "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files/" + fName;
            f = fopen(fullPath.c_str(), "wb");
        }

        if (!f) {
            s_StatusMessage = "❌ Ошибка записи: " + fName;
            s_StatusTimer = 3.0f;
            return;
        }

        uint32_t tps = targetTPS;
        uint32_t count = (uint32_t)s_Frames.size();
        uint32_t frameSize = sizeof(MacroFrame);

        fwrite("KTAS", 1, 4, f);
        fwrite(&tps, sizeof(uint32_t), 1, f);
        fwrite(&count, sizeof(uint32_t), 1, f);
        fwrite(&frameSize, sizeof(uint32_t), 1, f);
        fwrite(s_Frames.data(), sizeof(MacroFrame), count, f);
        fclose(f);

        RefreshMacroList();
        s_StatusMessage = "💾 Сохранено (" + std::to_string(count) + " кадров): " + fName;
        s_StatusTimer = 3.0f;
    }

    void LoadMacroFromFile(const char* name) {
        std::string fName = name && strlen(name) > 0 ? name : s_MacroFileName;
        if (fName.find(".tas") == std::string::npos) fName += ".tas";

        std::string fullPath = "/sdcard/Kahanium/" + fName;
        FILE* f = fopen(fullPath.c_str(), "rb");
        if (!f) {
            fullPath = "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files/" + fName;
            f = fopen(fullPath.c_str(), "rb");
        }

        if (!f) {
            s_StatusMessage = "❌ Файл не найден: " + fName;
            s_StatusTimer = 3.0f;
            return;
        }

        char magic[4];
        uint32_t tps = 120;
        uint32_t count = 0;
        uint32_t frameSize = 0;

        fread(magic, 1, 4, f);
        fread(&tps, sizeof(uint32_t), 1, f);
        fread(&count, sizeof(uint32_t), 1, f);
        fread(&frameSize, sizeof(uint32_t), 1, f);

        if (strncmp(magic, "KTAS", 4) != 0 || count == 0 || count > 500000 || frameSize != sizeof(MacroFrame)) {
            fclose(f);
            s_StatusMessage = "❌ Несовместимый .tas файл";
            s_StatusTimer = 3.5f;
            return;
        }

        targetTPS = tps;
        s_Frames.resize(count);
        fread(s_Frames.data(), sizeof(MacroFrame), count, f);
        fclose(f);

        s_StatusMessage = "✓ Загружено " + std::to_string(count) + " кадров (" + std::to_string(tps) + " TPS)";
        s_StatusTimer = 3.0f;
    }

    void DrawMenu() {
        bool dummyToggle = (currentState != STATE_IDLE);
        DrawFeatureCard("macro_tas_card",
            LOC("TAS Макрос & Спидран Бот", "TAS Macro & Speedrun Bot"),
            LOC("Запись ранов на 0.2x с точными ивентами действий", "Record at 0.2x with exact one-shot events"),
            &dummyToggle);

        if (BeginSubSettingsAnim("macro_tas_card", true, 310.0f * g_UiScale)) {
            if (currentState == STATE_ARMED_RECORD) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", LOC("⏳ ОЖИДАНИЕ СПАВНА: запись начнется при старте рана...", "⏳ STANDBY: Recording will start upon spawn..."));
            }
            else if (currentState == STATE_ARMED_PLAY) {
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), "%s", LOC("⏳ ОЖИДАНИЕ СПАВНА: повтор начнется при старте рана...", "⏳ STANDBY: Playback will start upon spawn..."));
            }
            else if (currentState == STATE_RECORDING) {
                float recordedSec = (float)s_Frames.size() / (float)targetTPS;
                ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "● ИДЁТ ЗАПИСЬ: %zu кадров (%.1f с) | %d TPS (0.20x)", s_Frames.size(), recordedSec, targetTPS);
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "🎯 Действий: %d  |  📦 Дропов: %d", s_RecordedInteracts, s_RecordedDrops);
            }
            else if (currentState == STATE_PLAYING) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "▶ ВОСПРОИЗВЕДЕНИЕ: %zu / %zu кадров (%.1f%%)", s_CurrentPlayIndex, s_Frames.size(), s_Frames.empty() ? 0.0f : ((float)s_CurrentPlayIndex / (float)s_Frames.size() * 100.0f));
            }
            else {
                float totalSec = (float)s_Frames.size() / (float)targetTPS;
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "⏸ ГОТОВ: %zu кадров (%.1f с) | %d TPS", s_Frames.size(), totalSec, targetTPS);
            }

            ImGui::Spacing();

            float availW = ImGui::GetContentRegionAvail().x;
            float mainBtnW = (availW - 8.0f * g_UiScale) * 0.5f;

            if (currentState != STATE_RECORDING && currentState != STATE_ARMED_RECORD) {
                if (ImGui::Button(LOC(" ⏺ Начать запись ", " ⏺ Start Record "), ImVec2(mainBtnW, 32.0f * g_UiScale))) {
                    ArmRecording();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.15f, 0.15f, 1.0f));
                if (ImGui::Button(LOC(" ⏹ Стоп запись ", " ⏹ Stop Record "), ImVec2(mainBtnW, 32.0f * g_UiScale))) {
                    StopRecording();
                }
                ImGui::PopStyleColor();
            }

            ImGui::SameLine(0, 8.0f * g_UiScale);

            if (currentState != STATE_PLAYING && currentState != STATE_ARMED_PLAY) {
                if (ImGui::Button(LOC(" ▶ Воспроизвести ", " ▶ Play Macro "), ImVec2(mainBtnW, 32.0f * g_UiScale))) {
                    ArmPlayback();
                }
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.2f, 1.0f));
                if (ImGui::Button(LOC(" ⏹ Стоп повтор ", " ⏹ Stop Play "), ImVec2(mainBtnW, 32.0f * g_UiScale))) {
                    StopPlayback();
                }
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::Text("%s:", LOC("Частота тиков (TPS)", "Tickrate (TPS)"));
            ImGui::RadioButton("60 TPS", &targetTPS, 60);
            ImGui::SameLine(0, 10.0f * g_UiScale);
            ImGui::RadioButton("120 TPS", &targetTPS, 120);
            ImGui::SameLine(0, 10.0f * g_UiScale);
            ImGui::RadioButton("240 TPS", &targetTPS, 240);

            ImGui::Text("%s:", LOC("Скорость замедления записи", "Record Speed"));
            DrawSliderWithInput("##recSpeed", &recordingTimeScale, 0.05f, 1.0f, "%.2fx");

            ImGui::Text("%s:", LOC("Скорость воспроизведения", "Playback Speed"));
            DrawSliderWithInput("##playSpeed", &playbackTimeScale, 0.5f, 5.0f, "%.2fx");

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::TextColored(Theme::TextSecondary, "%s", LOC("УПРАВЛЕНИЕ ФАЙЛАМИ (.TAS):", "FILE MANAGER (.TAS):"));
            ImGui::SetNextItemWidth(availW - 170.0f * g_UiScale);
            ImGui::InputTextWithHint("##tasName", "Имя макроса...", s_MacroFileName, sizeof(s_MacroFileName));

            ImGui::SameLine();
            if (ImGui::Button(LOC(" 💾 Сохранить ", " Save "), ImVec2(80 * g_UiScale, 28 * g_UiScale))) {
                SaveMacroToFile(s_MacroFileName);
            }
            ImGui::SameLine(0, 6.0f * g_UiScale);
            if (ImGui::Button(LOC(" 📁 Загрузить ", " Load "), ImVec2(80 * g_UiScale, 28 * g_UiScale))) {
                LoadMacroFromFile(s_MacroFileName);
            }

            if (s_StatusTimer > 0.0f) {
                s_StatusTimer -= ImGui::GetIO().DeltaTime;
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", s_StatusMessage.c_str());
            }

            EndSubSettingsAnim("macro_tas_card");
        }
    }
}