#include "auto_farm.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <algorithm>

namespace auto_farm
{
    bool enabled = false;
    bool showSettings = false;
    int escapeMethod = 0; // 0 = Door, 1 = Car, 2 = Cellar, 3 = Robo
    float fastForwardSpeed = 15.0f;

    // ДЕФОЛТНЫЕ БЫСТРЫЕ ТАЙМИНГИ ДЛЯ ТОПОВЫХ ЧИПОВ
    float menuDelay = 0.5f;       // 0.5 сек в меню
    float cutsceneDelay = 1.5f;   // 1.5 сек на катсцену
    int totalWins = 0;

    static float s_Timer = 0.0f;
    static bool s_WasActive = false;
    static bool s_EscapeExecuted = false;

    void ClearCache() {
        s_EscapeExecuted = false;
    }

    void Init() {
        ClearCache();
        s_Timer = 0.0f;
        LOGI("[+] Auto-Win Infinite Farm Loop Ready!");
    }

    void LoadSceneByName(const char* sceneName) {
        if (!sceneName) return;
        int sceneIndex = (strcasecmp(sceneName, "Menu") == 0) ? 0 : 1;

        // 1. Попытка через Application.LoadLevel
        void* appClass = FindClass("UnityEngine", "Application");
        if (appClass) {
            void* loadLevelMethod = il2cpp_class_get_method_from_name(appClass, "LoadLevel", 1);
            if (loadLevelMethod) {
                if (il2cpp_string_new) {
                    void* strMono = il2cpp_string_new(sceneName);
                    if (strMono) {
                        void* exc = nullptr;
                        void* args[1] = { strMono };
                        il2cpp_runtime_invoke(loadLevelMethod, nullptr, args, &exc);
                        if (!exc) return;
                    }
                }
                void* exc = nullptr;
                void* args[1] = { &sceneIndex };
                il2cpp_runtime_invoke(loadLevelMethod, nullptr, args, &exc);
                return;
            }
        }

        // 2. Попытка через SceneManager.LoadScene
        void* sceneMgrClass = FindClass("UnityEngine.SceneManagement", "SceneManager");
        if (!sceneMgrClass) sceneMgrClass = FindClass("UnityEngine", "SceneManager");
        if (sceneMgrClass) {
            void* method = il2cpp_class_get_method_from_name(sceneMgrClass, "LoadScene", 1);
            if (method) {
                if (il2cpp_string_new) {
                    void* strMono = il2cpp_string_new(sceneName);
                    if (strMono) {
                        void* exc = nullptr;
                        void* args[1] = { strMono };
                        il2cpp_runtime_invoke(method, nullptr, args, &exc);
                        if (!exc) return;
                    }
                }
                void* exc = nullptr;
                void* args[1] = { &sceneIndex };
                il2cpp_runtime_invoke(method, nullptr, args, &exc);
            }
        }
    }

    // СВЕРХБЫСТРЫЙ ДЕТЕКТ СЦЕНЫ (0.001 МС БЕЗ ПЕРЕБОРА 3000 ОБЪЕКТОВ)
    std::string GetCurrentSceneName() {
        static void* s_GetLoadedNameMethod = nullptr;
        static bool s_MethodResolved = false;

        if (!s_MethodResolved) {
            void* appClass = FindClass("UnityEngine", "Application");
            if (appClass) {
                s_GetLoadedNameMethod = il2cpp_class_get_method_from_name(appClass, "get_loadedLevelName", 0);
            }
            s_MethodResolved = true;
        }

        if (s_GetLoadedNameMethod) {
            void* exc = nullptr;
            void* nameMono = il2cpp_runtime_invoke(s_GetLoadedNameMethod, nullptr, nullptr, &exc);
            if (nameMono && !exc) {
                std::string n = GetUnityObjectName(nameMono);
                if (!n.empty()) return n;
            }
        }

        // Быстрая проверка без перебора массива
        void* cam = GetCurrentCamera();
        if (cam && IsNativeObjectAlive(cam)) {
            return "Scene";
        }

        return "Menu";
    }

    bool ExecuteEscapeMethod(int methodIndex) {
        void* escapesClass = FindClass("", "Escapes");
        if (!escapesClass || !il2cpp_class_get_type || !il2cpp_type_get_object) return false;

        void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(escapesClass));
        if (!typeObj) return false;

        std::vector<void*> list = FindObjectsOfUnityType(typeObj);
        if (list.empty()) return false;

        void* escapesComp = list[0];
        if (!escapesComp || !IsNativeObjectAlive(escapesComp)) return false;

        const char* methodName = "EscapeDoor";
        if (methodIndex == 1) methodName = "EscapeCar";
        else if (methodIndex == 2) methodName = "EscapeCellar";
        else if (methodIndex == 3) methodName = "EscapeRobo";

        void* method = il2cpp_class_get_method_from_name(escapesClass, methodName, 0);
        if (!method) method = il2cpp_class_get_method_from_name(escapesClass, "EscapeDoor", 0);
        if (!method) return false;

        void* exc = nullptr;
        il2cpp_runtime_invoke(method, escapesComp, nullptr, &exc);
        return (exc == nullptr);
    }

    bool TriggerInstantWin() {
        return ExecuteEscapeMethod(escapeMethod);
    }

    void Update() {
        if (!enabled) {
            if (s_WasActive) {
                if (oSetTimeScale) oSetTimeScale(1.0f);
                if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f);
                s_WasActive = false;
                s_Timer = 0.0f;
                s_EscapeExecuted = false;
            }
            return;
        }

        s_WasActive = true;
        float dt = ImGui::GetIO().DeltaTime;
        std::string sceneName = GetCurrentSceneName();

        // 1. НАХОДИМСЯ В МЕНЮ
        if (sceneName == "Menu") {
            s_EscapeExecuted = false;

            if (oSetTimeScale) oSetTimeScale(1.0f);
            if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f);

            s_Timer += dt;
            // Используем настраиваемую задержку в меню
            if (s_Timer >= menuDelay) {
                LoadSceneByName("Scene");
                s_Timer = 0.0f;
            }
        }
        // 2. НАХОДИМСЯ В ИГРЕ
        else if (sceneName == "Scene") {
            if (!s_EscapeExecuted) {
                if (ExecuteEscapeMethod(escapeMethod)) {
                    s_EscapeExecuted = true;
                    totalWins++;
                    s_Timer = 0.0f;
                }
            } else {
                if (oSetTimeScale) oSetTimeScale(fastForwardSpeed);
                if (oSetFixedDeltaTime) oSetFixedDeltaTime(0.02f * fastForwardSpeed);

                s_Timer += dt;
                // Используем настраиваемую задержку завершения победы
                if (s_Timer >= cutsceneDelay) {
                    LoadSceneByName("Menu");
                    s_Timer = 0.0f;
                    s_EscapeExecuted = false;
                }
            }
        }
        // 3. ПЕРЕХОДНОЕ СОСТОЯНИЕ (СТАРТ)
        else {
            s_Timer += dt;
            if (s_Timer >= 2.0f) {
                LoadSceneByName("Scene");
                s_Timer = 0.0f;
            }
        }
    }

    void DrawMenu() {
        DrawFeatureCardWithGear("auto_farm",
            LOC("Авто-победа (Бесконечный фарм)", "Auto-Win Farm Loop"),
            LOC("Автоматический фарм побед и монет", "Infinite automated win & coin farmer"),
            &enabled, &showSettings);

        if (BeginSubSettingsAnim("auto_farm", showSettings || enabled, 290.0f * g_UiScale)) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOC("СПОСОБ ПОБЕДЫ / КОНЦОВКА:", "ESCAPE ROUTE / ENDING:"));
            ImGui::RadioButton(LOC("Главная дверь", "Main Door"), &escapeMethod, 0);
            ImGui::SameLine(0, 10.0f * g_UiScale);
            ImGui::RadioButton(LOC("Машина", "Car"), &escapeMethod, 1);
            ImGui::SameLine(0, 10.0f * g_UiScale);
            ImGui::RadioButton(LOC("Подвал", "Cellar"), &escapeMethod, 2);
            ImGui::SameLine(0, 10.0f * g_UiScale);
            ImGui::RadioButton(LOC("Робот", "Robo"), &escapeMethod, 3);

            ImGui::Spacing();
            ImGui::Text("%s", LOC("Скорость промотки катсцены:", "Cutscene Turbo Speed:"));
            DrawSliderWithInput("##cutsceneSpeed", &fastForwardSpeed, 5.0f, 30.0f, "%.0fx");

            ImGui::Spacing();
            ImGui::Text("%s", LOC("Задержка в меню перед стартом:", "Menu Wait Delay (Before Start):"));
            DrawSliderWithInput("##menuDelay", &menuDelay, 0.1f, 4.0f, "%.2f s");

            ImGui::Spacing();
            ImGui::Text("%s", LOC("Задержка катсцены до выхода:", "Cutscene Delay (Before Menu):"));
            DrawSliderWithInput("##cutsceneDelay", &cutsceneDelay, 0.3f, 5.0f, "%.2f s");

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s: %d", LOC("ВСЕГО ПОБЕД ВЫПОЛНЕНО", "TOTAL WINS COMPLETED"), totalWins);

            ImGui::Spacing();
            float btnW = ImGui::GetContentRegionAvail().x;
            if (ImGui::Button(LOC(" [⚡] Выиграть прямо сейчас (Разово) ", " [⚡] Trigger Win Right Now (Once) "), ImVec2(btnW, 30.0f * g_UiScale))) {
                TriggerInstantWin();
            }

            EndSubSettingsAnim("auto_farm");
        }
    }
}