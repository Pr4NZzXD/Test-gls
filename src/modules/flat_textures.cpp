#include "flat_textures.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"
#include <unordered_map>
#include <vector>
#include <random>
#include <algorithm>

namespace flat_textures {
    bool flatEnabled = false;
    float flatColor[4] = { 0.85f, 0.85f, 0.85f, 1.0f };

    bool pixelateEnabled = false;
    int pixelateLevel = 3;

    bool randomTexturesActive = false;
    bool showRandomSettings = false;
    int randomizedTexturesCount = 0;

    static bool lastFlatState = false;
    static int lastPixelateLevel = -1;
    static std::unordered_map<void*, void*> g_OriginalTextures;

    void CacheOriginalTextures() {
        if (!IsSceneReady() || !g_RendererTypeObj || !g_GetMaterialsMethod || !g_GetMainTextureMethod) return;

        std::vector<void*> renderers = FindObjectsOfUnityType(g_RendererTypeObj);
        for (void* ren : renderers) {
            if (!ren || !IsNativeObjectAlive(ren)) continue;

            void* exc = nullptr;
            void* matsArray = il2cpp_runtime_invoke(g_GetMaterialsMethod, ren, nullptr, &exc);
            if (!matsArray || exc) continue;

            uint64_t count = *(uint64_t*)((uintptr_t)matsArray + 0x18);
            if (count == 0 || count > 32) continue;
            void** mats = (void**)((uintptr_t)matsArray + 0x20);

            for (uint64_t m = 0; m < count; m++) {
                void* mat = mats[m];
                if (!mat || !IsNativeObjectAlive(mat)) continue;

                if (g_OriginalTextures.find(mat) == g_OriginalTextures.end()) {
                    exc = nullptr;
                    void* origTex = il2cpp_runtime_invoke(g_GetMainTextureMethod, mat, nullptr, &exc);
                    if (!exc && origTex && IsNativeObjectAlive(origTex)) {
                        g_OriginalTextures[mat] = origTex;
                    }
                }
            }
        }
    }

    void ApplyFlatTextures(bool enable) {
        if (!IsSceneReady() || !g_RendererTypeObj || !g_GetMaterialsMethod) return;

        void* whiteTex = oGetWhiteTexture ? oGetWhiteTexture() : nullptr;
        if (!whiteTex) return;

        CacheOriginalTextures();

        std::vector<void*> renderers = FindObjectsOfUnityType(g_RendererTypeObj);

        for (void* ren : renderers) {
            if (!ren || !IsNativeObjectAlive(ren)) continue;

            void* exc = nullptr;
            void* matsArray = il2cpp_runtime_invoke(g_GetMaterialsMethod, ren, nullptr, &exc);
            if (!matsArray || exc) continue;

            uint64_t count = *(uint64_t*)((uintptr_t)matsArray + 0x18);
            if (count == 0 || count > 32) continue;
            void** mats = (void**)((uintptr_t)matsArray + 0x20);

            for (uint64_t m = 0; m < count; m++) {
                void* mat = mats[m];
                if (!mat || !IsNativeObjectAlive(mat)) continue;

                if (enable) {
                    if (g_SetMainTextureMethod) {
                        void* args[1] = { whiteTex };
                        il2cpp_runtime_invoke(g_SetMainTextureMethod, mat, args, &exc);
                    }
                    if (g_SetColorMethod) {
                        Color c{ flatColor[0], flatColor[1], flatColor[2], flatColor[3] };
                        void* args[1] = { &c };
                        il2cpp_runtime_invoke(g_SetColorMethod, mat, args, &exc);
                    }
                } else {
                    if (g_SetMainTextureMethod && g_OriginalTextures.find(mat) != g_OriginalTextures.end()) {
                        void* orig = g_OriginalTextures[mat];
                        if (orig && IsNativeObjectAlive(orig)) {
                            void* args[1] = { orig };
                            il2cpp_runtime_invoke(g_SetMainTextureMethod, mat, args, &exc);
                        }
                    }
                    if (g_SetColorMethod) {
                        Color whiteCol{ 1.0f, 1.0f, 1.0f, 1.0f };
                        void* args[1] = { &whiteCol };
                        il2cpp_runtime_invoke(g_SetColorMethod, mat, args, &exc);
                    }
                }
            }
        }
    }

    void ApplyRandomTextures() {
        if (!IsSceneReady() || !g_RendererTypeObj || !g_GetMaterialsMethod || !g_SetMainTextureMethod) return;

        CacheOriginalTextures();

        std::vector<void*> allMats;
        std::vector<void*> allTexs;

        for (auto& pair : g_OriginalTextures) {
            if (pair.first && IsNativeObjectAlive(pair.first) && pair.second && IsNativeObjectAlive(pair.second)) {
                allMats.push_back(pair.first);
                allTexs.push_back(pair.second);
            }
        }

        if (allTexs.size() < 2 || allMats.empty()) return;

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(allTexs.begin(), allTexs.end(), g);

        for (size_t i = 0; i < allMats.size(); i++) {
            void* mat = allMats[i];
            void* rndTex = allTexs[i % allTexs.size()];
            if (!mat || !IsNativeObjectAlive(mat) || !rndTex || !IsNativeObjectAlive(rndTex)) continue;

            void* exc = nullptr;
            void* args[1] = { rndTex };
            il2cpp_runtime_invoke(g_SetMainTextureMethod, mat, args, &exc);
        }

        randomizedTexturesCount = (int)allMats.size();
        randomTexturesActive = true;
    }

    void RestoreOriginalTextures() {
        if (!IsSceneReady() || !g_SetMainTextureMethod) return;

        for (auto& pair : g_OriginalTextures) {
            void* mat = pair.first;
            void* orig = pair.second;
            if (mat && IsNativeObjectAlive(mat) && orig && IsNativeObjectAlive(orig)) {
                void* exc = nullptr;
                void* args[1] = { orig };
                il2cpp_runtime_invoke(g_SetMainTextureMethod, mat, args, &exc);
            }
        }

        randomizedTexturesCount = 0;
        randomTexturesActive = false;
    }

    void Update() {
        if (!IsSceneReady()) {
            g_OriginalTextures.clear();
            randomizedTexturesCount = 0;
            randomTexturesActive = false;
            return;
        }

        if (oSetMasterTextureLimit) {
            int currentTarget = pixelateEnabled ? pixelateLevel : 0;
            if (currentTarget != lastPixelateLevel) {
                oSetMasterTextureLimit(currentTarget);
                lastPixelateLevel = currentTarget;
            }
        }

        if (flatEnabled != lastFlatState) {
            ApplyFlatTextures(flatEnabled);
            lastFlatState = flatEnabled;
        }
    }

    void DrawMenu() {
        DrawFeatureCard("flat_world",
            LOC("Однотонный мир (Flat World)", "Flat World"),
            LOC("Сплошные текстуры стен выбранного цвета", "Solid single color wall textures"),
            &flatEnabled);

        if (BeginSubSettingsAnim("flat_world", flatEnabled, 56.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOC("ЦВЕТ СТЕН:", "FLAT WORLD COLOR:"));
            ImGui::ColorEdit4("##flatColor", flatColor, ImGuiColorEditFlags_NoInputs);
            EndSubSettingsAnim("flat_world");
        }

        DrawFeatureCard("lofi_tex",
            LOC("Potato-текстуры", "Potato Textures"),
            LOC("Сверхнизкое разрешение для буста FPS", "Low-resolution FPS boost mode"),
            &pixelateEnabled);

        if (BeginSubSettingsAnim("lofi_tex", pixelateEnabled, 58.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", LOC("КАЧЕСТВО (1=128px, 4=16px):", "QUALITY (1=128px, 4=16px):"));
            float pLvl = (float)pixelateLevel;
            if (DrawSliderWithInput("##pixLvl", &pLvl, 1.0f, 4.0f, "Level %.0f"), false) {
                pixelateLevel = (int)pLvl;
            } else {
                pixelateLevel = (int)pLvl;
            }
            EndSubSettingsAnim("lofi_tex");
        }

        // АККУРАТНАЯ КАРТОЧКА С ШЕСТЕРЁНКОЙ ДЛЯ РАНДОМАЙЗЕРА
        DrawFeatureCardWithGear("cursed_textures",
            LOC("Рандомайзер текстур (Cursed)", "Texture Randomizer (Cursed)"),
            LOC("Случайно перемешивает текстуры всех стен и мебели", "Shuffles textures between all meshes in scene"),
            &randomTexturesActive, &showRandomSettings);

        if (BeginSubSettingsAnim("cursed_textures", showRandomSettings || randomTexturesActive, 90.0f * g_UiScale)) {
            float availW = ImGui::GetContentRegionAvail().x;
            float btnW = (availW - 8.0f * g_UiScale) * 0.5f;

            if (ImGui::Button(LOC(" 🎲 Перемешать ", " 🎲 Shuffle "), ImVec2(btnW, 30.0f * g_UiScale))) {
                ApplyRandomTextures();
            }

            ImGui::SameLine(0, 8.0f * g_UiScale);

            if (ImGui::Button(LOC(" ↺ Сбросить ", " ↺ Reset "), ImVec2(btnW, 30.0f * g_UiScale))) {
                RestoreOriginalTextures();
            }

            ImGui::Spacing();
            if (randomizedTexturesCount > 0) {
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s: %d %s",
                    LOC("✓ Активно", "✓ Randomized"), randomizedTexturesCount, LOC("материалов", "materials"));
            } else {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LOC("Текстуры в оригинальном состоянии", "Original textures active"));
            }

            EndSubSettingsAnim("cursed_textures");
        }
    }
}