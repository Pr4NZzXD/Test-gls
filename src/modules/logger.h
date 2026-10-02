#pragma once
#include <string>
#include <vector>

enum LogLevel {
    LOG_CALL, // Белый: технические вызовы функций
    LOG_OK,   // Зелёный: успешные вызовы Unity/IL2CPP
    LOG_WARN, // Жёлтый: предупреждения
    LOG_ERR   // Красный: ошибки, nullptrs и исключения
};

struct LogEntry {
    std::string tag;
    std::string text;
    LogLevel level;
    float time;
};

namespace Logger {
    extern std::vector<LogEntry> logs;
    extern bool autoScroll;

    void Init();
    void Clear();
    void Log(const char* tag, LogLevel level, const char* fmt, ...);
    void DrawTab();
}