# Changelog

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
