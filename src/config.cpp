#include "config.h"
#include "esp_enemies.h"
#include "esp_items.h"
#include "speedhack.h"
#include "flat_textures.h"
#include "player_mods.h"
#include "game_unlocks.h"
#include "reshade.h"
#include "auto_farm.h"
#include "menu.h"
#include "Aim.h"
#include "speedrun.h"

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <map>
#include <cstdarg>

namespace config {
    char statusText[64] = "Ready";
    char currentConfigName[64] = "kahanium.cfg";
    std::vector<std::string> foundConfigs;
    const char* configFolder = "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files";

    // Berkas autosave: dimuat otomatis saat game dibuka, ditulis otomatis saat pengaturan berubah
    static const char* kAutoFile = "autosave.cfg";
    static std::string s_LastAutoSaved;
    static float s_AutoSaveTimer = 0.0f;

    static std::string Fmt(const char* fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        return std::string(buf);
    }

    // Умный поиск доступной для записи папки (обход защит OriginOS/HyperOS)
    std::string GetActiveConfigPath() {
        const char* tryPaths[] = {
            "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files",
            "/storage/emulated/0/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files",
            "/data/data/com.OmegaMegaGigalIntel.GrannyLegacy/files",
            "/sdcard/Download",
            "/sdcard/Kahanium"
        };

        for (const char* p : tryPaths) {
            mkdir(p, 0777);
            std::string testPath = std::string(p) + "/.perm_test";
            FILE* f = fopen(testPath.c_str(), "w");
            if (f) {
                fclose(f);
                unlink(testPath.c_str());
                return p;
            }
        }
        return "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files";
    }

    std::string GetNextAutoConfigName() {
        std::string folder = GetActiveConfigPath();
        std::string baseFile = folder + "/kahanium.cfg";
        if (access(baseFile.c_str(), F_OK) != 0) {
            return "kahanium.cfg";
        }

        int index = 1;
        while (true) {
            std::string candidateName = "kahanium" + std::to_string(index) + ".cfg";
            std::string fullCandidate = folder + "/" + candidateName;
            if (access(fullCandidate.c_str(), F_OK) != 0) {
                return candidateName;
            }
            index++;
        }
    }

    void RefreshConfigList() {
        std::string folder = GetActiveConfigPath();
        foundConfigs.clear();

        DIR* dir = opendir(folder.c_str());
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string fName = entry->d_name;
                if (fName == kAutoFile) continue;
                if (fName.length() > 4 && fName.rfind(".cfg") == fName.length() - 4) {
                    foundConfigs.push_back(fName);
                }
            }
            closedir(dir);
        }

        std::sort(foundConfigs.begin(), foundConfigs.end());

        // Автоматически подставляем следующий свободный номер в поле ввода
        std::string nextName = GetNextAutoConfigName();
        snprintf(currentConfigName, sizeof(currentConfigName), "%s", nextName.c_str());

        if (foundConfigs.empty()) {
            foundConfigs.push_back("kahanium.cfg");
        }
    }

    // Susun seluruh pengaturan menjadi satu teks
    static std::string BuildConfigText() {
        std::string o;
        o += Fmt("language=%d\n", g_Language);
        o += Fmt("snowEnabled=%d\n", g_SnowEnabled ? 1 : 0);
        o += Fmt("glowEnabled=%d\n", g_WindowGlowEnabled ? 1 : 0);
        o += Fmt("showFPS=%d\n", g_ShowFPS ? 1 : 0);
        o += Fmt("backgroundDim=%.2f\n", g_BackgroundDim);
        o += Fmt("menuAlpha=%.2f\n", g_MenuAlpha);
        o += Fmt("cornerRounding=%.1f\n", g_CornerRounding);
        o += Fmt("popupScale=%.2f\n", g_PopupScale);
        o += Fmt("popupAlpha=%.2f\n", g_PopupAlpha);
        o += Fmt("hideGInGame=%d\n", g_HideGInGame ? 1 : 0);
        o += Fmt("menuBgColor=%.3f,%.3f,%.3f\n", g_MenuBgColor[0], g_MenuBgColor[1], g_MenuBgColor[2]);
        o += Fmt("popupColor=%.3f,%.3f,%.3f\n", g_PopupColor[0], g_PopupColor[1], g_PopupColor[2]);
        o += Fmt("popupBgColor=%.3f,%.3f,%.3f\n", g_PopupBgColor[0], g_PopupBgColor[1], g_PopupBgColor[2]);

        o += Fmt("enemiesEnabled=%d\n", esp_enemies::enabled ? 1 : 0);
        o += Fmt("targetGranny=%d\n", esp_enemies::targetGranny ? 1 : 0);
        o += Fmt("targetGrandpa=%d\n", esp_enemies::targetGrandpa ? 1 : 0);
        o += Fmt("targetSpiderMom=%d\n", esp_enemies::targetSpiderMom ? 1 : 0);
        o += Fmt("enemiesWidth=%.2f\n", esp_enemies::outlineWidth);
        o += Fmt("enemiesColor=%.3f,%.3f,%.3f,%.3f\n",
            esp_enemies::color[0], esp_enemies::color[1], esp_enemies::color[2], esp_enemies::color[3]);
        o += Fmt("enemiesDrawBox=%d\n", esp_enemies::drawBox ? 1 : 0);
        o += Fmt("enemiesBoxType=%d\n", esp_enemies::boxType);
        o += Fmt("enemiesBoxFill=%d\n", esp_enemies::boxFill ? 1 : 0);
        o += Fmt("enemiesDrawName=%d\n", esp_enemies::drawName ? 1 : 0);
        o += Fmt("enemiesDrawDistance=%d\n", esp_enemies::drawDistance ? 1 : 0);
        o += Fmt("enemiesDrawTracers=%d\n", esp_enemies::drawTracers ? 1 : 0);
        o += Fmt("enemiesChams=%d\n", esp_enemies::enableChams ? 1 : 0);
        o += Fmt("enemyHeight=%.3f\n", esp_enemies::enemyHeight);
        o += Fmt("enemyWidthMul=%.3f\n", esp_enemies::enemyWidthMul);
        o += Fmt("enemyYOffset=%.3f\n", esp_enemies::yOffset);
        o += Fmt("spiderHeight=%.3f\n", esp_enemies::spiderHeight);
        o += Fmt("spiderWidthMul=%.3f\n", esp_enemies::spiderWidthMul);
        o += Fmt("tracerColor=%.3f,%.3f,%.3f,%.3f\n",
            esp_enemies::tracerColor[0], esp_enemies::tracerColor[1], esp_enemies::tracerColor[2], esp_enemies::tracerColor[3]);

        o += Fmt("itemsEnabled=%d\n", esp_items::enabled ? 1 : 0);
        o += Fmt("itemsWidth=%.2f\n", esp_items::width);
        o += Fmt("itemsColor=%.3f,%.3f,%.3f,%.3f\n",
            esp_items::color[0], esp_items::color[1], esp_items::color[2], esp_items::color[3]);

        o += Fmt("grannyAimEnabled=%d\n", aim::grannyAimEnabled ? 1 : 0);
        o += Fmt("aimOnlyWithWeapon=%d\n", aim::onlyWithWeapon ? 1 : 0);
        o += Fmt("grannySmoothness=%.2f\n", aim::grannySmoothness);
        o += Fmt("itemAimEnabled=%d\n", aim::itemAimEnabled ? 1 : 0);
        o += Fmt("itemSmoothness=%.2f\n", aim::itemSmoothness);
        o += Fmt("itemAimDistance=%.1f\n", aim::itemAimDistance);
        for (const std::string& nm : aim::ItemTypeNames()) {
            o += Fmt("itemType_%s=%d\n", nm.c_str(), *aim::ItemTypeEnabledPtr(nm) ? 1 : 0);
        }

        o += Fmt("disableDarkerFog=%d\n", reshade::disableDarkerFog ? 1 : 0);
        o += Fmt("disableLightmaps=%d\n", reshade::disableLightmaps ? 1 : 0);
        o += Fmt("godmode=%d\n", player_mods::godmode ? 1 : 0);
        o += Fmt("noFallDamage=%d\n", player_mods::noFallDamage ? 1 : 0);
        o += Fmt("fovChanger=%d\n", player_mods::fovChanger ? 1 : 0);
        o += Fmt("customFov=%.1f\n", player_mods::customFov);
        o += Fmt("infiniteAmmo=%d\n", player_mods::infiniteAmmo ? 1 : 0);
        o += Fmt("electricDarts=%d\n", player_mods::electricDarts ? 1 : 0);
        o += Fmt("explosiveShotgun=%d\n", player_mods::explosiveShotgun ? 1 : 0);

        o += Fmt("autoUnlockShop=%d\n", speedrun::autoUnlockShop ? 1 : 0);
        o += Fmt("restartButton=%d\n", speedrun::restartButton ? 1 : 0);
        o += Fmt("restartPosX=%.1f\n", speedrun::restartPosX);
        o += Fmt("restartPosY=%.1f\n", speedrun::restartPosY);
        o += Fmt("ratChoice=%d\n", speedrun::ratChoice);
        o += Fmt("momRoute=%d\n", speedrun::momRoute);
        o += Fmt("vaseChoice=%d\n", speedrun::vaseChoice);
        o += Fmt("vaseCount=%d\n", speedrun::vaseCount);
        o += Fmt("extraTraps=%d\n", speedrun::extraTraps ? 1 : 0);
        o += Fmt("lavaMode=%d\n", speedrun::lavaMode ? 1 : 0);

        o += Fmt("speedhackEnabled=%d\n", speedhack::enabled ? 1 : 0);
        o += Fmt("speedMultiplier=%.2f\n", speedhack::multiplier);

        o += Fmt("flatEnabled=%d\n", flat_textures::flatEnabled ? 1 : 0);
        o += Fmt("flatColor=%.3f,%.3f,%.3f,%.3f\n",
            flat_textures::flatColor[0], flat_textures::flatColor[1], flat_textures::flatColor[2], flat_textures::flatColor[3]);
        o += Fmt("pixelateEnabled=%d\n", flat_textures::pixelateEnabled ? 1 : 0);
        o += Fmt("pixelateLevel=%d\n", flat_textures::pixelateLevel);

        o += Fmt("autoFarmEnabled=%d\n", auto_farm::enabled ? 1 : 0);
        o += Fmt("autoFarmMethod=%d\n", auto_farm::escapeMethod);
        o += Fmt("autoFarmSpeed=%.1f\n", auto_farm::fastForwardSpeed);
        o += Fmt("autoFarmMenuDelay=%.2f\n", auto_farm::menuDelay);
        o += Fmt("autoFarmCutsceneDelay=%.2f\n", auto_farm::cutsceneDelay);

        o += Fmt("unlocksMasterEnabled=%d\n", game_unlocks::enabled ? 1 : 0);
        // diurutkan agar isi teks stabil (dipakai untuk mendeteksi perubahan)
        std::map<std::string, bool> sortedToggles(game_unlocks::savedToggles.begin(), game_unlocks::savedToggles.end());
        for (auto& pair : sortedToggles) {
            o += Fmt("unlock_%s=%d\n", pair.first.c_str(), pair.second ? 1 : 0);
        }

        o += Fmt("uiScale=%.2f\n", g_UiScale);
        o += Fmt("homeBarWidth=%.1f\n", g_HomeBarWidth);
        o += Fmt("scrollbarSize=%.1f\n", g_ScrollbarSize);
        o += Fmt("accentColor=%.3f,%.3f,%.3f,%.3f\n",
            g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], g_AccentColor[3]);
        return o;
    }

    // Baca satu berkas config. Mengembalikan false kalau berkas tidak ada.
    static bool ParseConfigFile(const std::string& fullPath) {
        FILE* f = fopen(fullPath.c_str(), "r");
        if (!f) return false;

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            int iVal = 0;
            float fVal = 0.0f;
            float c0, c1, c2, c3;
            char keyBuf[128];

            if (sscanf(line, "language=%d", &iVal) == 1) g_Language = 1;   // hanya Inggris
            else if (sscanf(line, "snowEnabled=%d", &iVal) == 1) g_SnowEnabled = (iVal != 0);
            else if (sscanf(line, "glowEnabled=%d", &iVal) == 1) g_WindowGlowEnabled = (iVal != 0);
            else if (sscanf(line, "showFPS=%d", &iVal) == 1) g_ShowFPS = (iVal != 0);
            else if (sscanf(line, "backgroundDim=%f", &fVal) == 1) g_BackgroundDim = fVal;

            else if (sscanf(line, "enemiesEnabled=%d", &iVal) == 1) esp_enemies::enabled = (iVal != 0);
            else if (sscanf(line, "targetGranny=%d", &iVal) == 1) esp_enemies::targetGranny = (iVal != 0);
            else if (sscanf(line, "targetGrandpa=%d", &iVal) == 1) esp_enemies::targetGrandpa = (iVal != 0);
            else if (sscanf(line, "targetSpiderMom=%d", &iVal) == 1) esp_enemies::targetSpiderMom = (iVal != 0);
            else if (sscanf(line, "enemiesWidth=%f", &fVal) == 1) esp_enemies::outlineWidth = fVal;
            else if (sscanf(line, "enemiesColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                esp_enemies::color[0] = c0; esp_enemies::color[1] = c1; esp_enemies::color[2] = c2; esp_enemies::color[3] = c3;
            }
            else if (sscanf(line, "enemiesDrawBox=%d", &iVal) == 1) esp_enemies::drawBox = (iVal != 0);
            else if (sscanf(line, "enemiesBoxType=%d", &iVal) == 1) esp_enemies::boxType = iVal;
            else if (sscanf(line, "enemiesBoxFill=%d", &iVal) == 1) esp_enemies::boxFill = (iVal != 0);
            else if (sscanf(line, "enemiesDrawName=%d", &iVal) == 1) esp_enemies::drawName = (iVal != 0);
            else if (sscanf(line, "enemiesDrawDistance=%d", &iVal) == 1) esp_enemies::drawDistance = (iVal != 0);
            else if (sscanf(line, "enemiesDrawTracers=%d", &iVal) == 1) esp_enemies::drawTracers = (iVal != 0);
            else if (sscanf(line, "enemiesChams=%d", &iVal) == 1) esp_enemies::enableChams = (iVal != 0);
            else if (sscanf(line, "enemyHeight=%f", &fVal) == 1) esp_enemies::enemyHeight = fVal;
            else if (sscanf(line, "enemyWidthMul=%f", &fVal) == 1) esp_enemies::enemyWidthMul = fVal;
            else if (sscanf(line, "enemyYOffset=%f", &fVal) == 1) esp_enemies::yOffset = fVal;
            else if (sscanf(line, "spiderHeight=%f", &fVal) == 1) esp_enemies::spiderHeight = fVal;
            else if (sscanf(line, "spiderWidthMul=%f", &fVal) == 1) esp_enemies::spiderWidthMul = fVal;
            else if (sscanf(line, "tracerColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                esp_enemies::tracerColor[0] = c0; esp_enemies::tracerColor[1] = c1; esp_enemies::tracerColor[2] = c2; esp_enemies::tracerColor[3] = c3;
            }

            else if (sscanf(line, "itemsEnabled=%d", &iVal) == 1) esp_items::enabled = (iVal != 0);
            else if (sscanf(line, "itemsWidth=%f", &fVal) == 1) esp_items::width = fVal;
            else if (sscanf(line, "itemsColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                esp_items::color[0] = c0; esp_items::color[1] = c1; esp_items::color[2] = c2; esp_items::color[3] = c3;
            }

            else if (sscanf(line, "grannyAimEnabled=%d", &iVal) == 1) aim::grannyAimEnabled = (iVal != 0);
            else if (sscanf(line, "aimOnlyWithWeapon=%d", &iVal) == 1) aim::onlyWithWeapon = (iVal != 0);
            else if (sscanf(line, "grannySmoothness=%f", &fVal) == 1) aim::grannySmoothness = fVal;
            else if (sscanf(line, "itemAimEnabled=%d", &iVal) == 1) aim::itemAimEnabled = (iVal != 0);
            else if (sscanf(line, "itemSmoothness=%f", &fVal) == 1) aim::itemSmoothness = fVal;
            else if (sscanf(line, "itemAimDistance=%f", &fVal) == 1) aim::itemAimDistance = fVal;
            else if (sscanf(line, "itemType_%127[^=]=%d", keyBuf, &iVal) == 2) aim::SetItemTypeEnabled(keyBuf, iVal != 0);
            else if (sscanf(line, "menuAlpha=%f", &fVal) == 1) g_MenuAlpha = fVal;
            else if (sscanf(line, "cornerRounding=%f", &fVal) == 1) g_CornerRounding = fVal;
            else if (sscanf(line, "popupScale=%f", &fVal) == 1) g_PopupScale = fVal;
            else if (sscanf(line, "popupAlpha=%f", &fVal) == 1) g_PopupAlpha = fVal;
            else if (sscanf(line, "hideGInGame=%d", &iVal) == 1) g_HideGInGame = (iVal != 0);
            else if (sscanf(line, "menuBgColor=%f,%f,%f", &c0, &c1, &c2) == 3) { g_MenuBgColor[0] = c0; g_MenuBgColor[1] = c1; g_MenuBgColor[2] = c2; }
            else if (sscanf(line, "popupColor=%f,%f,%f", &c0, &c1, &c2) == 3) { g_PopupColor[0] = c0; g_PopupColor[1] = c1; g_PopupColor[2] = c2; }
            else if (sscanf(line, "popupBgColor=%f,%f,%f", &c0, &c1, &c2) == 3) { g_PopupBgColor[0] = c0; g_PopupBgColor[1] = c1; g_PopupBgColor[2] = c2; }

            else if (sscanf(line, "disableDarkerFog=%d", &iVal) == 1) reshade::disableDarkerFog = (iVal != 0);
            else if (sscanf(line, "disableLightmaps=%d", &iVal) == 1) reshade::disableLightmaps = (iVal != 0);
            else if (sscanf(line, "godmode=%d", &iVal) == 1) player_mods::godmode = (iVal != 0);
            else if (sscanf(line, "noFallDamage=%d", &iVal) == 1) player_mods::noFallDamage = (iVal != 0);
            else if (sscanf(line, "fovChanger=%d", &iVal) == 1) player_mods::fovChanger = (iVal != 0);
            else if (sscanf(line, "customFov=%f", &fVal) == 1) player_mods::customFov = fVal;
            else if (sscanf(line, "infiniteAmmo=%d", &iVal) == 1) player_mods::infiniteAmmo = (iVal != 0);
            else if (sscanf(line, "electricDarts=%d", &iVal) == 1) player_mods::electricDarts = (iVal != 0);
            else if (sscanf(line, "explosiveShotgun=%d", &iVal) == 1) player_mods::explosiveShotgun = (iVal != 0);

            else if (sscanf(line, "autoUnlockShop=%d", &iVal) == 1) speedrun::autoUnlockShop = (iVal != 0);
            else if (sscanf(line, "restartButton=%d", &iVal) == 1) speedrun::restartButton = (iVal != 0);
            else if (sscanf(line, "restartPosX=%f", &fVal) == 1) speedrun::restartPosX = fVal;
            else if (sscanf(line, "restartPosY=%f", &fVal) == 1) speedrun::restartPosY = fVal;
            else if (sscanf(line, "ratChoice=%d", &iVal) == 1) speedrun::ratChoice = iVal;
            else if (sscanf(line, "momRoute=%d", &iVal) == 1) speedrun::momRoute = iVal;
            else if (sscanf(line, "vaseChoice=%d", &iVal) == 1) speedrun::vaseChoice = iVal;
            else if (sscanf(line, "vaseCount=%d", &iVal) == 1) speedrun::vaseCount = iVal;
            else if (sscanf(line, "extraTraps=%d", &iVal) == 1) speedrun::extraTraps = (iVal != 0);
            else if (sscanf(line, "lavaMode=%d", &iVal) == 1) speedrun::lavaMode = (iVal != 0);

            else if (sscanf(line, "speedhackEnabled=%d", &iVal) == 1) speedhack::enabled = (iVal != 0);
            else if (sscanf(line, "speedMultiplier=%f", &fVal) == 1) speedhack::multiplier = fVal;

            else if (sscanf(line, "flatEnabled=%d", &iVal) == 1) flat_textures::flatEnabled = (iVal != 0);
            else if (sscanf(line, "flatColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                flat_textures::flatColor[0] = c0; flat_textures::flatColor[1] = c1; flat_textures::flatColor[2] = c2; flat_textures::flatColor[3] = c3;
            }
            else if (sscanf(line, "pixelateEnabled=%d", &iVal) == 1) flat_textures::pixelateEnabled = (iVal != 0);
            else if (sscanf(line, "pixelateLevel=%d", &iVal) == 1) flat_textures::pixelateLevel = iVal;

            else if (sscanf(line, "autoFarmEnabled=%d", &iVal) == 1) auto_farm::enabled = (iVal != 0);
            else if (sscanf(line, "autoFarmMethod=%d", &iVal) == 1) auto_farm::escapeMethod = iVal;
            else if (sscanf(line, "autoFarmSpeed=%f", &fVal) == 1) auto_farm::fastForwardSpeed = fVal;
            else if (sscanf(line, "autoFarmMenuDelay=%f", &fVal) == 1) auto_farm::menuDelay = fVal;
            else if (sscanf(line, "autoFarmCutsceneDelay=%f", &fVal) == 1) auto_farm::cutsceneDelay = fVal;

            else if (sscanf(line, "unlocksMasterEnabled=%d", &iVal) == 1) game_unlocks::enabled = (iVal != 0);
            else if (sscanf(line, "unlock_%127[^=]=%d", keyBuf, &iVal) == 2) {
                game_unlocks::savedToggles[keyBuf] = (iVal != 0);
            }
            else if (sscanf(line, "uiScale=%f", &fVal) == 1) g_UiScale = fVal;
            else if (sscanf(line, "homeBarWidth=%f", &fVal) == 1) g_HomeBarWidth = fVal;
            else if (sscanf(line, "scrollbarSize=%f", &fVal) == 1) g_ScrollbarSize = fVal;
            else if (sscanf(line, "accentColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                g_AccentColor[0] = c0; g_AccentColor[1] = c1; g_AccentColor[2] = c2; g_AccentColor[3] = c3;
            }
        }

        fclose(f);
        return true;
    }

    void Init() {
        RefreshConfigList();

        // Muat otomatis pengaturan terakhir. Kalau autosave belum ada, pakai kahanium.cfg lama.
        std::string folder = GetActiveConfigPath();
        std::string autoPath = folder + "/" + kAutoFile;
        if (access(autoPath.c_str(), F_OK) != 0) {
            autoPath = folder + "/kahanium.cfg";
        }
        if (ParseConfigFile(autoPath)) {
            SetupPremiumStyle(g_UiScale);
            snprintf(statusText, sizeof(statusText), "Loaded");
        }
        s_LastAutoSaved = BuildConfigText();
    }

    // Dipanggil tiap frame. Menyimpan otomatis (maks. tiap 2 detik) jika ada pengaturan yang berubah.
    void AutoSaveTick() {
        s_AutoSaveTimer += ImGui::GetIO().DeltaTime;
        if (s_AutoSaveTimer < 2.0f) return;
        s_AutoSaveTimer = 0.0f;

        std::string text = BuildConfigText();
        if (text == s_LastAutoSaved) return;

        std::string fullPath = GetActiveConfigPath() + "/" + kAutoFile;
        FILE* f = fopen(fullPath.c_str(), "w");
        if (!f) return;
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);
        s_LastAutoSaved = text;
    }

    void SaveConfig(const char* customName) {
        std::string folder = GetActiveConfigPath();
        std::string targetFile;

        if (customName && strlen(customName) > 0) {
            targetFile = customName;
        } else if (strlen(currentConfigName) > 0) {
            targetFile = currentConfigName;
        } else {
            targetFile = GetNextAutoConfigName();
        }

        if (targetFile.find(".cfg") == std::string::npos) {
            targetFile += ".cfg";
        }

        std::string fullPath = folder + "/" + targetFile;
        FILE* f = fopen(fullPath.c_str(), "w");
        if (!f) {
            snprintf(statusText, sizeof(statusText), "Write error: %s", targetFile.c_str());
            return;
        }

        std::string text = BuildConfigText();
        fwrite(text.data(), 1, text.size(), f);
        fclose(f);

        RefreshConfigList();
        snprintf(statusText, sizeof(statusText), "Saved: %s", targetFile.c_str());
    }

    void LoadConfig(const char* customName) {
        std::string folder = GetActiveConfigPath();
        std::string targetFile = (customName && strlen(customName) > 0) ? customName : currentConfigName;
        if (targetFile.find(".cfg") == std::string::npos) {
            targetFile += ".cfg";
        }

        std::string fullPath = folder + "/" + targetFile;
        if (!ParseConfigFile(fullPath)) {
            snprintf(statusText, sizeof(statusText), "Not found: %s", targetFile.c_str());
            return;
        }

        SetupPremiumStyle(g_UiScale);
        snprintf(statusText, sizeof(statusText), "Loaded: %s", targetFile.c_str());
    }
}