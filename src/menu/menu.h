#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"

#include "ui_theme.h"
#include "ui_anim.h"
#include "ui_widgets.h"

#include <vector>
#include <string>
#include <algorithm>

extern bool g_ShowMenu;
extern bool g_MenuMinimized;
extern bool g_InitImGui;
extern int activeTab;

extern float g_UiScale;
extern float g_HomeBarWidth;
extern float g_ScrollbarSize;
extern float g_AccentColor[4];

extern int g_Language;
// Menu hanya memakai bahasa Inggris
inline const char* LOC(const char* ru, const char* en) {
    (void)ru;
    return en;
}

extern bool g_SnowEnabled;
extern bool g_WindowGlowEnabled;
extern bool g_ShowFPS;
extern float g_BackgroundDim;
extern float g_MenuAlpha;
extern float g_CornerRounding;
extern float g_PopupScale;
extern float g_PopupAlpha;
extern bool g_HideGInGame;
extern float g_MenuBgColor[3];
extern float g_PopupColor[3];
extern float g_PopupBgColor[3];
extern float g_RealFPS;
extern float g_FrameTimeMs;

// Совместимость с остальными модулями
namespace Theme {
    inline ImVec4 GetAccent() { return c::accent; }
    inline ImVec4 GetAccentHover() { return c::accent; }
    const ImVec4 BgDark          = ImVec4(0.07f, 0.07f, 0.09f, 0.98f);
    const ImVec4 BgSidebar       = ImVec4(0.09f, 0.09f, 0.12f, 0.98f);
    const ImVec4 CardBg          = ImVec4(0.11f, 0.11f, 0.15f, 1.00f);
    const ImVec4 CardBgHover     = ImVec4(0.14f, 0.14f, 0.19f, 1.00f);
    const ImVec4 BorderColor     = ImVec4(0.20f, 0.20f, 0.26f, 0.90f);
    const ImVec4 TextPrimary     = ImVec4(0.95f, 0.95f, 0.97f, 1.00f);
    const ImVec4 TextSecondary   = ImVec4(0.52f, 0.52f, 0.58f, 1.00f);
}

void SetupPremiumStyle(float scale);
void DrawMenu();
void OpenFolderInExternalFileManager(const char* targetPath);

// Алиасы для модулей
inline bool SmoothToggle(const char* label, bool* v) { return edited::checkbox(label, v); }
inline void DrawFeatureCard(const char* id, const char* title, const char* description, bool* state) { edited::checkbox(title, state); }
inline void DrawFeatureCardWithGear(const char* id, const char* title, const char* description, bool* state, bool* gearState) { edited::checkbox(title, state); }
inline void DrawSliderWithInput(const char* id, float* v, float v_min, float v_max, const char* format) { edited::slider_float(id, v, v_min, v_max, format); }
inline bool BeginSubSettingsAnim(const char* id, bool isOpen, float maxContentHeight) { return isOpen; }
inline void EndSubSettingsAnim(const char* id) { ImGui::Spacing(); }