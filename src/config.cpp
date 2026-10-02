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

#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <string>

namespace config {
    char statusText[64] = "Готов / Ready";
    char currentConfigName[64] = "kahanium.cfg";
    std::vector<std::string> foundConfigs;
    const char* configFolder = "/sdcard/Android/data/com.OmegaMegaGigalIntel.GrannyLegacy/files";

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

    void Init() {
        RefreshConfigList();
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
            snprintf(statusText, sizeof(statusText), "Ошибка записи: %s", targetFile.c_str());
            return;
        }

        fprintf(f, "language=%d\n", g_Language);
        fprintf(f, "snowEnabled=%d\n", g_SnowEnabled ? 1 : 0);
        fprintf(f, "glowEnabled=%d\n", g_WindowGlowEnabled ? 1 : 0);
        fprintf(f, "backgroundDim=%.2f\n", g_BackgroundDim);

        fprintf(f, "enemiesEnabled=%d\n", esp_enemies::enabled ? 1 : 0);
        fprintf(f, "targetGranny=%d\n", esp_enemies::targetGranny ? 1 : 0);
        fprintf(f, "targetGrandpa=%d\n", esp_enemies::targetGrandpa ? 1 : 0);
        fprintf(f, "targetSpiderMom=%d\n", esp_enemies::targetSpiderMom ? 1 : 0);
        fprintf(f, "enemiesWidth=%.2f\n", esp_enemies::outlineWidth);
        fprintf(f, "enemiesColor=%.3f,%.3f,%.3f,%.3f\n",
            esp_enemies::color[0], esp_enemies::color[1], esp_enemies::color[2], esp_enemies::color[3]);

        fprintf(f, "itemsEnabled=%d\n", esp_items::enabled ? 1 : 0);
        fprintf(f, "itemsWidth=%.2f\n", esp_items::width);
        fprintf(f, "itemsColor=%.3f,%.3f,%.3f,%.3f\n",
            esp_items::color[0], esp_items::color[1], esp_items::color[2], esp_items::color[3]);

        fprintf(f, "disableDarkerFog=%d\n", reshade::disableDarkerFog ? 1 : 0);
        fprintf(f, "disableLightmaps=%d\n", reshade::disableLightmaps ? 1 : 0);
        fprintf(f, "godmode=%d\n", player_mods::godmode ? 1 : 0);
        fprintf(f, "noFallDamage=%d\n", player_mods::noFallDamage ? 1 : 0);
        fprintf(f, "fovChanger=%d\n", player_mods::fovChanger ? 1 : 0);
        fprintf(f, "customFov=%.1f\n", player_mods::customFov);

        fprintf(f, "speedhackEnabled=%d\n", speedhack::enabled ? 1 : 0);
        fprintf(f, "speedMultiplier=%.2f\n", speedhack::multiplier);

        fprintf(f, "flatEnabled=%d\n", flat_textures::flatEnabled ? 1 : 0);
        fprintf(f, "flatColor=%.3f,%.3f,%.3f,%.3f\n",
            flat_textures::flatColor[0], flat_textures::flatColor[1], flat_textures::flatColor[2], flat_textures::flatColor[3]);
        fprintf(f, "pixelateEnabled=%d\n", flat_textures::pixelateEnabled ? 1 : 0);
        fprintf(f, "pixelateLevel=%d\n", flat_textures::pixelateLevel);

        fprintf(f, "autoFarmEnabled=%d\n", auto_farm::enabled ? 1 : 0);
        fprintf(f, "autoFarmMethod=%d\n", auto_farm::escapeMethod);
        fprintf(f, "autoFarmSpeed=%.1f\n", auto_farm::fastForwardSpeed);
        fprintf(f, "autoFarmMenuDelay=%.2f\n", auto_farm::menuDelay);
        fprintf(f, "autoFarmCutsceneDelay=%.2f\n", auto_farm::cutsceneDelay);

        fprintf(f, "unlocksMasterEnabled=%d\n", game_unlocks::enabled ? 1 : 0);
        for (auto& pair : game_unlocks::savedToggles) {
            fprintf(f, "unlock_%s=%d\n", pair.first.c_str(), pair.second ? 1 : 0);
        }

        fprintf(f, "uiScale=%.2f\n", g_UiScale);
        fprintf(f, "homeBarWidth=%.1f\n", g_HomeBarWidth);
        fprintf(f, "scrollbarSize=%.1f\n", g_ScrollbarSize);
        fprintf(f, "accentColor=%.3f,%.3f,%.3f,%.3f\n",
            g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], g_AccentColor[3]);

        fclose(f);
        RefreshConfigList();
        snprintf(statusText, sizeof(statusText), "Сохранено: %s", targetFile.c_str());
    }

    void LoadConfig(const char* customName) {
        std::string folder = GetActiveConfigPath();
        std::string targetFile = (customName && strlen(customName) > 0) ? customName : currentConfigName;
        if (targetFile.find(".cfg") == std::string::npos) {
            targetFile += ".cfg";
        }

        std::string fullPath = folder + "/" + targetFile;
        FILE* f = fopen(fullPath.c_str(), "r");
        if (!f) {
            snprintf(statusText, sizeof(statusText), "Не найден: %s", targetFile.c_str());
            return;
        }

        char line[256];
        while (fgets(line, sizeof(line), f)) {
            int iVal = 0;
            float fVal = 0.0f;
            float c0, c1, c2, c3;
            char keyBuf[128];

            if (sscanf(line, "language=%d", &iVal) == 1) g_Language = iVal;
            else if (sscanf(line, "snowEnabled=%d", &iVal) == 1) g_SnowEnabled = (iVal != 0);
            else if (sscanf(line, "glowEnabled=%d", &iVal) == 1) g_WindowGlowEnabled = (iVal != 0);
            else if (sscanf(line, "backgroundDim=%f", &fVal) == 1) g_BackgroundDim = fVal;

            else if (sscanf(line, "enemiesEnabled=%d", &iVal) == 1) esp_enemies::enabled = (iVal != 0);
            else if (sscanf(line, "targetGranny=%d", &iVal) == 1) esp_enemies::targetGranny = (iVal != 0);
            else if (sscanf(line, "targetGrandpa=%d", &iVal) == 1) esp_enemies::targetGrandpa = (iVal != 0);
            else if (sscanf(line, "targetSpiderMom=%d", &iVal) == 1) esp_enemies::targetSpiderMom = (iVal != 0);
            else if (sscanf(line, "enemiesWidth=%f", &fVal) == 1) esp_enemies::outlineWidth = fVal;
            else if (sscanf(line, "enemiesColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                esp_enemies::color[0] = c0; esp_enemies::color[1] = c1; esp_enemies::color[2] = c2; esp_enemies::color[3] = c3;
            }
            else if (sscanf(line, "itemsEnabled=%d", &iVal) == 1) esp_items::enabled = (iVal != 0);
            else if (sscanf(line, "itemsWidth=%f", &fVal) == 1) esp_items::width = fVal;
            else if (sscanf(line, "itemsColor=%f,%f,%f,%f", &c0, &c1, &c2, &c3) == 4) {
                esp_items::color[0] = c0; esp_items::color[1] = c1; esp_items::color[2] = c2; esp_items::color[3] = c3;
            }
            else if (sscanf(line, "disableDarkerFog=%d", &iVal) == 1) reshade::disableDarkerFog = (iVal != 0);
            else if (sscanf(line, "disableLightmaps=%d", &iVal) == 1) reshade::disableLightmaps = (iVal != 0);
            else if (sscanf(line, "godmode=%d", &iVal) == 1) player_mods::godmode = (iVal != 0);
            else if (sscanf(line, "noFallDamage=%d", &iVal) == 1) player_mods::noFallDamage = (iVal != 0);
            else if (sscanf(line, "fovChanger=%d", &iVal) == 1) player_mods::fovChanger = (iVal != 0);
            else if (sscanf(line, "customFov=%f", &fVal) == 1) player_mods::customFov = fVal;

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
        SetupPremiumStyle(g_UiScale);
        snprintf(statusText, sizeof(statusText), "Загружено: %s", targetFile.c_str());
    }
}