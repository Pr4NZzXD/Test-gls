#include "reshade.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>

namespace reshade {
    bool disableDarkerFog = true;
    bool disableLightmaps = false;

    static bool s_LastLightmapState = false;
    static std::unordered_map<void*, int32_t> s_OriginalLightmapIndices;

    void CleanDarkerFromHierarchy(void* tr) {
        if (!tr || !IsNativeObjectAlive(tr) || !oTransformGetChildCount || !oTransformGetChild) return;

        int32_t count = oTransformGetChildCount(tr);
        for (int32_t i = 0; i < count; i++) {
            void* childTr = oTransformGetChild(tr, i);
            if (!childTr || !IsNativeObjectAlive(childTr)) continue;

            std::string name = GetUnityObjectName(childTr);
            std::string lowerName = name;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.find("dark") != std::string::npos ||
                lowerName.find("vignette") != std::string::npos ||
                lowerName.find("filter") != std::string::npos ||
                lowerName.find("fog") != std::string::npos ||
                lowerName.find("black") != std::string::npos) {

                void* childGO = oComponentGetGameObject ? oComponentGetGameObject(childTr) : nullptr;
                if (childGO && IsNativeObjectAlive(childGO) && oSetGameObjectActive) {
                    oSetGameObjectActive(childGO, false);
                }
            }

            CleanDarkerFromHierarchy(childTr);
        }
    }

    void KillGrannyDarkness() {
        if (oSetFog) oSetFog(false);
        if (oSetFogDensity) oSetFogDensity(0.0f);

        if (oSetAmbientLight) {
            Color fullLight = { 0.85f, 0.85f, 0.85f, 1.0f };
            oSetAmbientLight(&fullLight);
        }

        static int s_CleanTimer = 0;
        if (++s_CleanTimer % 45 == 0) {
            void* mainCam = oGetMainCamera ? oGetMainCamera() : nullptr;
            if (mainCam && IsNativeObjectAlive(mainCam)) {
                void* camTr = oComponentGetTransform ? oComponentGetTransform(mainCam) : nullptr;
                if (camTr && IsNativeObjectAlive(camTr)) {
                    CleanDarkerFromHierarchy(camTr);

                    if (oTransformGetParent) {
                        void* pivotTr = oTransformGetParent(camTr);
                        if (pivotTr && IsNativeObjectAlive(pivotTr)) {
                            CleanDarkerFromHierarchy(pivotTr);
                        }
                    }
                }
            }
        }
    }

    // ОТКЛЮЧЕНИЕ БЕЗ МЕРЦАНИЯ (ПРИМЕНЯЕТСЯ СТРОГО 1 РАЗ)
    void ApplyNoLightmaps(bool enable) {
        if (!IsSceneReady() || !g_RendererTypeObj) return;

        void* renClass = FindClass("UnityEngine", "Renderer");
        void* setLightmapIndexMethod = renClass ? il2cpp_class_get_method_from_name(renClass, "set_lightmapIndex", 1) : nullptr;
        void* getLightmapIndexMethod = renClass ? il2cpp_class_get_method_from_name(renClass, "get_lightmapIndex", 0) : nullptr;

        std::vector<void*> renderers = FindObjectsOfUnityType(g_RendererTypeObj);

        for (void* ren : renderers) {
            if (!ren || !IsNativeObjectAlive(ren)) continue;

            if (enable) {
                if (getLightmapIndexMethod && s_OriginalLightmapIndices.find(ren) == s_OriginalLightmapIndices.end()) {
                    void* exc = nullptr;
                    void* res = il2cpp_runtime_invoke(getLightmapIndexMethod, ren, nullptr, &exc);
                    if (res && !exc) {
                        s_OriginalLightmapIndices[ren] = *(int32_t*)((uintptr_t)res + 0x10);
                    }
                }

                if (setLightmapIndexMethod) {
                    int32_t disabledIdx = -1;
                    void* args[1] = { &disabledIdx };
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(setLightmapIndexMethod, ren, args, &exc);
                }
            } else {
                if (setLightmapIndexMethod && s_OriginalLightmapIndices.find(ren) != s_OriginalLightmapIndices.end()) {
                    int32_t origIdx = s_OriginalLightmapIndices[ren];
                    void* args[1] = { &origIdx };
                    void* exc = nullptr;
                    il2cpp_runtime_invoke(setLightmapIndexMethod, ren, args, &exc);
                }
            }
        }

        if (oGameObjectFind && il2cpp_string_new && oSetGameObjectActive) {
            const char* lmRoots[] = { "!ftraceLightmaps", "Lightmaps", "!ftrace" };
            for (const char* root : lmRoots) {
                void* str = il2cpp_string_new(root);
                if (str) {
                    void* go = oGameObjectFind(str);
                    if (go && IsNativeObjectAlive(go)) {
                        oSetGameObjectActive(go, !enable);
                    }
                }
            }
        }

        if (oSetAmbientLight) {
            Color ambColor = enable ? Color{ 0.95f, 0.95f, 0.95f, 1.0f } : Color{ 0.25f, 0.25f, 0.25f, 1.0f };
            oSetAmbientLight(&ambColor);
        }

        if (!enable) {
            s_OriginalLightmapIndices.clear();
        }
    }

    void Update() {
        if (disableDarkerFog) {
            KillGrannyDarkness();
        }

        // Вызываем ТОЛЬКО при смене тумблера (0 мерцания)
        if (disableLightmaps != s_LastLightmapState) {
            ApplyNoLightmaps(disableLightmaps);
            s_LastLightmapState = disableLightmaps;
        }
    }

    void DrawMenu() {
        DrawFeatureCard("no_darker_fog",
            LOC("Убрать темный туман и Fullbright", "Remove Darker Fog & Fullbright"),
            LOC("Удаляет черноту и полностью освещает дом", "Kills black fog and illuminates the mansion"),
            &disableDarkerFog);

        DrawFeatureCard("no_lightmaps",
            LOC("Отключить запечку (No Lightmaps)", "Disable Lightmaps (No Shadows)"),
            LOC("Сбрасывает запеченные тени FTrace, делает графику плоской и чистой", "Strips baked FTrace shadows for pure flat look"),
            &disableLightmaps);
    }
}