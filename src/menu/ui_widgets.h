#pragma once
#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui.h"
#include "imgui_internal.h"
#include "ui_theme.h"

namespace edited {
    bool checkbox(const char* label, bool* v);
    bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format = "%.1f", ImGuiSliderFlags flags = 0);
    bool slider_int(const char* label, int* v, int v_min, int v_max, const char* format = "%d", ImGuiSliderFlags flags = 0);
    bool slider_scalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format = NULL, ImGuiSliderFlags flags = 0);
    bool buttonn(const char* label, const ImVec2& size = ImVec2(0, 0));
    void colortext(const ImVec4& col, const char* fmt, ...);
    bool combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items = -1);
    bool color_edit3(const char* label, float col[3], ImGuiColorEditFlags flags = 0);
    bool color_edit4(const char* label, float col[4], ImGuiColorEditFlags flags = 0);
}