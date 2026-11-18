# NMMF - Norse Mod Menu Framework

One shared mod-settings menu for the Norse overhaul (NORSE: Oath of Blood, UE4SS v4.0.0), drawn with the game's own
UI: a **Mods** tab inside Norse's Options screen - at the title screen and in the pause menu during a mission - built at
runtime from the game's own widget classes (its on/off row `WBP_OptionsEntry_Toggle_C`, the owner's choice of switch,
and its slider row `WBP_OptionsEntry_Slider_C`). Not an overlay. Design survey: `4. plans/norse-realtime-combat-20261116/NMMF-SURVEY.md`;
the Lua spike that proved the route: `docs/spike`.

## For mods

With a DLL - the C API in `include/NMMF.h`:

```cpp
if (const nmmf::ApiTable* m = nmmf::Api()) {
    m->AddSwitch("MyMod", "Main", "bDodge", "Dodge roll", "Roll instead of a step.", 1, NMMF_GLOBAL);
    m->AddSlider("MyMod", "Main", "fSpeed", "Dodge speed", "How fast the roll is.", 0.5, 2.0, 1, 1.0, NMMF_GLOBAL);
    double speed = m->GetNumber("MyMod", "fSpeed");
}
```

Without a DLL - `pages\*.ini` (see `package\pages\README.txt`):

```ini
[MyMod]
Switch = bDodge | Dodge roll | Roll instead of a step. | true | global
Slider = fSpeed | Dodge speed | How fast the roll is. | 0.5 | 2.0 | 1 | 1.0 | global
```

A slider moves one unit of its last shown digit (decimals 0 -> 1, 1 -> 0.1). Values: global ones in
`settings\<mod>.ini` (written on change; defaults on the first run; under MO2 a new file lands in overwrite and deploys
with the next launch), per-save ones through NorseCoSave (owner `NMMF.<mod>`, reloaded on GameLoaded). Changes go to
subscribers (`Subscribe`) and the log. Console: `nmmf.list`, `nmmf.get <mod> <id>`, `nmmf.set <mod> <id> <value>`.

## How

Four times a second NMMF looks for live Options screens (`WBP_OptionsMenu_C` under /Engine/Transient - FindAllOf also
returns the class's template) and adds, once each: a `WBP_OptionsTab_C` with a VerticalBox in its Content slot, one row
per control (`WidgetBlueprintLibrary.Create`, Name/Description as `INVTEXT(...)`), the tab into `TabSwitcher`, and
"Mods" into `NavigatorPanel.Buttons` + `SetNavButtons()`. A switch is set once the row has its two options (`Toggle_On`
= option 0, found by its string-table key); a slider's step goes on the inner slider. While a screen is up, the rows
are read and a change is snapped, stored and announced.

0.1.0 tested in game (TEST-RESULT-0.1.0.txt). A slider with decimals writes its own number as an engine-made text
(the game's row shows whole numbers once moved; docs/DECIMAL-DISPLAY-FIX.txt). Since then: choice rows, section headers, translated labels and per-save values over a real save/load
(docs/TEST-RESULT-0.2-additions.txt). Not yet: a keybind row (Norse has none - needs the editor-built route), a
controller pass, translation files for each language (per mod).
