NMMF pages
==========

Every *.ini here adds settings to the Mods tab in Norse's Options screen, for a mod that has no DLL of its own (a mod
with a DLL uses the C API in NMMF.h). One [section] per mod:

    [MyMod]
    Title = My Mod
    Switch = bDodge | Dodge roll | Roll instead of a step. | true | global
    Slider = fSpeed | Dodge speed | How fast the roll is. | 0.5 | 2.0 | 1 | 1.0 | global

Header = label                                          ; a section header
Switch = id | label | description | default (true/false) | global or save
Choice = id | label | description | option, option, ... | default option | global or save
Slider = id | label | description | min | max | decimals | default | global or save
A slider moves one unit of its last shown digit: decimals 0 -> 1, 1 -> 0.1, 2 -> 0.01. Values: global ones in
settings\<mod>.ini (written when changed; defaults written on the first run), "save" ones with the save game
(NorseCoSave). Console: nmmf.list, nmmf.get <mod> <id>, nmmf.set <mod> <id> <value>.

A label, description or option written as $key is translated: Translations\<Mod>_<language>.txt, one "$key<TAB>text"
per line (UTF-16LE with a BOM, or UTF-8); the game's language picks the file, English is the fallback.
