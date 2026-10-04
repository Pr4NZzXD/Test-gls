#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "menu.h"
#include "modules.h"
#include "config.h"
#include "scene_explorer.h"
#include "game_unlocks.h"
#include "auto_farm.h"
#include "esp_enemies.h"
#include "esp_items.h"
#include "player_mods.h"
#include "speedhack.h"
#include "flat_textures.h"
#include "reshade.h"
#include "Aim.h"
#include "logger.h"
#include "il2cpp_api.h"
#include "iconss.h"
#include "ui_anim.h"
#include "ui_widgets.h"
#include "ui_theme.h"

#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>
#include <cstdio>
#include <cmath>

bool g_ShowMenu = false;
bool g_InitImGui = false;
int activeTab = 0;

float g_UiScale = 1.0f;
float g_HomeBarWidth = 180.0f;
float g_ScrollbarSize = 10.0f;
float g_AccentColor[4] = { 112.f/255.f, 110.f/255.f, 215.f/255.f, 1.0f };

int g_Language = 1; 
bool g_SnowEnabled = true;
bool g_WindowGlowEnabled = true;
bool g_ShowFPS = true;
float g_BackgroundDim = 0.65f;

// Variabel RGB dan Animasi (Dibaca oleh speedrun.cpp)
float g_MenuAnimSpeed = 6.0f;     
bool  g_PopupRGBEnabled = false;  
float g_PopupRGBSpeed = 0.5f;     

float g_MenuAlpha = 0.90f; 
float g_CornerRounding = 12.0f; 
float g_PopupScale = 1.0f; 
float g_PopupAlpha = 0.85f; 
bool g_HideGInGame = false; 
float g_MenuBgColor[3] = { 0.06f, 0.06f, 0.06f };                 
float g_PopupColor[3] = { 112.f / 255.f, 110.f / 255.f, 215.f / 255.f }; 
float g_PopupBgColor[3] = { 6.f / 255.f, 6.f / 255.f, 9.f / 255.f };     

static bool g_DevModeUnlocked = false;
static char s_DevPinInput[32] = "";

#define oxorany(x) x

struct Snowflake {
    ImVec2 position;
    float speed, size, oscillation, oscillationSpeed;
};
static std::vector<Snowflake> s_Snowflakes;
static bool s_SnowInitialized = false;

void InitializeSnow() {
    if (s_SnowInitialized) return;
    s_Snowflakes.clear();
    ImVec2 sc = ImGui::GetIO().DisplaySize;
    for (int i = 0; i < 300; i++) {
        Snowflake f;
        f.position = ImVec2((float)(rand() % (int)(sc.x > 0 ? sc.x : 1920)), (float)(rand() % (int)(sc.y > 0 ? sc.y : 1080)));
        f.speed = 35.0f + (float)(rand() % 75);
        f.size = 1.8f + (float)(rand() % 4);
        f.oscillation = (float)(rand() % 100) / 100.0f;
        f.oscillationSpeed = 1.0f + (float)(rand() % 3);
        s_Snowflakes.push_back(f);
    }
    s_SnowInitialized = true;
}

void UpdateAndDrawSnow() {
    if (!g_ShowMenu || !s_SnowInitialized || !g_SnowEnabled) return;
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    ImVec2 sc = ImGui::GetIO().DisplaySize;
    float dt = ImGui::GetIO().DeltaTime;
    for (auto& f : s_Snowflakes) {
        f.position.y += f.speed * dt;
        f.position.x += sinf(ImGui::GetTime() * f.oscillationSpeed + f.oscillation) * 0.5f;
        if (f.position.y > sc.y) { f.position.y = -10.0f; f.position.x = (float)(rand() % (int)sc.x); }
        dl->AddCircleFilled(f.position, f.size, IM_COL32(255, 255, 255, 170));
    }
}

static void DrawRestartIcon(ImDrawList* dl, ImVec2 ctr, float r, ImU32 col, float sc) {
    dl->PathArcTo(ctr, r, -1.2f, 4.2f, 28);
    dl->PathStroke(col, 0, 2.8f * sc);
    float a = 4.2f;
    ImVec2 tip(ctr.x + cosf(a) * r, ctr.y + sinf(a) * r);
    ImVec2 dir(-sinf(a), cosf(a));
    ImVec2 nrm(cosf(a), sinf(a));
    float hh = 5.5f * sc;
    dl->AddTriangleFilled(
        ImVec2(tip.x + dir.x * hh, tip.y + dir.y * hh),
        ImVec2(tip.x - dir.x * hh * 0.2f + nrm.x * hh * 0.8f, tip.y - dir.y * hh * 0.2f + nrm.y * hh * 0.8f),
        ImVec2(tip.x - dir.x * hh * 0.2f - nrm.x * hh * 0.8f, tip.y - dir.y * hh * 0.2f - nrm.y * hh * 0.8f),
        col);
}

void watqermark() {
    bool inGame = speedrun::IsGameplay();
    bool paused = inGame && speedrun::IsPaused();
    if (g_HideGInGame && inGame && !paused && !g_ShowMenu) return;

    if (g_PopupRGBEnabled) {
        float time = (float)ImGui::GetTime();
        float hue = fmodf(time * g_PopupRGBSpeed, 1.0f); 
        ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, g_PopupColor[0], g_PopupColor[1], g_PopupColor[2]);
    }

    float sc = menuscale::menuscale * g_PopupScale;
    float size = 54.0f * sc;
    float gap = 8.0f * sc;
    bool showRestart = paused;
    float totalW = showRestart ? (size * 2.0f + gap) : size;

    static ImVec2 s_WmPos(-1.0f, -1.0f);
    static bool s_WmMoved = false;
    if (s_WmPos.x < 0.0f) s_WmPos = ImVec2(20.0f * menuscale::menuscale, 16.0f * menuscale::menuscale);

    ImVec2 dispSize = ImGui::GetIO().DisplaySize;
    s_WmPos.x = ImClamp(s_WmPos.x, 0.0f, ImMax(0.0f, dispSize.x - totalW));
    s_WmPos.y = ImClamp(s_WmPos.y, 0.0f, ImMax(0.0f, dispSize.y - size));

    ImVec2 bg_min = s_WmPos;
    ImVec2 bg_max = ImVec2(s_WmPos.x + size, s_WmPos.y + size);
    ImVec2 r_min = ImVec2(bg_max.x + gap, bg_min.y);
    ImVec2 r_max = ImVec2(r_min.x + size, r_min.y + size);

    ImGui::SetNextWindowPos(bg_min);
    ImGui::SetNextWindowSize(ImVec2(totalW, size));
    ImGui::Begin("##wm_click_main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);

    bool pressed = ImGui::InvisibleButton("##wm_btn", ImVec2(size, size));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(0, 8.0f * sc)) {
        s_WmPos.x += ImGui::GetIO().MouseDelta.x;
        s_WmPos.y += ImGui::GetIO().MouseDelta.y;
        s_WmMoved = true;
    }
    if (pressed && !s_WmMoved) g_ShowMenu = !g_ShowMenu;
    if (!ImGui::IsMouseDown(0)) s_WmMoved = false;

    bool restartPressed = false;
    if (showRestart) {
        ImGui::SetCursorScreenPos(r_min);
        restartPressed = ImGui::InvisibleButton("##wm_btn_restart", ImVec2(size, size));
    }
    ImGui::End();

    if (restartPressed) speedrun::RestartRun(true);

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    float t = (float)ImGui::GetTime();
    float pulse = g_ShowMenu ? 1.0f : (0.70f + 0.30f * sinf(t * 2.2f));
    float round = size * 0.34f;
    ImVec4 ac = ImVec4(g_PopupColor[0], g_PopupColor[1], g_PopupColor[2], 1.0f);
    int alphaBody = (int)(245.0f * g_PopupAlpha);
    ImU32 bodyCol = IM_COL32((int)(g_PopupBgColor[0] * 255.0f), (int)(g_PopupBgColor[1] * 255.0f), (int)(g_PopupBgColor[2] * 255.0f), alphaBody);

    ImVec4 ring = ac; ring.w = (0.55f + 0.45f * pulse) * g_PopupAlpha;
    ImVec4 inner = ac; inner.w = 0.18f * g_PopupAlpha;
    ImVec4 core = ImVec4(ac.x * 0.45f + 0.55f, ac.y * 0.45f + 0.55f, ac.z * 0.45f + 0.55f, g_PopupAlpha);
    ImVec4 glow1 = ac; glow1.w = 0.22f * pulse * g_PopupAlpha;
    ImVec4 glow2 = ac; glow2.w = 0.55f * g_PopupAlpha;

    if (g_WindowGlowEnabled) {
        for (int i = 3; i >= 1; i--) {
            float ex = (float)i * 3.0f * sc;
            ImVec4 g = ac; g.w = 0.10f * pulse * g_PopupAlpha;
            draw->AddRectFilled(ImVec2(bg_min.x - ex, bg_min.y - ex), ImVec2(bg_max.x + ex, bg_max.y + ex), ImGui::GetColorU32(g), round + ex);
        }
    }
    draw->AddRectFilled(bg_min, bg_max, bodyCol, round);
    draw->AddRect(bg_min, bg_max, ImGui::GetColorU32(ring), round, 0, 2.0f * sc);
    float in = 4.0f * sc;
    draw->AddRect(ImVec2(bg_min.x + in, bg_min.y + in), ImVec2(bg_max.x - in, bg_max.y - in), ImGui::GetColorU32(inner), round - in, 0, 1.0f * sc);

    ImVec2 ctr = ImVec2((bg_min.x + bg_max.x) * 0.5f, (bg_min.y + bg_max.y) * 0.5f);
    float r = size * 0.24f;
    const float aEnd = IM_PI * 1.75f;
    float widths[3]  = { 8.0f * sc, 5.2f * sc, 3.2f * sc };
    ImU32  colors[3] = { ImGui::GetColorU32(glow1), ImGui::GetColorU32(glow2), ImGui::GetColorU32(core) };
    for (int k = 0; k < 3; k++) {
        draw->PathArcTo(ctr, r, 0.0f, aEnd, 40);
        draw->PathStroke(colors[k], 0, widths[k]);
        draw->AddLine(ImVec2(ctr.x + r, ctr.y), ImVec2(ctr.x + r * 0.10f, ctr.y), colors[k], widths[k]);
    }
    draw->AddCircleFilled(ImVec2(ctr.x + r * 0.85f, ctr.y - r * 0.95f), 2.2f * sc, IM_COL32(255, 255, 255, alphaBody));

    if (showRestart) {
        draw->AddRectFilled(r_min, r_max, bodyCol, round);
        draw->AddRect(r_min, r_max, ImGui::GetColorU32(ring), round, 0, 2.0f * sc);
        ImVec2 rc = ImVec2((r_min.x + r_max.x) * 0.5f, (r_min.y + r_max.y) * 0.5f);
        DrawRestartIcon(draw, rc, size * 0.22f, ImGui::GetColorU32(core), sc);
    }
}

void OpenFolderInExternalFileManager(const char* targetPath) { /* ... Fungsi tidak diubah ... */ }

static ImVec2 menuPosition(0, 0);
static bool menuPosInit = false;
static bool isDragging = false;
static ImVec2 dragOffset(0, 0);

void HandleMenuDragging(ImVec2& wPos, ImVec2 wSize, ImVec2 fullSize) {
    ImGuiIO& io = ImGui::GetIO();
    if (!menuPosInit && g_ShowMenu) {
        menuPosition = ImVec2((io.DisplaySize.x - fullSize.x) * 0.5f, (io.DisplaySize.y - fullSize.y) * 0.5f);
        menuPosInit = true;
    }
    if (menuPosInit) wPos = menuPosition;
    ImVec2 dMin = wPos, dMax = ImVec2(wPos.x + wSize.x, wPos.y + 40.0f * menuscale::menuscale);
    bool over = io.MousePos.x >= dMin.x && io.MousePos.x <= dMax.x && io.MousePos.y >= dMin.y && io.MousePos.y <= dMax.y;
    if (over && io.MouseClicked[0] && !isDragging) { isDragging = true; dragOffset = io.MousePos - wPos; }
    if (isDragging) {
        if (io.MouseDown[0]) {
            menuPosition = io.MousePos - dragOffset;
            menuPosition.x = ImClamp(menuPosition.x, 10.0f, io.DisplaySize.x - wSize.x - 10.0f);
            menuPosition.y = ImClamp(menuPosition.y, 20.0f, io.DisplaySize.y - wSize.y - 15.0f);
            wPos = menuPosition;
        } else isDragging = false;
    }
}

void SetupPremiumStyle(float scale) {
    menuscale::menuscale = scale;
    menuscale::menuscalee = scale;
    c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], g_AccentColor[3]);
    
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = g_CornerRounding;
    style.ChildRounding = g_CornerRounding;
    style.FrameRounding = g_CornerRounding;
    style.PopupRounding = g_CornerRounding;
}

struct TabDef {
    const char* name;
    const char* icon;
};

void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;

    config::AutoSaveTick();  
    
    ImGui::GetStyle().WindowRounding = g_CornerRounding;
    ImGui::GetStyle().ChildRounding = g_CornerRounding;
    ImGui::GetStyle().FrameRounding = g_CornerRounding;

    // [PERBAIKAN ANIMASI] Menggunakan ImLerp murni agar lebih mulus dan tidak nge-freeze/snap.
    static float currentAlpha = 0.0f;
    static float currentScale = 0.85f;
    
    currentAlpha = ImLerp(currentAlpha, g_ShowMenu ? g_MenuAlpha : 0.0f, dt * g_MenuAnimSpeed * 2.0f);
    currentScale = ImLerp(currentScale, g_ShowMenu ? 1.0f : 0.85f, dt * g_MenuAnimSpeed * 2.0f);

    if (!s_SnowInitialized) InitializeSnow();

    if (g_ShowMenu && g_BackgroundDim > 0.0f) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, static_cast<int>(g_BackgroundDim * (currentAlpha / g_MenuAlpha) * 190)));
    }
    if (g_ShowMenu && g_SnowEnabled) UpdateAndDrawSnow();

    watqermark(); 

    // Jika alpha sangat kecil (menu hampir tertutup penuh), hentikan render.
    if (currentAlpha <= 0.01f) return;

    float targetW = io.DisplaySize.x * 0.60f;
    float targetH = io.DisplaySize.y * 0.80f;
    if (targetW < 640.0f) targetW = 640.0f;
    if (targetH < 420.0f) targetH = 420.0f;
    if (targetW > io.DisplaySize.x * 0.96f) targetW = io.DisplaySize.x * 0.96f;
    if (targetH > io.DisplaySize.y * 0.96f) targetH = io.DisplaySize.y * 0.96f;

    ImVec2 winSize = ImVec2(targetW * currentScale, targetH * currentScale);
    ImVec2 winPos = ImVec2((io.DisplaySize.x - winSize.x) * 0.5f, (io.DisplaySize.y - winSize.y) * 0.5f);

    HandleMenuDragging(winPos, winSize, ImVec2(targetW, targetH));

    ImGui::SetNextWindowPos(winPos);
    ImGui::SetNextWindowSize(winSize);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, currentAlpha);

    ImGui::GetStyle().ScrollbarSize = g_ScrollbarSize * menuscale::menuscale;
    
    ImVec4 menuBg(g_MenuBgColor[0], g_MenuBgColor[1], g_MenuBgColor[2], 1.0f);
    ImVec4 menuBgLift(ImMin(menuBg.x + 0.02f, 1.0f), ImMin(menuBg.y + 0.02f, 1.0f), ImMin(menuBg.z + 0.02f, 1.0f), 1.0f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(menuBg.x, menuBg.y, menuBg.z, currentAlpha));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(menuBgLift.x, menuBgLift.y, menuBgLift.z, currentAlpha * 0.8f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(menuBgLift.x, menuBgLift.y, menuBgLift.z, currentAlpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.2f, 0.2f, currentAlpha * 0.5f));

    ImGui::Begin("chuvashi_main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 curSize = ImGui::GetWindowSize();

    float sidebarW = g_HomeBarWidth * menuscale::menuscale * currentScale;
    draw->AddLine(ImVec2(pos.x + sidebarW, pos.y), ImVec2(pos.x + sidebarW, pos.y + curSize.y), IM_COL32(50, 50, 50, (int)(255 * currentAlpha)));
    
    draw->AddRectFilled(pos, ImVec2(pos.x + curSize.x, pos.y + 38.0f * menuscale::menuscale * currentScale), 
                        ImGui::ColorConvertFloat4ToU32(ImVec4(menuBg.x * 0.7f, menuBg.y * 0.7f, menuBg.z * 0.7f, currentAlpha)), g_CornerRounding, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight);

    if (g_ShowFPS) {
        char fpsBuf[64];
        snprintf(fpsBuf, sizeof(fpsBuf), "%.0f FPS  |  %.1f ms", g_RealFPS, g_FrameTimeMs);
        ImVec2 fpsSz = ImGui::CalcTextSize(fpsBuf);
        draw->AddText(ImVec2(pos.x + curSize.x - fpsSz.x - 12.0f * menuscale::menuscale,
                             pos.y + (38.0f * menuscale::menuscale * currentScale - fpsSz.y) * 0.5f),
                      IM_COL32(150, 150, 160, 255), fpsBuf);
    }

    const char* kTitle = "Granny Legacy Mod menu";
    ImVec2 tSz = ImGui::CalcTextSize(kTitle);
    draw->AddText(ImVec2(pos.x + (curSize.x - tSz.x) * 0.5f, pos.y + (38.0f * menuscale::menuscale * currentScale - tSz.y) * 0.5f), ImGui::GetColorU32(c::accent), kTitle);

    ImGui::SetCursorPos(ImVec2(10 * menuscale::menuscale * currentScale, 48 * menuscale::menuscale * currentScale));
    ImGui::BeginGroup();

    static animation_vec2 tabBgAnim;
    std::vector<TabDef> navTabs = {
        { "Combat",   ICON_FA_CROSSHAIRS }, { "Visuals",  ICON_FA_EYE },
        { "Player",   ICON_FA_CIRCLE_USER }, { "World",    ICON_FA_PERSON_RUNNING },
        { "Speedrun", ICON_FA_PERSON_RUNNING }, { "Config",   ICON_FA_CLOUD }, { "Settings", ICON_FA_GEAR }
    };
    if (g_DevModeUnlocked) {
        navTabs.push_back({ "Explorer", ICON_FA_FOLDER_OPEN });
        navTabs.push_back({ "Debugger", ICON_FA_TERMINAL });
    }

    ImVec2 tabSz(sidebarW - 20.0f * menuscale::menuscale * currentScale, 38.0f * menuscale::menuscale * currentScale);

    draw->AddRectFilled(
        ImVec2(pos.x + tabBgAnim.value.x, pos.y + tabBgAnim.value.y),
        ImVec2(pos.x + tabBgAnim.value.x + tabSz.x, pos.y + tabBgAnim.value.y + tabSz.y),
        IM_COL32(30, 30, 30, (int)(255 * currentAlpha)), g_CornerRounding
    );
    draw->AddRect(
        ImVec2(pos.x + tabBgAnim.value.x, pos.y + tabBgAnim.value.y),
        ImVec2(pos.x + tabBgAnim.value.x + tabSz.x, pos.y + tabBgAnim.value.y + tabSz.y),
        ImGui::GetColorU32(c::tab::tab_outline), g_CornerRounding
    );

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 5.0f * menuscale::menuscale * currentScale));
    for (size_t t = 0; t < navTabs.size(); t++) {
        auto& tab = navTabs[t];
        ImVec2 cPos = ImGui::GetCursorPos();
        ImVec2 sPos = ImGui::GetCursorScreenPos();
        if (ImGui::InvisibleButton(tab.name, tabSz)) activeTab = (int)t;
        bool isCur = (activeTab == (int)t);
        if (isCur) tabBgAnim.update(cPos, 0.15f);

        ImU32 textCol = isCur ? IM_COL32(255, 255, 255, (int)(255 * currentAlpha)) : IM_COL32(130, 130, 145, (int)(255 * currentAlpha));
        if (isCur) {
            draw->AddRectFilled(ImVec2(sPos.x + 5 * menuscale::menuscale * currentScale, sPos.y + 10 * menuscale::menuscale * currentScale),
                                ImVec2(sPos.x + 8 * menuscale::menuscale * currentScale, sPos.y + tabSz.y - 10 * menuscale::menuscale * currentScale),
                                ImGui::GetColorU32(c::accent), 2.0f);
        }
        draw->AddText(ImVec2(sPos.x + 18 * menuscale::menuscale * currentScale, sPos.y + (tabSz.y - ImGui::GetFontSize()) * 0.5f), textCol, tab.name);
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    float mainStartX = sidebarW + 12.0f * menuscale::menuscale * currentScale;
    float mainW = curSize.x - mainStartX - 12.0f * menuscale::menuscale * currentScale;
    float colGap = 12.0f * menuscale::menuscale * currentScale;
    float colW = (mainW - colGap) * 0.5f;
    float colH = curSize.y - 54.0f * menuscale::menuscale * currentScale;
    ImGui::SetCursorPos(ImVec2(mainStartX, 46.0f * menuscale::menuscale * currentScale));

    if (activeTab == 0) {
        ImGui::BeginChild("##c0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Aimbot");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Enable Aimlock", &aim::grannyAimEnabled);
        edited::checkbox("Only With Weapon", &aim::onlyWithWeapon);
        edited::slider_float("Aim Smoothness", &aim::grannySmoothness, 5.0f, 50.0f, "%.0f");
        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Item Auto-Look");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Item Aim", &aim::itemAimEnabled);
        edited::slider_float("Item Aim Speed", &aim::itemSmoothness, 5.0f, 50.0f, "%.0f");
        edited::slider_float("Aim Distance", &aim::itemAimDistance, 1.0f, 20.0f, "%.1f m");
        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Item Filter");
        {
            float sc = menuscale::menuscale;
            float halfW = (ImGui::GetContentRegionAvail().x - 8.0f * sc) * 0.5f;
            if (edited::buttonn("Select All", ImVec2(halfW, 34 * sc))) aim::SetAllItemTypes(true);
            ImGui::SameLine(0, 8.0f * sc);
            if (edited::buttonn("Unselect All", ImVec2(halfW, 34 * sc))) aim::SetAllItemTypes(false);
        }
        static char s_ItemSearch[48] = "";
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##itemSearch", "Search item...", s_ItemSearch, sizeof(s_ItemSearch));
        ImGui::BeginChild("##itemTypeList", ImVec2(-1, 190.0f * menuscale::menuscale), true);
        {
            std::string flt = s_ItemSearch;
            std::transform(flt.begin(), flt.end(), flt.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
            for (const std::string& nm : aim::ItemTypeNames()) {
                std::string label = aim::PrettyItemName(nm);
                if (!flt.empty()) {
                    std::string low = label;
                    std::transform(low.begin(), low.end(), low.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
                    if (low.find(flt) == std::string::npos) continue;
                }
                edited::checkbox(label.c_str(), aim::ItemTypeEnabledPtr(nm));
            }
        }
        ImGui::EndChild();
        if (edited::buttonn("Rescan Map Items", ImVec2(-1, 40 * menuscale::menuscale))) aim::ScanLiveItems();
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);

        ImGui::BeginChild("##c1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Weapon Mods");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Infinite Ammo", &player_mods::infiniteAmmo);
        edited::checkbox("Rapid Fire & Shock", &player_mods::electricDarts);
        edited::checkbox("Explosive Shotgun", &player_mods::explosiveShotgun);
        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Accuracy");
        ImGui::Separator();
        ImGui::Spacing();
        static bool noRecoil = true;
        edited::checkbox("No Spread / Recoil", &noRecoil);
        ImGui::EndChild();
    }
    else if (activeTab == 1) {
        ImGui::BeginChild("##v0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Enemies ESP");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Enable ESP", &esp_enemies::enabled);
        edited::checkbox("Draw 2D Box", &esp_enemies::drawBox);
        const char* bTypes[] = { "Solid Box", "Corner Box" };
        edited::combo("Box Style", &esp_enemies::boxType, bTypes, 2);
        edited::checkbox("Draw Name", &esp_enemies::drawName);
        edited::checkbox("Draw Distance", &esp_enemies::drawDistance);
        edited::checkbox("Draw Tracers", &esp_enemies::drawTracers);
        edited::checkbox("Enable Chams", &esp_enemies::enableChams);
        ImGui::Spacing();
        edited::slider_float("Line Width", &esp_enemies::outlineWidth, 1.0f, 5.0f, "%.1f px");
        edited::color_edit3("ESP Color", esp_enemies::color);
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);
        ImGui::BeginChild("##v1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Items & World");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Items ESP", &esp_items::enabled);
        edited::slider_float("Items Width", &esp_items::width, 1.0f, 15.0f, "%.1f px");
        edited::color_edit3("Items Color", esp_items::color);
        if (edited::buttonn("TP All Items", ImVec2(-1, 40 * menuscale::menuscale))) {
            for (auto& itm : esp_items::itemsList) if (itm.transform) esp_items::TeleportItemToPlayer(itm.transform);
        }
        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Lighting & Fog");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("No Darker Fog", &reshade::disableDarkerFog);
        edited::checkbox("Disable Baked Lightmaps", &reshade::disableLightmaps);
        ImGui::EndChild();
    }
    else if (activeTab == 2) {
        ImGui::BeginChild("##p0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Player Status");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Godmode", &player_mods::godmode);
        edited::checkbox("Anti-Fall (No Damage)", &player_mods::noFallDamage);
        edited::checkbox("FOV Changer", &player_mods::fovChanger);
        if (player_mods::fovChanger) edited::slider_float("FOV", &player_mods::customFov, 60.0f, 130.0f, "%.0f");
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);
        ImGui::BeginChild("##p1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Textures");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Flat World", &flat_textures::flatEnabled);
        edited::color_edit3("Flat Color", flat_textures::flatColor);
        edited::checkbox("Potato Textures", &flat_textures::pixelateEnabled);
        if (flat_textures::pixelateEnabled) {
            float pLvl = (float)flat_textures::pixelateLevel;
            if (edited::slider_float("Potato Level", &pLvl, 1.0f, 4.0f, "Lvl %.0f")) {
                flat_textures::pixelateLevel = (int)pLvl;
            }
        }
        ImGui::EndChild();
    }
    else if (activeTab == 3) {
        ImGui::BeginChild("##w0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "World Speed");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Speedhack", &speedhack::enabled);
        edited::slider_float("Multiplier", &speedhack::multiplier, 0.05f, 10.0f, "%.2fx");
        ImGui::Spacing();
        float bw = (ImGui::GetContentRegionAvail().x - 12) / 3.0f;
        if (edited::buttonn("0.5x", ImVec2(bw, 36 * menuscale::menuscale))) speedhack::multiplier = 0.5f;
        ImGui::SameLine(0, 6);
        if (edited::buttonn("1.0x", ImVec2(bw, 36 * menuscale::menuscale))) speedhack::multiplier = 1.0f;
        ImGui::SameLine(0, 6);
        if (edited::buttonn("2.0x", ImVec2(bw, 36 * menuscale::menuscale))) speedhack::multiplier = 2.0f;
        ImGui::Spacing();
        edited::checkbox("Unlock Hidden Features", &game_unlocks::enabled);
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);
        ImGui::BeginChild("##w1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Auto-Win Farmer");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Auto-Win Loop", &auto_farm::enabled);
        const char* routes[] = { "Door", "Car", "Cellar", "Robo" };
        edited::combo("Route", &auto_farm::escapeMethod, routes, 4);
        edited::slider_float("Turbo Speed", &auto_farm::fastForwardSpeed, 5.0f, 30.0f, "%.0fx");
        ImGui::Spacing();
        if (edited::buttonn("Win Instant (Once)", ImVec2(-1, 40 * menuscale::menuscale))) auto_farm::TriggerInstantWin();
        ImGui::EndChild();
    }
    else if (activeTab == 4) {
        speedrun::DrawMenu(colW, colH, colGap);
    }
    else if (activeTab == 5) {
        ImGui::BeginChild("##cfg0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Config Manager");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::InputTextWithHint("##cfgin", "name...", config::currentConfigName, sizeof(config::currentConfigName));
        if (edited::buttonn("Save Config", ImVec2(-1, 40 * menuscale::menuscale))) config::SaveConfig(config::currentConfigName);
        if (edited::buttonn("Load Config", ImVec2(-1, 40 * menuscale::menuscale))) config::LoadConfig(config::currentConfigName);
        if (edited::buttonn("Open in ZArchiver", ImVec2(-1, 40 * menuscale::menuscale))) OpenFolderInExternalFileManager("/sdcard/Kahanium/");
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);
        ImGui::BeginChild("##cfg1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Saved Profiles");
        ImGui::Separator();
        ImGui::Spacing();
        for (size_t i = 0; i < config::foundConfigs.size(); i++) {
            if (ImGui::Selectable(config::foundConfigs[i].c_str(), config::currentConfigName == config::foundConfigs[i])) {
                snprintf(config::currentConfigName, sizeof(config::currentConfigName), "%s", config::foundConfigs[i].c_str());
            }
        }
        ImGui::EndChild();
    }
    else if (activeTab == 6) {
        ImGui::BeginChild("##st0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Menu Settings");
        ImGui::Separator();
        ImGui::Spacing();

        edited::slider_float("Menu Anim Speed", &g_MenuAnimSpeed, 1.0f, 15.0f, "%.1f");
        
        edited::checkbox("Snow Effect", &g_SnowEnabled);
        edited::checkbox("Window Glow", &g_WindowGlowEnabled);
        edited::checkbox("Show FPS Counter", &g_ShowFPS);
        edited::slider_float("Background Dim", &g_BackgroundDim, 0.0f, 1.0f, "%.2f");
        edited::slider_float("Menu Opacity", &g_MenuAlpha, 0.1f, 1.0f, "%.2f");
        ImGui::ColorEdit3("Menu Color", g_MenuBgColor, ImGuiColorEditFlags_NoInputs);
        edited::slider_float("Menu Scale", &menuscale::menuscalee, 0.5f, 1.5f, "%.2f");
        edited::slider_float("Menu Rounding", &g_CornerRounding, 0.0f, 24.0f, "%.0f px");
        
        ImGui::Separator();
        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Popup 'G' Settings");
        edited::slider_float("Popup 'G' Scale", &g_PopupScale, 0.5f, 2.0f, "%.2fx");
        edited::slider_float("Popup 'G' Alpha", &g_PopupAlpha, 0.1f, 1.0f, "%.2f");
        
        edited::checkbox("Enable RGB Popup", &g_PopupRGBEnabled);
        if (g_PopupRGBEnabled) {
            edited::slider_float("RGB Speed", &g_PopupRGBSpeed, 0.1f, 3.0f, "%.2fx");
        } else {
            ImGui::ColorEdit3("Popup Color", g_PopupColor, ImGuiColorEditFlags_NoInputs);
        }
        
        ImGui::ColorEdit3("Popup Background", g_PopupBgColor, ImGuiColorEditFlags_NoInputs);
        edited::checkbox("Hide Popup During Gameplay", &g_HideGInGame);
        
        ImGui::Spacing();
        edited::checkbox("Show Floating Restart Button", &speedrun::restartButton);
        ImGui::TextWrapped("%s", "The popup comes back when the game is paused, or use the floating restart.");

        edited::slider_float("Sidebar Width", &g_HomeBarWidth, 120.0f, 240.0f, "%.0f px");
        edited::slider_float("Scrollbar Size", &g_ScrollbarSize, 6.0f, 20.0f, "%.0f px");
        if (ImGui::ColorEdit3("Accent Color", g_AccentColor, ImGuiColorEditFlags_NoInputs)) {
            c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.0f);
            g_AccentColor[3] = 1.0f;
        }
        
        if (edited::buttonn("Apply UI Settings", ImVec2(-1, 40 * menuscale::menuscale))) {
            menuscale::menuscale = menuscale::menuscalee;
            c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], g_AccentColor[3]);
            ImGui::GetStyle().ScrollbarSize = g_ScrollbarSize * menuscale::menuscale;
        }
        if (edited::buttonn("Reset UI Settings", ImVec2(-1, 40 * menuscale::menuscale))) {
            g_SnowEnabled = true; g_WindowGlowEnabled = true; g_ShowFPS = true;
            g_BackgroundDim = 0.65f; g_MenuAlpha = 0.90f; g_CornerRounding = 12.0f;
            g_PopupScale = 1.0f; g_PopupAlpha = 0.85f; g_HideGInGame = false;
            g_HomeBarWidth = 180.0f; g_ScrollbarSize = 10.0f;
            g_MenuAnimSpeed = 6.0f; g_PopupRGBEnabled = false; g_PopupRGBSpeed = 0.5f;     
            g_MenuBgColor[0] = 0.06f; g_MenuBgColor[1] = 0.06f; g_MenuBgColor[2] = 0.06f;
            g_PopupColor[0] = 112.0f/255.0f; g_PopupColor[1] = 110.0f/255.0f; g_PopupColor[2] = 215.0f/255.0f;
            g_PopupBgColor[0] = 6.0f/255.0f; g_PopupBgColor[1] = 6.0f/255.0f; g_PopupBgColor[2] = 9.0f/255.0f;
            g_AccentColor[0] = 112.0f/255.0f; g_AccentColor[1] = 110.0f/255.0f; g_AccentColor[2] = 215.0f/255.0f; g_AccentColor[3] = 1.0f;
            menuscale::menuscalee = 1.0f; menuscale::menuscale = 1.0f;
            c::accent = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], 1.0f);
            ImGui::GetStyle().ScrollbarSize = g_ScrollbarSize;
        }
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);
        ImGui::BeginChild("##st1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Developer Access");
        ImGui::Separator();
        ImGui::Spacing();
        if (!g_DevModeUnlocked) {
            ImGui::InputTextWithHint("##pin", "PIN...", s_DevPinInput, sizeof(s_DevPinInput), ImGuiInputTextFlags_Password);
            if (edited::buttonn("Unlock Dev", ImVec2(-1, 40 * menuscale::menuscale))) {
                if (strcmp(s_DevPinInput, "948201") == 0 || strcmp(s_DevPinInput, "kahanium") == 0) g_DevModeUnlocked = true;
            }
        } else {
            edited::colortext(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "Dev Mode Unlocked");
        }
        ImGui::EndChild();
    }
    else if (activeTab == 7 && g_DevModeUnlocked) scene_explorer::DrawMenu();
    else if (activeTab == 8 && g_DevModeUnlocked) Logger::DrawTab();

    ImGui::End();
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar();
}
