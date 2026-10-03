#include "speedrun.h"
#include "il2cpp_api.h"
#include "auto_farm.h"
#include "menu.h"
#include "config.h"
#include "logger.h"
#include "imgui.h"
#include "ui_widgets.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>
#include <dlfcn.h>

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

    static bool s_AutoDone = false;
    static bool s_Applied = false;
    static float s_InGameTimer = 0.0f;
    static std::string s_Status = "";
    static std::string s_DumpStatus = "";
    static std::string s_SceneName = "";
    static float s_SceneTimer = 0.0f;

    void ClearCache() {
        s_AutoDone = false;
        s_Applied = false;
        s_InGameTimer = 0.0f;
        s_SceneName.clear();
        s_SceneTimer = 0.0f;
    }

    void Init() {
        ClearCache();
        Logger::Log("SPEEDRUN", LOG_OK, "speedrun::Init() -> Speedrun module initialized.");
    }

    // ==============================================================
    // 1. UNLOCK SEMUA ITEM SHOP (PlayerPrefs.SetInt(key, 1))
    // ==============================================================
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
        Logger::Log("SPEEDRUN", LOG_OK, "Shop unlock: %d item(s) newly unlocked.", changed);
        return changed;
    }

    // ==============================================================
    // HELPER AKSES OBJEK/FIELD IL2CPP
    // ==============================================================
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

    // ==============================================================
    // 2. KONTROL RNG + EKSTRA (diterapkan sekali setelah run dimulai)
    // ==============================================================
    static void ApplyRat(void* om) {
        if (ratChoice < 1 || ratChoice > 2) return;
        void* r1 = ReadPtr(om, "ObjectsManager", "RemoteRatHang1");
        void* r2 = ReadPtr(om, "ObjectsManager", "RemoteRatHang2");
        if (!Alive(r1) || !Alive(r2)) {
            Logger::Log("SPEEDRUN", LOG_WARN, "Remote rat: RemoteRatHang1/2 tidak ditemukan.");
            return;
        }
        WriteBool(r1, "RemoteRat", "TheOne", ratChoice == 1);
        WriteBool(r2, "RemoteRat", "TheOne", ratChoice == 2);
        Logger::Log("SPEEDRUN", LOG_OK, "Remote rat TheOne -> RemoteRatHang%d", ratChoice);
    }

    static void ApplyVase(void* om) {
        void* arr = ReadPtr(om, "ObjectsManager", "VasePositionsGrandpa");
        if (!arr) return;
        int len = (int)*(uint64_t*)((uintptr_t)arr + 0x18);
        if (len > 0) vaseCount = len;   // simpan jumlah posisi untuk label dropdown
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
        Logger::Log("SPEEDRUN", LOG_OK, "Vase dipindah ke posisi %d dari %d", vaseChoice, len);
    }

    static void ApplyMomSpider() {
        if (momRoute < 1 || momRoute > 2) return;

        void* comp = nullptr;
        void* esc = FirstInstance("Escapes");
        if (esc && g_GetComponentMethod) {
            void* go = ReadPtr(esc, "Escapes", "MomSpider");   // GameObject (bisa nonaktif)
            void* typeObj = TypeObjOf("AI_MomSpider");
            if (Alive(go) && typeObj) {
                void* exc = nullptr;
                void* args[1] = { typeObj };
                comp = il2cpp_runtime_invoke(g_GetComponentMethod, go, args, &exc);
                if (exc) comp = nullptr;
            }
        }
        if (!Alive(comp)) comp = FirstInstance("AI_MomSpider");
        if (!Alive(comp)) {
            Logger::Log("SPEEDRUN", LOG_WARN, "Spider Mom: komponen AI_MomSpider tidak ditemukan.");
            return;
        }

        void* run1 = ReadPtr(comp, "AI_MomSpider", "Run1");
        void* run2 = ReadPtr(comp, "AI_MomSpider", "Run2");
        if (!run1 || !run2) return;

        // Dua-duanya diarahkan ke titik yang sama, sehingga pilihan acak game tidak berpengaruh
        if (momRoute == 1) WritePtr(comp, "AI_MomSpider", "Run2", run1);
        else               WritePtr(comp, "AI_MomSpider", "Run1", run2);
        Logger::Log("SPEEDRUN", LOG_OK, "Spider Mom route -> %s", momRoute == 1 ? "Run1 (Tunnel)" : "Run2 (Elevator)");
    }

    static void ApplyEffects() {
        if (!extraTraps && !lavaMode) return;
        void* sem = FirstInstance("SpecialEffectsManager");
        if (!sem) {
            Logger::Log("SPEEDRUN", LOG_WARN, "SpecialEffectsManager tidak ditemukan.");
            return;
        }
        if (extraTraps) {
            void* go = ReadPtr(sem, "SpecialEffectsManager", "extraTraps");
            if (Alive(go) && oSetGameObjectActive) {
                oSetGameObjectActive(go, true);
                Logger::Log("SPEEDRUN", LOG_OK, "Extra Traps diaktifkan.");
            }
        }
        if (lavaMode) {
            WriteBool(sem, "SpecialEffectsManager", "lavaAtmosphereOn", true);
            void* rise = ReadPtr(sem, "SpecialEffectsManager", "lavaRise");
            if (Alive(rise) && oSetGameObjectActive) {
                oSetGameObjectActive(rise, true);
                Logger::Log("SPEEDRUN", LOG_OK, "Lava Mode diaktifkan (lavaRise).");
            }
        }
    }

    static void ApplyOverrides() {
        void* om = FirstInstance("ObjectsManager");
        if (om) {
            ApplyRat(om);
            ApplyVase(om);
        } else {
            Logger::Log("SPEEDRUN", LOG_WARN, "ObjectsManager tidak ditemukan.");
        }
        ApplyMomSpider();
        ApplyEffects();
    }

    // ==============================================================
    // 3. TOMBOL RESTART MENGAMBANG (memanggil Paused.RestartP())
    // ==============================================================
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
        // Tombol melayang tidak berbuat apa pun saat game di-pause (sama seperti versi desktop);
        // tombol restart di popup justru dipakai saat pause, jadi diizinkan.
        if (!allowWhilePaused && IsPaused()) return;

        void* pausedCls = FindClass("", "Paused");
        void* inst = FirstInstance("Paused");
        void* method = pausedCls ? il2cpp_class_get_method_from_name(pausedCls, "RestartP", 0) : nullptr;
        if (inst && method) {
            void* exc = nullptr;
            il2cpp_runtime_invoke(method, inst, nullptr, &exc);
            if (!exc) {
                Logger::Log("SPEEDRUN", LOG_OK, "Restart: Paused.RestartP() called.");
                return;
            }
        }
        // Cadangan: muat ulang scene
        if (oSetTimeScale) oSetTimeScale(1.0f);
        auto_farm::LoadSceneByName("Scene");
        Logger::Log("SPEEDRUN", LOG_WARN, "Restart: Paused.RestartP() failed, reloading scene.");
    }

    static void DoRestart() { RestartRun(false); }

    void DrawRestartButton() {
        if (!restartButton) return;
        if (s_SceneName != "Scene") return;   // hanya tampil saat gameplay
        if (g_ShowMenu) return;               // sembunyi saat menu terbuka

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

        // gambar: squircle gelap + panah melingkar
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

    // ==============================================================
    // UPDATE (dipanggil tiap frame saat scene siap)
    // ==============================================================
    void Update() {
        float dt = ImGui::GetIO().DeltaTime;

        // nama scene di-cache agar tidak dipanggil tiap frame
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

        // Override RNG/ekstra: tunggu 2 detik setelah gameplay dimulai agar Start() game selesai
        if (s_SceneName == "Scene") {
            s_InGameTimer += dt;
            if (!s_Applied && s_InGameTimer >= 2.0f) {
                s_Applied = true;
                ApplyOverrides();
            }
        } else {
            s_InGameTimer = 0.0f;
        }
    }

    // ==============================================================
    // 3. DUMP CLASS GAME (untuk menyusun fitur RNG / ekstra dengan akurat)
    // ==============================================================
    typedef size_t      (*image_get_class_count_t)(void* image);
    typedef void*       (*image_get_class_t)(void* image, size_t index);
    typedef void*       (*class_get_methods_t)(void* klass, void** iter);
    typedef void*       (*class_get_fields_t)(void* klass, void** iter);
    typedef void*       (*class_get_parent_t)(void* klass);
    typedef const char* (*method_get_name_t)(void* method);
    typedef uint32_t    (*method_get_param_count_t)(void* method);
    typedef void*       (*method_get_param_t)(void* method, uint32_t index);
    typedef void*       (*method_get_return_type_t)(void* method);
    typedef uint32_t    (*method_get_flags_t)(void* method, uint32_t* iflags);
    typedef const char* (*field_get_name_t)(void* field);
    typedef void*       (*field_get_type_t)(void* field);
    typedef char*       (*type_get_name_t)(void* type);
    typedef const char* (*image_get_name_t)(void* image);

    static void* Sym(const char* name) {
        static void* h = dlopen("libil2cpp.so", RTLD_NOW | RTLD_NOLOAD);
        void* p = h ? dlsym(h, name) : nullptr;
        if (!p) p = dlsym(RTLD_DEFAULT, name);
        return p;
    }

    bool DumpClasses() {
        auto image_get_class_count = (image_get_class_count_t)Sym("il2cpp_image_get_class_count");
        auto image_get_class       = (image_get_class_t)Sym("il2cpp_image_get_class");
        auto class_get_methods     = (class_get_methods_t)Sym("il2cpp_class_get_methods");
        auto class_get_fields      = (class_get_fields_t)Sym("il2cpp_class_get_fields");
        auto class_get_parent      = (class_get_parent_t)Sym("il2cpp_class_get_parent");
        auto method_get_name       = (method_get_name_t)Sym("il2cpp_method_get_name");
        auto method_get_param_count= (method_get_param_count_t)Sym("il2cpp_method_get_param_count");
        auto method_get_param      = (method_get_param_t)Sym("il2cpp_method_get_param");
        auto method_get_return_type= (method_get_return_type_t)Sym("il2cpp_method_get_return_type");
        auto field_get_name        = (field_get_name_t)Sym("il2cpp_field_get_name");
        auto field_get_type        = (field_get_type_t)Sym("il2cpp_field_get_type");
        auto type_get_name         = (type_get_name_t)Sym("il2cpp_type_get_name");
        auto image_get_name        = (image_get_name_t)Sym("il2cpp_image_get_name");

        if (!image_get_class_count || !image_get_class || !class_get_methods || !class_get_fields ||
            !method_get_name || !field_get_name || !il2cpp_class_get_name ||
            !il2cpp_domain_get || !il2cpp_domain_get_assemblies || !il2cpp_assembly_get_image || !image_get_name) {
            s_DumpStatus = "Failed: IL2CPP functions not found";
            return false;
        }

        std::string path = config::GetActiveConfigPath() + "/dump_classes.txt";
        FILE* f = fopen(path.c_str(), "w");
        if (!f) { s_DumpStatus = "Failed to write: dump_classes.txt"; return false; }

        // Kelas yang didump penuh (nama persis)
        static const char* kFullClasses[] = {
            "EnemyController", "Paused", "Days", "MainMenu", "ColorToyCapsule",
            "VersionControl", "HE_Spawn", "Menu_Seed", "Lava", "Elevator", "PlayerStatus"
        };

        auto typeName = [&](void* t) -> std::string {
            if (!t || !type_get_name) return "?";
            char* n = type_get_name(t);
            return n ? std::string(n) : "?";
        };

        auto dumpClass = [&](void* klass) {
            const char* ns = il2cpp_class_get_namespace ? il2cpp_class_get_namespace(klass) : "";
            const char* cn = il2cpp_class_get_name(klass);
            std::string parent = "";
            if (class_get_parent) {
                void* p = class_get_parent(klass);
                if (p) parent = std::string(" : ") + il2cpp_class_get_name(p);
            }
            fprintf(f, "\n=== class %s%s%s%s ===\n", (ns && *ns) ? ns : "", (ns && *ns) ? "." : "", cn, parent.c_str());
            void* it = nullptr;
            while (void* fld = class_get_fields(klass, &it)) {
                size_t off = il2cpp_field_get_offset ? il2cpp_field_get_offset(fld) : 0;
                fprintf(f, "  [field] %s %s  // 0x%zx\n", typeName(field_get_type ? field_get_type(fld) : nullptr).c_str(),
                        field_get_name(fld), off);
            }
            it = nullptr;
            while (void* m = class_get_methods(klass, &it)) {
                std::string sig;
                uint32_t pc = method_get_param_count ? method_get_param_count(m) : 0;
                for (uint32_t i = 0; i < pc; i++) {
                    if (i) sig += ", ";
                    sig += typeName(method_get_param ? method_get_param(m, i) : nullptr);
                }
                fprintf(f, "  [method] %s %s(%s)\n",
                        typeName(method_get_return_type ? method_get_return_type(m) : nullptr).c_str(),
                        method_get_name(m), sig.c_str());
            }
        };

        size_t asmCount = 0;
        const void** asms = il2cpp_domain_get_assemblies(il2cpp_domain_get(), &asmCount);
        int dumped = 0, hits = 0;
        fprintf(f, "# dump_classes.txt - Granny Legacy (Assembly-CSharp)\n");

        for (size_t a = 0; a < asmCount; a++) {
            void* img = il2cpp_assembly_get_image(asms[a]);
            if (!img) continue;
            const char* iname = image_get_name(img);
            if (!iname || strcmp(iname, "Assembly-CSharp.dll") != 0) continue;

            size_t cc = image_get_class_count(img);
            // Bagian A: kelas target, dump lengkap
            for (size_t i = 0; i < cc; i++) {
                void* k = image_get_class(img, i);
                if (!k) continue;
                std::string cn = il2cpp_class_get_name(k);
                for (const char* t : kFullClasses) {
                    if (cn == t) { dumpClass(k); dumped++; break; }
                }
            }
        }

        fclose(f);
        char buf[160];
        snprintf(buf, sizeof(buf), "Done: %d classes dumped. File: dump_classes.txt", dumped);
        s_DumpStatus = buf;
        Logger::Log("SPEEDRUN", LOG_OK, "Class dump written: %s", path.c_str());
        return true;
    }

    // ==============================================================
    // MENU (tab Speedrun)
    // ==============================================================
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
        ImGui::TextWrapped("%s", "Applied when a run starts. Restart the run after changing.");
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);

        ImGui::BeginChild("##sr1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "RNG Control");
        ImGui::Separator();
        ImGui::Spacing();

        static const char* ratItems[] = { "Random", "Left", "Right" };
        static const char* momItems[] = { "Random", "Tunnel", "Elevator" };
        edited::combo("Door-Opening Rat", &ratChoice, ratItems, 3);
        edited::combo("Spider Mom Route", &momRoute, momItems, 3);

        // Dropdown posisi guci: "Random" + nama posisi (atau "Position N" jika jumlahnya bukan 9)
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
        ImGui::TextWrapped("%s", "RNG choices apply 2 seconds after a run starts. Change them, then restart the run.");

        ImGui::Spacing();
        edited::colortext(ImVec4(1, 1, 1, 1), "Developer Tools");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped("%s", "Dump Game Classes writes dump_classes.txt to the game data folder. Send it to add the remaining features.");
        if (edited::buttonn("Dump Game Classes", ImVec2(-1, 38 * sc))) {
            DumpClasses();
        }
        if (!s_DumpStatus.empty()) ImGui::TextWrapped("%s", s_DumpStatus.c_str());
        ImGui::EndChild();
    }
}
