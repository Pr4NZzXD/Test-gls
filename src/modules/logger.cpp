#include "logger.h"
#include "il2cpp_api.h"
#include "menu.h"
#include "imgui.h"

#include <cstdio>
#include <cstdarg>
#include <algorithm>

namespace Logger {
    std::vector<LogEntry> logs;
    bool autoScroll = true;
    static char s_Filter[64] = "";
    static int s_ActiveTagFilter = 0; // 0=ALL, 1=AIM, 2=PLAYER, 3=WEAPON, 4=IL2CPP, 5=SCENE

    void Init() {
        Clear();
        Log("SYSTEM", LOG_OK, "Logger initialized. Base: 0x%lx", g_Il2CppBase);
    }

    void Clear() {
        logs.clear();
    }

    void Log(const char* tag, LogLevel level, const char* fmt, ...) {
        char buf[512];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        float curTime = ImGui::GetTime();
        if (logs.size() > 500) {
            logs.erase(logs.begin(), logs.begin() + 50);
        }
        logs.push_back({ tag ? tag : "SYS", buf, level, curTime });
    }

    void DrawTab() {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "IL2CPP & NATIVE FUNCTION TRACE DEBUGGER");
        ImGui::Spacing();

        ImVec4 accentColor = ImVec4(g_AccentColor[0], g_AccentColor[1], g_AccentColor[2], g_AccentColor[3]);

        // Панель фильтров по модулям
        const char* tags[] = { "ALL", "AIM", "PLAYER", "WEAPON", "IL2CPP", "SCENE" };
        float btnW = (ImGui::GetContentRegionAvail().x - 25.0f * g_UiScale) / 6.0f;

        for (int i = 0; i < 6; i++) {
            if (s_ActiveTagFilter == i) ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
            if (ImGui::Button(tags[i], ImVec2(btnW, 26.0f * g_UiScale))) {
                s_ActiveTagFilter = i;
            }
            if (s_ActiveTagFilter == i) ImGui::PopStyleColor();
            if (i < 5) ImGui::SameLine(0, 5.0f * g_UiScale);
        }

        ImGui::Spacing();

        // Строка поиска и управление
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 210.0f * g_UiScale);
        ImGui::InputTextWithHint("##logFlt", "Search logs...", s_Filter, sizeof(s_Filter));

        ImGui::SameLine();
        if (ImGui::Button(" Copy ", ImVec2(100.0f * g_UiScale, 28.0f * g_UiScale))) {
            std::string allText = "";
            for (auto& l : logs) {
                allText += "[" + l.tag + "] " + l.text + "\n";
            }
            CopyToAndroidClipboard(allText.c_str());
        }

        ImGui::SameLine();
        if (ImGui::Button(" Clear ", ImVec2(95.0f * g_UiScale, 28.0f * g_UiScale))) {
            Clear();
        }

        ImGui::Spacing();
        ImGui::Checkbox("Авто-скролл вниз (Auto-scroll)", &autoScroll);
        ImGui::Spacing();

        // Окно вывода логов с кодовой подсветкой
        ImGui::BeginChild("##traceConsole", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);

        std::string filterStr = s_Filter;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

        for (size_t i = 0; i < logs.size(); i++) {
            auto& l = logs[i];

            if (s_ActiveTagFilter != 0 && l.tag != tags[s_ActiveTagFilter]) {
                continue;
            }

            if (!filterStr.empty()) {
                std::string lMsg = l.text;
                std::transform(lMsg.begin(), lMsg.end(), lMsg.begin(), ::tolower);
                if (lMsg.find(filterStr) == std::string::npos && l.tag.find(filterStr) == std::string::npos) {
                    continue;
                }
            }

            ImVec4 col;
            const char* prefix = "";
            switch (l.level) {
                case LOG_OK:   col = ImVec4(0.2f, 1.0f, 0.4f, 1.0f); prefix = "[OK]  "; break;
                case LOG_CALL: col = ImVec4(0.9f, 0.9f, 0.95f, 1.0f); prefix = "[CALL]"; break;
                case LOG_WARN: col = ImVec4(1.0f, 0.8f, 0.2f, 1.0f); prefix = "[WARN]"; break;
                case LOG_ERR:  col = ImVec4(1.0f, 0.25f, 0.25f, 1.0f); prefix = "[ERR] "; break;
            }

            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.6f, 1.0f), "[%06.2f]", l.time);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%-6s]", l.tag.c_str());
            ImGui::SameLine();
            ImGui::TextColored(col, "%s %s", prefix, l.text.c_str());
        }

        if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }

        ImGui::EndChild();
    }
}