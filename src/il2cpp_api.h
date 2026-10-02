#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <jni.h>
#include <android/log.h>

#define LOG_TAG "KahaniumAndroid"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct Vector3 { float x, y, z; };
struct Vector2 { float x, y; };
struct Color { float r, g, b, a; };
struct Bounds { Vector3 center; Vector3 extents; };

struct MonoClass { void* klass; void* monitor; };
struct MonoArray { MonoClass klass; void* bounds; uint64_t max_length; void* vector[1]; };
struct MonoList { MonoClass klass; MonoArray* items; int32_t size; int32_t version; };

extern uintptr_t g_Il2CppBase;
extern JavaVM* g_JavaVM;

typedef void* (*il2cpp_domain_get_t)();
typedef void* (*il2cpp_thread_attach_t)(void* domain);
typedef const void** (*il2cpp_domain_get_assemblies_t)(const void* domain, size_t* size);
typedef void* (*il2cpp_assembly_get_image_t)(const void* assembly);
typedef const char* (*il2cpp_image_get_name_t)(const void* image);
typedef void* (*il2cpp_class_from_name_t)(void* image, const char* namespaze, const char* name);
typedef void* (*il2cpp_class_get_type_t)(void* klass);
typedef void* (*il2cpp_type_get_object_t)(void* type);
typedef void* (*il2cpp_class_get_method_from_name_t)(void* klass, const char* name, int argsCount);
typedef void* (*il2cpp_class_get_field_from_name_t)(void* klass, const char* name);
typedef size_t (*il2cpp_field_get_offset_t)(void* field);
typedef void* (*il2cpp_runtime_invoke_t)(void* method, void* obj, void** params, void** exc);
typedef void* (*il2cpp_string_new_t)(const char* str);
typedef void* (*il2cpp_resolve_icall_t)(const char* name);
typedef const char* (*il2cpp_class_get_name_t)(void* klass);
typedef const char* (*il2cpp_class_get_namespace_t)(void* klass);
typedef void* (*il2cpp_object_unbox_t)(void* obj);

extern il2cpp_domain_get_t il2cpp_domain_get;
extern il2cpp_thread_attach_t il2cpp_thread_attach;
extern il2cpp_domain_get_assemblies_t il2cpp_domain_get_assemblies;
extern il2cpp_assembly_get_image_t il2cpp_assembly_get_image;
extern il2cpp_class_from_name_t il2cpp_class_from_name;
extern il2cpp_class_get_type_t il2cpp_class_get_type;
extern il2cpp_type_get_object_t il2cpp_type_get_object;
extern il2cpp_class_get_method_from_name_t il2cpp_class_get_method_from_name;
extern il2cpp_class_get_field_from_name_t il2cpp_class_get_field_from_name;
extern il2cpp_field_get_offset_t il2cpp_field_get_offset;
extern il2cpp_runtime_invoke_t il2cpp_runtime_invoke;
extern il2cpp_string_new_t il2cpp_string_new;
extern il2cpp_resolve_icall_t il2cpp_resolve_icall;
extern il2cpp_class_get_name_t il2cpp_class_get_name;
extern il2cpp_class_get_namespace_t il2cpp_class_get_namespace;
extern il2cpp_object_unbox_t il2cpp_object_unbox;

// Unity ICalls
typedef void*(*GameObjectFind_t)(void* monoString);
typedef void(*GetMousePosition_t)(Vector3* outPos);
typedef bool(*GetMouseButton_t)(int32_t button);
typedef void*(*GetInputString_t)();
typedef void(*SetBehaviourEnabled_t)(void* behaviour, bool enabled);
typedef bool(*GetBehaviourEnabled_t)(void* behaviour);
typedef void(*SetGameObjectActive_t)(void* go, bool active);
typedef bool(*GetGameObjectActive_t)(void* go);
typedef void(*SetTimeScale_t)(float scale);
typedef void(*SetFixedDeltaTime_t)(float dt);
typedef void(*SetAudioListenerPitch_t)(float pitch);
typedef void(*SetAudioSourcePitch_t)(void* audioSource, float pitch);
typedef void(*SetMasterTextureLimit_t)(int32_t limit);
typedef void*(*GetWhiteTexture_t)();
typedef void*(*GetMainCamera_t)();
typedef void(*WorldToScreenPoint_t)(void* cam, Vector3* pos, int32_t eye, Vector3* outPos);
typedef void*(*ComponentGetGameObject_t)(void* component);
typedef void*(*ComponentGetTransform_t)(void* component);
typedef void*(*GameObjectGetTransform_t)(void* go);
typedef void(*TransformGetPosition_t)(void* tr, Vector3* outPos);
typedef void(*TransformSetPosition_t)(void* tr, Vector3* inPos);
typedef void(*TransformGetEulerAngles_t)(void* tr, Vector3* outAngles);
typedef void(*TransformSetEulerAngles_t)(void* tr, Vector3* inAngles);
typedef void(*TransformGetDirection_t)(void* tr, Vector3* outDir);
typedef int32_t(*TransformGetChildCount_t)(void* tr);
typedef void*(*TransformGetChild_t)(void* tr, int32_t index);
typedef void*(*TransformGetParent_t)(void* tr);
typedef void(*SetDetectCollisions_t)(void* cc, bool detect);
typedef void(*SetFieldOfView_t)(void* cam, float fov);
typedef float(*GetFieldOfView_t)(void* cam);
typedef void*(*ObjectGetName_t)(void* obj);
typedef void(*RendererGetBounds_t)(void* renderer, Bounds* outBounds);
typedef void(*SetAmbientLight_t)(Color* color);
typedef void(*SetFog_t)(bool enabled);
typedef void(*SetFogDensity_t)(float density);

extern GameObjectFind_t oGameObjectFind;
extern GetMousePosition_t oGetMousePosition;
extern GetMouseButton_t oGetMouseButton;
extern GetInputString_t oGetInputString;
extern SetBehaviourEnabled_t oSetBehaviourEnabled;
extern GetBehaviourEnabled_t oGetBehaviourEnabled;
extern SetGameObjectActive_t oSetGameObjectActive;
extern GetGameObjectActive_t oGetGameObjectActive;
extern SetTimeScale_t oSetTimeScale;
extern SetFixedDeltaTime_t oSetFixedDeltaTime;
extern SetAudioListenerPitch_t oSetAudioListenerPitch;
extern SetAudioSourcePitch_t oSetAudioSourcePitch;
extern SetMasterTextureLimit_t oSetMasterTextureLimit;
extern GetWhiteTexture_t oGetWhiteTexture;
extern GetMainCamera_t oGetMainCamera;
extern WorldToScreenPoint_t oWorldToScreenPoint;
extern ComponentGetGameObject_t oComponentGetGameObject;
extern ComponentGetTransform_t oComponentGetTransform;
extern GameObjectGetTransform_t oGameObjectGetTransform;
extern TransformGetPosition_t oTransformGetPosition;
extern TransformSetPosition_t oTransformSetPosition;
extern TransformGetEulerAngles_t oTransformGetEulerAngles;
extern TransformSetEulerAngles_t oTransformSetEulerAngles;
extern TransformGetDirection_t oTransformGetForward;
extern TransformGetDirection_t oTransformGetRight;
extern TransformGetDirection_t oTransformGetUp;
extern TransformGetChildCount_t oTransformGetChildCount;
extern TransformGetChild_t oTransformGetChild;
extern TransformGetParent_t oTransformGetParent;
extern SetDetectCollisions_t oSetDetectCollisions;
extern SetFieldOfView_t oSetFieldOfView;
extern GetFieldOfView_t oGetFieldOfView;
extern ObjectGetName_t oObjectGetName;
extern RendererGetBounds_t oRendererGetBounds;
extern SetAmbientLight_t oSetAmbientLight;
extern SetFog_t oSetFog;
extern SetFogDensity_t oSetFogDensity;

extern void* g_OutlineTypeObj;
extern void* g_TransformTypeObj;
extern void* g_ComponentTypeObj;
extern void* g_CameraTypeObj;
extern void* g_AudioSourceTypeObj;
extern void* g_CharacterControllerTypeObj;
extern void* g_FindObjectsOfTypeMethod;
extern void* g_AddComponentMethod;
extern void* g_GetComponentMethod;
extern void* g_GetComponentsMethod;
extern void* g_GetComponentInChildrenMethod;
extern void* g_TransformGetParentMethod;
extern void* g_TransformGetChildCountMethod;
extern void* g_TransformGetChildMethod;
extern void* g_SetOutlineModeMethod;
extern void* g_SetOutlineColorMethod;
extern void* g_SetOutlineWidthMethod;
extern void* g_UpdateMatPropsMethod;

extern void* g_RendererTypeObj;
extern void* g_SkinnedMeshRendererTypeObj;
extern void* g_GetMaterialsMethod;
extern void* g_SetMainTextureMethod;
extern void* g_GetMainTextureMethod;
extern void* g_SetColorMethod;

bool InitIL2CPP();
uintptr_t GetLibBase64(const char* libName);
void* FindClass(const char* namespaze, const char* className);
void A64Hook(void* target, void* replace, void** original);
void* SetOutlineOnObject(void* go, bool enable, float* colorArr, float width);
bool IsNativeObjectAlive(void* unityObj);
void* GetCurrentCamera();
bool IsSceneReady();
void ShowSoftKeyboard(bool show);
void* SafeGetParentTransform(void* tr);
int32_t SafeGetChildCount(void* tr);
void* SafeGetChild(void* tr, int32_t index);
std::string GetUnityObjectName(void* unityObj);
std::string GetTransformPath(void* tr);
std::vector<void*> GetGameObjectComponents(void* go);
std::vector<void*> GetRootTransforms();
void CopyToAndroidClipboard(const char* text);
void SetFieldFloat(void* obj, void* klass, const char* fieldName, float val);
void SetFieldBool(void* obj, void* klass, const char* fieldName, bool val);
bool WorldToScreen(const Vector3& worldPos, Vector3& screenPos);
std::vector<void*> FindObjectsOfUnityType(void* typeObj);
// IL2CPP Type Enum
enum Il2CppTypeEnum {
    IL2CPP_TYPE_END = 0x00,
    IL2CPP_TYPE_VOID = 0x01,
    IL2CPP_TYPE_BOOLEAN = 0x02,
    IL2CPP_TYPE_CHAR = 0x03,
    IL2CPP_TYPE_I1 = 0x04,
    IL2CPP_TYPE_U1 = 0x05,
    IL2CPP_TYPE_I2 = 0x06,
    IL2CPP_TYPE_U2 = 0x07,
    IL2CPP_TYPE_I4 = 0x08,
    IL2CPP_TYPE_U4 = 0x09,
    IL2CPP_TYPE_I8 = 0x0a,
    IL2CPP_TYPE_U8 = 0x0b,
    IL2CPP_TYPE_R4 = 0x0c, // float
    IL2CPP_TYPE_R8 = 0x0d, // double
    IL2CPP_TYPE_STRING = 0x0e,
    IL2CPP_TYPE_PTR = 0x0f,
    IL2CPP_TYPE_BYREF = 0x10,
    IL2CPP_TYPE_VALUETYPE = 0x11,
    IL2CPP_TYPE_CLASS = 0x12,
    IL2CPP_TYPE_OBJECT = 0x1c,
    IL2CPP_TYPE_SZARRAY = 0x1d,
};

typedef void* (*il2cpp_class_get_fields_t)(void* klass, void** iter);
typedef const char* (*il2cpp_field_get_name_t)(void* field);
typedef void* (*il2cpp_field_get_type_t)(void* field);
typedef int (*il2cpp_type_get_type_t)(void* type);
typedef const char* (*il2cpp_type_get_name_t)(void* type);

typedef void* (*il2cpp_class_get_methods_t)(void* klass, void** iter);
typedef const char* (*il2cpp_method_get_name_t)(void* method);
typedef uint32_t (*il2cpp_method_get_param_count_t)(void* method);
typedef void* (*il2cpp_method_get_return_type_t)(void* method);

extern il2cpp_class_get_fields_t il2cpp_class_get_fields;
extern il2cpp_field_get_name_t il2cpp_field_get_name;
extern il2cpp_field_get_type_t il2cpp_field_get_type;
extern il2cpp_type_get_type_t il2cpp_type_get_type;
extern il2cpp_type_get_name_t il2cpp_type_get_name;

extern il2cpp_class_get_methods_t il2cpp_class_get_methods;
extern il2cpp_method_get_name_t il2cpp_method_get_name;
extern il2cpp_method_get_param_count_t il2cpp_method_get_param_count;
extern il2cpp_method_get_return_type_t il2cpp_method_get_return_type;
typedef void (*il2cpp_class_init_t)(void* klass);
extern il2cpp_class_init_t il2cpp_class_init;
typedef void* (*il2cpp_class_get_parent_t)(void* klass);
extern il2cpp_class_get_parent_t il2cpp_class_get_parent;