#include "player_mods.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace player_mods {
    bool godmode = false;
    static bool lastGodmode = false;

    bool noFallDamage = false;
    static bool lastNoFallDamage = false;

    bool fovChanger = false;
    float customFov = 85.0f;

    bool infiniteAmmo = false;
    bool electricDarts = true;
    bool explosiveShotgun = false;
    bool showWeaponSettings = false;

    static void* s_CachedPlayerTr = nullptr;
    static void* s_CachedCameraTr = nullptr;
    static void* s_CachedCamPivotTr = nullptr;
    static void* s_CachedPickRayComp = nullptr;
    static void* s_PickRayClass = nullptr;

    void ClearCache() {
        s_CachedPlayerTr = nullptr;
        s_CachedCameraTr = nullptr;
        s_CachedCamPivotTr = nullptr;
        s_CachedPickRayComp = nullptr;
        s_PickRayClass = nullptr;
    }

    void Init() {
        ClearCache();
        LOGI("[+] Player Mods Initialized!");
    }

    void* GetPlayerRootTransform() {
        if (s_CachedPlayerTr && IsNativeObjectAlive(s_CachedPlayerTr)) return s_CachedPlayerTr;
        if (oGameObjectFind && il2cpp_string_new && oGameObjectGetTransform) {
            void* pStr = il2cpp_string_new("PlayerStuff/Player");
            if (pStr) {
                void* go = oGameObjectFind(pStr);
                if (go && IsNativeObjectAlive(go)) {
                    s_CachedPlayerTr = oGameObjectGetTransform(go);
                    return s_CachedPlayerTr;
                }
            }
        }
        return nullptr;
    }

    void* GetPlayerCameraTransform() {
        if (s_CachedCameraTr && IsNativeObjectAlive(s_CachedCameraTr)) return s_CachedCameraTr;
        if (oGameObjectFind && il2cpp_string_new && oGameObjectGetTransform) {
            void* cStr = il2cpp_string_new("PlayerStuff/Player/CameraShakeAnim/CameraPivot/Main Camera");
            if (cStr) {
                void* go = oGameObjectFind(cStr);
                if (go && IsNativeObjectAlive(go)) {
                    s_CachedCameraTr = oGameObjectGetTransform(go);
                    return s_CachedCameraTr;
                }
            }
        }
        return nullptr;
    }

    void* GetPlayerCamPivotTransform() {
        if (s_CachedCamPivotTr && IsNativeObjectAlive(s_CachedCamPivotTr)) return s_CachedCamPivotTr;
        if (oGameObjectFind && il2cpp_string_new && oGameObjectGetTransform) {
            void* pStr = il2cpp_string_new("PlayerStuff/Player/CameraShakeAnim/CameraPivot");
            if (pStr) {
                void* go = oGameObjectFind(pStr);
                if (go && IsNativeObjectAlive(go)) {
                    s_CachedCamPivotTr = oGameObjectGetTransform(go);
                    return s_CachedCamPivotTr;
                }
            }
        }
        return nullptr;
    }

    void UpdateWeaponMods() {
        if (!infiniteAmmo || !IsSceneReady()) return;

        if (!s_CachedPickRayComp || !IsNativeObjectAlive(s_CachedPickRayComp)) {
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
                            }
                        }
                    }
                }
            }
        }

        if (s_CachedPickRayComp && s_PickRayClass && IsNativeObjectAlive(s_CachedPickRayComp)) {
            SetFieldFloat(s_CachedPickRayComp, s_PickRayClass, "AmmoAmount", 2.0f);
            SetFieldBool(s_CachedPickRayComp, s_PickRayClass, "LoadedGun", true);
            SetFieldBool(s_CachedPickRayComp, s_PickRayClass, "IsCharged", true);
            SetFieldBool(s_CachedPickRayComp, s_PickRayClass, "DartHasEnergy", electricDarts);
            SetFieldFloat(s_CachedPickRayComp, s_PickRayClass, "ShootSpeed", 40.0f);
            SetFieldFloat(s_CachedPickRayComp, s_PickRayClass, "SprayLeft", 999.0f);
            SetFieldFloat(s_CachedPickRayComp, s_PickRayClass, "Gun1RayDistance", 500.0f);
            SetFieldFloat(s_CachedPickRayComp, s_PickRayClass, "Gun2RayDistance", 500.0f);
            SetFieldBool(s_CachedPickRayComp, s_PickRayClass, "IsPOWER", explosiveShotgun);
        }
    }

    void ApplyGodmode(bool enable) {
        if (!IsSceneReady()) return;

        // 1. ДЕД (AI_Grandpa)
        void* grandpaClass = FindClass("", "AI_Grandpa");
        if (grandpaClass && il2cpp_class_get_type && il2cpp_type_get_object) {
            void* type = il2cpp_type_get_object(il2cpp_class_get_type(grandpaClass));
            if (type) {
                std::vector<void*> list = FindObjectsOfUnityType(type);
                for (void* comp : list) {
                    if (!comp || !IsNativeObjectAlive(comp)) continue;

                    SetFieldFloat(comp, grandpaClass, "DistanceJumpscare", enable ? 0.0f : 1.5f);
                    SetFieldFloat(comp, grandpaClass, "DistanceAttack", enable ? 0.0f : 1.5f);
                    SetFieldBool(comp, grandpaClass, "CaughtPlayer", enable ? false : true);
                    SetFieldFloat(comp, grandpaClass, "TimerBedCaught", enable ? 0.0f : 1.0f);
                    SetFieldBool(comp, grandpaClass, "IsShooting", false);
                    SetFieldFloat(comp, grandpaClass, "TimerShoot", 0.0f);
                    SetFieldBool(comp, grandpaClass, "UnSeenPlayer", enable ? true : false);
                    SetFieldFloat(comp, grandpaClass, "DistanceRayPL", enable ? 0.0f : 15.0f);
                    SetFieldBool(comp, grandpaClass, "PlayerTouchedRay", false);
                }
            }
        }

        // 2. БАБКА (AI_Granny)
        void* grannyClass = FindClass("", "AI_Granny");
        if (grannyClass && il2cpp_class_get_type && il2cpp_type_get_object) {
            void* type = il2cpp_type_get_object(il2cpp_class_get_type(grannyClass));
            if (type) {
                std::vector<void*> list = FindObjectsOfUnityType(type);
                for (void* comp : list) {
                    if (!comp || !IsNativeObjectAlive(comp)) continue;

                    SetFieldFloat(comp, grannyClass, "DistanceJumpscare", enable ? 0.0f : 1.5f);
                    SetFieldBool(comp, grannyClass, "CaughtPlayer", false);
                    SetFieldFloat(comp, grannyClass, "DistanceRayPL", enable ? 0.0f : 15.0f);
                    SetFieldBool(comp, grannyClass, "PlayerTouchedRay", false);
                    SetFieldFloat(comp, grannyClass, "TimerShoot", 0.0f);
                    SetFieldBool(comp, grannyClass, "UnSeenPlayer", enable ? true : false);
                    SetFieldBool(comp, grannyClass, "UsingWeaponProjectile", false);
                }
            }
        }

        // 3. ЗАЩИТА ОТ СМЕРТИ
        void* psClass = FindClass("", "PlayerStatus");
        if (psClass && il2cpp_class_get_type && il2cpp_type_get_object) {
            void* type = il2cpp_type_get_object(il2cpp_class_get_type(psClass));
            if (type) {
                std::vector<void*> list = FindObjectsOfUnityType(type);
                for (void* ps : list) {
                    if (!ps || !IsNativeObjectAlive(ps)) continue;
                    SetFieldBool(ps, psClass, "isDead", false);
                    SetFieldBool(ps, psClass, "PlayerDead", false);
                    SetFieldBool(ps, psClass, "Dead", false);
                }
            }
        }
    }

    void ApplyAntiFall(bool disableFalling) {
        const char* fallClasses[] = { "FallingHolder", "FallZeroCheck", "ResetFloorPlayer", "FallDamage" };
        for (const char* cls : fallClasses) {
            void* klass = FindClass("", cls);
            if (!klass) continue;
            void* type = il2cpp_class_get_type(klass);
            if (!type) continue;
            void* typeObj = il2cpp_type_get_object(type);
            if (!typeObj) continue;

            std::vector<void*> list = FindObjectsOfUnityType(typeObj);
            for (void* comp : list) {
                if (comp && IsNativeObjectAlive(comp) && oSetBehaviourEnabled) {
                    oSetBehaviourEnabled(comp, !disableFalling);
                }
            }
        }
    }

    void Update() {
        if (!IsSceneReady()) {
            ClearCache();
            return;
        }

        static int s_SlowTimer = 0;
        if (++s_SlowTimer % 20 == 0) {
            if (godmode) ApplyGodmode(true);
            if (noFallDamage) ApplyAntiFall(true);
        }

        if (godmode != lastGodmode) {
            ApplyGodmode(godmode);
            lastGodmode = godmode;
        }

        if (noFallDamage != lastNoFallDamage) {
            ApplyAntiFall(noFallDamage);
            lastNoFallDamage = noFallDamage;
        }

        if (fovChanger && oSetFieldOfView) {
            void* camTr = GetPlayerCameraTransform();
            if (camTr && g_GetComponentMethod && g_CameraTypeObj) {
                void* args[1] = { g_CameraTypeObj };
                void* exc = nullptr;
                void* cam = il2cpp_runtime_invoke(g_GetComponentMethod, camTr, args, &exc);
                if (cam && !exc && IsNativeObjectAlive(cam)) {
                    oSetFieldOfView(cam, customFov);
                }
            }
        }

        UpdateWeaponMods();
    }

    void DrawMenu() {
        DrawFeatureCard("godmode",
            LOC("Бессмертие (Godmode)", "Godmode"),
            LOC("Неуязвимость к врагам, дробовику, взрывам и ящикам", "Invulnerable to all enemies, shotguns, bombs & traps"),
            &godmode);

        DrawFeatureCardWithGear("infinite_ammo",
            LOC("Бесконечные патроны и Rapid Fire", "Infinite Ammo & Rapid Fire"),
            LOC("Дробовик и арбалет без перезарядки, бесконечный спрей", "Infinite ammo, rapid fire, shock darts & unlimited spray"),
            &infiniteAmmo, &showWeaponSettings);

        if (BeginSubSettingsAnim("infinite_ammo", showWeaponSettings || infiniteAmmo, 90.0f * g_UiScale)) {
            ImGui::Checkbox(LOC("Электро-заряд арбалета (Шокер на 30с)", "Shock Dart (Lightning Stun)"), &electricDarts);
            ImGui::Checkbox(LOC("Взрывные снаряды дробовика (Mega Power)", "Explosive Shotgun Shells (Mega Shot)"), &explosiveShotgun);
            EndSubSettingsAnim("infinite_ammo");
        }

        DrawFeatureCard("anti_fall",
            LOC("Анти-падение", "Anti-Fall"),
            LOC("Защита от урона при падении в бездну", "No void fall damage or position resets"),
            &noFallDamage);

        DrawFeatureCard("fov_changer",
            LOC("Кастомный угол обзора (FOV)", "Custom FOV"),
            LOC("Увеличение угла обзора камеры", "Wider camera vision angle"),
            &fovChanger);

        if (BeginSubSettingsAnim("fov_changer", fovChanger, 80.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", LOC("УГОЛ ОБЗОРА (FOV):", "FOV ANGLE:"));
            DrawSliderWithInput("##customFov", &customFov, 60.0f, 130.0f, "%.0f deg");
            if (ImGui::Button(LOC(" Сбросить (60°) ", " Reset FOV (60 deg) "), ImVec2(180.0f * g_UiScale, 30.0f * g_UiScale))) {
                customFov = 60.0f;
            }
            EndSubSettingsAnim("fov_changer");
        }
    }
}