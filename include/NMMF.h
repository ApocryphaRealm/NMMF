// NMMF.h - the public API of NMMF, the Norse Mod Menu Framework (2026-11-18). Plain C exports from
// ue4ss\Mods\NMMF\dlls\main.dll, so any mod (any compiler) can use them:
//
//   #include "NMMF.h"
//   if (const nmmf::ApiTable* m = nmmf::Api()) {
//       m->RegisterPage("MyMod", "Main", "My Mod");
//       m->AddSwitch("MyMod", "Main", "bDodge", "Dodge roll", "Roll instead of a step.", 1, NMMF_GLOBAL);
//       m->AddSlider("MyMod", "Main", "fSpeed", "Dodge speed", "How fast the roll is.", 0.5, 2.0, 1, 1.0, NMMF_GLOBAL);
//       double speed = m->GetNumber("MyMod", "fSpeed");
//   }
//
// Controls appear in a "Mods" tab inside Norse's own Options screen, drawn with the game's own rows (its on/off row and
// slider). A slider's step is one unit of its last shown digit: decimals 0 -> 1, 1 -> 0.1, 2 -> 0.01.
// Values: NMMF_GLOBAL in ue4ss\Mods\NMMF\settings\<mod>.ini; NMMF_PER_SAVE in the save's NorseCoSave side file
// (owner "NMMF.<mod>"). Registration may come before the menu exists; the rows are built whenever an Options screen
// is. 0.1.0: switches and sliders; choices, keybinds and headers are refused (logged) until 0.2.
#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>

extern "C"
{
    enum NMMF_Scope
    {
        NMMF_GLOBAL = 0,
        NMMF_PER_SAVE = 1,
    };
    typedef void (*NMMF_Changed)(const char* mod, const char* id, const char* newValue, void* ctx);

    typedef uint32_t (*NMMF_GetAPIVersionFn)(void);
    typedef const char* (*NMMF_GetVersionFn)(void);
    typedef int (*NMMF_RegisterPageFn)(const char* mod, const char* page, const char* title);
    typedef int (*NMMF_AddSwitchFn)(const char* mod, const char* page, const char* id, const char* label, const char* desc, int defaultOn, int scope);
    typedef int (*NMMF_AddSliderFn)(const char* mod, const char* page, const char* id, const char* label, const char* desc, double min, double max,
                                    int decimals, double defaultValue, int scope);
    typedef int (*NMMF_GetBoolFn)(const char* mod, const char* id);
    typedef double (*NMMF_GetNumberFn)(const char* mod, const char* id);
    typedef int (*NMMF_SetFn)(const char* mod, const char* id, const char* value);
    typedef int (*NMMF_SubscribeFn)(const char* mod, NMMF_Changed cb, void* ctx);
    typedef void (*NMMF_UnsubscribeFn)(int id);
    typedef int (*NMMF_IsMenuOpenFn)(void);
}

namespace nmmf
{
    struct ApiTable
    {
        NMMF_GetAPIVersionFn GetAPIVersion;
        NMMF_GetVersionFn GetVersion;
        NMMF_RegisterPageFn RegisterPage;
        NMMF_AddSwitchFn AddSwitch;
        NMMF_AddSliderFn AddSlider;
        NMMF_GetBoolFn GetBool;
        NMMF_GetNumberFn GetNumber;
        NMMF_SetFn Set;
        NMMF_SubscribeFn Subscribe;
        NMMF_UnsubscribeFn Unsubscribe;
        NMMF_IsMenuOpenFn IsMenuOpen;
    };

    // NMMF beside the calling mod (...\ue4ss\Mods\<this mod>\dlls\main.dll -> ...\ue4ss\Mods\NMMF\dlls\main.dll).
    // Null while NMMF is not loaded; call again later (mods load in name order).
    inline const ApiTable* Api()
    {
        static ApiTable t{};
        if (t.GetAPIVersion) return &t;
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&Api), &self);
        wchar_t buf[MAX_PATH * 2]{};
        GetModuleFileNameW(self, buf, static_cast<DWORD>(std::size(buf)));
        auto dll = std::filesystem::path(buf).parent_path().parent_path().parent_path() / L"NMMF" / L"dlls" / L"main.dll";
        HMODULE m = GetModuleHandleW(dll.c_str());
        if (!m) return nullptr;
        auto get = [&](const char* n) { return reinterpret_cast<void*>(GetProcAddress(m, n)); };
        t.GetAPIVersion = reinterpret_cast<NMMF_GetAPIVersionFn>(get("NMMF_GetAPIVersion"));
        t.GetVersion = reinterpret_cast<NMMF_GetVersionFn>(get("NMMF_GetVersion"));
        t.RegisterPage = reinterpret_cast<NMMF_RegisterPageFn>(get("NMMF_RegisterPage"));
        t.AddSwitch = reinterpret_cast<NMMF_AddSwitchFn>(get("NMMF_AddSwitch"));
        t.AddSlider = reinterpret_cast<NMMF_AddSliderFn>(get("NMMF_AddSlider"));
        t.GetBool = reinterpret_cast<NMMF_GetBoolFn>(get("NMMF_GetBool"));
        t.GetNumber = reinterpret_cast<NMMF_GetNumberFn>(get("NMMF_GetNumber"));
        t.Set = reinterpret_cast<NMMF_SetFn>(get("NMMF_Set"));
        t.Subscribe = reinterpret_cast<NMMF_SubscribeFn>(get("NMMF_Subscribe"));
        t.Unsubscribe = reinterpret_cast<NMMF_UnsubscribeFn>(get("NMMF_Unsubscribe"));
        t.IsMenuOpen = reinterpret_cast<NMMF_IsMenuOpenFn>(get("NMMF_IsMenuOpen"));
        if (!t.GetAPIVersion || t.GetAPIVersion() != 1 || !t.AddSwitch || !t.GetNumber)
        {
            t = {};
            return nullptr;
        }
        return &t;
    }
} // namespace nmmf
