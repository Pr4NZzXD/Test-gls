#include "speedrun.h"
#include "il2cpp_api.h"
#include "auto_farm.h"
#include "menu.h"
#include "config.h"
#include "logger.h"
#include "imgui.h"
#include "ui_widgets.h"
#include "Aim.h" 

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <dlfcn.h>
#include <cstdlib> 
#include <ctime>   

namespace config { std::string GetActiveConfigPath(); }

namespace speedrun {
    bool autoUnlockShop = false;
    bool restartButton = false;
    float restartPosX = -1.0f;
    float restartPosY = -1.0f;

    int  ratChoice = 0;
    int  momRoute = 0;
    int  vaseChoice = 0;
    int  vaseCount = 0;
    bool extraTraps = false;
    bool lavaMode = false;

    // State RNG Entity
    int grannySpawnChoice = 0;
    int grandpaSpawnChoice = 0;

    static bool s_AutoDone = false;
    static bool s_Applied = false;
    static float s_InGameTimer = 0.0f;
    static std::string s_Status = "";
    static std::string s_DumpStatus = "";
    static std::string s_SceneName = "";
    static float s_SceneTimer = 0.0f;

    static std::vector<EntitySpawnPos> grannyPositions = {
        {"Random", 0},
        {"Pos 1 (Basement)", 1},
        {"Pos 2 (Attic)", 2},
        {"Pos 3 (Kitchen)", 3},
        {"Pos 4 (Bedroom)", 4}
    };

    static std::vector<EntitySpawnPos> grandpaPositions = {
        {"Random", 0},
        {"Pos 1 (Living Room)", 1},
        {"Pos 2 (Garage)", 2},
        {"Pos 3 (Bathroom)", 3},
        {"Pos 4 (Yard)", 4}
    };

    const std::vector<EntitySpawnPos>& GetAvailableSpawnPos(EntityType type) {
        if (type == EntityType::GRANNY) return grannyPositions;
        if (type == EntityType::GRANDPA) return grandpaPositions;
        return grannyPositions;
    }

    void ClearCache() {
        s_AutoDone = false;
        s_Applied = false;
        s_InGameTimer = 0.0f;
        s_SceneName.clear();
        s_SceneTimer = 0.0f;
    }

    void Init() {
        ClearCache();
        srand((unsigned)time(0));
        Logger::Log("SPEEDRUN", LOG_OK, "speedrun::Init() -> Speedrun module initialized.");
    }

    static const char* kShopKeys[] = {
        "CC_BOUGHT", "CH_BOUGHT", "EXTRAS_BOUGHT", "EXTRAPS_BOUGHT", "FLASH_BOUGHT",
        "GRANDPA_BOUGHT", "HO_BOUGHT", "IM_BOUGHT", "NM_BOUGHT", "ROBO_BOUGHT",
        "SH_BOUGHT", "TRAP_BOUGHT", "VERC_BOUGHT", "SlendrinaUnlock"
    };

    int UnlockAllShop() {
        if (!il2cpp_string_new || !il2cpp_runtime_invoke || !il2cpp_object_unbox) return -1;
        void* pp = FindClass("UnityEngine", "PlayerPrefs");
        if (!pp) return -1;
        void* mGetInt = il2cpp_class_get_method_from_name(pp, "GetInt", 2);
        void* mSetInt = il2cpp_class_get_method_from_name(pp, "SetInt", 2);
        void* mSave   = il2cpp_class_get_method_from_name(pp, "Save", 0);
        if (!mGetInt || !mSetInt) return -1;

        int changed = 0;
        for (const char* key : kShopKeys) {
            void* kStr = il2cpp_string_new(key);
            if (!kStr) continue;

            int def = 0;
            void* exc = nullptr;
            void* gArgs[2] = { kStr, &def };
            void* boxed = il2cpp_runtime_invoke(mGetInt, nullptr, gArgs, &exc);
            int cur = 0;
            if (boxed && !exc) cur = *(int*)il2cpp_object_unbox(boxed);
            if (exc) continue;

            if (cur != 1) {
                int one = 1;
                exc = nullptr;
                void* sArgs[2] = { kStr, &one };
                il2cpp_runtime_invoke(mSetInt, nullptr, sArgs, &exc);
                if (!exc) changed++;
            }
        }

        if (changed > 0 && mSave) {
            void* exc = nullptr;
            il2cpp_runtime_invoke(mSave, nullptr, nullptr, &exc);
        }
        return changed;
    }

    static void* TypeObjOf(const char* cls) {
        void* k = FindClass("", cls);
        if (!k || !il2cpp_class_get_type || !il2cpp_type_get_object) return nullptr;
        return il2cpp_type_get_object(il2cpp_class_get_type(k));
    }

    static void* FirstInstance(const char* cls) {
        void* t = TypeObjOf(cls);
        if (!t) return nullptr;
        std::vector<void*> v = FindObjectsOfUnityType(t);
        for (void* o : v) if (o && IsNativeObjectAlive(o)) return o;
        return nullptr;
    }

    static size_t FieldOff(const char* cls, const char* field) {
        void* k = FindClass("", cls);
        if (!k || !il2cpp_class_get_field_from_name || !il2cpp_field_get_offset) return 0;
        void* f = il2cpp_class_get_field_from_name(k, field);
        return f ? il2cpp_field_get_offset(f) : 0;
    }

    static void* ReadPtr(void* obj, const char* cls, const char* field) {
        size_t off = FieldOff(cls, field);
        if (!obj || !off) return nullptr;
        return *(void**)((uintptr_t)obj + off);
    }

    static bool WritePtr(void* obj, const char* cls, const char* field, void* value) {
        size_t off = FieldOff(cls, field);
        if (!obj || !off) return false;
        *(void**)((uintptr_t)obj + off) = value;
        return true;
    }

    static bool WriteBool(void* obj, const char* cls, const char* field, bool value) {
        size_t off = FieldOff(cls, field);
        if (!obj || !off) return false;
        *(uint8_t*)((uintptr_t)obj + off) = value ? 1 : 0;
        return true;
    }

    static bool Alive(void* o) { return o && IsNativeObjectAlive(o); }

    // Logika Pengaturan RNG Entitas berdasarkan class EnemyController[span_1](start_span)[span_1](end_span)
    static void ApplyEntitySpawnPositions(void* ec) {
        if (!ec) return;

        // Atur posisi Granny menggunakan field Pos1Granny hingga Pos4Granny[span_2](start_span)[span_2](end_span)
        if (grannySpawnChoice > 0) {
            std::string fieldName = "Pos" + std::to_string(grannySpawnChoice) + "Granny";
            void* targetTransform = ReadPtr(ec, "EnemyController", fieldName.c_str());
            if (Alive(targetTransform)) {
                WritePtr(ec, "EnemyController", "Pos1Granny", targetTransform);
                WritePtr(ec, "EnemyController", "Pos2Granny", targetTransform);
                WritePtr(ec, "EnemyController", "Pos3Granny", targetTransform);
                WritePtr(ec, "EnemyController", "Pos4Granny", targetTransform);
                Logger::Log("SPEEDRUN", LOG_OK, "Granny spawn locked to: %s", fieldName.c_str());
            }
        }

        // Atur posisi Grandpa menggunakan field Pos1Grandpa hingga Pos4Grandpa[span_3](start_span)[span_3](end_span)
        if (grandpaSpawnChoice > 0) {
            std::string fieldName = "Pos" + std::to_string(grandpaSpawnChoice) + "Grandpa";
            void* targetTransform = ReadPtr(ec, "EnemyController", fieldName.c_str());
            if (Alive(targetTransform)) {
                WritePtr(ec, "EnemyController", "Pos1Grandpa", targetTransform);
                WritePtr(ec, "EnemyController", "Pos2Grandpa", targetTransform);
                WritePtr(ec, "EnemyController", "Pos3Grandpa", targetTransform);
                WritePtr(ec, "EnemyController", "Pos4Grandpa", targetTransform);
                Logger::Log("SPEEDRUN", LOG_OK, "Grandpa spawn locked to: %s", fieldName.c_str());
            }
        }
    }

    static void ApplyRat(void* om) {
        if (ratChoice < 1 || ratChoice > 2) return;
        void* r1 = ReadPtr(om, "ObjectsManager", "RemoteRatHang1");
        void* r2 = ReadPtr(om, "ObjectsManager", "RemoteRatHang2");
        if (!Alive(r1) || !Alive(r2)) return;
        WriteBool(r1, "RemoteRat", "TheOne", ratChoice == 1);
        WriteBool(r2, "RemoteRat", "TheOne", ratChoice == 2);
    }

    static void ApplyVase(void* om) {
        void* arr = ReadPtr(om, "ObjectsManager", "VasePositionsGrandpa");
        if (!arr) return;
        int len = (int)*(uint64_t*)((uintptr_t)arr + 0x18);
        if (len > 0) vaseCount = len;   
        if (vaseChoice < 1) return;

        void* vase = ReadPtr(om, "ObjectsManager", "GrandpaVase");
        int idx = vaseChoice - 1;
        if (!Alive(vase) || idx >= len) return;
        void* posTr = ((void**)((uintptr_t)arr + 0x20))[idx];
        if (!Alive(posTr) || !oGameObjectGetTransform || !oTransformGetPosition || !oTransformSetPosition) return;

        void* vaseTr = oGameObjectGetTransform(vase);
        if (!Alive(vaseTr)) return;
        Vector3 p{ 0, 0, 0 };
        oTransformGetPosition(posTr, &p);
        oTransformSetPosition(vaseTr, &p);
    }

    static void ApplyMomSpider() {
        if (momRoute < 1 || momRoute > 2) return;
        void* comp = FirstInstance("AI_MomSpider");
        if (!Alive(comp)) return;

        void* run1 = ReadPtr(comp, "AI_MomSpider", "Run1");
        void* run2 = ReadPtr(comp, "AI_MomSpider", "Run2");
        if (!run1 || !run2) return;

        if (momRoute == 1) WritePtr(comp, "AI_MomSpider", "Run2", run1);
        else               WritePtr(comp, "AI_MomSpider", "Run1", run2);
    }

    static void ApplyEffects() {
        if (!extraTraps && !lavaMode) return;
        void* sem = FirstInstance("SpecialEffectsManager");
        if (!sem) return;

        if (extraTraps) {
            void* go = ReadPtr(sem, "SpecialEffectsManager", "extraTraps");
            if (Alive(go) && oSetGameObjectActive) oSetGameObjectActive(go, true);
        }
        if (lavaMode) {
            WriteBool(sem, "SpecialEffectsManager", "lavaAtmosphereOn", true);
            void* rise = ReadPtr(sem, "SpecialEffectsManager", "lavaRise");
            if (Alive(rise) && oSetGameObjectActive) oSetGameObjectActive(rise, true);
        }
    }

    static void ApplyOverrides() {
        void* om = FirstInstance("ObjectsManager");
        if (om) {
            ApplyRat(om);
            ApplyVase(om);
        }
        
        void* ec = FirstInstance("EnemyController");
        if (ec) {
            ApplyEntitySpawnPositions(ec);
        }

        ApplyMomSpider();
        ApplyEffects();
    }

    typedef float (*GetTimeScale_t)();
    static float CurrentTimeScale() {
        static GetTimeScale_t getTS = nullptr;
        static bool resolved = false;
        if (!resolved) {
            if (il2cpp_resolve_icall) getTS = (GetTimeScale_t)il2cpp_resolve_icall("UnityEngine.Time::get_timeScale");
            resolved = true;
        }
        return getTS ? getTS() : 1.0f;
    }

    bool IsPaused() { return CurrentTimeScale() < 0.001f; }

    bool IsGameplay() {
        static double s_last = -1.0;
        static bool s_val = false;
        double now = ImGui::GetTime();
        if (s_last < 0.0 || now - s_last > 0.5) {
            s_last = now;
            s_val = (auto_farm::GetCurrentSceneName() == "Scene");
        }
        return s_val;
    }

    void RestartRun(bool allowWhilePaused) {
        if (!allowWhilePaused && IsPaused()) return;
        
        aim::ClearCache(); 

        void* pausedCls = FindClass("", "Paused");
        void* inst = FirstInstance("Paused");
        void* method = pausedCls ? il2cpp_class_get_method_from_name(pausedCls, "RestartP", 0) : nullptr;
        if (inst && method) {
            void* exc = nullptr;
            il2cpp_runtime_invoke(method, inst, nullptr, &exc);
            if (!exc) return;
        }
        if (oSetTimeScale) oSetTimeScale(1.0f);
        auto_farm::LoadSceneByName("Scene");
    }

    static void DoRestart() { RestartRun(false); }

    void DrawRestartButton() {
        if (!restartButton) return;
        if (s_SceneName != "Scene" || g_ShowMenu) return;               

        float sc = menuscale::menuscale;
        float size = 46.0f * sc;
        ImVec2 disp = ImGui::GetIO().DisplaySize;

        if (restartPosX < 0.0f || restartPosY < 0.0f) {
            restartPosX = disp.x - size - 24.0f * sc;
            restartPosY = 110.0f * sc;
        }
        restartPosX = ImClamp(restartPosX, 0.0f, ImMax(0.0f, disp.x - size));
        restartPosY = ImClamp(restartPosY, 0.0f, ImMax(0.0f, disp.y - size));

        static bool s_Moved = false;
        ImVec2 bmin(restartPosX, restartPosY), bmax(restartPosX + size, restartPosY + size);

        ImGui::SetNextWindowPos(bmin);
        ImGui::SetNextWindowSize(ImVec2(size, size));
        ImGui::Begin("##sr_restart", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBackground);
        bool pressed = ImGui::InvisibleButton("##sr_restart_btn", ImVec2(size, size));
        bool held = ImGui::IsItemActive();
        if (held && ImGui::IsMouseDragging(0, 8.0f * sc)) {
            restartPosX += ImGui::GetIO().MouseDelta.x;
            restartPosY += ImGui::GetIO().MouseDelta.y;
            s_Moved = true;
        }
        if (pressed && !s_Moved) DoRestart();
        if (!ImGui::IsMouseDown(0)) s_Moved = false;
        ImGui::End();

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec4 ac = c::accent;
        float round = size * 0.34f;
        dl->AddRectFilled(bmin, bmax, IM_COL32(6, 6, 9, held ? 255 : 215), round);
        ImVec4 ring = ac; ring.w = held ? 1.0f : 0.65f;
        dl->AddRect(bmin, bmax, ImGui::GetColorU32(ring), round, 0, 1.8f * sc);

        ImVec2 ctr((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f);
        float r = size * 0.22f;
        ImU32 col = IM_COL32(235, 235, 245, 255);
        dl->PathArcTo(ctr, r, -1.2f, 4.2f, 28);
        dl->PathStroke(col, 0, 2.6f * sc);
        float a = 4.2f;
        ImVec2 tip(ctr.x + cosf(a) * r, ctr.y + sinf(a) * r);
        ImVec2 dir(-sinf(a), cosf(a));
        ImVec2 nrm(cosf(a), sinf(a));
        float hh = 5.5f * sc;
        dl->AddTriangleFilled(
            ImVec2(tip.x + dir.x * hh, tip.y + dir.y * hh),
            ImVec2(tip.x - dir.x * hh * 0.2f + nrm.x * hh * 0.8f, tip.y - dir.y * hh * 0.2f + nrm.y * hh * 0.8f),
            ImVec2(tip.x - dir.x * hh * 0.2f - nrm.x * hh * 0.8f, tip.y - dir.y * hh * 0.2f - nrm.y * hh * 0.8f),
            col);
    }

    void Update() {
        float dt = ImGui::GetIO().DeltaTime;

        s_SceneTimer += dt;
        if (s_SceneName.empty() || s_SceneTimer > 0.5f) {
            s_SceneName = auto_farm::GetCurrentSceneName();
            s_SceneTimer = 0.0f;
        }

        if (autoUnlockShop && !s_AutoDone) {
            s_AutoDone = true;
            int n = UnlockAllShop();
            if (n > 0) s_Status = "Auto-unlock: " + std::to_string(n) + " item(s) unlocked";
        }

        if (s_SceneName == "Scene") {
            s_InGameTimer += dt;
            if (!s_Applied && s_InGameTimer >= 1.5f) {
                s_Applied = true;
                ApplyOverrides();
            }
        } else {
            s_InGameTimer = 0.0f;
            s_Applied = false; 
        }
    }

    static const char* kVaseNames[9] = {
        "Shed", "Crow Room", "Sewer Exit Room", "Spider Room", "Sewer Drain",
        "Hidden Closet", "Old Dining Room Table", "Bookshelf Room", "Bedroom 1"
    };

    void DrawMenu(float colW, float colH, float colGap) {
        float sc = menuscale::menuscale;

        ImGui::BeginChild("##sr0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Shop & Utility");
        ImGui::Separator();
        ImGui::Spacing();

        edited::checkbox("Auto-Unlock Shop Items", &autoUnlockShop);
        if (edited::buttonn("Unlock All Shop Now", ImVec2(-1, 38 * sc))) {
            int n = UnlockAllShop();
            if (n < 0) s_Status = "Failed to unlock shop items";
            else if (n == 0) s_Status = "All shop items are already unlocked";
            else s_Status = std::to_string(n) + " shop item(s) unlocked";
        }
        if (!s_Status.empty()) ImGui::TextWrapped("%s", s_Status.c_str());

        ImGui::Spacing();
        edited::checkbox("Restart Button", &restartButton);
        if (edited::buttonn("Reset Button Position", ImVec2(-1, 38 * sc))) {
            restartPosX = -1.0f;
            restartPosY = -1.0f;
        }

        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Game Extras");
        ImGui::Separator();
        ImGui::Spacing();
        edited::checkbox("Extra Traps", &extraTraps);
        edited::checkbox("Lava Mode", &lavaMode);
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);

        ImGui::BeginChild("##sr1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "RNG & Spawn Control");
        ImGui::Separator();
        ImGui::Spacing();

        // Menu Dropdown Granny RNG
        const auto& gPos = GetAvailableSpawnPos(EntityType::GRANNY);
        std::vector<const char*> gItems;
        for (const auto& p : gPos) gItems.push_back(p.name.c_str());
        edited::combo("Granny Location", &grannySpawnChoice, gItems.data(), gItems.size());

        // Menu Dropdown Grandpa RNG
        const auto& gpPos = GetAvailableSpawnPos(EntityType::GRANDPA);
        std::vector<const char*> gpItems;
        for (const auto& p : gpPos) gpItems.push_back(p.name.c_str());
        edited::combo("Grandpa Location", &grandpaSpawnChoice, gpItems.data(), gpItems.size());

        ImGui::Spacing();

        static const char* ratItems[] = { "Random", "Left", "Right" };
        static const char* momItems[] = { "Random", "Tunnel", "Elevator" };
        edited::combo("Door-Opening Rat", &ratChoice, ratItems, 3);
        edited::combo("Spider Mom Route", &momRoute, momItems, 3);

        static std::vector<std::string> s_VaseLabels;
        static std::vector<const char*> s_VasePtrs;
        static int s_VaseBuiltFor = -1;
        int n = (vaseCount > 0) ? vaseCount : 9;
        if (s_VaseBuiltFor != n) {
            s_VaseLabels.clear();
            s_VaseLabels.push_back("Random");
            for (int i = 0; i < n; i++) {
                if (n == 9) s_VaseLabels.push_back(kVaseNames[i]);
                else s_VaseLabels.push_back("Position " + std::to_string(i + 1));
            }
            s_VasePtrs.clear();
            for (auto& s : s_VaseLabels) s_VasePtrs.push_back(s.c_str());
            s_VaseBuiltFor = n;
        }
        if (vaseChoice > n) vaseChoice = 0;
        edited::combo("Grandpa Vase Location", &vaseChoice, s_VasePtrs.data(), (int)s_VasePtrs.size());

        ImGui::Spacing();
        ImGui::TextWrapped("%s", "RNG choices apply 1.5 seconds after a run starts. Change them, then restart the run.");
        ImGui::EndChild();
    }
}
