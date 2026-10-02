#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "il2cpp_api.h"
#include "modules.h"
#include "menu.h"
#include "config.h"
#include "reshade.h"
#include "auto_farm.h"
#include "game_unlocks.h"
#include "Aim.h"
#include "imgui_impl_opengl3.h"

// ПОДКЛЮЧАЕМ МАССИВЫ ШРИФТОВ И ИКОНОК ТОЛЬКО ЗДЕСЬ
#include "menu/verdana.h"
#include "menu/icons.h"
#include "menu/iconss.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <unistd.h>
#include <dlfcn.h>
#include <pthread.h>
#include <chrono>

float g_RealFPS = 60.0f;
float g_FrameTimeMs = 16.6f;

typedef EGLBoolean (*eglSwapBuffers_t)(EGLDisplay dpy, EGLSurface surface);
eglSwapBuffers_t o_eglSwapBuffers = nullptr;

void UpdateGameInputBlock(bool block) {
    if (!IsSceneReady()) return;

    void* eventSysClass = FindClass("UnityEngine.EventSystems", "EventSystem");
    if (eventSysClass && il2cpp_class_get_type && il2cpp_type_get_object) {
        void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(eventSysClass));
        if (typeObj) {
            std::vector<void*> list = FindObjectsOfUnityType(typeObj);
            for (void* es : list) {
                if (es && IsNativeObjectAlive(es) && oSetBehaviourEnabled) {
                    oSetBehaviourEnabled(es, !block);
                }
            }
        }
    }

    void* mfpsClass = FindClass("", "MobileFPS");
    if (mfpsClass && il2cpp_class_get_type && il2cpp_type_get_object) {
        void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(mfpsClass));
        if (typeObj) {
            std::vector<void*> list = FindObjectsOfUnityType(typeObj);
            for (void* mfps : list) {
                if (mfps && IsNativeObjectAlive(mfps) && oSetBehaviourEnabled) {
                    oSetBehaviourEnabled(mfps, !block);
                }
            }
        }
    }
}

EGLBoolean hk_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    EGLint width = 0, height = 0;
    eglQuerySurface(dpy, surface, EGL_WIDTH, &width);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &height);

    if (width <= 0 || height <= 0) return o_eglSwapBuffers(dpy, surface);

    static thread_local bool s_ThreadAttached = false;
    if (!s_ThreadAttached && il2cpp_domain_get && il2cpp_thread_attach) {
        void* domain = il2cpp_domain_get();
        if (domain) {
            il2cpp_thread_attach(domain);
            s_ThreadAttached = true;
        }
    }

    static auto s_LastFrameTime = std::chrono::steady_clock::now();
    auto currentFrameTime = std::chrono::steady_clock::now();
    std::chrono::duration<float, std::milli> frameDelta = currentFrameTime - s_LastFrameTime;
    s_LastFrameTime = currentFrameTime;

    float dtMs = frameDelta.count();
    if (dtMs > 0.001f) {
        float rawFPS = 1000.0f / dtMs;
        g_RealFPS = g_RealFPS * 0.88f + rawFPS * 0.12f;
        g_FrameTimeMs = dtMs;
    }

    if (!g_InitImGui) {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2((float)width, (float)height);

        g_UiScale = (float)height / 720.0f;
        if (g_UiScale < 1.0f) g_UiScale = 1.0f;

        // 1. Основной шрифт текста Verdana
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        ImFont* mainFont = io.Fonts->AddFontFromMemoryTTF(
            (void*)Verdana, sizeof(Verdana), 13.5f * g_UiScale, &font_cfg
        );
        io.FontDefault = mainFont;

        // 2. Сливаем иконки FontAwesome прямо в основной шрифт (MergeMode = true)
        ImFontConfig icons_cfg;
        icons_cfg.MergeMode = true;
        icons_cfg.PixelSnapH = true;
        icons_cfg.FontDataOwnedByAtlas = false;
        static const ImWchar icons_ranges[] = { 0xe000, 0xf8ff, 0 };
        io.Fonts->AddFontFromMemoryTTF(
            (void*)icon, sizeof(icon), 14.0f * g_UiScale, &icons_cfg, icons_ranges
        );

        SetupPremiumStyle(g_UiScale);
        ImGui_ImplOpenGL3_Init("#version 300 es");
        g_InitImGui = true;

        config::Init();
        config::LoadConfig();
    }

    ImGuiIO& io = ImGui::GetIO();

    // Сенсорный ввод
    if (oGetMousePosition && oGetMouseButton) {
        Vector3 mousePos{ 0, 0, 0 };
        oGetMousePosition(&mousePos);
        bool isDown = oGetMouseButton(0);

        float touchX = mousePos.x;
        float touchY = io.DisplaySize.y - mousePos.y;

        io.AddMousePosEvent(touchX, touchY);
        io.AddMouseButtonEvent(0, isDown);
    }

    // ВОТ ОНО: ВЫЗОВ КЛАВИАТУРЫ ДЛЯ ВВОДА DEV PIN И ИМЕНИ КОНФИГА
    static bool s_LastWantText = false;
    if (io.WantTextInput != s_LastWantText) {
        ShowSoftKeyboard(io.WantTextInput);
        s_LastWantText = io.WantTextInput;
    }

    if (io.WantTextInput && oGetInputString) {
        void* monoStr = oGetInputString();
        if (monoStr) {
            int32_t len = *(int32_t*)((uintptr_t)monoStr + 0x10);
            uint16_t* chars = (uint16_t*)((uintptr_t)monoStr + 0x14);
            for (int i = 0; i < len; i++) {
                if (chars[i] == '\b' || chars[i] == 8) {
                    io.AddKeyEvent(ImGuiKey_Backspace, true);
                    io.AddKeyEvent(ImGuiKey_Backspace, false);
                } else if (chars[i] == '\n' || chars[i] == '\r') {
                    io.AddKeyEvent(ImGuiKey_Enter, true);
                    io.AddKeyEvent(ImGuiKey_Enter, false);
                } else {
                    io.AddInputCharacterUTF16(chars[i]);
                }
            }
        }
    }

    static bool s_LastMenuState = false;
    if (g_ShowMenu != s_LastMenuState) {
        UpdateGameInputBlock(g_ShowMenu);
        s_LastMenuState = g_ShowMenu;
    }

    static void* s_LastActiveCamera = nullptr;
    void* curCam = GetCurrentCamera();
    if (curCam != s_LastActiveCamera) {
        modules::ResetOnSceneChange();
        s_LastActiveCamera = curCam;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();

    if (IsSceneReady()) {
        modules::DrawOverlays();
    }

    DrawMenu();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    player_mods::Update();
    speedhack::Update();
    auto_farm::Update();
    game_unlocks::Update();
    reshade::Update();
    aim::Update();

    static int frameCount = 0;
    if (++frameCount % 25 == 0) {
        esp_enemies::Update();
        esp_items::Update();
        flat_textures::Update();
        scene_explorer::Update();
    }

    return o_eglSwapBuffers(dpy, surface);
}

void* ModThread(void*) {
    LOGI("[*] Kahanium Core Started...");

    void* eglHandle = dlopen("libEGL.so", RTLD_NOW);
    if (eglHandle) {
        void* swapBuffersAddr = dlsym(eglHandle, "eglSwapBuffers");
        if (swapBuffersAddr) {
            A64Hook(swapBuffersAddr, (void*)&hk_eglSwapBuffers, (void**)&o_eglSwapBuffers);
            LOGI("[+] eglSwapBuffers Hooked!");
        }
    }

    sleep(10);
    while (!InitIL2CPP()) sleep(1);

    modules::InitAll();
    LOGI("[+] Kahanium Ready!");
    return nullptr;
}

__attribute__((constructor))
void Init() {
    pthread_t thread;
    pthread_create(&thread, nullptr, ModThread, nullptr);
}