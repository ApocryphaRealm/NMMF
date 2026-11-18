// NMMF - Norse Mod Menu Framework (UE4SS v4.0.0 C++ mod for NORSE: Oath of Blood), 2026-11-18.
// One shared mod-settings menu, drawn with the game's own UI: a "Mods" tab added to every live Options screen
// (WBP_OptionsMenu_C: its TabSwitcher and NavigatorPanel), holding rows made from the game's own entry classes -
// WBP_OptionsEntry_Toggle_C (its on/off row: a left/right On/Off selector, the owner's choice 2026-11-18) and
// WBP_OptionsEntry_Slider_C. Proven by the Lua spike (docs/spike). API: include/NMMF.h. Survey: NMMF-SURVEY.md.
//
// Mods register through the C API, or without code through pages\*.ini (NorseManifest order):
//   [MyMod]                                   ; the mod's name (a page each)
//   Title = My Mod
//   Switch = bDodge | Dodge roll | Roll instead of a step. | true | global
//   Slider = fSpeed | Dodge speed | How fast the roll is. | 0.5 | 2.0 | 1 | 1.0 | global     ; min | max | decimals | default
// Values: global in settings\<mod>.ini (written on change; defaults written on first run), per-save through NorseCoSave
// (owner "NMMF.<mod>"; reloaded on GameLoaded). While an Options screen is up, the rows are read four times a second;
// a change is stored, logged and passed to subscribers. Console: nmmf.list, nmmf.get <mod> <id>, nmmf.set <mod> <id> <v>.
#include "NorseReflect.hpp"
#include "NorseManifest.hpp"
#include "NorseFilters.hpp"
#include "NorseEvents.h"
#include "NMMF.h"

#include <cmath>
#include <map>
#include <mutex>
#include <set>

namespace fs = std::filesystem;
using namespace nrn;
using namespace nrn::ue;
using namespace norse;

namespace
{
    fs::path g_mod;
    std::recursive_mutex g_mx;

    fs::path ModDir()
    {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&ModDir), &self);
        wchar_t buf[MAX_PATH * 2]{};
        GetModuleFileNameW(self, buf, static_cast<DWORD>(std::size(buf)));
        return fs::path(buf).parent_path().parent_path();
    }
    std::wstring PathOfObj(UObject* o)
    {
        std::wstring full = o->GetFullName(nullptr);
        return full.substr(full.find(L' ') + 1);
    }

    // ------------------------------------------------------------------ the model
    enum class CKind
    {
        Switch,
        Slider
    };
    struct Control
    {
        std::string mod, page, id, label, desc;
        CKind kind = CKind::Switch;
        int scope = 0;
        double min = 0, max = 1, step = 1, def = 0;
        int decimals = 0;
        double value = 0;
    };
    struct Page
    {
        std::string mod, page, title;
    };
    std::vector<Page> g_pages;
    std::vector<Control> g_controls; // in registration order
    struct Sub
    {
        int id;
        std::string mod;
        void (*cb)(const char*, const char*, const char*, void*);
        void* ctx;
    };
    std::vector<Sub> g_subs;
    int g_nextSub = 1;

    Control* Find(const std::string& mod, const std::string& id)
    {
        for (auto& c : g_controls)
            if (_stricmp(c.mod.c_str(), mod.c_str()) == 0 && _stricmp(c.id.c_str(), id.c_str()) == 0) return &c;
        return nullptr;
    }
    std::string Text(const Control& c)
    {
        if (c.kind == CKind::Switch) return c.value != 0 ? "true" : "false";
        char b[64];
        snprintf(b, sizeof(b), "%.*f", c.decimals, c.value);
        return b;
    }
    double Snap(const Control& c, double v)
    {
        if (c.kind == CKind::Switch) return v != 0 ? 1 : 0;
        v = std::clamp(v, c.min, c.max);
        double steps = std::round((v - c.min) / c.step);
        double r = c.min + steps * c.step;
        double p = std::pow(10.0, c.decimals);
        return std::round(r * p) / p;
    }
    double Parse(const Control& c, const std::string& s)
    {
        if (c.kind == CKind::Switch) return (_stricmp(s.c_str(), "true") == 0 || s == "1" || _stricmp(s.c_str(), "on") == 0) ? 1 : 0;
        return atof(s.c_str());
    }

    // ---- global storage: settings\<mod>.ini, id=value lines
    fs::path IniOf(const std::string& mod)
    {
        return g_mod / "settings" / (mod + ".ini");
    }
    std::map<std::string, std::string> ReadIni(const std::string& mod)
    {
        std::map<std::string, std::string> out;
        for (const auto& raw : ReadLines(IniOf(mod)))
        {
            std::wstring l = Trim(StripComment(raw));
            size_t eq = l.find(L'=');
            if (eq == std::wstring::npos || l.front() == L'[') continue;
            out[Narrow(Lower(Trim(l.substr(0, eq))))] = Narrow(Trim(l.substr(eq + 1)));
        }
        return out;
    }
    void WriteIni(const std::string& mod)
    {
        std::error_code ec;
        fs::create_directories(g_mod / "settings", ec);
        std::string s = "; NMMF settings for " + mod + " - written by the Options screen's Mods tab. Per-save values are not here.\n[" + mod + "]\n";
        for (const auto& c : g_controls)
            if (c.mod == mod && c.scope == NMMF_GLOBAL) s += c.id + "=" + Text(c) + "\n";
        FILE* f = nullptr;
        if (_wfopen_s(&f, IniOf(mod).c_str(), L"wb") == 0 && f)
        {
            fwrite(s.data(), 1, s.size(), f);
            fclose(f);
        }
    }

    // ---- per-save storage: NorseCoSave
    using NcsSetFn = int (*)(const char*, const char*, const char*);
    using NcsGetFn = int (*)(const char*, const char*, char*, int);
    template <typename F>
    F ModExport(const wchar_t* mod, const char* name)
    {
        HMODULE m = GetModuleHandleW((g_mod.parent_path() / mod / L"dlls" / L"main.dll").c_str());
        return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
    }
    bool SaveGet(const Control& c, std::string& out)
    {
        static NcsGetFn get = nullptr;
        if (!get) get = ModExport<NcsGetFn>(L"NorseCoSave", "NCS_Get");
        if (!get) return false;
        char buf[256]{};
        int n = get(("NMMF." + c.mod).c_str(), c.id.c_str(), buf, sizeof(buf));
        if (n < 0) return false;
        out = buf;
        return true;
    }
    void SaveSet(const Control& c)
    {
        static NcsSetFn set = nullptr;
        if (!set) set = ModExport<NcsSetFn>(L"NorseCoSave", "NCS_Set");
        if (set) set(("NMMF." + c.mod).c_str(), c.id.c_str(), Text(c).c_str());
        else WarnOnce("ncs", "a per-save value changed, but %s is not loaded - it lasts until the game closes", "NorseCoSave");
    }

    void Notify(const Control& c)
    {
        std::string v = Text(c);
        NRN_INFO("%s.%s = %s", c.mod.c_str(), c.id.c_str(), v.c_str());
        std::vector<Sub> subs;
        {
            std::lock_guard lk(g_mx);
            subs = g_subs;
        }
        for (const auto& s : subs)
            if (s.mod.empty() || _stricmp(s.mod.c_str(), c.mod.c_str()) == 0)
            {
                try
                {
                    s.cb(c.mod.c_str(), c.id.c_str(), v.c_str(), s.ctx);
                }
                catch (...)
                {
                    NRN_ERROR("a subscriber to %s threw", c.mod.c_str());
                }
            }
    }

    // Store a new value (already snapped): the INI or the co-save, then the subscribers
    void Store(Control& c, double v)
    {
        v = Snap(c, v);
        if (v == c.value) return;
        c.value = v;
        if (c.scope == NMMF_GLOBAL) WriteIni(c.mod);
        else SaveSet(c);
        Notify(c);
    }

    int AddControl(Control c)
    {
        std::lock_guard lk(g_mx);
        if (c.mod.empty() || c.id.empty())
        {
            NRN_WARN("a control with no mod or id - refused");
            return 0;
        }
        if (Find(c.mod, c.id))
        {
            NRN_WARN("%s.%s is already registered - refused", c.mod.c_str(), c.id.c_str());
            return 0;
        }
        if (c.kind == CKind::Slider)
        {
            c.decimals = std::clamp(c.decimals, 0, 4);
            c.step = std::pow(10.0, -c.decimals);
            if (!(c.max > c.min))
            {
                NRN_WARN("%s.%s: max must be above min - refused", c.mod.c_str(), c.id.c_str());
                return 0;
            }
        }
        c.def = Snap(c, c.def);
        c.value = c.def;
        bool known = false;
        if (c.scope == NMMF_GLOBAL)
        {
            auto ini = ReadIni(c.mod);
            auto it = ini.find(Narrow(Lower(Widen(c.id))));
            if (it != ini.end())
            {
                c.value = Snap(c, Parse(c, it->second));
                known = true;
            }
        }
        else
        {
            std::string v;
            if (SaveGet(c, v))
            {
                c.value = Snap(c, Parse(c, v));
                known = true;
            }
        }
        g_controls.push_back(c);
        if (c.scope == NMMF_GLOBAL && !known) WriteIni(c.mod); // rule 16: defaults written on the first run
        NRN_INFO("registered %s %s.%s (%s) = %s%s", c.kind == CKind::Switch ? "switch" : "slider", c.mod.c_str(), c.id.c_str(),
                 c.scope == NMMF_GLOBAL ? "global" : "per save", Text(c).c_str(), known ? "" : " (default)");
        return 1;
    }

    // ---- pages\*.ini: pages for mods with no DLL
    void LoadPages()
    {
        fs::path dir = g_mod / "pages";
        std::vector<fs::path> files;
        std::error_code ec;
        if (fs::is_directory(dir, ec))
            for (const auto& f : fs::directory_iterator(dir, ec))
                if (f.is_regular_file() && Lower(f.path().extension().wstring()) == L".ini") files.push_back(f.path());
        nm::Manifest man;
        man.Resolve(g_mod.parent_path().parent_path());
        files = man.Arrange(files, [](const std::string& s) { NRN_INFO("load order: %s", s.c_str()); });
        for (const auto& f : files)
        {
            std::string mod;
            int n = 0;
            for (const auto& raw : ReadLines(f))
            {
                ++n;
                std::wstring l = Trim(StripComment(raw));
                if (l.empty()) continue;
                std::string where = Narrow(f.filename().wstring()) + ":" + std::to_string(n);
                if (l.front() == L'[' && l.back() == L']')
                {
                    mod = Narrow(Trim(l.substr(1, l.size() - 2)));
                    std::lock_guard lk(g_mx);
                    g_pages.push_back({mod, "Main", mod});
                    continue;
                }
                size_t eq = l.find(L'=');
                if (mod.empty() || eq == std::wstring::npos)
                {
                    NRN_WARN("%s: not a [Mod] or key = value line - skipped", where.c_str());
                    continue;
                }
                std::wstring k = Lower(Trim(l.substr(0, eq)));
                auto p = SplitList(Trim(l.substr(eq + 1)), L'|');
                auto at = [&](size_t i) { return i < p.size() ? Narrow(p[i]) : std::string(); };
                auto scope = [&](size_t i) { return _wcsicmp((i < p.size() ? p[i] : L"").c_str(), L"save") == 0 ? NMMF_PER_SAVE : NMMF_GLOBAL; };
                if (k == L"title")
                {
                    std::lock_guard lk(g_mx);
                    g_pages.back().title = at(0);
                }
                else if (k == L"switch")
                {
                    Control c{mod, "Main", at(0), at(1), at(2), CKind::Switch, scope(4)};
                    c.def = (_stricmp(at(3).c_str(), "true") == 0 || at(3) == "1") ? 1 : 0;
                    AddControl(c);
                }
                else if (k == L"slider")
                {
                    Control c{mod, "Main", at(0), at(1), at(2), CKind::Slider, scope(7)};
                    c.min = atof(at(3).c_str());
                    c.max = atof(at(4).c_str());
                    c.decimals = atoi(at(5).c_str());
                    c.def = atof(at(6).c_str());
                    AddControl(c);
                }
                else NRN_WARN("%s: unknown key '%s' (Title, Switch, Slider; Choice and Keybind come in 0.2)", where.c_str(), Narrow(k).c_str());
            }
        }
    }

    // ------------------------------------------------------------------ the widgets
    UObject* Cls(const wchar_t* path)
    {
        return FindObj(path);
    }
    bool SetField(UObject* o, const wchar_t* name, const std::wstring& text)
    {
        FProperty* p = FindProp(reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(o)), name);
        if (!p) return false;
        bool faulted = false;
        return SehImport(p, text.c_str(), reinterpret_cast<uint8_t*>(o) + p->GetOffset_Internal(), o, &faulted) && !faulted;
    }
    template <typename T>
    bool ReadField(UObject* o, const wchar_t* name, T& out)
    {
        FProperty* p = FindProp(reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(o)), name);
        if (!p) return false;
        std::memcpy(&out, reinterpret_cast<uint8_t*>(o) + p->GetOffset_Internal(), sizeof(T));
        return true;
    }
    UObject* ObjField(UObject* o, const wchar_t* name)
    {
        UObject* v = nullptr;
        return ReadField(o, name, v) ? v : nullptr;
    }
    std::wstring Quoted(const std::string& s)
    {
        // Unreal's literal-text form: a plain quoted string kept its quotes on screen (NMMF 0.1.0 first run)
        std::wstring w = Widen(s), out = L"INVTEXT(\"";
        for (wchar_t ch : w) out += (ch == L'"' || ch == L'\\') ? std::wstring(L"\\") + ch : std::wstring(1, ch);
        return out + L"\")";
    }
    UObject* Call1(UObject* o, const wchar_t* fn, UObject* arg, uint16_t size, size_t retOff)
    {
        Buf b;
        std::memcpy(b.b, &arg, 8);
        if (!Reflect::Get().Call(o, fn, b.b, size, sizeof(b.b))) return nullptr;
        UObject* r = nullptr;
        std::memcpy(&r, b.b + retOff, 8);
        return r;
    }
    UObject* CreateWidget(UObject* ctx, UObject* cls)
    {
        static UObject* wbl = FindObj(L"/Script/UMG.Default__WidgetBlueprintLibrary");
        if (!wbl || !cls) return nullptr;
        Buf b; // Create(WorldContextObject @0, WidgetType @8, OwningPlayer @16) -> @24
        std::memcpy(b.b, &ctx, 8);
        std::memcpy(b.b + 8, &cls, 8);
        if (!Reflect::Get().Call(wbl, L"Create", b.b, 32, sizeof(b.b))) return nullptr;
        UObject* r = nullptr;
        std::memcpy(&r, b.b + 24, 8);
        return r;
    }
    UObject* SpawnObject(UObject* cls, UObject* outer)
    {
        static UObject* gs = FindObj(L"/Script/Engine.Default__GameplayStatics");
        Buf b; // SpawnObject(ObjectClass @0, Outer @8) -> @16
        std::memcpy(b.b, &cls, 8);
        std::memcpy(b.b + 8, &outer, 8);
        if (!gs || !Reflect::Get().Call(gs, L"SpawnObject", b.b, 24, sizeof(b.b))) return nullptr;
        UObject* r = nullptr;
        std::memcpy(&r, b.b + 16, 8);
        return r;
    }
    int IntCall(UObject* o, const wchar_t* fn)
    {
        Buf b;
        if (!Reflect::Get().Call(o, fn, b.b, 4, sizeof(b.b))) return -1;
        int32_t v = 0;
        std::memcpy(&v, b.b, 4);
        return v;
    }

    struct Row
    {
        UObject* entry;
        int32_t idx;
        std::string mod, id;
        CKind kind;
        int onIndex = 0;
        bool ready = false; // a switch is set up once its options exist (the row fills them when it is constructed)
        double shown = -1e300; // the number last written into a decimal slider's text
        bool stepSet = false;  // the inner slider's step is set once: setting it each poll rewrote the row's number
    };
    struct Built
    {
        UObject* menu;
        int32_t idx;
        std::vector<Row> rows;
    };
    std::vector<Built> g_built;

    bool SetSwitch(UObject* e, bool on)
    {
        Buf b;
        b.b[0] = on ? 1 : 0;
        return Reflect::Get().Call(e, L"SetIsChecked", b.b, 1, sizeof(b.b));
    }

    UObject* MakeRow(UObject* menu, const Control& c, int& onIndex)
    {
        static UObject* toggleC = Cls(L"/Game/Norse/UI/Widgets/Core/OptionsMenu/WBP_OptionsEntry_Toggle.WBP_OptionsEntry_Toggle_C");
        static UObject* sliderC = Cls(L"/Game/Norse/UI/Widgets/Core/OptionsMenu/WBP_OptionsEntry_Slider.WBP_OptionsEntry_Slider_C");
        UObject* e = CreateWidget(menu, c.kind == CKind::Switch ? toggleC : sliderC);
        if (!e) return nullptr;
        SetField(e, L"Name", Quoted(c.label.empty() ? c.id : c.label));
        SetField(e, L"Description", Quoted(c.desc));
        if (c.kind == CKind::Switch)
        {
            onIndex = -1; // learnt in SetUpSwitch, once the row is constructed and has its options
        }
        else
        {
            wchar_t num[64];
            swprintf_s(num, L"%.6f", c.min);
            SetField(e, L"MinValue", num);
            swprintf_s(num, L"%.6f", c.max);
            SetField(e, L"MaxValue", num);
            swprintf_s(num, L"%.6f", c.step);
            SetField(e, L"StepSize", num);
            swprintf_s(num, L"%.6f", c.value);
            SetField(e, L"CurrentValue", num);
            SetField(e, L"IncludeCheckbox", L"False");
            SetField(e, L"ShowFractionalPart", c.decimals > 0 ? L"True" : L"False");
        }
        return e;
    }

    // The step goes on the slider inside the row (the spike: the row's StepSize alone left a click at 7.887). The
    // inner slider exists once the row is constructed, so this runs on the poll after building.
    void FixInnerSlider(UObject* e, const Control& c)
    {
        UObject* s = ObjField(e, L"Slider");
        if (!s) return;
        Buf b;
        float step = static_cast<float>(c.step);
        std::memcpy(b.b, &step, 4);
        Reflect::Get().Call(s, L"SetStepSize", b.b, 4, sizeof(b.b));
        SetField(s, L"MouseUsesStep", L"True");
    }

    // TextBlock.SetText with an FText built in the call's own parameter buffer (FText import of INVTEXT("...")).
    // The game's slider row shows whole numbers only - ShowFractionalPart true still showed "1" for 1.6 (NMMF 0.1.0) -
    // so a slider with decimals gets its number written here. The FText in the buffer is not destroyed: one small
    // text per change, while the screen is up.
    bool SetTextBlock(UObject* tb, const std::wstring& text)
    {
        UFunction* fn = tb ? Reflect::Get().Find(tb, L"SetText") : nullptr;
        if (!fn)
        {
            static bool said = false;
            if (!said) NRN_WARN("SetText route: no SetText on the text block (%p)", tb);
            said = true;
            return false;
        }
        FProperty* param = nullptr;
        ForEachProp(reinterpret_cast<UStruct*>(fn), [&](FProperty* p) {
            if (!param) param = p;
        });
        uint16_t ps = Reflect::Get().ParmsSizeOf(tb, L"SetText");
        // an FText is 16 bytes in this engine build (SetText parms 16) - the old guard wanted 0x18 and refused every call
        if (!param || ps < 0x10 || ps > 0x40)
        {
            static bool said = false;
            if (!said) NRN_WARN("SetText route: SetText parms %d, first param %p - refused", ps, param);
            said = true;
            return false;
        }
        // The plan after five attempts (docs/DECIMAL-DISPLAY-ANALYSIS.txt): the ENGINE makes the text -
        // KismetTextLibrary.Conv_StringToText(FString @0) -> FText @0x10 - and those bytes go into SetText's parameter
        // block as they are (SetText copies them into the TextBlock). The made FText is not released: a small leak per
        // change while the screen is up. Every step is logged once so a refusal is seen (the earlier helper never ran).
        static UObject* ktl = FindObj(L"/Script/Engine.Default__KismetTextLibrary");
        uint16_t cps = ktl ? Reflect::Get().ParmsSizeOf(ktl, L"Conv_StringToText") : 0;
        static bool told = false;
        if (!told)
        {
            told = true;
            NRN_INFO("SetText route: KismetTextLibrary %p, Conv_StringToText parms %d, SetText parms %d, its param at +%d", ktl, cps, ps,
                     param->GetOffset_Internal());
        }
        if (!ktl || cps < 0x20 || cps > 0x30) return false; // FString @0 (16) + FText @0x10 (16)
        struct FStr
        {
            const wchar_t* data;
            int32_t num, max;
        } str{text.c_str(), static_cast<int32_t>(text.size() + 1), static_cast<int32_t>(text.size() + 1)};
        Buf conv;
        std::memcpy(conv.b, &str, sizeof(str));
        if (!Reflect::Get().Call(ktl, L"Conv_StringToText", conv.b, cps, sizeof(conv.b)))
        {
            NRN_WARN("Conv_StringToText refused");
            return false;
        }
        Buf b;
        std::memcpy(b.b + param->GetOffset_Internal(), conv.b + 0x10, static_cast<size_t>(cps) - 0x10);
        bool ok = Reflect::Get().Call(tb, L"SetText", b.b, ps, sizeof(b.b));
        if (!ok) NRN_WARN("SetText refused (parms %d)", ps);
        return ok;
    }

    int OptionCount(UObject* e)
    {
        FProperty* p = FindProp(reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(e)), L"Options");
        int32_t n = 0;
        if (p) std::memcpy(&n, reinterpret_cast<uint8_t*>(e) + p->GetOffset_Internal() + 8, 4); // TArray: data, num
        return n;
    }
    // Once the row has its two options (filled when it is constructed - before that SetIsChecked meant nothing and
    // the first run showed both switches inverted): set the stored value and learn which index means on
    bool SetUpSwitch(Row& r, const Control& c)
    {
        if (OptionCount(r.entry) < 2) return false;
        // Which option reads "On": by its text, else option 0. SetIsChecked(true) put this row on option 1, which
        // displays "Off" (NMMF 0.1.0 second run), so the function's meaning is not trusted - the index is set directly.
        FProperty* op = FindProp(reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(r.entry)), L"Options");
        std::wstring opts = op ? Export(op, reinterpret_cast<uint8_t*>(r.entry) + op->GetOffset_Internal(), r.entry) : L"";
        size_t onAt = opts.find(L"Toggle_On"), offAt = opts.find(L"Toggle_Off"); // string-table keys: the same in every language
        r.onIndex = (onAt != std::wstring::npos && offAt != std::wstring::npos && offAt < onAt) ? 1 : 0;
        Buf b;
        int32_t want = c.value != 0 ? r.onIndex : 1 - r.onIndex;
        std::memcpy(b.b, &want, 4);
        Reflect::Get().Call(r.entry, L"SetSelectedIndex", b.b, 4, sizeof(b.b));
        int idx = -1;
        ReadField(r.entry, L"CurrentIndex", idx);
        r.ready = true;
        static bool shown = false;
        if (!shown) NRN_INFO("switch row options: %s", Narrow(opts.substr(0, 300)).c_str());
        shown = true;
        NRN_INFO("%s.%s: switch set %s (on = option %d, now option %d)", c.mod.c_str(), c.id.c_str(), c.value != 0 ? "on" : "off", r.onIndex, idx);
        return true;
    }

    void BuildIn(UObject* menu)
    {
        static UObject* tabC = Cls(L"/Game/Norse/UI/Widgets/Core/OptionsMenu/WBP_OptionsTab.WBP_OptionsTab_C");
        static UObject* vboxC = Cls(L"/Script/UMG.VerticalBox");
        UObject* sw = ObjField(menu, L"TabSwitcher");
        UObject* nav = ObjField(menu, L"NavigatorPanel");
        int tabs = sw ? IntCall(sw, L"GetChildrenCount") : -1;
        if (!sw || !nav || !tabC || !vboxC || tabs < 1)
        {
            static bool said = false;
            if (!said) NRN_WARN("the Options screen is not shaped as expected (switcher %p, navigator %p, tabs %d) - no Mods tab", sw, nav, tabs);
            said = true;
            return;
        }
        Built b{menu, IndexOf(menu), {}};
        UObject* tab = CreateWidget(menu, tabC);
        UObject* content = tab ? ObjField(tab, L"Content") : nullptr;
        UObject* tree = tab ? ObjField(tab, L"WidgetTree") : nullptr;
        UObject* vbox = tree ? SpawnObject(vboxC, tree) : nullptr;
        if (!tab || !content || !vbox || !Call1(content, L"SetContent", vbox, 16, 8))
        {
            NRN_ERROR("building the Mods tab failed (tab %p, content %p, box %p) - no Mods tab", tab, content, vbox);
            return;
        }
        int rows = 0;
        {
            std::lock_guard lk(g_mx);
            for (const Control& c : g_controls)
            {
                int onIndex = 0;
                UObject* e = MakeRow(menu, c, onIndex);
                if (!e || !Call1(vbox, L"AddChildToVerticalBox", e, 16, 8))
                {
                    NRN_WARN("%s.%s: the row could not be made", c.mod.c_str(), c.id.c_str());
                    continue;
                }
                b.rows.push_back({e, IndexOf(e), c.mod, c.id, c.kind, onIndex});
                ++rows;
            }
        }
        Call1(sw, L"AddChild", tab, 16, 8);
        // the tab bar: append "Mods" to the navigator's Buttons (TArray<FText>) and rebuild it
        FProperty* bp = FindProp(reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(nav)), L"Buttons");
        if (bp)
        {
            void* addr = reinterpret_cast<uint8_t*>(nav) + bp->GetOffset_Internal();
            std::wstring arr = Export(bp, addr, nav);
            std::wstring add = L"INVTEXT(\"Mods\")";
            std::wstring now = (arr.size() > 2) ? arr.substr(0, arr.size() - 1) + L"," + add + L")" : L"(" + add + L")";
            bool faulted = false;
            SehImport(bp, now.c_str(), addr, nav, &faulted);
            Buf nb;
            Reflect::Get().Call(nav, L"SetNavButtons", nb.b, 0, sizeof(nb.b));
        }
        int after = IntCall(sw, L"GetChildrenCount");
        NRN_INFO("Mods tab added to %s: %d row(s), tabs %d -> %d", Narrow(ShortName(PathOfObj(menu))).c_str(), rows, tabs, after);
        g_built.push_back(b);
    }

    bool g_menuOpen = false;

    void Poll()
    {
        // new live Options screens (not the class's template, which FindAllOf also returns)
        std::vector<UObject*> menus;
        try
        {
            RC::Unreal::UObjectGlobals::FindAllOf(L"WBP_OptionsMenu_C", menus);
        }
        catch (...)
        {
        }
        g_built.erase(std::remove_if(g_built.begin(), g_built.end(), [](const Built& b) { return !Alive(b.menu, b.idx); }), g_built.end());
        for (UObject* m : menus)
        {
            if (!Alive(m, IndexOf(m)) || PathOfObj(m).find(L"/Engine/Transient") == std::wstring::npos) continue;
            if (std::any_of(g_built.begin(), g_built.end(), [&](const Built& b) { return b.menu == m; })) continue;
            if (g_controls.empty()) continue;
            try
            {
                BuildIn(m);
            }
            catch (...)
            {
                NRN_ERROR("building the Mods tab threw");
            }
        }
        // read the rows of any screen that is up
        bool open = false;
        for (Built& b : g_built)
        {
            Buf v;
            bool visible = Reflect::Get().Call(b.menu, L"IsVisible", v.b, 1, sizeof(v.b)) && v.b[0];
            if (!visible) continue;
            open = true;
            for (Row& r : b.rows)
            {
                if (!Alive(r.entry, r.idx)) continue;
                std::lock_guard lk(g_mx);
                Control* c = Find(r.mod, r.id);
                if (!c) continue;
                if (r.kind == CKind::Switch)
                {
                    if (!r.ready && !SetUpSwitch(r, *c)) continue;
                    int idx = -1;
                    if (ReadField(r.entry, L"CurrentIndex", idx) && idx >= 0) Store(*c, idx == r.onIndex ? 1 : 0);
                }
                else
                {
                    if (!r.stepSet && ObjField(r.entry, L"Slider"))
                    {
                        FixInnerSlider(r.entry, *c);
                        r.stepSet = true;
                        r.shown = -1e300; // the game's handler may have rewritten the number: write it again
                    }
                    double d = 0;
                    if (ReadField(r.entry, L"CurrentValue", d))
                    {
                        double snapped = Snap(*c, d);
                        Store(*c, snapped);
                        // show the stepped value on the row too - but only for a real difference: the slider keeps a float,
                        // so 1.6 reads back as 1.6000000238; a 1e-9 test re-sent the value every poll, and the row's own
                        // handler then rewrote the number as a whole one ("1") - the decimal display fault (refinement)
                        if (std::fabs(snapped - d) > c->step * 1e-3)
                        {
                            Buf s;
                            float f = static_cast<float>(snapped);
                            std::memcpy(s.b, &f, 4);
                            Reflect::Get().Call(r.entry, L"SetSliderValue", s.b, 4, sizeof(s.b));
                        }
                        if (c->decimals > 0)
                        {
                            // compare what the row SHOWS with what it should: the game's handler rewrites the number as a
                            // whole one when the mouse lets go of the slider, after this poll has written it (refinement)
                            UObject* tb = ObjField(r.entry, L"ValueText");
                            FProperty* tp = tb ? FindProp(reinterpret_cast<UStruct*>(Reflect::Get().ClassOf(tb)), L"Text") : nullptr;
                            std::wstring want = Widen(Text(*c));
                            std::wstring shownText = tp ? Export(tp, reinterpret_cast<uint8_t*>(tb) + tp->GetOffset_Internal(), tb) : L"";
                            bool same = shownText.find(L"\"" + want + L"\"") != std::wstring::npos;
                            bool wrote = false, ok = false;
                            if (!same)
                            {
                                wrote = true;
                                ok = SetTextBlock(tb, want);
                                if (ok) r.shown = c->value;
                            }
                            // ANALYSIS (rule 49, after four attempts): what the poll reads and does, for 3 s after a change
                            static std::map<std::string, ULONGLONG> watchUntil;
                            static std::map<std::string, double> lastVal;
                            std::string key = c->mod + "." + c->id;
                            ULONGLONG nowMs = GetTickCount64();
                            if (lastVal.count(key) == 0 || lastVal[key] != c->value)
                            {
                                lastVal[key] = c->value;
                                watchUntil[key] = nowMs + 3000;
                            }
                            if (nowMs < watchUntil[key])
                            {
                                std::wstring after = tp ? Export(tp, reinterpret_cast<uint8_t*>(tb) + tp->GetOffset_Internal(), tb) : L"";
                                NRN_DEBUG("ANALYSIS %s: value %s, export before '%s', want '%s', same %d, wrote %d ok %d, export after '%s'", key.c_str(),
                                         Text(*c).c_str(), Narrow(shownText).c_str(), Narrow(want).c_str(), same, wrote, ok, Narrow(after).c_str());
                            }
                        }
                    }
                }
            }
        }
        if (open != g_menuOpen)
        {
            g_menuOpen = open;
            NRN_INFO("Options screen %s", open ? "open" : "closed");
        }
    }

    // Per-save values come back with the save
    void OnEvent(const NE_Event* e, void*)
    {
        if (!e || !e->name || std::strcmp(e->name, "GameLoaded") != 0) return;
        std::lock_guard lk(g_mx);
        for (Control& c : g_controls)
        {
            if (c.scope != NMMF_PER_SAVE) continue;
            std::string v;
            double now = SaveGet(c, v) ? Snap(c, Parse(c, v)) : c.def;
            if (now != c.value)
            {
                c.value = now;
                Notify(c);
            }
        }
        g_built.clear(); // the rows show the old values; screens are rebuilt with the loaded ones
    }

    bool RunCommand(const std::wstring& line)
    {
        auto parts = SplitList(Trim(line), L' ');
        std::wstring cmd = Lower(parts[0]);
        NRN_INFO("command: %s", Narrow(line).c_str());
        if (cmd == L"nmmf.list")
        {
            std::lock_guard lk(g_mx);
            for (const auto& c : g_controls)
                NRN_INFO("  %s.%s (%s, %s) = %s", c.mod.c_str(), c.id.c_str(), c.kind == CKind::Switch ? "switch" : "slider",
                         c.scope == NMMF_GLOBAL ? "global" : "per save", Text(c).c_str());
            NRN_INFO("nmmf.list: %zu control(s), %zu Options screen(s) with a Mods tab", g_controls.size(), g_built.size());
        }
        else if (cmd == L"nmmf.get" && parts.size() >= 3)
        {
            std::lock_guard lk(g_mx);
            Control* c = Find(Narrow(parts[1]), Narrow(parts[2]));
            NRN_INFO("nmmf.get %s.%s = %s", Narrow(parts[1]).c_str(), Narrow(parts[2]).c_str(), c ? Text(*c).c_str() : "(not registered)");
        }
        else if (cmd == L"nmmf.set" && parts.size() >= 4)
        {
            std::lock_guard lk(g_mx);
            Control* c = Find(Narrow(parts[1]), Narrow(parts[2]));
            if (c)
            {
                Store(*c, Parse(*c, Narrow(parts[3])));
                g_built.clear(); // rebuilt with the new value at the next screen
            }
            else NRN_WARN("nmmf.set: %s.%s is not registered", Narrow(parts[1]).c_str(), Narrow(parts[2]).c_str());
        }
        else NRN_WARN("unknown command - nmmf.list | nmmf.get <mod> <id> | nmmf.set <mod> <id> <value>");
        return true;
    }
} // namespace

// ------------------------------------------------------------------ the C API (include/NMMF.h)
extern "C" __declspec(dllexport) uint32_t NMMF_GetAPIVersion()
{
    return 1;
}
extern "C" __declspec(dllexport) const char* NMMF_GetVersion()
{
    return NMMF_VERSION;
}
extern "C" __declspec(dllexport) int NMMF_RegisterPage(const char* mod, const char* page, const char* title)
{
    if (!mod) return 0;
    std::lock_guard lk(g_mx);
    g_pages.push_back({mod, page ? page : "Main", title ? title : mod});
    return 1;
}
extern "C" __declspec(dllexport) int NMMF_AddSwitch(const char* mod, const char* page, const char* id, const char* label, const char* desc, int defaultOn,
                                                    int scope)
{
    Control c{mod ? mod : "", page ? page : "Main", id ? id : "", label ? label : "", desc ? desc : "", CKind::Switch, scope};
    c.def = defaultOn ? 1 : 0;
    return AddControl(c);
}
extern "C" __declspec(dllexport) int NMMF_AddSlider(const char* mod, const char* page, const char* id, const char* label, const char* desc, double min,
                                                    double max, int decimals, double defaultValue, int scope)
{
    Control c{mod ? mod : "", page ? page : "Main", id ? id : "", label ? label : "", desc ? desc : "", CKind::Slider, scope};
    c.min = min;
    c.max = max;
    c.decimals = decimals;
    c.def = defaultValue;
    return AddControl(c);
}
extern "C" __declspec(dllexport) int NMMF_GetBool(const char* mod, const char* id)
{
    std::lock_guard lk(g_mx);
    Control* c = (mod && id) ? Find(mod, id) : nullptr;
    return c && c->value != 0 ? 1 : 0;
}
extern "C" __declspec(dllexport) double NMMF_GetNumber(const char* mod, const char* id)
{
    std::lock_guard lk(g_mx);
    Control* c = (mod && id) ? Find(mod, id) : nullptr;
    return c ? c->value : 0.0;
}
extern "C" __declspec(dllexport) int NMMF_Set(const char* mod, const char* id, const char* value)
{
    std::lock_guard lk(g_mx);
    Control* c = (mod && id && value) ? Find(mod, id) : nullptr;
    if (!c) return 0;
    Store(*c, Parse(*c, value));
    return 1;
}
extern "C" __declspec(dllexport) int NMMF_Subscribe(const char* mod, NMMF_Changed cb, void* ctx)
{
    if (!cb) return 0;
    std::lock_guard lk(g_mx);
    g_subs.push_back({g_nextSub, mod ? mod : "", cb, ctx});
    return g_nextSub++;
}
extern "C" __declspec(dllexport) void NMMF_Unsubscribe(int id)
{
    std::lock_guard lk(g_mx);
    g_subs.erase(std::remove_if(g_subs.begin(), g_subs.end(), [&](const Sub& s) { return s.id == id; }), g_subs.end());
}
extern "C" __declspec(dllexport) int NMMF_IsMenuOpen()
{
    return g_menuOpen ? 1 : 0;
}

class NMMF : public RC::CppUserModBase
{
  public:
    NMMF()
    {
        ModName = L"NMMF";
        ModVersion = L"" NMMF_VERSION;
        ModDescription = L"Norse Mod Menu Framework: a Mods tab in the game's own Options screen for every mod's settings.";
        ModAuthors = L"ApocryphaRealm";
        for (auto& s : g_nopVtbl) s = reinterpret_cast<void*>(&NopFn);
        g_errOut.vtbl = g_nopVtbl;
        g_mod = ModDir();
        std::wstring dir;
        for (const auto& raw : ReadLines(g_mod / "NMMF.ini"))
        {
            std::wstring l = Trim(raw);
            if (l.rfind(L"sLogDir=", 0) == 0) dir = Trim(l.substr(8));
        }
        Log::Get().Open((dir.empty() ? g_mod : fs::path(dir)) / "NMMF.log", 2);
        Log::Get().Write(Level::Error, "NMMF v%s", NMMF_VERSION);
        LoadPages();
    }

    auto on_unreal_init() -> void override
    {
        try
        {
            RC::Unreal::Hook::RegisterEngineTickPreCallback([](RC::Unreal::UEngine*, float) {
                static ULONGLONG next = 0, nextSub = 0;
                ULONGLONG now = GetTickCount64();
                if (now >= nextSub)
                {
                    nextSub = now + 2000;
                    static bool subscribed = false;
                    if (!subscribed)
                        if (const ne::ApiTable* api = ne::Api())
                        {
                            api->Subscribe("GameLoaded", &OnEvent, nullptr);
                            subscribed = true;
                        }
                }
                if (now < next) return;
                next = now + 250;
                try
                {
                    Poll();
                }
                catch (...)
                {
                    NRN_ERROR("poll threw");
                }
            });
            RC::Unreal::Hook::RegisterProcessConsoleExecCallback(
                    [](UObject*, const wchar_t* cmd, RC::Unreal::FOutputDevice&, UObject*) -> bool {
                        if (!cmd || _wcsnicmp(cmd, L"nmmf.", 5) != 0) return false;
                        static ULONGLONG last = 0;
                        static std::wstring lastCmd;
                        ULONGLONG now = GetTickCount64();
                        if (lastCmd == cmd && now - last < 250) return true;
                        last = now;
                        lastCmd = cmd;
                        return RunCommand(cmd);
                    });
        }
        catch (const std::exception& e)
        {
            NRN_ERROR("callback registration failed: %s", e.what());
        }
    }
};

extern "C" __declspec(dllexport) RC::CppUserModBase* start_mod()
{
    return new NMMF();
}

extern "C" __declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod)
{
    delete mod;
}
