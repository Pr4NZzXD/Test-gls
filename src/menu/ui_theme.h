#pragma once

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include "imgui.h"
#include "imgui_internal.h"

namespace menuscale {
    extern float menuscale;
    extern float menuscalee;
    extern bool menuscaletrue;
}

namespace font {
    inline ImFont* icomoon = nullptr;
    inline ImFont* lexend_bold = nullptr;
    inline ImFont* lexend_regular = nullptr;
    inline ImFont* icomoon_widget = nullptr;
}

inline ImFont* fontsicons = nullptr;

namespace c {
    inline ImVec4 accent = ImColor(112, 110, 215);

    namespace background {
        inline ImVec4 filling = ImColor(12, 12, 12);
        inline ImVec4 stroke = ImColor(14, 14, 15);
        inline ImVec2 size = ImVec2(1000, 660);
        inline float rounding = 6.0f;
    }

    namespace elements {
        inline ImVec4 mark = ImColor(255, 255, 255);
        inline ImVec4 stroke = ImColor(30, 30, 30);
        inline ImVec4 background = ImColor(18, 18, 18);
        inline ImVec4 backgroundd = ImColor(23, 25, 25);
        inline ImVec4 background_widget = ImColor(18, 18, 18);
        inline ImVec4 text_active = ImColor(255, 255, 255);
        inline ImVec4 text_hov = ImColor(150, 150, 160);
        inline ImVec4 text = ImColor(100, 105, 115);
        inline float rounding = 6.0f;
    }

    namespace tab {
        inline ImVec4 tab_active = ImColor(18, 18, 18);
        inline ImVec4 tab_outline = ImColor(28, 28, 28);
        inline ImVec4 tab_outlinee = ImColor(25, 25, 25);
        inline ImVec4 border = ImColor(14, 14, 15);
    }
}

namespace color_picker {
    inline ImVec4 picker_bg = ImColor(21, 22, 24);
    inline ImVec4 picker_rect = ImColor(15, 16, 19);
    inline ImVec4 picker_circle = ImColor(255, 255, 255);
    inline ImVec4 text_active = ImColor(255, 255, 255);
    inline ImVec4 text_inactive = ImColor(76, 76, 77);
}

namespace button {
    inline ImVec4 button_bg = ImColor(18, 18, 18);
    inline ImVec4 text_active = ImColor(255, 255, 255);
    inline ImVec4 text_inactive = ImColor(76, 76, 77);
}