# TFC Refresh

An SKSE plugin for Skyrim Special Edition / Anniversary Edition.

## The problem

While the free camera (`tfc`) is active, the player character's model is not rebuilt
after equipment changes. Items equipped or removed (by you, a trap, a strip script, …)
stay invisible until free camera is turned off. Turning it off and on again shows the
change, but throws the camera back to the player and the shot is lost.

This is vanilla behaviour, regardless of how free camera was entered (console `tfc`,
`tfc 1`, or mods using PapyrusUtil's `ToggleFreeCamera`, e.g. SexLab's hotkey).

## What the plugin does

When the player equips or unequips something while free camera is active, it waits for
the changes to settle, briefly leaves free camera so the model is rebuilt, re-enters it
and restores the exact camera position, rotation and settings. If time was frozen
(`tfc 1`), it stays frozen. An optional hotkey triggers the same refresh manually, for
changes that aren't equip events.

The toggle uses the engine's `PlayerCamera::ToggleFreeCameraMode` directly (not the
console command), so the "TFC Mode" input context stack is left untouched, whichever way
free camera was entered.

No ESP, safe to add or remove mid-playthrough.

## Requirements

- SKSE64
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)

Built with CommonLibSSE-NG (address-library based). Tested on 1.6.1170; the library also
supports 1.7.x (incl. 1.7.104).

## Settings

`Data/SKSE/Plugins/TFCRefresh.ini`

| Key | Default | Meaning |
| --- | --- | --- |
| `bAutoRefreshOnEquip` | `1` | Refresh automatically on player equip/unequip in free camera |
| `iHotkey` | `0` | Manual refresh key (DirectInput scancode, decimal), only active in free camera; `0` = off |
| `iDebounceMs` | `250` | Wait after the last equip event, so a full strip refreshes once |
| `iGapMs` | `50` | Time spent outside free camera during a refresh |

Log: `Documents/My Games/Skyrim Special Edition/SKSE/TFCRefresh.log`

## Building

Requirements: Visual Studio 2022 Build Tools (C++ workload), CMake ≥ 3.25, Ninja,
[vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set.

```
git clone --recurse-submodules https://github.com/Cederien/TFCRefresh.git
cd TFCRefresh
build.cmd
```

Output: `build/TFCRefresh.dll`. Install it together with `TFCRefresh.ini` into
`Data/SKSE/Plugins/`.

## License

GPL-3.0, see [LICENSE](LICENSE). Uses [CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng).
