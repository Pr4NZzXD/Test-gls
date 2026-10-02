#include "il2cpp_api.h"
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

uintptr_t g_Il2CppBase = 0;
JavaVM* g_JavaVM = nullptr;

// =======================================================
// УКАЗАТЕЛИ НА БАЗОВЫЕ ФУНКЦИИ IL2CPP
// =======================================================
il2cpp_domain_get_t il2cpp_domain_get = nullptr;
il2cpp_class_get_parent_t il2cpp_class_get_parent = nullptr;
il2cpp_thread_attach_t il2cpp_thread_attach = nullptr;
il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies = nullptr;
il2cpp_assembly_get_image_t il2cpp_assembly_get_image = nullptr;
il2cpp_class_from_name_t il2cpp_class_from_name = nullptr;
il2cpp_class_get_type_t il2cpp_class_get_type = nullptr;
il2cpp_type_get_object_t il2cpp_type_get_object = nullptr;
il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name = nullptr;
il2cpp_class_get_field_from_name_t il2cpp_class_get_field_from_name = nullptr;
il2cpp_field_get_offset_t il2cpp_field_get_offset = nullptr;
il2cpp_runtime_invoke_t il2cpp_runtime_invoke = nullptr;
il2cpp_string_new_t il2cpp_string_new = nullptr;
il2cpp_resolve_icall_t il2cpp_resolve_icall = nullptr;
il2cpp_class_get_name_t il2cpp_class_get_name = nullptr;
il2cpp_class_get_namespace_t il2cpp_class_get_namespace = nullptr;
il2cpp_object_unbox_t il2cpp_object_unbox = nullptr;
il2cpp_class_init_t il2cpp_class_init = nullptr;

// =======================================================
// РЕФЛЕКСИЯ ПОЛЕЙ И МЕТОДОВ ДЛЯ UNITY EXPLORER
// =======================================================
il2cpp_class_get_fields_t il2cpp_class_get_fields = nullptr;
il2cpp_field_get_name_t il2cpp_field_get_name = nullptr;
il2cpp_field_get_type_t il2cpp_field_get_type = nullptr;
il2cpp_type_get_type_t il2cpp_type_get_type = nullptr;
il2cpp_type_get_name_t il2cpp_type_get_name = nullptr;

il2cpp_class_get_methods_t il2cpp_class_get_methods = nullptr;
il2cpp_method_get_name_t il2cpp_method_get_name = nullptr;
il2cpp_method_get_param_count_t il2cpp_method_get_param_count = nullptr;
il2cpp_method_get_return_type_t il2cpp_method_get_return_type = nullptr;

// =======================================================
// УКАЗАТЕЛИ НА UNITY ICALLS
// =======================================================
GameObjectFind_t oGameObjectFind = nullptr;
GetMousePosition_t oGetMousePosition = nullptr;
GetMouseButton_t oGetMouseButton = nullptr;
GetInputString_t oGetInputString = nullptr;
SetBehaviourEnabled_t oSetBehaviourEnabled = nullptr;
GetBehaviourEnabled_t oGetBehaviourEnabled = nullptr;
SetGameObjectActive_t oSetGameObjectActive = nullptr;
GetGameObjectActive_t oGetGameObjectActive = nullptr;
SetTimeScale_t oSetTimeScale = nullptr;
SetFixedDeltaTime_t oSetFixedDeltaTime = nullptr;
SetAudioListenerPitch_t oSetAudioListenerPitch = nullptr;
SetAudioSourcePitch_t oSetAudioSourcePitch = nullptr;
SetMasterTextureLimit_t oSetMasterTextureLimit = nullptr;
GetWhiteTexture_t oGetWhiteTexture = nullptr;
GetMainCamera_t oGetMainCamera = nullptr;
WorldToScreenPoint_t oWorldToScreenPoint = nullptr;
ComponentGetGameObject_t oComponentGetGameObject = nullptr;
ComponentGetTransform_t oComponentGetTransform = nullptr;
GameObjectGetTransform_t oGameObjectGetTransform = nullptr;
TransformGetPosition_t oTransformGetPosition = nullptr;
TransformSetPosition_t oTransformSetPosition = nullptr;
TransformGetEulerAngles_t oTransformGetEulerAngles = nullptr;
TransformSetEulerAngles_t oTransformSetEulerAngles = nullptr;
TransformGetDirection_t oTransformGetForward = nullptr;
TransformGetDirection_t oTransformGetRight = nullptr;
TransformGetDirection_t oTransformGetUp = nullptr;
TransformGetChildCount_t oTransformGetChildCount = nullptr;
TransformGetChild_t oTransformGetChild = nullptr;
TransformGetParent_t oTransformGetParent = nullptr;
SetDetectCollisions_t oSetDetectCollisions = nullptr;
SetFieldOfView_t oSetFieldOfView = nullptr;
GetFieldOfView_t oGetFieldOfView = nullptr;
ObjectGetName_t oObjectGetName = nullptr;
RendererGetBounds_t oRendererGetBounds = nullptr;
SetAmbientLight_t oSetAmbientLight = nullptr;
SetFog_t oSetFog = nullptr;
SetFogDensity_t oSetFogDensity = nullptr;

// =======================================================
// КЭШ ТИПОВ И МЕТОДОВ UNITY
// =======================================================
std::vector<void*> g_Images;
void* g_OutlineTypeObj = nullptr;
void* g_TransformTypeObj = nullptr;
void* g_ComponentTypeObj = nullptr;
void* g_CameraTypeObj = nullptr;
void* g_AudioSourceTypeObj = nullptr;
void* g_CharacterControllerTypeObj = nullptr;
void* g_FindObjectsOfTypeMethod = nullptr;
void* g_AddComponentMethod = nullptr;
void* g_GetComponentMethod = nullptr;
void* g_GetComponentsMethod = nullptr;
void* g_GetComponentInChildrenMethod = nullptr;
void* g_TransformGetParentMethod = nullptr;
void* g_TransformGetChildCountMethod = nullptr;
void* g_TransformGetChildMethod = nullptr;
void* g_SetOutlineModeMethod = nullptr;
void* g_SetOutlineColorMethod = nullptr;
void* g_SetOutlineWidthMethod = nullptr;
void* g_UpdateMatPropsMethod = nullptr;

void* g_RendererTypeObj = nullptr;
void* g_SkinnedMeshRendererTypeObj = nullptr;
void* g_GetMaterialsMethod = nullptr;
void* g_SetMainTextureMethod = nullptr;
void* g_GetMainTextureMethod = nullptr;
void* g_SetColorMethod = nullptr;

void* g_CachedMaskMat = nullptr;
void* g_CachedFillMat = nullptr;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    g_JavaVM = vm;
    LOGI("[+] JNI_OnLoad hooked! JavaVM: %p", vm);
    return JNI_VERSION_1_6;
}

void ShowSoftKeyboard(bool show) {
    if (!g_JavaVM) return;
    JNIEnv* env = nullptr;
    if (g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_JavaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    }

    jclass unityPlayerClass = env->FindClass("com/unity3d/player/UnityPlayer");
    if (!unityPlayerClass) return;

    jfieldID activityField = env->GetStaticFieldID(unityPlayerClass, "currentActivity", "Landroid/app/Activity;");
    if (!activityField) return;

    jobject activity = env->GetStaticObjectField(unityPlayerClass, activityField);
    if (!activity) return;

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getSystemServiceMethod = env->GetMethodID(activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring serviceName = env->NewStringUTF("input_method");
    jobject imm = env->CallObjectMethod(activity, getSystemServiceMethod, serviceName);
    env->DeleteLocalRef(serviceName);

    if (!imm) return;

    jclass immClass = env->GetObjectClass(imm);
    if (show) {
        jmethodID toggleMethod = env->GetMethodID(immClass, "toggleSoftInput", "(II)V");
        if (toggleMethod) {
            env->CallVoidMethod(imm, toggleMethod, 2, 0);
        }
    } else {
        jmethodID getWindowMethod = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
        jobject window = env->CallObjectMethod(activity, getWindowMethod);
        if (window) {
            jclass windowClass = env->GetObjectClass(window);
            jmethodID getDecorViewMethod = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
            jobject decorView = env->CallObjectMethod(window, getDecorViewMethod);
            if (decorView) {
                jclass viewClass = env->GetObjectClass(decorView);
                jmethodID getWindowTokenMethod = env->GetMethodID(viewClass, "getWindowToken", "()Landroid/os/IBinder;");
                jobject token = env->CallObjectMethod(decorView, getWindowTokenMethod);
                if (token) {
                    jmethodID hideMethod = env->GetMethodID(immClass, "hideSoftInputFromWindow", "(Landroid/os/IBinder;I)Z");
                    env->CallBooleanMethod(imm, hideMethod, token, 0);
                }
            }
        }
    }
}

uintptr_t GetLibBase64(const char* libName) {
    FILE* fp = fopen("/proc/self/maps", "r");
    if (!fp) return 0;

    char line[512];
    uintptr_t base = 0;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, libName)) {
            base = strtoull(line, nullptr, 16);
            break;
        }
    }

    fclose(fp);
    return base;
}

bool IsNativeObjectAlive(void* unityObj) {
    if (!unityObj) return false;
    uintptr_t addr = (uintptr_t)unityObj;
    if (addr < 0x10000 || (addr % sizeof(void*)) != 0) return false;

    // Проверяем m_CachedPtr внутри UnityEngine.Object
    void* nativePtr = *(void**)(addr + 0x10);
    return (nativePtr != nullptr && (uintptr_t)nativePtr >= 0x10000);
}

void* GetCurrentCamera() {
    // 1. Быстрый нативный вызов Camera.main
    if (oGetMainCamera) {
        void* cam = oGetMainCamera();
        if (cam && IsNativeObjectAlive(cam)) return cam;
    }

    // 2. Строгий поиск камеры игрока по пути (без перебора тысяч объектов)
    if (oGameObjectFind && il2cpp_string_new && g_GetComponentMethod && g_CameraTypeObj) {
        void* str = il2cpp_string_new("PlayerStuff/Player/CameraShakeAnim/CameraPivot/Main Camera");
        if (str) {
            void* go = oGameObjectFind(str);
            if (go && IsNativeObjectAlive(go)) {
                void* exc = nullptr;
                void* args[1] = { g_CameraTypeObj };
                void* c = il2cpp_runtime_invoke(g_GetComponentMethod, go, args, &exc);
                if (c && !exc && IsNativeObjectAlive(c)) return c;
            }
        }
    }
    return nullptr;
}

// МГНОВЕННАЯ ПРОВЕРКА (0.0001 МС, БЕЗ ПЕРЕБОРА ВСЕХ ТРАНСФОРМОВ)
bool IsSceneReady() {
    if (oGetMainCamera) {
        void* cam = oGetMainCamera();
        if (cam && IsNativeObjectAlive(cam)) return true;
    }

    if (oGameObjectFind && il2cpp_string_new) {
        void* pStr = il2cpp_string_new("PlayerStuff/Player");
        if (pStr) {
            void* go = oGameObjectFind(pStr);
            if (go && IsNativeObjectAlive(go)) return true;
        }
    }

    return false;
}

void* SafeGetParentTransform(void* tr) {
    if (!tr || !IsNativeObjectAlive(tr)) return nullptr;
    if (oTransformGetParent) {
        void* p = oTransformGetParent(tr);
        if (p && IsNativeObjectAlive(p)) return p;
    }
    if (g_TransformGetParentMethod) {
        void* exc = nullptr;
        void* p = il2cpp_runtime_invoke(g_TransformGetParentMethod, tr, nullptr, &exc);
        if (!exc && p && IsNativeObjectAlive(p)) return p;
    }
    return nullptr;
}

int32_t SafeGetChildCount(void* tr) {
    if (!tr || !IsNativeObjectAlive(tr)) return 0;
    if (oTransformGetChildCount) return oTransformGetChildCount(tr);
    if (g_TransformGetChildCountMethod) {
        void* exc = nullptr;
        void* res = il2cpp_runtime_invoke(g_TransformGetChildCountMethod, tr, nullptr, &exc);
        if (!exc && res) return *(int32_t*)((uintptr_t)res + 0x10);
    }
    return 0;
}

void* SafeGetChild(void* tr, int32_t index) {
    if (!tr || !IsNativeObjectAlive(tr)) return nullptr;
    if (oTransformGetChild) {
        void* c = oTransformGetChild(tr, index);
        if (c && IsNativeObjectAlive(c)) return c;
    }
    if (g_TransformGetChildMethod) {
        void* exc = nullptr;
        void* args[1] = { &index };
        void* c = il2cpp_runtime_invoke(g_TransformGetChildMethod, tr, args, &exc);
        if (!exc && c && IsNativeObjectAlive(c)) return c;
    }
    return nullptr;
}

std::string GetUnityObjectName(void* unityObj) {
    if (!unityObj || !IsNativeObjectAlive(unityObj) || !oObjectGetName) return "";
    void* monoStr = oObjectGetName(unityObj);
    if (!monoStr) return "";

    int32_t len = *(int32_t*)((uintptr_t)monoStr + 0x10);
    uint16_t* chars = (uint16_t*)((uintptr_t)monoStr + 0x14);
    std::string result = "";
    result.reserve(len);
    for (int32_t i = 0; i < len; i++) {
        if (chars[i] < 128) result += (char)chars[i];
    }
    return result;
}

std::string GetTransformPath(void* tr) {
    if (!tr || !IsNativeObjectAlive(tr)) return "";
    std::string path = GetUnityObjectName(tr);
    void* cur = tr;
    while (true) {
        cur = SafeGetParentTransform(cur);
        if (!cur || !IsNativeObjectAlive(cur)) break;
        std::string pName = GetUnityObjectName(cur);
        path = pName + "/" + path;
    }
    return path;
}

std::vector<void*> GetGameObjectComponents(void* go) {
    std::vector<void*> list;
    if (!go || !IsNativeObjectAlive(go) || !g_GetComponentsMethod || !g_ComponentTypeObj) return list;

    void* exc = nullptr;
    void* args[1] = { g_ComponentTypeObj };
    void* arrayResult = il2cpp_runtime_invoke(g_GetComponentsMethod, go, args, &exc);
    if (!arrayResult || exc) return list;

    uint64_t len = *(uint64_t*)((uintptr_t)arrayResult + 0x18);
    void** items = (void**)((uintptr_t)arrayResult + 0x20);
    for (uint64_t i = 0; i < len; i++) {
        if (items[i] && IsNativeObjectAlive(items[i])) {
            list.push_back(items[i]);
        }
    }
    return list;
}

std::vector<void*> GetRootTransforms() {
    std::vector<void*> roots;
    if (!g_TransformTypeObj || !g_FindObjectsOfTypeMethod) return roots;

    std::vector<void*> allTransforms = FindObjectsOfUnityType(g_TransformTypeObj);
    for (void* tr : allTransforms) {
        if (!tr || !IsNativeObjectAlive(tr)) continue;
        void* parent = SafeGetParentTransform(tr);
        if (!parent || !IsNativeObjectAlive(parent)) {
            roots.push_back(tr);
        }
    }
    return roots;
}

bool WorldToScreen(const Vector3& worldPos, Vector3& screenPos) {
    if (!oWorldToScreenPoint) return false;
    void* mainCam = GetCurrentCamera();
    if (!mainCam || !IsNativeObjectAlive(mainCam)) return false;

    Vector3 inPos = worldPos;
    Vector3 outPos{ 0, 0, 0 };

    oWorldToScreenPoint(mainCam, &inPos, 2, &outPos);

    if (outPos.z <= 0.05f) return false;

    screenPos = outPos;
    return true;
}

void SetFieldFloat(void* obj, void* klass, const char* fieldName, float val) {
    if (!obj || !klass || !il2cpp_class_get_field_from_name || !il2cpp_field_get_offset) return;
    void* field = il2cpp_class_get_field_from_name(klass, fieldName);
    if (field) {
        size_t offset = il2cpp_field_get_offset(field);
        *(float*)((uintptr_t)obj + offset) = val;
    }
}

void SetFieldBool(void* obj, void* klass, const char* fieldName, bool val) {
    if (!obj || !klass || !il2cpp_class_get_field_from_name || !il2cpp_field_get_offset) return;
    void* field = il2cpp_class_get_field_from_name(klass, fieldName);
    if (field) {
        size_t offset = il2cpp_field_get_offset(field);
        *(bool*)((uintptr_t)obj + offset) = val;
    }
}

void* FindClass(const char* namespaze, const char* className) {
    if (!il2cpp_class_from_name) return nullptr;
    for (void* img : g_Images) {
        if (!img) continue;
        void* klass = il2cpp_class_from_name(img, namespaze, className);
        if (klass) return klass;
    }
    return nullptr;
}

std::vector<void*> FindObjectsOfUnityType(void* typeObj) {
    std::vector<void*> result;
    if (!typeObj || !g_FindObjectsOfTypeMethod) return result;

    void* exc = nullptr;
    void* args[1] = { typeObj };
    void* arrayResult = il2cpp_runtime_invoke(g_FindObjectsOfTypeMethod, nullptr, args, &exc);
    if (!arrayResult || exc) return result;

    uint64_t length = *(uint64_t*)((uintptr_t)arrayResult + 0x18);
    void** items = (void**)((uintptr_t)arrayResult + 0x20);

    for (uint64_t i = 0; i < length; i++) {
        if (items[i] && IsNativeObjectAlive(items[i])) {
            result.push_back(items[i]);
        }
    }
    return result;
}

void CacheOutlineMaterials() {
    if (g_CachedMaskMat && g_CachedFillMat) return;

    if (g_OutlineTypeObj && g_FindObjectsOfTypeMethod) {
        std::vector<void*> outlines = FindObjectsOfUnityType(g_OutlineTypeObj);
        for (void* existing : outlines) {
            void* mask = *(void**)((uintptr_t)existing + 0x28);
            void* fill = *(void**)((uintptr_t)existing + 0x30);
            if (mask && fill) {
                g_CachedMaskMat = mask;
                g_CachedFillMat = fill;
                return;
            }
        }
    }
}

void* SetOutlineOnObject(void* go, bool enable, float* colorArr, float width) {
    if (!go || !IsNativeObjectAlive(go) || !g_OutlineTypeObj || !g_AddComponentMethod) return nullptr;

    void* exc = nullptr;
    void* outlineComp = nullptr;

    if (g_GetComponentMethod) {
        void* args[1] = { g_OutlineTypeObj };
        outlineComp = il2cpp_runtime_invoke(g_GetComponentMethod, go, args, &exc);
    }
    if (!outlineComp && enable && g_AddComponentMethod) {
        void* args[1] = { g_OutlineTypeObj };
        outlineComp = il2cpp_runtime_invoke(g_AddComponentMethod, go, args, &exc);

        CacheOutlineMaterials();
        if (g_CachedMaskMat && g_CachedFillMat && outlineComp) {
            *(void**)((uintptr_t)outlineComp + 0x28) = g_CachedMaskMat;
            *(void**)((uintptr_t)outlineComp + 0x30) = g_CachedFillMat;
        }
    }
    if (!outlineComp || !IsNativeObjectAlive(outlineComp)) return nullptr;

    if (oSetBehaviourEnabled) oSetBehaviourEnabled(outlineComp, enable);

    if (enable) {
        if (g_SetOutlineModeMethod) {
            int32_t mode = 0;
            void* modeArgs[1] = { &mode };
            il2cpp_runtime_invoke(g_SetOutlineModeMethod, outlineComp, modeArgs, &exc);
        }
        if (g_SetOutlineColorMethod) {
            Color c{ colorArr[0], colorArr[1], colorArr[2], colorArr[3] };
            void* colorArgs[1] = { &c };
            il2cpp_runtime_invoke(g_SetOutlineColorMethod, outlineComp, colorArgs, &exc);
        }
        if (g_SetOutlineWidthMethod) {
            void* widthArgs[1] = { &width };
            il2cpp_runtime_invoke(g_SetOutlineWidthMethod, outlineComp, widthArgs, &exc);
        }
        if (g_UpdateMatPropsMethod) {
            il2cpp_runtime_invoke(g_UpdateMatPropsMethod, outlineComp, nullptr, &exc);
        }
    }
    return outlineComp;
}

void CopyToAndroidClipboard(const char* text) {
    if (!g_JavaVM || !text) return;
    JNIEnv* env = nullptr;
    if (g_JavaVM->GetEnv((void**)&env, JNI_VERSION_1_6) != JNI_OK) {
        if (g_JavaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    }

    jclass unityPlayerClass = env->FindClass("com/unity3d/player/UnityPlayer");
    if (!unityPlayerClass) return;

    jfieldID activityField = env->GetStaticFieldID(unityPlayerClass, "currentActivity", "Landroid/app/Activity;");
    if (!activityField) return;

    jobject activity = env->GetStaticObjectField(unityPlayerClass, activityField);
    if (!activity) return;

    jclass activityClass = env->GetObjectClass(activity);
    jmethodID getSystemServiceMethod = env->GetMethodID(activityClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");

    jstring serviceName = env->NewStringUTF("clipboard");
    jobject clipboardMgr = env->CallObjectMethod(activity, getSystemServiceMethod, serviceName);
    env->DeleteLocalRef(serviceName);

    if (!clipboardMgr) return;

    jclass clipDataClass = env->FindClass("android/content/ClipData");
    if (!clipDataClass) return;

    jmethodID newPlainTextMethod = env->GetStaticMethodID(clipDataClass, "newPlainText", "(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;");
    if (!newPlainTextMethod) return;

    jstring label = env->NewStringUTF("KahaniumPath");
    jstring content = env->NewStringUTF(text);
    jobject clipData = env->CallStaticObjectMethod(clipDataClass, newPlainTextMethod, label, content);
    env->DeleteLocalRef(label);
    env->DeleteLocalRef(content);

    jclass clipboardMgrClass = env->GetObjectClass(clipboardMgr);
    jmethodID setPrimaryClipMethod = env->GetMethodID(clipboardMgrClass, "setPrimaryClip", "(Landroid/content/ClipData;)V");
    if (setPrimaryClipMethod && clipData) {
        env->CallVoidMethod(clipboardMgr, setPrimaryClipMethod, clipData);
    }
}

bool InitIL2CPP() {
    while (g_Il2CppBase == 0) {
        g_Il2CppBase = GetLibBase64("libil2cpp.so");
        usleep(200000);
    }

    void* handle = dlopen("libil2cpp.so", RTLD_NOLOAD);
    if (!handle) handle = dlopen("libil2cpp.so", RTLD_LAZY);
    if (!handle) return false;

    // Базовые функции
    il2cpp_domain_get = (il2cpp_domain_get_t)dlsym(handle, "il2cpp_domain_get");
    il2cpp_thread_attach = (il2cpp_thread_attach_t)dlsym(handle, "il2cpp_thread_attach");
    il2cpp_domain_get_assemblies = (il2cpp_domain_get_assemblies_t)dlsym(handle, "il2cpp_domain_get_assemblies");
    il2cpp_assembly_get_image = (il2cpp_assembly_get_image_t)dlsym(handle, "il2cpp_assembly_get_image");
    il2cpp_class_from_name = (il2cpp_class_from_name_t)dlsym(handle, "il2cpp_class_from_name");
    il2cpp_class_get_type = (il2cpp_class_get_type_t)dlsym(handle, "il2cpp_class_get_type");
    il2cpp_type_get_object = (il2cpp_type_get_object_t)dlsym(handle, "il2cpp_type_get_object");
    il2cpp_class_get_method_from_name = (il2cpp_class_get_method_from_name_t)dlsym(handle, "il2cpp_class_get_method_from_name");
    il2cpp_class_get_field_from_name = (il2cpp_class_get_field_from_name_t)dlsym(handle, "il2cpp_class_get_field_from_name");
    il2cpp_field_get_offset = (il2cpp_field_get_offset_t)dlsym(handle, "il2cpp_field_get_offset");
    il2cpp_runtime_invoke = (il2cpp_runtime_invoke_t)dlsym(handle, "il2cpp_runtime_invoke");
    il2cpp_string_new = (il2cpp_string_new_t)dlsym(handle, "il2cpp_string_new");
    il2cpp_resolve_icall = (il2cpp_resolve_icall_t)dlsym(handle, "il2cpp_resolve_icall");
    il2cpp_class_get_name = (il2cpp_class_get_name_t)dlsym(handle, "il2cpp_class_get_name");
    il2cpp_class_get_namespace = (il2cpp_class_get_namespace_t)dlsym(handle, "il2cpp_class_get_namespace");
    il2cpp_object_unbox = (il2cpp_object_unbox_t)dlsym(handle, "il2cpp_object_unbox");
    il2cpp_class_init = (il2cpp_class_init_t)dlsym(handle, "il2cpp_class_init");
    il2cpp_class_get_parent = (il2cpp_class_get_parent_t)dlsym(handle, "il2cpp_class_get_parent");

    // Функции рефлексии для UnityExplorer инспектора
    il2cpp_class_get_fields = (il2cpp_class_get_fields_t)dlsym(handle, "il2cpp_class_get_fields");
    il2cpp_field_get_name = (il2cpp_field_get_name_t)dlsym(handle, "il2cpp_field_get_name");
    il2cpp_field_get_type = (il2cpp_field_get_type_t)dlsym(handle, "il2cpp_field_get_type");
    il2cpp_type_get_type = (il2cpp_type_get_type_t)dlsym(handle, "il2cpp_type_get_type");
    il2cpp_type_get_name = (il2cpp_type_get_name_t)dlsym(handle, "il2cpp_type_get_name");

    il2cpp_class_get_methods = (il2cpp_class_get_methods_t)dlsym(handle, "il2cpp_class_get_methods");
    il2cpp_method_get_name = (il2cpp_method_get_name_t)dlsym(handle, "il2cpp_method_get_name");
    il2cpp_method_get_param_count = (il2cpp_method_get_param_count_t)dlsym(handle, "il2cpp_method_get_param_count");
    il2cpp_method_get_return_type = (il2cpp_method_get_return_type_t)dlsym(handle, "il2cpp_method_get_return_type");

    if (il2cpp_resolve_icall) {
        oGameObjectFind = (GameObjectFind_t)il2cpp_resolve_icall("UnityEngine.GameObject::Find");
        oGetMousePosition = (GetMousePosition_t)il2cpp_resolve_icall("UnityEngine.Input::get_mousePosition_Injected");
        oGetMouseButton = (GetMouseButton_t)il2cpp_resolve_icall("UnityEngine.Input::GetMouseButton");
        oGetInputString = (GetInputString_t)il2cpp_resolve_icall("UnityEngine.Input::get_inputString");
        oSetBehaviourEnabled = (SetBehaviourEnabled_t)il2cpp_resolve_icall("UnityEngine.Behaviour::set_enabled");
        oGetBehaviourEnabled = (GetBehaviourEnabled_t)il2cpp_resolve_icall("UnityEngine.Behaviour::get_enabled");
        oSetGameObjectActive = (SetGameObjectActive_t)il2cpp_resolve_icall("UnityEngine.GameObject::SetActive");
        oGetGameObjectActive = (GetGameObjectActive_t)il2cpp_resolve_icall("UnityEngine.GameObject::get_activeSelf");
        oSetTimeScale = (SetTimeScale_t)il2cpp_resolve_icall("UnityEngine.Time::set_timeScale");
        oSetFixedDeltaTime = (SetFixedDeltaTime_t)il2cpp_resolve_icall("UnityEngine.Time::set_fixedDeltaTime");
        oSetAudioListenerPitch = (SetAudioListenerPitch_t)il2cpp_resolve_icall("UnityEngine.AudioListener::set_pitch");
        oSetAudioSourcePitch = (SetAudioSourcePitch_t)il2cpp_resolve_icall("UnityEngine.AudioSource::set_pitch");
        oSetMasterTextureLimit = (SetMasterTextureLimit_t)il2cpp_resolve_icall("UnityEngine.QualitySettings::set_masterTextureLimit");
        oGetWhiteTexture = (GetWhiteTexture_t)il2cpp_resolve_icall("UnityEngine.Texture2D::get_whiteTexture");
        oGetMainCamera = (GetMainCamera_t)il2cpp_resolve_icall("UnityEngine.Camera::get_main");
        oWorldToScreenPoint = (WorldToScreenPoint_t)il2cpp_resolve_icall("UnityEngine.Camera::WorldToScreenPoint_Injected");
        oComponentGetGameObject = (ComponentGetGameObject_t)il2cpp_resolve_icall("UnityEngine.Component::get_gameObject");
        oComponentGetTransform = (ComponentGetTransform_t)il2cpp_resolve_icall("UnityEngine.Component::get_transform");
        oGameObjectGetTransform = (GameObjectGetTransform_t)il2cpp_resolve_icall("UnityEngine.GameObject::get_transform");
        oTransformGetPosition = (TransformGetPosition_t)il2cpp_resolve_icall("UnityEngine.Transform::get_position_Injected");
        oTransformSetPosition = (TransformSetPosition_t)il2cpp_resolve_icall("UnityEngine.Transform::set_position_Injected");
        oTransformGetEulerAngles = (TransformGetEulerAngles_t)il2cpp_resolve_icall("UnityEngine.Transform::get_eulerAngles_Injected");
        oTransformSetEulerAngles = (TransformSetEulerAngles_t)il2cpp_resolve_icall("UnityEngine.Transform::set_eulerAngles_Injected");
        oTransformGetForward = (TransformGetDirection_t)il2cpp_resolve_icall("UnityEngine.Transform::get_forward_Injected");
        oTransformGetRight = (TransformGetDirection_t)il2cpp_resolve_icall("UnityEngine.Transform::get_right_Injected");
        oTransformGetUp = (TransformGetDirection_t)il2cpp_resolve_icall("UnityEngine.Transform::get_up_Injected");
        oTransformGetChildCount = (TransformGetChildCount_t)il2cpp_resolve_icall("UnityEngine.Transform::get_childCount");
        oTransformGetChild = (TransformGetChild_t)il2cpp_resolve_icall("UnityEngine.Transform::GetChild");
        oTransformGetParent = (TransformGetParent_t)il2cpp_resolve_icall("UnityEngine.Transform::get_parent");
        oSetDetectCollisions = (SetDetectCollisions_t)il2cpp_resolve_icall("UnityEngine.CharacterController::set_detectCollisions");
        oSetFieldOfView = (SetFieldOfView_t)il2cpp_resolve_icall("UnityEngine.Camera::set_fieldOfView");
        oGetFieldOfView = (GetFieldOfView_t)il2cpp_resolve_icall("UnityEngine.Camera::get_fieldOfView");
        oObjectGetName = (ObjectGetName_t)il2cpp_resolve_icall("UnityEngine.Object::GetName");
        if (!oObjectGetName) oObjectGetName = (ObjectGetName_t)il2cpp_resolve_icall("UnityEngine.Object::get_name");
        oRendererGetBounds = (RendererGetBounds_t)il2cpp_resolve_icall("UnityEngine.Renderer::get_bounds_Injected");
        oSetAmbientLight = (SetAmbientLight_t)il2cpp_resolve_icall("UnityEngine.RenderSettings::set_ambientLight_Injected");
        oSetFog = (SetFog_t)il2cpp_resolve_icall("UnityEngine.RenderSettings::set_fog");
        oSetFogDensity = (SetFogDensity_t)il2cpp_resolve_icall("UnityEngine.RenderSettings::set_fogDensity");
    }

    void* domain = il2cpp_domain_get ? il2cpp_domain_get() : nullptr;
    if (!domain) return false;
    if (il2cpp_thread_attach) il2cpp_thread_attach(domain);

    size_t count = 0;
    const void** assemblies = il2cpp_domain_get_assemblies(domain, &count);
    if (!assemblies || count == 0) return false;

    g_Images.clear();
    for (size_t i = 0; i < count; i++) {
        if (!assemblies[i]) continue;
        void* img = il2cpp_assembly_get_image(assemblies[i]);
        if (img) g_Images.push_back(img);
    }

    void* goClass = FindClass("UnityEngine", "GameObject");
    void* outlineClass = FindClass("", "Outline");
    void* objClass = FindClass("UnityEngine", "Object");
    void* trClass = FindClass("UnityEngine", "Transform");
    void* compClass = FindClass("UnityEngine", "Component");
    void* camClass = FindClass("UnityEngine", "Camera");
    void* audioSourceClass = FindClass("UnityEngine", "AudioSource");
    void* ccClass = FindClass("UnityEngine", "CharacterController");
    void* renClass = FindClass("UnityEngine", "Renderer");
    void* skinRenClass = FindClass("UnityEngine", "SkinnedMeshRenderer");
    void* matClass = FindClass("UnityEngine", "Material");

    if (objClass) {
        g_FindObjectsOfTypeMethod = il2cpp_class_get_method_from_name(objClass, "FindObjectsOfType", 1);
    }
    if (camClass) {
        void* type = il2cpp_class_get_type(camClass);
        if (type) g_CameraTypeObj = il2cpp_type_get_object(type);
    }
    if (audioSourceClass) {
        void* type = il2cpp_class_get_type(audioSourceClass);
        if (type) g_AudioSourceTypeObj = il2cpp_type_get_object(type);
    }
    if (trClass) {
        void* type = il2cpp_class_get_type(trClass);
        if (type) g_TransformTypeObj = il2cpp_type_get_object(type);
        g_TransformGetParentMethod = il2cpp_class_get_method_from_name(trClass, "get_parent", 0);
        g_TransformGetChildCountMethod = il2cpp_class_get_method_from_name(trClass, "get_childCount", 0);
        g_TransformGetChildMethod = il2cpp_class_get_method_from_name(trClass, "GetChild", 1);
    }
    if (compClass) {
        void* type = il2cpp_class_get_type(compClass);
        if (type) g_ComponentTypeObj = il2cpp_type_get_object(type);
    }
    if (ccClass) {
        void* type = il2cpp_class_get_type(ccClass);
        if (type) g_CharacterControllerTypeObj = il2cpp_type_get_object(type);
    }
    if (goClass) {
        g_AddComponentMethod = il2cpp_class_get_method_from_name(goClass, "AddComponent", 1);
        g_GetComponentMethod = il2cpp_class_get_method_from_name(goClass, "GetComponent", 1);
        g_GetComponentsMethod = il2cpp_class_get_method_from_name(goClass, "GetComponents", 1);
        g_GetComponentInChildrenMethod = il2cpp_class_get_method_from_name(goClass, "GetComponentInChildren", 1);
    }
    if (renClass) {
        g_GetMaterialsMethod = il2cpp_class_get_method_from_name(renClass, "get_materials", 0);
        void* type = il2cpp_class_get_type(renClass);
        if (type) g_RendererTypeObj = il2cpp_type_get_object(type);
    }
    if (skinRenClass) {
        void* type = il2cpp_class_get_type(skinRenClass);
        if (type) g_SkinnedMeshRendererTypeObj = il2cpp_type_get_object(type);
    }
    if (matClass) {
        g_SetMainTextureMethod = il2cpp_class_get_method_from_name(matClass, "set_mainTexture", 1);
        g_GetMainTextureMethod = il2cpp_class_get_method_from_name(matClass, "get_mainTexture", 0);
        g_SetColorMethod = il2cpp_class_get_method_from_name(matClass, "set_color", 1);
    }
    if (outlineClass) {
        void* type = il2cpp_class_get_type(outlineClass);
        if (type) g_OutlineTypeObj = il2cpp_type_get_object(type);

        g_SetOutlineModeMethod = il2cpp_class_get_method_from_name(outlineClass, "set_OutlineMode", 1);
        g_SetOutlineColorMethod = il2cpp_class_get_method_from_name(outlineClass, "set_OutlineColor", 1);
        g_SetOutlineWidthMethod = il2cpp_class_get_method_from_name(outlineClass, "set_OutlineWidth", 1);
        g_UpdateMatPropsMethod = il2cpp_class_get_method_from_name(outlineClass, "UpdateMaterialProperties", 0);
    }

    LOGI("[+] IL2CPP Ready! Base: 0x%lx", g_Il2CppBase);
    return (g_AddComponentMethod != nullptr && g_OutlineTypeObj != nullptr);
}

void A64Hook(void* target, void* replace, void** original) {
    uintptr_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t targetPage = (uintptr_t)target & ~(pageSize - 1);
    mprotect((void*)targetPage, pageSize * 2, PROT_READ | PROT_WRITE | PROT_EXEC);

    void* trampoline = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    memcpy(trampoline, target, 16);

    uint32_t* trampJump = (uint32_t*)((uintptr_t)trampoline + 16);
    trampJump[0] = 0x58000050; // LDR X16, #8
    trampJump[1] = 0xD61F0200; // BR X16
    *(uint64_t*)((uintptr_t)trampJump + 8) = (uintptr_t)target + 16;

    if (original) *original = trampoline;

    uint32_t* targetJump = (uint32_t*)target;
    targetJump[0] = 0x58000050; // LDR X16, #8
    targetJump[1] = 0xD61F0200; // BR X16
    *(uint64_t*)((uintptr_t)targetJump + 8) = (uintptr_t)replace;

    __builtin___clear_cache((char*)target, (char*)target + 24);
    __builtin___clear_cache((char*)trampoline, (char*)trampoline + 32);
}