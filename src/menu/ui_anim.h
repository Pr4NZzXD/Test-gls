#pragma once
#include "imgui.h"

class animation {
public:
    animation(float i = 0.f) : value(i) {}
    void update(float to_value, float duration = 0.10f);
    float value = 0.f;
};

class animation_vec2 {
public:
    animation_vec2(ImVec2 value = ImVec2(0, 0)) : value(value) {}
    void update(ImVec2 to_value, float duration = 0.10f);
    ImVec2 value = { 0, 0 };
};

class color_utils {
public:
    ImVec4 accent = ImColor(255, 211, 145).Value;
    ImVec4 process_alpha(ImVec4 input, float alpha = 1.0f);
    ImVec4 get_accent_imv4(float alpha = 1.0f, float shading = 1.0f);
    ImColor get_accent_imc(float alpha = 1.0f, float shading = 1.0f);
};

inline color_utils* c_utils = new color_utils();