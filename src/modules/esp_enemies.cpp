#include "esp_enemies.h"
#include "il2cpp_api.h"
#include "player_mods.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace esp_enemies {
    bool enabled = true;

    bool targetGranny = true;
    bool targetGrandpa = true;
    bool targetSpiderMom = true;

    bool drawBox = true;
    int  boxType = 1; // 0 = Solid Box, 1 = Corner Box
    bool boxFill = false;
    bool drawName = true;
    bool drawDistance = true;
    bool drawTracers = false;
    bool enableChams = true;

    // Базовые параметры калибровки по тестам
    float enemyHeight    = 4.00f; // В игре Бабка ~4 метра с учётом меша
    float enemyWidthMul  = 0.60f;
    float yOffset        = 0.80f;
    float spiderHeight   = 1.20f;
    float spiderWidthMul = 1.20f;

    float outlineWidth = 1.8f;
    float color[4] = { 1.0f, 0.20f, 0.25f, 1.0f };
    float tracerColor[4] = { 1.0f, 1.0f, 1.0f, 0.70f };
    bool showSettings = false;

    struct EnemyTarget {
        const char* name;
        void* gameObject;
        void* transform;
        bool isSpider;
    };
    static std::vector<EnemyTarget> g_ActiveEnemies;

    struct Rect2D {
        float left;
        float top;
        float right;
        float bottom;
    };

    void ClearCache() {
        g_ActiveEnemies.clear();
    }

    void FindAndTrackEnemies() {
        g_ActiveEnemies.clear();
        if (!IsSceneReady()) return;

        struct AIScanDef {
            const char* className;
            const char* displayName;
            bool active;
            bool isSpider;
        };
        AIScanDef aiClasses[] = {
            { "AI_Granny",    "Granny",     targetGranny,    false },
            { "AI_Grandpa",   "Grandpa",    targetGrandpa,   false },
            { "AI_MomSpider", "Spider Mom", targetSpiderMom, true },
            { "MomSpider",    "Spider Mom", targetSpiderMom, true },
            { "Spider",       "Spider Mom", targetSpiderMom, true }
        };

        for (auto& ai : aiClasses) {
            if (!ai.active) continue;
            void* klass = FindClass("", ai.className);
            if (!klass) continue;
            void* type = il2cpp_class_get_type(klass);
            if (!type) continue;
            void* typeObj = il2cpp_type_get_object(type);
            if (!typeObj) continue;

            std::vector<void*> comps = FindObjectsOfUnityType(typeObj);
            for (void* comp : comps) {
                if (!comp || !IsNativeObjectAlive(comp)) continue;
                void* go = oComponentGetGameObject ? oComponentGetGameObject(comp) : nullptr;
                void* tr = oComponentGetTransform ? oComponentGetTransform(comp) : nullptr;

                if (go && tr && IsNativeObjectAlive(go) && IsNativeObjectAlive(tr)) {
                    bool exists = false;
                    for (auto& e : g_ActiveEnemies) {
                        if (e.gameObject == go) { exists = true; break; }
                    }
                    if (!exists) {
                        g_ActiveEnemies.push_back({ ai.displayName, go, tr, ai.isSpider });
                    }
                }
            }
        }

        if (g_ActiveEnemies.empty() && oGameObjectFind && il2cpp_string_new) {
            struct TargetDef {
                const char* name;
                const char* displayName;
                bool active;
                bool isSpider;
            };
            TargetDef defs[] = {
                { "GrannyParent",   "Granny",     targetGranny,    false },
                { "Enemy_Granny",   "Granny",     targetGranny,    false },
                { "Granny",         "Granny",     targetGranny,    false },
                { "GrandpaParent",  "Grandpa",    targetGrandpa,   false },
                { "Enemy_Grandpa",  "Grandpa",    targetGrandpa,   false },
                { "MomSpider",      "Spider Mom", targetSpiderMom, true }
            };

            for (auto& d : defs) {
                if (!d.active) continue;
                void* monoName = il2cpp_string_new(d.name);
                if (!monoName) continue;

                void* go = oGameObjectFind(monoName);
                if (go && IsNativeObjectAlive(go)) {
                    void* tr = oGameObjectGetTransform ? oGameObjectGetTransform(go) : nullptr;
                    if (tr && IsNativeObjectAlive(tr)) {
                        bool exists = false;
                        for (auto& e : g_ActiveEnemies) {
                            if (e.gameObject == go) { exists = true; break; }
                        }
                        if (!exists) {
                            g_ActiveEnemies.push_back({ d.displayName, go, tr, d.isSpider });
                        }
                    }
                }
            }
        }
    }

    void Update() {
        if (!IsSceneReady()) {
            ClearCache();
            return;
        }

        bool hasDeadEnemies = false;
        for (auto& e : g_ActiveEnemies) {
            if (!e.gameObject || !IsNativeObjectAlive(e.gameObject)) {
                hasDeadEnemies = true;
                break;
            }
        }

        if (hasDeadEnemies || g_ActiveEnemies.empty()) {
            ClearCache();
            FindAndTrackEnemies();
        }

        for (auto& e : g_ActiveEnemies) {
            if (e.gameObject && IsNativeObjectAlive(e.gameObject)) {
                bool shouldChams = enabled && enableChams;
                SetOutlineOnObject(e.gameObject, shouldChams, color, outlineWidth * 2.0f);
            }
        }
    }

    bool GetEnemy2DBox(const Vector3& feetPos, float height, float widthMul, float yOff, float screenW, float screenH, Rect2D& outBox, float& outDist) {
        Vector3 topWorld    = { feetPos.x, feetPos.y + height + yOff, feetPos.z };
        Vector3 bottomWorld = { feetPos.x, feetPos.y + yOff, feetPos.z };

        Vector3 topScreenRaw{ 0, 0, 0 };
        Vector3 botScreenRaw{ 0, 0, 0 };

        if (!WorldToScreen(topWorld, topScreenRaw) || !WorldToScreen(bottomWorld, botScreenRaw)) {
            return false;
        }

        float topY = screenH - topScreenRaw.y;
        float botY = screenH - botScreenRaw.y;

        float boxH = botY - topY;
        if (boxH < 6.0f || boxH > screenH * 2.5f) return false;

        float boxW = boxH * widthMul;
        float centerX = (topScreenRaw.x + botScreenRaw.x) * 0.5f;

        outBox.left   = centerX - boxW * 0.5f;
        outBox.right  = centerX + boxW * 0.5f;
        outBox.top    = topY;
        outBox.bottom = botY;
        outDist       = botScreenRaw.z;

        return true;
    }

    void DrawOverlay() {
        if (!enabled || !IsSceneReady()) return;

        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        ImVec2 disp = ImGui::GetIO().DisplaySize;

        ImU32 boxCol     = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], color[3]));
        ImU32 shadowCol  = IM_COL32(0, 0, 0, 220);
        ImU32 fillCol    = ImGui::ColorConvertFloat4ToU32(ImVec4(color[0], color[1], color[2], 0.12f));
        ImU32 tracCol    = ImGui::ColorConvertFloat4ToU32(ImVec4(tracerColor[0], tracerColor[1], tracerColor[2], tracerColor[3]));

        for (auto& e : g_ActiveEnemies) {
            if (!e.transform || !IsNativeObjectAlive(e.transform)) continue;

            Vector3 feetPos{ 0, 0, 0 };
            if (oTransformGetPosition) oTransformGetPosition(e.transform, &feetPos);

            float modelH = e.isSpider ? spiderHeight : enemyHeight;
            float wMul   = e.isSpider ? spiderWidthMul : enemyWidthMul;

            Rect2D box;
            float dist = 0.0f;

            if (GetEnemy2DBox(feetPos, modelH, wMul, yOffset, disp.x, disp.y, box, dist)) {
                float boxW = box.right - box.left;
                float boxH = box.bottom - box.top;
                float centerX = box.left + boxW * 0.5f;

                ImVec2 pMin(box.left, box.top);
                ImVec2 pMax(box.right, box.bottom);

                // 1. Трейсеры
                if (drawTracers) {
                    draw->AddLine(ImVec2(disp.x * 0.5f, disp.y), ImVec2(centerX, box.bottom), tracCol, 1.2f);
                }

                // 2. Полупрозрачная заливка
                if (boxFill && drawBox) {
                    draw->AddRectFilled(pMin, pMax, fillCol, 0.0f);
                }

                // 3. 2D Боксы
                if (drawBox) {
                    if (boxType == 0) {
                        draw->AddRect(ImVec2(pMin.x - 1, pMin.y - 1), ImVec2(pMax.x + 1, pMax.y + 1), shadowCol, 0.0f, 0, outlineWidth + 1.2f);
                        draw->AddRect(pMin, pMax, boxCol, 0.0f, 0, outlineWidth);
                    }
                    else if (boxType == 1) {
                        float lineW = boxW * 0.25f;
                        float lineH = boxH * 0.18f;
                        float th = outlineWidth;

                        auto DrawCorner = [&](ImVec2 p1, ImVec2 p2, ImVec2 p3) {
                            draw->AddLine(p1, p2, shadowCol, th + 1.4f);
                            draw->AddLine(p1, p3, shadowCol, th + 1.4f);
                            draw->AddLine(p1, p2, boxCol, th);
                            draw->AddLine(p1, p3, boxCol, th);
                        };

                        DrawCorner(pMin, ImVec2(pMin.x + lineW, pMin.y), ImVec2(pMin.x, pMin.y + lineH));
                        DrawCorner(ImVec2(pMax.x, pMin.y), ImVec2(pMax.x - lineW, pMin.y), ImVec2(pMax.x, pMin.y + lineH));
                        DrawCorner(ImVec2(pMin.x, pMax.y), ImVec2(pMin.x + lineW, pMax.y), ImVec2(pMin.x, pMax.y - lineH));
                        DrawCorner(pMax, ImVec2(pMax.x - lineW, pMax.y), ImVec2(pMax.x, pMax.y - lineH));
                    }
                }

                // 4. Имя
                if (drawName) {
                    ImVec2 txtSz = ImGui::CalcTextSize(e.name);
                    ImVec2 namePos(centerX - txtSz.x * 0.5f, box.top - txtSz.y - 2.0f);
                    draw->AddText(ImVec2(namePos.x + 1, namePos.y + 1), IM_COL32(0, 0, 0, 255), e.name);
                    draw->AddText(namePos, IM_COL32(255, 255, 255, 255), e.name);
                }

                // 5. Дистанция
                if (drawDistance) {
                    char distBuf[32];
                    snprintf(distBuf, sizeof(distBuf), "[%.1fm]", dist);
                    ImVec2 dSz = ImGui::CalcTextSize(distBuf);
                    ImVec2 distPos(centerX - dSz.x * 0.5f, box.bottom + 2.0f);
                    draw->AddText(ImVec2(distPos.x + 1, distPos.y + 1), IM_COL32(0, 0, 0, 255), distBuf);
                    draw->AddText(distPos, IM_COL32(210, 215, 225, 255), distBuf);
                }
            }
        }
    }

    void DrawMenuCard() {}
}