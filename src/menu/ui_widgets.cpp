#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include "ui_widgets.h"
#include "ui_theme.h"
#include "ui_anim.h"

#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <cstdio>

namespace menuscale {
    float menuscale = 1.0f;
    float menuscalee = 1.0f;
    bool menuscaletrue = true;
}

static void RenderTextColorr(ImFont* font, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, const char* text, const ImVec2& align) {
    if (font && font->IsLoaded()) ImGui::PushFont(font);
    ImGui::PushStyleColor(ImGuiCol_Text, col);
    ImGui::RenderTextClipped(p_min, p_max, text, NULL, NULL, align, NULL);
    ImGui::PopStyleColor();
    if (font && font->IsLoaded()) ImGui::PopFont();
}

namespace edited {

// 1. КРУПНЫЙ МОБИЛЬНЫЙ ЧЕКБОКС (КВАДРАТ 19px, СТРОКА 32px)
bool checkbox(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const float square_sz = 19.0f * menuscale::menuscale;
    const float row_height = 32.0f * menuscale::menuscale;
    const ImVec2 pos = window->DC.CursorPos;
    const float availW = ImGui::GetContentRegionAvail().x;

    const ImRect bb(pos, pos + ImVec2(availW, row_height));
    ImGui::ItemSize(bb, 0.f);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);
    if (pressed) {
        *v = !(*v);
        ImGui::MarkItemEdited(id);
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 boxMin = pos + ImVec2(2.0f, (row_height - square_sz) * 0.5f);
    ImVec2 boxMax = boxMin + ImVec2(square_sz, square_sz);

    // Квадратик чекбокса
    draw->AddRectFilled(boxMin, boxMax, *v ? ImGui::GetColorU32(c::accent) : IM_COL32(24, 24, 28, 255), 3.0f);
    draw->AddRect(boxMin, boxMax, *v ? ImGui::GetColorU32(c::accent) : (hovered ? IM_COL32(110, 110, 130, 255) : IM_COL32(50, 50, 60, 255)), 3.0f);

    // Четкая белая галочка
    if (*v) {
        float sz = square_sz;
        draw->AddLine(ImVec2(boxMin.x + sz * 0.22f, boxMin.y + sz * 0.50f),
                      ImVec2(boxMin.x + sz * 0.44f, boxMin.y + sz * 0.74f), IM_COL32(255, 255, 255, 255), 2.0f);
        draw->AddLine(ImVec2(boxMin.x + sz * 0.44f, boxMin.y + sz * 0.74f),
                      ImVec2(boxMin.x + sz * 0.80f, boxMin.y + sz * 0.25f), IM_COL32(255, 255, 255, 255), 2.0f);
    }

    // Крупный текст справа
    ImU32 txtCol = *v ? IM_COL32(245, 245, 250, 255) : (hovered ? IM_COL32(210, 215, 225, 255) : IM_COL32(145, 145, 160, 255));
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    draw->AddText(ImVec2(boxMax.x + 10.0f * menuscale::menuscale, pos.y + (row_height - txtSz.y) * 0.5f), txtCol, label);

    return pressed;
}

// 2. УДОБНЫЙ СЛАЙДЕР С ЖИРНОЙ ДОРОЖКОЙ (ВЫСОТА 44px)
bool slider_scalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = window->DC.CursorPos;
    const float totalH = 46.0f * menuscale::menuscale;

    char value_buf[64];
    if (data_type == ImGuiDataType_Float) snprintf(value_buf, sizeof(value_buf), format ? format : "%.1f", *(float*)p_data);
    else if (data_type == ImGuiDataType_S32) snprintf(value_buf, sizeof(value_buf), format ? format : "%d", *(int*)p_data);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    // Верхняя строчка: Текст слева, число справа
    draw->AddText(pos + ImVec2(2.0f, 2.0f), IM_COL32(150, 150, 165, 255), label);
    ImVec2 valSz = ImGui::CalcTextSize(value_buf);
    draw->AddText(ImVec2(pos.x + w - valSz.x - 2.0f, pos.y + 2.0f), IM_COL32(240, 240, 245, 255), value_buf);

    // Жирная дорожка слайдера (высота 8px — удобно тапать пальцем)
    const float trackH = 8.0f * menuscale::menuscale;
    const ImRect frame_bb(pos + ImVec2(2.0f, 26.0f * menuscale::menuscale), pos + ImVec2(w - 2.0f, 26.0f * menuscale::menuscale + trackH));
    const ImRect total_bb(pos, pos + ImVec2(w, totalH));

    ImGui::ItemSize(total_bb, 0.f);
    if (!ImGui::ItemAdd(frame_bb, id, &frame_bb)) return false;

    bool hovered, held;
    ImGui::ButtonBehavior(frame_bb, id, &hovered, &held);

    // Фон дорожки
    draw->AddRectFilled(frame_bb.Min, frame_bb.Max, IM_COL32(24, 24, 30, 255), 3.0f);
    draw->AddRect(frame_bb.Min, frame_bb.Max, IM_COL32(45, 45, 55, 255), 3.0f);

    ImRect grab_bb;
    const bool value_changed = ImGui::SliderBehavior(frame_bb, id, data_type, p_data, p_min, p_max, "", flags, &grab_bb);
    if (value_changed) ImGui::MarkItemEdited(id);

    // Заливка пройденного пути
    float filledW = grab_bb.Min.x - frame_bb.Min.x + 6.0f;
    draw->AddRectFilled(frame_bb.Min, ImVec2(frame_bb.Min.x + filledW, frame_bb.Max.y), ImGui::GetColorU32(c::accent), 3.0f);

    return value_changed;
}

bool slider_float(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) {
    return slider_scalar(label, ImGuiDataType_Float, v, &v_min, &v_max, format, flags);
}

bool slider_int(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) {
    return slider_scalar(label, ImGuiDataType_S32, v, &v_min, &v_max, format, flags);
}

// 3. УДОБНАЯ КРУПНАЯ КНОПКА ПОД ПАЛЕЦ (ВЫСОТА 40px)
bool buttonn(const char* label, const ImVec2& size_arg) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);
    const ImVec2 pos = window->DC.CursorPos;

    float defW = ImGui::GetContentRegionAvail().x;
    float defH = 40.0f * menuscale::menuscale;
    ImVec2 size = ImVec2(size_arg.x > 0.0f ? size_arg.x : defW, size_arg.y > 0.0f ? size_arg.y : defH);

    const ImRect bb(pos, pos + size);
    ImGui::ItemSize(bb);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(bb.Min, bb.Max, hovered ? IM_COL32(32, 32, 40, 255) : IM_COL32(22, 22, 26, 255), 4.0f);
    draw->AddRect(bb.Min, bb.Max, hovered ? ImGui::GetColorU32(c::accent) : IM_COL32(50, 50, 60, 255), 4.0f);

    ImVec2 txtPos = bb.Min + (size - label_size) * 0.5f;
    draw->AddText(txtPos, IM_COL32(240, 240, 245, 255), label);

    return pressed;
}

void colortext(const ImVec4& col, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    ImGui::TextColoredV(col, fmt, args);
    va_end(args);
}

// 4. КОМБОБОКС ПОД ПАЛЕЦ
bool combo(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    const float w = ImGui::GetContentRegionAvail().x;
    const ImVec2 pos = window->DC.CursorPos;
    float comboW = 120.0f * menuscale::menuscale;
    float rowH = 34.0f * menuscale::menuscale;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    draw->AddText(pos + ImVec2(2.0f, (rowH - txtSz.y) * 0.5f), IM_COL32(150, 150, 165, 255), label);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + w - comboW, pos.y + (rowH - 26.0f * menuscale::menuscale) * 0.5f));
    ImGui::SetNextItemWidth(comboW);

    std::string id = std::string("##cmb_") + label;
    bool changed = ImGui::Combo(id.c_str(), current_item, items, items_count);

    ImGui::SetCursorScreenPos(pos + ImVec2(0, rowH));
    return changed;
}

bool color_edit4(const char* label, float col[4], ImGuiColorEditFlags flags) {
    float availW = ImGui::GetContentRegionAvail().x;
    float rowH = 32.0f * menuscale::menuscale;
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 txtSz = ImGui::CalcTextSize(label);
    draw->AddText(pos + ImVec2(2.0f, (rowH - txtSz.y) * 0.5f), IM_COL32(150, 150, 165, 255), label);

    ImGui::SetCursorScreenPos(ImVec2(pos.x + availW - 32.0f * menuscale::menuscale, pos.y + 4.0f));
    std::string id = std::string("##ce4_") + label;
    bool changed = ImGui::ColorEdit4(id.c_str(), col, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar);

    ImGui::SetCursorScreenPos(pos + ImVec2(0, rowH));
    return changed;
}

bool color_edit3(const char* label, float col[3], ImGuiColorEditFlags flags) {
    float col4[4] = { col[0], col[1], col[2], 1.0f };
    bool changed = color_edit4(label, col4, flags | ImGuiColorEditFlags_NoAlpha);
    if (changed) { col[0] = col4[0]; col[1] = col4[1]; col[2] = col4[2]; }
    return changed;
}

} // namespace edited