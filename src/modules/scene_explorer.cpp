#include "scene_explorer.h"
#include "il2cpp_api.h"
#include "auto_farm.h"
#include "menu.h"
#include "imgui.h"

#include <vector>
#include <unordered_set>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace scene_explorer {
    static int g_ExplorerCategory = 0; // 0 = Tree, 1 = Search Object, 2 = Search Component
    static void* g_SelectedTransform = nullptr;
    static void* g_SelectedGameObject = nullptr;
    static void* g_InspectedComponent = nullptr;
    static void* g_LastInspectedComp = nullptr;
    static int g_InspectorSubTab = 0; // 0 = Fields, 1 = Methods

    static bool g_ShowInheritedMembers = false; // По умолчанию скрываем 300 мусорных методов System.Object!
    static char g_SearchFilter[64] = "";
    static char g_ComponentSearchFilter[64] = "";
    static char g_MemberFilter[64] = "";
    static float s_CopiedFeedbackTimer = 0.0f;
    static std::string s_InvokeStatus = "";
    static float s_InvokeStatusTimer = 0.0f;

    static std::unordered_set<void*> g_LockedDisabledComponents;
    static std::unordered_set<void*> g_CustomHighlightedObjects;

    struct SearchResult {
        std::string name;
        std::string fullPath;
        void* transform;
        void* gameObject;
        int rank;
    };
    static std::vector<SearchResult> g_SearchResults;

    struct ComponentSearchResult {
        std::string className;
        std::string gameObjectName;
        void* component;
        void* gameObject;
        void* transform;
    };
    static std::vector<ComponentSearchResult> g_ComponentSearchResults;

    // Кеш методов
    struct MethodItem {
        std::string className;
        std::string name;
        uint32_t paramCount;
        void* methodPtr;
        bool isGameScript;
    };
    static std::vector<MethodItem> s_AllMethods;

    // Кеш полей
    struct FieldItem {
        std::string className;
        std::string name;
        size_t offset;
        std::string typeName;
        int typeEnum;
    };
    static std::vector<FieldItem> s_AllFields;

    void ClearCache() {
        g_SearchResults.clear();
        g_ComponentSearchResults.clear();
        g_LockedDisabledComponents.clear();
        g_CustomHighlightedObjects.clear();
        g_SelectedTransform = nullptr;
        g_SelectedGameObject = nullptr;
        g_InspectedComponent = nullptr;
        g_LastInspectedComp = nullptr;
        s_AllMethods.clear();
        s_AllFields.clear();
        g_MemberFilter[0] = '\0';
        s_InvokeStatus = "";
        s_InvokeStatusTimer = 0.0f;
    }

    void Update() {
        if (!IsSceneReady()) return;

        for (auto it = g_LockedDisabledComponents.begin(); it != g_LockedDisabledComponents.end(); ) {
            void* comp = *it;
            if (!comp || !IsNativeObjectAlive(comp)) {
                it = g_LockedDisabledComponents.erase(it);
            } else {
                if (oSetBehaviourEnabled) oSetBehaviourEnabled(comp, false);
                ++it;
            }
        }

        for (auto it = g_CustomHighlightedObjects.begin(); it != g_CustomHighlightedObjects.end(); ) {
            void* go = *it;
            if (!go || !IsNativeObjectAlive(go)) {
                it = g_CustomHighlightedObjects.erase(it);
            } else {
                float greenCol[4] = { 0.0f, 1.0f, 0.2f, 1.0f };
                SetOutlineOnObject(go, true, greenCol, 6.0f);
                ++it;
            }
        }
    }

    inline bool IsValidMemoryPtr(void* ptr) {
        uintptr_t addr = (uintptr_t)ptr;
        return (addr >= 0x10000 && (addr % sizeof(void*)) == 0);
    }

    std::string SafeReadMonoString(void* monoStr) {
        if (!IsValidMemoryPtr(monoStr)) return "null";
        uintptr_t ptr = (uintptr_t)monoStr;

        int32_t len = *(int32_t*)(ptr + 0x10);
        if (len <= 0 || len > 256) return "\"\"";

        uint16_t* chars = (uint16_t*)(ptr + 0x14);
        if (!IsValidMemoryPtr((void*)chars)) return "\"\"";

        std::string res = "";
        res.reserve(len);
        for (int32_t i = 0; i < len; i++) {
            if (chars[i] < 128 && chars[i] >= 32) res += (char)chars[i];
            else if (chars[i] < 128) res += ' ';
            else res += '?';
        }
        return res;
    }

    const char* GetSafeTypeName(void* type) {
        if (!IsValidMemoryPtr(type) || !il2cpp_type_get_type) return "var";
        int t = il2cpp_type_get_type(type);
        switch (t) {
            case 0x01: return "void";
            case 0x02: return "bool";
            case 0x03: return "char";
            case 0x04: return "sbyte";
            case 0x05: return "byte";
            case 0x06: return "short";
            case 0x07: return "ushort";
            case 0x08: return "int";
            case 0x09: return "uint";
            case 0x0a: return "long";
            case 0x0b: return "ulong";
            case 0x0c: return "float";
            case 0x0d: return "double";
            case 0x0e: return "string";
            case 0x0f: return "pointer";
            case 0x11: return "struct";
            case 0x12: return "class";
            case 0x14: return "Array";
            case 0x1c: return "object";
            case 0x1d: return "Array";
            default:   return "var";
        }
    }

    // ==============================================================
    // БЕЗОПАСНЫЙ СБОР ПОЛЕЙ И МЕТОДОВ
    // ==============================================================
    void InspectComponentMembers(void* comp) {
        s_AllMethods.clear();
        s_AllFields.clear();

        if (!comp || !IsNativeObjectAlive(comp)) return;

        void* startKlass = *(void**)comp;
        if (!IsValidMemoryPtr(startKlass)) return;

        void* curKlass = startKlass;
        int depth = 0;
        std::unordered_set<std::string> seenMethods;
        std::unordered_set<std::string> seenFields;

        while (curKlass && IsValidMemoryPtr(curKlass) && depth++ < 6) {
            const char* clsName = il2cpp_class_get_name ? il2cpp_class_get_name(curKlass) : "Class";
            if (!clsName || !IsValidMemoryPtr((void*)clsName)) clsName = "Class";

            bool isUnityBase = (strcmp(clsName, "MonoBehaviour") == 0 ||
                                strcmp(clsName, "Behaviour") == 0 ||
                                strcmp(clsName, "Component") == 0 ||
                                strcmp(clsName, "Object") == 0);

            // 1. Сбор методов
            if (il2cpp_class_get_methods && il2cpp_method_get_name) {
                void* iter = nullptr;
                int count = 0;
                while (void* method = il2cpp_class_get_methods(curKlass, &iter)) {
                    if (++count > 250 || !IsValidMemoryPtr(method)) break;

                    const char* mName = il2cpp_method_get_name(method);
                    if (!mName || !IsValidMemoryPtr((void*)mName)) continue;
                    if (mName[0] == '\0' || mName[0] == '<' || mName[0] == '$') continue;

                    uint32_t pCount = il2cpp_method_get_param_count ? il2cpp_method_get_param_count(method) : 0;
                    if (pCount > 32) continue; // Защита от мусорных счетчиков

                    std::string sig = std::string(mName) + "_" + std::to_string(pCount);

                    if (seenMethods.find(sig) == seenMethods.end()) {
                        seenMethods.insert(sig);
                        s_AllMethods.push_back({ clsName, mName, pCount, method, !isUnityBase });
                    }
                }
            }

            // 2. Сбор полей
            if (il2cpp_class_get_fields && il2cpp_field_get_name && il2cpp_field_get_offset) {
                void* iter = nullptr;
                int count = 0;
                while (void* field = il2cpp_class_get_fields(curKlass, &iter)) {
                    if (++count > 250 || !IsValidMemoryPtr(field)) break;

                    const char* fName = il2cpp_field_get_name(field);
                    if (!fName || !IsValidMemoryPtr((void*)fName) || fName[0] == '\0') continue;

                    size_t off = il2cpp_field_get_offset(field);
                    if (off < 0x10 || off > 0x2500) continue; // Безопасные смещения инстанса

                    if (seenFields.find(fName) == seenFields.end()) {
                        seenFields.insert(fName);
                        void* fType = il2cpp_field_get_type ? il2cpp_field_get_type(field) : nullptr;
                        int tEnum = (fType && il2cpp_type_get_type && IsValidMemoryPtr(fType)) ? il2cpp_type_get_type(fType) : -1;
                        s_AllFields.push_back({ clsName, fName, off, GetSafeTypeName(fType), tEnum });
                    }
                }
            }

            curKlass = il2cpp_class_get_parent ? il2cpp_class_get_parent(curKlass) : nullptr;
        }
    }

    void PerformComponentSearch() {
        g_ComponentSearchResults.clear();
        if (g_ComponentSearchFilter[0] == '\0') return;

        void* targetKlass = FindClass("", g_ComponentSearchFilter);
        if (!targetKlass) targetKlass = FindClass("UnityEngine", g_ComponentSearchFilter);
        if (!targetKlass) targetKlass = FindClass("UnityEngine.UI", g_ComponentSearchFilter);

        if (!targetKlass || !il2cpp_class_get_type || !il2cpp_type_get_object) return;

        void* typeObj = il2cpp_type_get_object(il2cpp_class_get_type(targetKlass));
        if (!typeObj) return;

        std::vector<void*> comps = FindObjectsOfUnityType(typeObj);
        for (void* comp : comps) {
            if (!comp || !IsNativeObjectAlive(comp)) continue;

            void* go = oComponentGetGameObject ? oComponentGetGameObject(comp) : nullptr;
            void* tr = oComponentGetTransform ? oComponentGetTransform(comp) : nullptr;
            std::string goName = go ? GetUnityObjectName(go) : "<No GO>";
            const char* cName = il2cpp_class_get_name ? il2cpp_class_get_name(*(void**)comp) : g_ComponentSearchFilter;

            g_ComponentSearchResults.push_back({ cName, goName, comp, go, tr });
        }
    }

    void PerformRankedSearch() {
        g_SearchResults.clear();
        if (!IsSceneReady() || !g_TransformTypeObj || g_SearchFilter[0] == '\0') return;

        std::vector<void*> allTransforms = FindObjectsOfUnityType(g_TransformTypeObj);
        std::string query = g_SearchFilter;
        std::transform(query.begin(), query.end(), query.begin(), ::tolower);

        for (void* tr : allTransforms) {
            if (!tr || !IsNativeObjectAlive(tr)) continue;
            std::string name = GetUnityObjectName(tr);
            std::string lName = name;
            std::transform(lName.begin(), lName.end(), lName.begin(), ::tolower);

            int rank = -1;
            if (lName == query) rank = 0;
            else if (lName.rfind(query, 0) == 0) rank = 1;
            else if (lName.find(query) != std::string::npos) rank = 2;

            if (rank != -1) {
                void* go = oComponentGetGameObject ? oComponentGetGameObject(tr) : nullptr;
                g_SearchResults.push_back({ name, GetTransformPath(tr), tr, go, rank });
            }
        }

        std::sort(g_SearchResults.begin(), g_SearchResults.end(), [](const SearchResult& a, const SearchResult& b) {
            if (a.rank != b.rank) return a.rank < b.rank;
            return a.name.length() < b.name.length();
        });
    }

    void DrawHierarchyTreeNode(void* tr, void*& nextTr, void*& nextGo, int depth) {
        if (!tr || !IsNativeObjectAlive(tr) || depth > 12) return;

        std::string name = GetUnityObjectName(tr);
        if (name.empty()) name = "<Unnamed>";

        int childCount = SafeGetChildCount(tr);
        bool isSelected = (g_SelectedTransform == tr);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (childCount == 0) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::PushID((void*)tr);
        bool nodeOpen = ImGui::TreeNodeEx((void*)(uintptr_t)tr, flags, "%s", name.c_str());

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            nextTr = tr;
            nextGo = oComponentGetGameObject ? oComponentGetGameObject(tr) : nullptr;
            g_InspectedComponent = nullptr;
        }

        if (nodeOpen && childCount > 0) {
            for (int i = 0; i < childCount; i++) {
                void* cTr = SafeGetChild(tr, i);
                if (cTr && IsNativeObjectAlive(cTr)) {
                    DrawHierarchyTreeNode(cTr, nextTr, nextGo, depth + 1);
                }
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    // ==============================================================
    // БЕЗОПАСНЫЙ ИНСПЕКТОР (ПОЛЯ + МЕТОДЫ БЕЗ ВЫЛЕТОВ)
    // ==============================================================
    void DrawComponentInspector(void* comp) {
        if (!comp || !IsNativeObjectAlive(comp)) {
            g_InspectedComponent = nullptr;
            return;
        }

        if (comp != g_LastInspectedComp) {
            InspectComponentMembers(comp);
            g_LastInspectedComp = comp;
        }

        void* startKlass = *(void**)comp;
        const char* className = il2cpp_class_get_name ? il2cpp_class_get_name(startKlass) : "UnknownClass";
        ImVec4 accentColor = Theme::GetAccent();

        if (ImGui::Button(LOC(" < Назад к объекту ", " < Back to Object "), ImVec2(160 * g_UiScale, 28 * g_UiScale))) {
            g_InspectedComponent = nullptr;
            return;
        }

        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "C# Component: %s", className);
        ImGui::Spacing();

        // 2 вкладки: Поля и Методы
        float tabBtnW = (ImGui::GetContentRegionAvail().x - 10.0f * g_UiScale) * 0.5f;

        if (g_InspectorSubTab == 0) ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
        if (ImGui::Button(LOC("Поля (Fields)", "Fields"), ImVec2(tabBtnW, 26 * g_UiScale))) g_InspectorSubTab = 0;
        if (g_InspectorSubTab == 0) ImGui::PopStyleColor();

        ImGui::SameLine(0, 10.0f * g_UiScale);

        if (g_InspectorSubTab == 1) ImGui::PushStyleColor(ImGuiCol_Button, accentColor);
        if (ImGui::Button(LOC("Методы (Methods)", "Methods"), ImVec2(tabBtnW, 26 * g_UiScale))) g_InspectorSubTab = 1;
        if (g_InspectorSubTab == 1) ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##memberSearch", LOC("Поиск по имени...", "Filter name..."), g_MemberFilter, sizeof(g_MemberFilter));
        ImGui::Spacing();

        std::string filterStr = g_MemberFilter;
        std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

        // ==========================================================
        // 1. ВКЛАДКА FIELDS (ПОЛЯ)
        // ==========================================================
        if (g_InspectorSubTab == 0) {
            ImGui::BeginChild("##fieldsScrollArea", ImVec2(0, 0), true);

            int renderedFields = 0;
            for (size_t i = 0; i < s_AllFields.size(); i++) {
                auto& f = s_AllFields[i];

                if (!filterStr.empty()) {
                    std::string lF = f.name;
                    std::transform(lF.begin(), lF.end(), lF.begin(), ::tolower);
                    if (lF.find(filterStr) == std::string::npos) continue;
                }

                renderedFields++;
                ImGui::PushID((int)i);

                ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "[%s]", f.typeName.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", f.name.c_str());

                uintptr_t fieldAddr = (uintptr_t)comp + f.offset;

                if (f.typeEnum == 0x02) { // bool
                    bool val = *(bool*)fieldAddr;
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 70 * g_UiScale);
                    if (ImGui::Checkbox("##bVal", &val)) {
                        *(bool*)fieldAddr = val;
                    }
                }
                else if (f.typeEnum == 0x08) { // int
                    int32_t val = *(int32_t*)fieldAddr;
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130 * g_UiScale);
                    ImGui::SetNextItemWidth(125 * g_UiScale);
                    if (ImGui::InputInt("##iVal", &val)) {
                        *(int32_t*)fieldAddr = val;
                    }
                }
                else if (f.typeEnum == 0x0c) { // float
                    float val = *(float*)fieldAddr;
                    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 130 * g_UiScale);
                    ImGui::SetNextItemWidth(125 * g_UiScale);
                    if (ImGui::InputFloat("##fVal", &val, 0.0f, 0.0f, "%.2f")) {
                        *(float*)fieldAddr = val;
                    }
                }
                else if (f.typeEnum == 0x0e) { // string
                    void* monoStr = *(void**)fieldAddr;
                    std::string sVal = SafeReadMonoString(monoStr);
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.8f, 1.0f), "= \"%s\"", sVal.c_str());
                }
                else { // Object pointer
                    void* ptrVal = *(void**)fieldAddr;
                    ImGui::SameLine();
                    if (ptrVal) {
                        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "= (ptr: %p)", ptrVal);
                    } else {
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "= null");
                    }
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            if (renderedFields == 0) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LOC("Поля не найдены.", "No fields found."));
            }
            ImGui::EndChild();
        }
        // ==========================================================
        // 2. ВКЛАДКА METHODS (МЕТОДЫ С БЕЗОПАСНОЙ ВЁРСТКОЙ)
        // ==========================================================
        else if (g_InspectorSubTab == 1) {
            ImGui::Checkbox(LOC("Показать базовые методы движка (System/Unity)", "Show Base Engine Methods"), &g_ShowInheritedMembers);
            ImGui::BeginChild("##methodsScrollArea", ImVec2(0, 0), true);

            float winW = ImGui::GetWindowWidth();
            float btnW = 75.0f * g_UiScale;
            float btnTargetX = winW - btnW - 30.0f * g_UiScale;

            int renderedMethods = 0;
            for (size_t i = 0; i < s_AllMethods.size(); i++) {
                auto& m = s_AllMethods[i];
                // По умолчанию показываем ТОЛЬКО методы самого скрипта игры!
                if (!g_ShowInheritedMembers && !m.isGameScript) continue;

                if (!filterStr.empty()) {
                    std::string lM = m.name;
                    std::transform(lM.begin(), lM.end(), lM.begin(), ::tolower);
                    if (lM.find(filterStr) == std::string::npos) continue;
                }

                renderedMethods++;
                ImGui::PushID((int)i);
                ImGui::Bullet();

                ImGui::TextColored(m.isGameScript ? ImVec4(0.2f, 1.0f, 0.4f, 1.0f) : ImVec4(0.5f, 0.7f, 1.0f, 0.8f), "[%s]", m.className.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", m.name.c_str());

                // Безопасное выравнивание кнопки к правому краю
                if (ImGui::GetCursorPosX() < btnTargetX) {
                    ImGui::SameLine(btnTargetX);
                } else {
                    ImGui::SameLine();
                }

                if (m.paramCount == 0 && m.name[0] != '.') {
                    if (ImGui::Button(LOC("Вызвать", "Invoke"), ImVec2(btnW, 22 * g_UiScale))) {
                        void* exc = nullptr;
                        il2cpp_runtime_invoke(m.methodPtr, comp, nullptr, &exc);
                        if (!exc) {
                            s_InvokeStatus = std::string("✓ Вызван: ") + m.name;
                            s_InvokeStatusTimer = 3.0f;
                        } else {
                            s_InvokeStatus = std::string("❌ Ошибка: ") + m.name;
                            s_InvokeStatusTimer = 3.0f;
                        }
                    }
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%u args)", m.paramCount);
                }

                ImGui::Separator();
                ImGui::PopID();
            }

            if (renderedMethods == 0) {
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LOC("Методы скрипта не найдены (включи галочку для базовых).", "No script methods found."));
            }
            ImGui::EndChild();
        }
    }

    void DrawMenu() {
        if (!IsSceneReady()) {
            ClearCache();
            ImGui::TextColored(ImVec4(0.89f, 0.16f, 0.22f, 1.0f), "%s", LOC("Сцена загружается...", "Scene loading..."));
            return;
        }

        void* nextSelectedTransform = nullptr;
        void* nextSelectedGameObject = nullptr;

        ImGui::TextColored(Theme::GetAccent(), "%s", LOC("МОБИЛЬНЫЙ UNITY EXPLORER [DEV IL2CPP]", "MOBILE UNITY EXPLORER [DEV IL2CPP]"));
        ImGui::Spacing();

        // 3 режима
        float modeBtnW = (ImGui::GetContentRegionAvail().x - 16.0f * g_UiScale) / 3.0f;

        if (g_ExplorerCategory == 0) ImGui::PushStyleColor(ImGuiCol_Button, Theme::GetAccent());
        if (ImGui::Button(LOC(" [🌳] Дерево ", " [🌳] Hierarchy "), ImVec2(modeBtnW, 30.0f * g_UiScale))) g_ExplorerCategory = 0;
        if (g_ExplorerCategory == 0) ImGui::PopStyleColor();

        ImGui::SameLine(0, 8.0f * g_UiScale);

        if (g_ExplorerCategory == 1) ImGui::PushStyleColor(ImGuiCol_Button, Theme::GetAccent());
        if (ImGui::Button(LOC(" [🔍] Объекты ", " [🔍] GameObjects "), ImVec2(modeBtnW, 30.0f * g_UiScale))) g_ExplorerCategory = 1;
        if (g_ExplorerCategory == 1) ImGui::PopStyleColor();

        ImGui::SameLine(0, 8.0f * g_UiScale);

        if (g_ExplorerCategory == 2) ImGui::PushStyleColor(ImGuiCol_Button, Theme::GetAccent());
        if (ImGui::Button(LOC(" [⚡] C# Классы ", " [⚡] Components "), ImVec2(modeBtnW, 30.0f * g_UiScale))) g_ExplorerCategory = 2;
        if (g_ExplorerCategory == 2) ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Поиск объектов
        if (g_ExplorerCategory == 1) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 105.0f * g_UiScale);
            bool enter = ImGui::InputTextWithHint("##searchObj", LOC("Имя GameObject...", "GameObject Name..."), g_SearchFilter, sizeof(g_SearchFilter), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button(LOC(" Искать ", " Search "), ImVec2(95 * g_UiScale, 28 * g_UiScale)) || enter) {
                PerformRankedSearch();
            }
            ImGui::Spacing();
        }
        // Поиск компонентов по имени класса
        else if (g_ExplorerCategory == 2) {
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 105.0f * g_UiScale);
            bool enter = ImGui::InputTextWithHint("##searchComp", LOC("Имя C# Класса (PickRay, DoorRay)...", "Class Name..."), g_ComponentSearchFilter, sizeof(g_ComponentSearchFilter), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::SameLine();
            if (ImGui::Button(LOC(" Найти ", " Search "), ImVec2(95 * g_UiScale, 28 * g_UiScale)) || enter) {
                PerformComponentSearch();
            }
            ImGui::Spacing();
        }

        float availW = ImGui::GetContentRegionAvail().x;
        float leftPaneW = availW * 0.44f;
        float rightPaneW = availW - leftPaneW - 12.0f * g_UiScale;

        // Левая панель
        ImGui::BeginChild("##leftNavPane", ImVec2(leftPaneW, 0), true);

        if (g_ExplorerCategory == 0) {
            std::vector<void*> roots = GetRootTransforms();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s (%zu):", LOC("ИЕРАРХИЯ СЦЕНЫ", "SCENE TREE"), roots.size());
            ImGui::Spacing();
            for (void* rootTr : roots) {
                if (rootTr && IsNativeObjectAlive(rootTr)) {
                    DrawHierarchyTreeNode(rootTr, nextSelectedTransform, nextSelectedGameObject, 0);
                }
            }
        }
        else if (g_ExplorerCategory == 1) {
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s (%zu):", LOC("НАЙДЕНО ОБЪЕКТОВ", "FOUND OBJECTS"), g_SearchResults.size());
            ImGui::Spacing();
            for (size_t idx = 0; idx < g_SearchResults.size(); idx++) {
                auto& res = g_SearchResults[idx];
                if (!res.transform || !IsNativeObjectAlive(res.transform)) continue;

                bool isSelected = (g_SelectedTransform == res.transform);
                ImGui::PushID((int)idx);
                if (ImGui::Selectable(res.name.c_str(), isSelected, 0, ImVec2(0, 26.0f * g_UiScale))) {
                    nextSelectedTransform = res.transform;
                    nextSelectedGameObject = res.gameObject;
                    g_InspectedComponent = nullptr;
                }
                ImGui::PopID();
            }
        }
        else if (g_ExplorerCategory == 2) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s (%zu):", LOC("НАЙДЕНО КОМПОНЕНТОВ", "FOUND COMPONENTS"), g_ComponentSearchResults.size());
            ImGui::Spacing();
            for (size_t idx = 0; idx < g_ComponentSearchResults.size(); idx++) {
                auto& res = g_ComponentSearchResults[idx];
                if (!res.component || !IsNativeObjectAlive(res.component)) continue;

                ImGui::PushID((int)idx);
                std::string label = res.className + " -> [" + res.gameObjectName + "]";
                if (ImGui::Selectable(label.c_str(), g_InspectedComponent == res.component, 0, ImVec2(0, 26.0f * g_UiScale))) {
                    g_InspectedComponent = res.component;
                    g_LastInspectedComp = nullptr;
                    g_SelectedTransform = res.transform;
                    g_SelectedGameObject = res.gameObject;
                }
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
        ImGui::SameLine();

        // Правая панель
        ImGui::BeginChild("##rightInspectorPane", ImVec2(rightPaneW, 0), true);

        if (s_InvokeStatusTimer > 0.0f) {
            s_InvokeStatusTimer -= ImGui::GetIO().DeltaTime;
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", s_InvokeStatus.c_str());
            ImGui::Separator();
        }

        if (g_InspectedComponent && IsNativeObjectAlive(g_InspectedComponent)) {
            DrawComponentInspector(g_InspectedComponent);
        }
        else if (g_SelectedTransform && IsNativeObjectAlive(g_SelectedTransform)) {
            std::string objName = GetUnityObjectName(g_SelectedTransform);
            std::string fullPath = GetTransformPath(g_SelectedTransform);
            void* go = g_SelectedGameObject;

            ImGui::TextColored(Theme::GetAccent(), "%s", LOC("GAMEOBJECT ИНСПЕКТОР", "GAMEOBJECT INSPECTOR"));
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Name: %s", objName.c_str());

            ImGui::Text("Path:");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
            if (ImGui::Selectable(fullPath.c_str(), false)) {
                CopyToAndroidClipboard(fullPath.c_str());
                s_CopiedFeedbackTimer = 2.0f;
            }
            ImGui::PopStyleColor();

            if (s_CopiedFeedbackTimer > 0.0f) {
                s_CopiedFeedbackTimer -= ImGui::GetIO().DeltaTime;
                ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "✓ %s", LOC("Путь скопирован!", "Path copied!"));
            }

            Vector3 pos{ 0, 0, 0 };
            if (oTransformGetPosition) oTransformGetPosition(g_SelectedTransform, &pos);
            ImGui::Text("Pos: X: %.2f | Y: %.2f | Z: %.2f", pos.x, pos.y, pos.z);

            if (go && IsNativeObjectAlive(go)) {
                bool isAct = oGetGameObjectActive ? oGetGameObjectActive(go) : true;
                if (ImGui::Checkbox(LOC(" GameObject ActiveSelf ", " GameObject ActiveSelf "), &isAct)) {
                    if (oSetGameObjectActive) oSetGameObjectActive(go, isAct);
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOC("ПРИКРЕПЛЕННЫЕ C# КОМПОНЕНТЫ:", "ATTACHED C# COMPONENTS:"));

            if (go && IsNativeObjectAlive(go)) {
                std::vector<void*> comps = GetGameObjectComponents(go);
                for (size_t i = 0; i < comps.size(); i++) {
                    void* comp = comps[i];
                    if (!comp || !IsNativeObjectAlive(comp)) continue;

                    void* klass = *(void**)comp;
                    const char* cName = il2cpp_class_get_name ? il2cpp_class_get_name(klass) : "Component";

                    ImGui::PushID((int)i);
                    ImGui::Bullet();
                    ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.95f, 1.0f), "%s", cName);
                    ImGui::SameLine();

                    if (ImGui::Button(LOC("🔍 Инспектор", "🔍 Inspect"), ImVec2(90 * g_UiScale, 20 * g_UiScale))) {
                        g_InspectedComponent = comp;
                        g_LastInspectedComp = nullptr;
                        g_InspectorSubTab = 0;
                    }

                    if (oGetBehaviourEnabled && oSetBehaviourEnabled) {
                        bool en = oGetBehaviourEnabled(comp);
                        ImGui::SameLine();
                        if (ImGui::Checkbox("##cEn", &en)) {
                            oSetBehaviourEnabled(comp, en);
                        }
                    }
                    ImGui::PopID();
                }
            }
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "%s", LOC("Выберите объект слева или найдите класс через [⚡] C# Классы.", "Select an object or search a class."));
        }

        ImGui::EndChild();

        if (nextSelectedTransform && IsNativeObjectAlive(nextSelectedTransform)) {
            g_SelectedTransform = nextSelectedTransform;
            g_SelectedGameObject = nextSelectedGameObject;
        }
    }
}