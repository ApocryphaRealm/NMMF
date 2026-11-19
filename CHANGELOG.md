# Changelog

## 1.0.0 - 2026-11-18 - untested

### Added
- First release. It contains everything built and tested in 0.1.0, listed here and kept as history below:
- A Mods tab in Norse's own Options screen (title and pause menu), built from the game's own on/off and slider rows.
- C API (include/NMMF.h): RegisterPage, AddSwitch, AddSlider, GetBool, GetNumber, Set, Subscribe, Unsubscribe, IsMenuOpen.
- pages/*.ini for mods without a DLL; settings/<mod>.ini (global) and NorseCoSave (per save).
- Console nmmf.list, nmmf.get, nmmf.set.
- Choice rows (the game's Cycle row), stored by the option's name: NMMF_AddChoice, NMMF_GetIndex, Choice = in pages.
- Section headers (the game's Banner row): NMMF_AddHeader, Header = in pages; each mod's settings start under its title.
- Translations: a label $key is read from Translations/<Mod>_<language>.txt (AMF format), language from the game's culture; NMMF_GetLanguage.
- Tested in game: per-save values over a real save and load (NorseCoSave).
- main.dll carries a Windows version resource (file and product version 1.0.0, shown in its Properties > Details).

### Fixed
- A slider with decimals shows them after it is moved too: the number is written as an engine-made text (Conv_StringToText -> SetText). The old write had been refused by a size check that expected a 24-byte FText; this engine build uses 16.

### Changed
- A slider with decimals shows them when the Options screen opens (the value is no longer re-sent every poll because of float rounding). After a click the number still shows rounded - known, under analysis.

### Known
- A slider with decimals shows its number rounded; the stored value is right.

## 0.1.0 - 2026-11-18 (tested in game: TEST-RESULT-0.1.0.txt)
### Added
- A Mods tab in Norse's own Options screen (title and pause menu), built from the game's own on/off and slider rows.
- C API (include/NMMF.h): RegisterPage, AddSwitch, AddSlider, GetBool, GetNumber, Set, Subscribe, Unsubscribe, IsMenuOpen.
- pages/*.ini for mods without a DLL; settings/<mod>.ini (global) and NorseCoSave (per save).
- Console nmmf.list, nmmf.get, nmmf.set.
### Known
- A slider with decimals shows its number rounded; the stored value is right.

### Changed (2026-11-18, refinement - partial)
- A slider with decimals shows them when the Options screen opens (the value is no longer re-sent every poll because of float rounding). After a click the number still shows rounded - known, under analysis.

### Fixed (2026-11-18)
- A slider with decimals shows them after it is moved too: the number is written as an engine-made text (Conv_StringToText -> SetText). The old write had been refused by a size check that expected a 24-byte FText; this engine build uses 16.

### Added (2026-11-18, second pass - tested in game: docs/TEST-RESULT-0.2-additions.txt)
- Choice rows (the game's Cycle row), stored by the option's name: NMMF_AddChoice, NMMF_GetIndex, Choice = in pages.
- Section headers (the game's Banner row): NMMF_AddHeader, Header = in pages; each mod's settings start under its title.
- Translations: a label "$key" is read from Translations/<Mod>_<language>.txt (AMF format), language from the game's culture; NMMF_GetLanguage.
### Tested
- Per-save values over a real save and load (NorseCoSave).
