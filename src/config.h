#pragma once
#include <vector>
#include <string>

namespace config {
    extern char statusText[64];
    extern char currentConfigName[64];
    extern std::vector<std::string> foundConfigs;
    extern const char* configFolder;

    void Init();
    void RefreshConfigList();
    std::string GetNextAutoConfigName();
    void SaveConfig(const char* customName = nullptr);
    void LoadConfig(const char* customName = nullptr);
}