#include "speedhack.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <algorithm>
#include <vector>
#include <cmath>

namespace speedhack {
    bool enabled = false;
    float multiplier = 2.0f;

    static bool s_LastWorldEnabled = false;

    void ClearCache() {
        s_LastWorldEnabled = false;
    }

    void Init() {
        ClearCache();
        LOGI("[+] Speedhack Module Ready!");
    }

    void SafeSetAudioSourcePitch(void* audioSource, float pitch) {
        if (!audioSource || !IsNativeObjectAlive(audioSource)) return;
        if (oSetAudioSourcePitch) {
            oSetAudioSourcePitch(audioSource, pitch);
            return;
        }

        void* audioClass = FindClass("UnityEngine", "AudioSource");
        if (audioClass && il2cpp_class_get_method_from_name && il2cpp_runtime_invoke) {
            void* method = il2cpp_class_get_method_from_name(audioClass, "set_pitch", 1);
            if (method) {
                void* exc = nullptr;
                void* args[1] = { &pitch };
                il2cpp_runtime_invoke(method, audioSource, args, &exc);
            }
        }
    }

    void ApplyPitchToAllAudioSources(float targetPitch) {
        void* audioClass = FindClass("UnityEngine", "AudioSource");
        if (!audioClass || !il2cpp_class_get_type || !il2cpp_type_get_object) return;

        void* audioTypeObj = il2cpp_type_get_object(il2cpp_class_get_type(audioClass));
        if (!audioTypeObj) return;

        std::vector<void*> sources = FindObjectsOfUnityType(audioTypeObj);
        for (void* src : sources) {
            if (src && IsNativeObjectAlive(src)) {
                SafeSetAudioSourcePitch(src, targetPitch);
            }
        }
    }

    // Проверка активности паузы игры (UI_Elements/SettingsOpened)
    bool IsGameSettingsOpened() {
        if (!oGameObjectFind || !il2cpp_string_new || !oGetGameObjectActive) return false;
        void* pStr = il2cpp_string_new("UI_Elements/SettingsOpened");
        if (pStr) {
            void* go = oGameObjectFind(pStr);
            if (go && IsNativeObjectAlive(go)) {
                return oGetGameObjectActive(go);
            }
        }
        return false;
    }

    void Update() {
        if (!IsSceneReady()) {
            ClearCache();
            return;
        }

        // РАБОТАЕМ СО ВРЕМЕНЕМ ТОЛЬКО ЕСЛИ ВКЛЮЧЕН ТУМБЛЕР СПИДХАКА
        if (enabled) {
            bool isPaused = IsGameSettingsOpened();

            if (isPaused) {
                // Во время паузы ставим timescale 0, НО fixedDeltaTime ВСЕГДА держим 0.02f (фикс бесконечного цикла)
                if (oSetTimeScale) oSetTimeScale(0.0f);
                if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f);
            } else {
                float target = multiplier;
                if (oSetTimeScale) oSetTimeScale(target);
                if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f * target);

                float pitch = std::clamp(target, 0.05f, 3.0f);
                ApplyPitchToAllAudioSources(pitch);
            }

            s_LastWorldEnabled = true;
        }
        else if (s_LastWorldEnabled) {
            // Возвращаем стандартные 1.0x и 0.02f при выключении
            if (oSetTimeScale) oSetTimeScale(1.0f);
            if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f);

            ApplyPitchToAllAudioSources(1.0f);
            s_LastWorldEnabled = false;
        }
    }

    void DrawMenu() {
        DrawFeatureCard("speedhack",
            LOC("Скорость мира (TimeScale)", "World Speed (TimeScale)"),
            LOC("Глобальное ускорение/замедление игры и звука", "Global game speed & audio slowdown"),
            &enabled);

        if (BeginSubSettingsAnim("speedhack", enabled, 130.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(0.95f, 0.65f, 0.2f, 1.0f), "%s", LOC("СКОРОСТЬ МИРА:", "WORLD SPEED:"));
            DrawSliderWithInput("##speedMult", &multiplier, 0.01f, 10.0f, "%.2fx"); // От 0.01x

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOC("БЫСТРЫЕ ПРЕСЕТЫ:", "QUICK PRESETS:"));

            float availW = ImGui::GetContentRegionAvail().x;
            float btnW = (availW - 20.0f * g_UiScale) / 6.0f;

            if (ImGui::Button("0.05x", ImVec2(btnW, 28.0f * g_UiScale))) multiplier = 0.05f;
            ImGui::SameLine(0, 4.0f * g_UiScale);
            if (ImGui::Button("0.1x", ImVec2(btnW, 28.0f * g_UiScale))) multiplier = 0.1f;
            ImGui::SameLine(0, 4.0f * g_UiScale);
            if (ImGui::Button("0.5x", ImVec2(btnW, 28.0f * g_UiScale))) multiplier = 0.5f;
            ImGui::SameLine(0, 4.0f * g_UiScale);
            if (ImGui::Button("1.0x", ImVec2(btnW, 28.0f * g_UiScale))) multiplier = 1.0f;
            ImGui::SameLine(0, 4.0f * g_UiScale);
            if (ImGui::Button("2.0x", ImVec2(btnW, 28.0f * g_UiScale))) multiplier = 2.0f;
            ImGui::SameLine(0, 4.0f * g_UiScale);
            if (ImGui::Button("5.0x", ImVec2(btnW, 28.0f * g_UiScale))) multiplier = 5.0f;

            EndSubSettingsAnim("speedhack");
        }
    }
}