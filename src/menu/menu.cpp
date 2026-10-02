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

#include <vector>
#include <string>
#include <ctime>
#include <cstdlib>

bool g_ShowMenu = false;
bool g_InitImGui = false;
int activeTab = 0;

float g_UiScale = 1.0f;
float g_HomeBarWidth = 180.0f;
float g_ScrollbarSize = 10.0f;
float g_AccentColor[4] = { 112.f/255.f, 110.f/255.f, 215.f/255.f, 1.0f };

int g_Language = 1; // Чистый английский
bool g_SnowEnabled = true;
bool g_WindowGlowEnabled = true;
float g_BackgroundDim = 0.65f;

static bool g_DevModeUnlocked = false;
static char s_DevPinInput[32] = "";

#define oxorany(x) x

// Снегопад
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

// Ватермарка
void watqermark() {
    float sc = menuscale::menuscale;
    float padding = 10.0f * sc;
    float x = 20.0f * sc;
    float y = 16.0f * sc;

    std::time_t now = std::time(nullptr);
    std::tm* lt = std::localtime(&now);
    char time_str[16]{};
    if (lt) strftime(time_str, sizeof(time_str), "%H:%M", lt);
    else snprintf(time_str, sizeof(time_str), "23:55");

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "chuvashi.win | tg @chuvashi_win | %s", time_str);

    ImVec2 text_size = ImGui::CalcTextSize(buffer);
    ImVec2 bg_min = ImVec2(x, y);
    ImVec2 bg_max = ImVec2(x + text_size.x + padding * 2, y + text_size.y + padding * 2);

    ImGui::SetNextWindowPos(bg_min);
    ImGui::SetNextWindowSize(ImVec2(bg_max.x - bg_min.x, bg_max.y - bg_min.y));
    ImGui::Begin("##wm_click_main", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
    if (ImGui::InvisibleButton("##wm_btn", ImVec2(bg_max.x - bg_min.x, bg_max.y - bg_min.y))) {
        g_ShowMenu = !g_ShowMenu;
    }
    ImGui::End();

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled(bg_min, bg_max, IM_COL32(14, 14, 18, 240), 6.0f * sc);
    draw->AddRect(bg_min, bg_max, g_ShowMenu ? ImGui::GetColorU32(c::accent) : IM_COL32(60, 60, 80, 255), 6.0f * sc);
    draw->AddText(ImVec2(x + padding, y + padding), IM_COL32(230, 230, 245, 255), buffer);
}

// Запуск ZArchiver
void OpenFolderInExternalFileManager(const char* targetPath) {
    if (!g_JavaVM || !targetPath) return;
    JNIEnv* env = nullptr;
    if (g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_JavaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    }

    jclass strictModeClass = env->FindClass("android/os/StrictMode");
    if (strictModeClass) {
        jmethodID disableDeath = env->GetStaticMethodID(strictModeClass, "disableDeathOnFileUriExposure", "()V");
        if (disableDeath) env->CallStaticVoidMethod(strictModeClass, disableDeath);
        env->ExceptionClear();
    }

    jclass upClass = env->FindClass("com/unity3d/player/UnityPlayer");
    if (!upClass) return;
    jfieldID actField = env->GetStaticFieldID(upClass, "currentActivity", "Landroid/app/Activity;");
    if (!actField) return;
    jobject act = env->GetStaticObjectField(upClass, actField);
    if (!act) return;

    jclass intentClass = env->FindClass("android/content/Intent");
    jmethodID init = env->GetMethodID(intentClass, "<init>", "(Ljava/lang/String;)V");
    jstring actView = env->NewStringUTF("android.intent.action.VIEW");
    jobject intent = env->NewObject(intentClass, init, actView);

    jclass uriClass = env->FindClass("android/net/Uri");
    jmethodID parse = env->GetStaticMethodID(uriClass, "parse", "(Ljava/lang/String;)Landroid/net/Uri;");
    std::string uriStr = std::string("file://") + targetPath;
    jstring jUri = env->NewStringUTF(uriStr.c_str());
    jobject uriObj = env->CallStaticObjectMethod(uriClass, parse, jUri);

    jmethodID setDT = env->GetMethodID(intentClass, "setDataAndType", "(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;");
    jstring mime = env->NewStringUTF("*/*");
    env->CallObjectMethod(intent, setDT, uriObj, mime);

    jmethodID setPkg = env->GetMethodID(intentClass, "setPackage", "(Ljava/lang/String;)Landroid/content/Intent;");
    jstring jPkg = env->NewStringUTF("ru.zdevs.zarchiver");
    env->CallObjectMethod(intent, setPkg, jPkg);

    jmethodID addFlags = env->GetMethodID(intentClass, "addFlags", "(I)Landroid/content/Intent;");
    env->CallObjectMethod(intent, addFlags, 0x10000000);

    jclass actClass = env->GetObjectClass(act);
    jmethodID startAct = env->GetMethodID(actClass, "startActivity", "(Landroid/content/Intent;)V");
    env->CallVoidMethod(act, startAct, intent);
    if (env->ExceptionCheck()) env->ExceptionClear();
}

// Перетаскивание
static ImVec2 menuPosition(0, 0);
static bool menuPosInit = false;
static bool isDragging = false;
static ImVec2 dragOffset(0, 0);

void HandleMenuDragging(ImVec2& wPos, ImVec2 wSize) {
    ImGuiIO& io = ImGui::GetIO();
    if (!menuPosInit && g_ShowMenu) {
        menuPosition = ImVec2((io.DisplaySize.x - wSize.x) * 0.5f, (io.DisplaySize.y - wSize.y) * 0.5f);
        menuPosInit = true;
    }
    if (menuPosInit) wPos = menuPosition;

    ImVec2 dMin = wPos, dMax = ImVec2(wPos.x + wSize.x, wPos.y + 50.0f);
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
}

struct TabDef {
    const char* name;
    const char* icon;
};

// ==============================================================
// ГЛАВНЫЙ РЕНДЕР (ПОЛНОРАЗМЕРНЫЙ ЭКРАН 82% x 85%)
// ==============================================================
void DrawMenu() {
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime;

    static float menuAnimAlpha = 0.0f;
    static float menuScaleAnim = 0.0f;
    menuAnimAlpha = ImLerp(menuAnimAlpha, g_ShowMenu ? 1.0f : 0.0f, dt * 10.0f);
    menuScaleAnim = ImLerp(menuScaleAnim, g_ShowMenu ? 1.0f : 0.6f, dt * 8.0f);

    if (!s_SnowInitialized) InitializeSnow();

    if (g_ShowMenu && g_BackgroundDim > 0.0f) {
        ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0, 0), io.DisplaySize, IM_COL32(0, 0, 0, static_cast<int>(g_BackgroundDim * 190)));
    }
    if (g_ShowMenu && g_SnowEnabled) UpdateAndDrawSnow();

    watqermark();

    if (menuAnimAlpha <= 0.01f) return;

    // ЧЕСТНЫЙ ПОЛНОРАЗМЕРНЫЙ ЭКРАН ПОД ТЕЛЕФОН (82% ширины, 85% высоты)
    float targetW = io.DisplaySize.x * 0.82f;
    float targetH = io.DisplaySize.y * 0.85f;
    if (targetW < 850.0f) targetW = 850.0f;
    if (targetH < 540.0f) targetH = 540.0f;

    ImVec2 winSize = ImVec2(targetW * menuScaleAnim, targetH * menuScaleAnim);
    ImVec2 winPos = ImVec2((io.DisplaySize.x - winSize.x) * 0.5f, (io.DisplaySize.y - winSize.y) * 0.5f);

    HandleMenuDragging(winPos, winSize);

    ImGui::SetNextWindowPos(winPos);
    ImGui::SetNextWindowSize(winSize);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, menuAnimAlpha);

    ImGui::Begin("chuvashi_main", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetWindowPos();
    ImVec2 curSize = ImGui::GetWindowSize();

    // Разделитель сайдбара
    float sidebarW = 220.0f * menuscale::menuscale;
    draw->AddLine(ImVec2(pos.x + sidebarW, pos.y), ImVec2(pos.x + sidebarW, pos.y + curSize.y), IM_COL32(28, 28, 34, 255));
    draw->AddRectFilled(pos, ImVec2(pos.x + curSize.x, pos.y + 44.0f * menuscale::menuscale), IM_COL32(16, 16, 20, 255), 6, ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight);

    // Заголовок chuvashi
    ImVec2 tSz = ImGui::CalcTextSize("chuvashi");
    draw->AddText(ImVec2(pos.x + (curSize.x - tSz.x) * 0.5f, pos.y + 12.0f * menuscale::menuscale), ImGui::GetColorU32(c::accent), "chuvashi");

    // ==============================================================
    // ЛЕВЫЙ САЙДБАР (КРУПНЫЕ КНОПКИ 200 x 46px)
    // ==============================================================
    ImGui::SetCursorPos(ImVec2(10 * menuscale::menuscale, 55 * menuscale::menuscale));
    ImGui::BeginGroup();

    static animation_vec2 tabBgAnim;
    std::vector<TabDef> navTabs = {
        { "Combat",   ICON_FA_CROSSHAIRS },
        { "Visuals",  ICON_FA_EYE },
        { "Player",   ICON_FA_CIRCLE_USER },
        { "World",    ICON_FA_PERSON_RUNNING },
        { "Config",   ICON_FA_CLOUD },
        { "Settings", ICON_FA_GEAR }
    };

    if (g_DevModeUnlocked) {
        navTabs.push_back({ "Explorer", ICON_FA_FOLDER_OPEN });
        navTabs.push_back({ "Debugger", ICON_FA_TERMINAL });
    }

    ImVec2 tabSz(sidebarW - 20.0f * menuscale::menuscale, 46.0f * menuscale::menuscale);

    // Анимированная подложка
    draw->AddRectFilled(
        ImVec2(pos.x + tabBgAnim.value.x, pos.y + tabBgAnim.value.y),
        ImVec2(pos.x + tabBgAnim.value.x + tabSz.x, pos.y + tabBgAnim.value.y + tabSz.y),
        IM_COL32(22, 22, 28, 255), 6.0f
    );
    draw->AddRect(
        ImVec2(pos.x + tabBgAnim.value.x, pos.y + tabBgAnim.value.y),
        ImVec2(pos.x + tabBgAnim.value.x + tabSz.x, pos.y + tabBgAnim.value.y + tabSz.y),
        ImGui::GetColorU32(c::tab::tab_outline), 6.0f
    );

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 5.0f * menuscale::menuscale));
    for (size_t t = 0; t < navTabs.size(); t++) {
        auto& tab = navTabs[t];
        ImVec2 cPos = ImGui::GetCursorPos();
        ImVec2 sPos = ImGui::GetCursorScreenPos();

        if (ImGui::InvisibleButton(tab.name, tabSz)) activeTab = (int)t;

        bool isCur = (activeTab == (int)t);
        if (isCur) tabBgAnim.update(cPos);

        ImU32 textCol = isCur ? IM_COL32(255, 255, 255, 255) : IM_COL32(130, 130, 145, 255);
        ImU32 iconCol = isCur ? ImGui::GetColorU32(c::accent) : IM_COL32(130, 130, 145, 255);

        // Иконка
        draw->AddText(ImVec2(sPos.x + 18 * menuscale::menuscale, sPos.y + 14 * menuscale::menuscale), iconCol, tab.icon);
        // Текст
        draw->AddText(ImVec2(sPos.x + 46 * menuscale::menuscale, sPos.y + 14 * menuscale::menuscale), textCol, tab.name);
    }
    ImGui::PopStyleVar();
    ImGui::EndGroup();

    // ==============================================================
    // ПРАВАЯ ЧАСТЬ: 2 ШИРОКИЕ КОЛОНКИ НА ВЕСЬ ЭКРАН
    // ==============================================================
    float mainStartX = sidebarW + 16.0f * menuscale::menuscale;
    float mainW = curSize.x - mainStartX - 16.0f * menuscale::menuscale;
    float colGap = 12.0f * menuscale::menuscale;
    float colW = (mainW - colGap) * 0.5f;
    float colH = curSize.y - 60.0f * menuscale::menuscale;

    ImGui::SetCursorPos(ImVec2(mainStartX, 50.0f * menuscale::menuscale));

    // TAB 0: COMBAT
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
    // TAB 1: VISUALS
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
    // TAB 2: PLAYER
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
    // TAB 3: WORLD
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
    // TAB 4: CONFIGS
    else if (activeTab == 4) {
        ImGui::BeginChild("##cfg0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Config Manager");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::InputTextWithHint("##cfgin", "name...", config::currentConfigName, sizeof(config::currentConfigName));
        if (edited::buttonn("Save Config", ImVec2(-1, 40 * menuscale::menuscale))) config::SaveConfig(config::currentConfigName);
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
    // TAB 5: SETTINGS
    else if (activeTab == 5) {
        ImGui::BeginChild("##st0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Menu Settings");
        ImGui::Separator();
        ImGui::Spacing();

        edited::checkbox("Snow Effect", &g_SnowEnabled);
        edited::slider_float("Background Dim", &g_BackgroundDim, 0.0f, 1.0f, "%.2f");
        edited::slider_float("Menu Scale", &menuscale::menuscalee, 0.5f, 1.5f, "%.2f");
        if (edited::buttonn("Apply Scale", ImVec2(-1, 40 * menuscale::menuscale))) menuscale::menuscale = menuscale::menuscalee;
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
    // DEV TABS
    else if (activeTab == 6 && g_DevModeUnlocked) scene_explorer::DrawMenu();
    else if (activeTab == 7 && g_DevModeUnlocked) Logger::DrawTab();

    ImGui::End();
    ImGui::PopStyleVar();
}