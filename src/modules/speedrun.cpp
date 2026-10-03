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

    static bool s_AutoDone = false;
    static std::string s_Status = "";
    static std::string s_DumpStatus = "";
    static std::string s_SceneName = "";
    static float s_SceneTimer = 0.0f;

    void ClearCache() {
        s_AutoDone = false;
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
    // 2. TOMBOL RESTART MENGAMBANG
    // ==============================================================
    static void DoRestart() {
        if (oSetTimeScale) oSetTimeScale(1.0f);
        auto_farm::LoadSceneByName("Scene");
        Logger::Log("SPEEDRUN", LOG_OK, "Restart button pressed -> reload scene 'Scene'.");
    }

    void DrawRestartButton() {
        if (!restartButton) return;

        // nama scene di-cache agar tidak dipanggil tiap frame
        s_SceneTimer += ImGui::GetIO().DeltaTime;
        if (s_SceneName.empty() || s_SceneTimer > 0.5f) {
            s_SceneName = auto_farm::GetCurrentSceneName();
            s_SceneTimer = 0.0f;
        }
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
        // kepala panah di ujung busur
        float a = 4.2f;
        ImVec2 tip(ctr.x + cosf(a) * r, ctr.y + sinf(a) * r);
        ImVec2 dir(-sinf(a), cosf(a));            // arah singgung
        ImVec2 nrm(cosf(a), sinf(a));
        float h = 5.5f * sc;
        dl->AddTriangleFilled(
            ImVec2(tip.x + dir.x * h, tip.y + dir.y * h),
            ImVec2(tip.x - dir.x * h * 0.2f + nrm.x * h * 0.8f, tip.y - dir.y * h * 0.2f + nrm.y * h * 0.8f),
            ImVec2(tip.x - dir.x * h * 0.2f - nrm.x * h * 0.8f, tip.y - dir.y * h * 0.2f - nrm.y * h * 0.8f),
            col);
    }

    // ==============================================================
    // UPDATE (dipanggil tiap frame saat scene siap)
    // ==============================================================
    void Update() {
        if (autoUnlockShop && !s_AutoDone) {
            s_AutoDone = true;
            int n = UnlockAllShop();
            if (n > 0) s_Status = "Auto-unlock: " + std::to_string(n) + " item dibuka";
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

    static bool ContainsAny(const std::string& s, const char* const* keys, int n) {
        for (int i = 0; i < n; i++) if (s.find(keys[i]) != std::string::npos) return true;
        return false;
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
            s_DumpStatus = "Gagal: fungsi IL2CPP tidak ditemukan";
            return false;
        }

        std::string path = config::GetActiveConfigPath() + "/dump_classes.txt";
        FILE* f = fopen(path.c_str(), "w");
        if (!f) { s_DumpStatus = "Gagal menulis: dump_classes.txt"; return false; }

        // Kelas yang didump penuh (nama persis)
        static const char* kFullClasses[] = {
            "ObjectsManager", "SpecialEffectsManager", "AI_MomSpider", "ExplodingCoffinItemManagement",
            "RemoteRat", "SeedManager", "Escapes", "DaysStart", "RoboGrandparents"
        };
        // Kata kunci nama method / field yang dicari di SEMUA kelas
        static const char* kMethodKeys[] = {
            "SpawnGranny", "SpawnGrandpa", "ReadGranny", "ReadGrandpa", "ReadPresetOrSeed",
            "Restart", "Cutscene", "Gambl", "Lava", "ExtraTraps", "Capsule", "ChangeScene", "LoadScene"
        };
        static const char* kFieldKeys[] = {
            "extraTraps", "lava", "Lava", "seed", "Seed", "vase", "Vase", "RemoteRat", "TheOne",
            "NumberPad", "Run1", "Run2", "RunningToPoint", "speedrun", "Spawn"
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
            // Bagian B: kelas lain yang punya method/field berkata kunci
            fprintf(f, "\n\n##### PENCARIAN KATA KUNCI DI SEMUA KELAS #####\n");
            for (size_t i = 0; i < cc; i++) {
                void* k = image_get_class(img, i);
                if (!k) continue;
                std::string cn = il2cpp_class_get_name(k);
                bool isFull = false;
                for (const char* t : kFullClasses) if (cn == t) { isFull = true; break; }
                if (isFull) continue;

                std::string lines;
                void* it = nullptr;
                while (void* m = class_get_methods(k, &it)) {
                    std::string mn = method_get_name(m);
                    if (ContainsAny(mn, kMethodKeys, (int)(sizeof(kMethodKeys) / sizeof(kMethodKeys[0])))) {
                        std::string sig;
                        uint32_t pc = method_get_param_count ? method_get_param_count(m) : 0;
                        for (uint32_t p = 0; p < pc; p++) {
                            if (p) sig += ", ";
                            sig += typeName(method_get_param ? method_get_param(m, p) : nullptr);
                        }
                        lines += "  [method] " + typeName(method_get_return_type ? method_get_return_type(m) : nullptr) +
                                 " " + mn + "(" + sig + ")\n";
                    }
                }
                it = nullptr;
                while (void* fld = class_get_fields(k, &it)) {
                    std::string fn = field_get_name(fld);
                    if (ContainsAny(fn, kFieldKeys, (int)(sizeof(kFieldKeys) / sizeof(kFieldKeys[0])))) {
                        lines += "  [field] " + typeName(field_get_type ? field_get_type(fld) : nullptr) + " " + fn + "\n";
                    }
                }
                if (!lines.empty()) {
                    fprintf(f, "\n-- class %s\n%s", cn.c_str(), lines.c_str());
                    hits++;
                }
            }
        }

        fclose(f);
        char buf[160];
        snprintf(buf, sizeof(buf), "Selesai: %d kelas target, %d kelas cocok. File: dump_classes.txt", dumped, hits);
        s_DumpStatus = buf;
        Logger::Log("SPEEDRUN", LOG_OK, "Class dump written: %s", path.c_str());
        return true;
    }

    // ==============================================================
    // MENU (tab Speedrun)
    // ==============================================================
    void DrawMenu(float colW, float colH, float colGap) {
        float sc = menuscale::menuscale;

        ImGui::BeginChild("##sr0", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Shop & Utility");
        ImGui::Separator();
        ImGui::Spacing();

        edited::checkbox("Auto-Unlock Shop Items", &autoUnlockShop);
        if (edited::buttonn("Unlock All Shop Now", ImVec2(-1, 38 * sc))) {
            int n = UnlockAllShop();
            if (n < 0) s_Status = "Gagal membuka item shop";
            else if (n == 0) s_Status = "Semua item sudah terbuka";
            else s_Status = std::to_string(n) + " item shop dibuka";
        }
        if (!s_Status.empty()) ImGui::TextWrapped("%s", s_Status.c_str());

        ImGui::Spacing();
        edited::checkbox("Restart Button", &restartButton);
        if (edited::buttonn("Reset Button Position", ImVec2(-1, 38 * sc))) {
            restartPosX = -1.0f;
            restartPosY = -1.0f;
        }
        ImGui::EndChild();

        ImGui::SameLine(0, colGap);

        ImGui::BeginChild("##sr1", ImVec2(colW, colH), false);
        edited::colortext(ImVec4(1, 1, 1, 1), "Developer Tools");
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextWrapped("Dump Game Classes menulis file dump_classes.txt ke folder data game. Kirim file itu untuk melengkapi fitur RNG, Extra Traps, Lava Mode, dan Auto Skip.");
        ImGui::Spacing();
        if (edited::buttonn("Dump Game Classes", ImVec2(-1, 38 * sc))) {
            DumpClasses();
        }
        if (!s_DumpStatus.empty()) ImGui::TextWrapped("%s", s_DumpStatus.c_str());
        ImGui::EndChild();
    }
}
