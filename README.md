# Pact Supply Tracker

A [Nexus](https://raidcore.gg/Nexus) addon for **Guild Wars 2** that copies the current Pact Supply Network Agent waypoint route to your clipboard from a small in-game barrel button.

## Features

- **Daily Pact Supply route** - detects the current Pact Supply day and copies the matching waypoint list.
- **One-click clipboard copy** - click the barrel and paste the route directly into in-game chat.
- **Floating barrel button** - a compact transparent button that stays out of the way.
- **Moveable position** - hold `Alt` and drag the barrel to place it where you prefer.
- **Short copy cooldown** - after copying, the button enters a small randomized delay to avoid accidental repeated copies.
- **Self-contained release DLL** - barrel images are embedded as `PNG` resources, so no external image files are required.

## Installation

1. Install the [Nexus](https://raidcore.gg/Nexus) addon loader for Guild Wars 2.
2. Download or build `pact_supply_tracker.dll`.
3. Drop the DLL into your Guild Wars 2 `addons` folder, for example:

```text
Guild Wars 2/addons/
```

4. Launch the game with Nexus installed. The addon appears as `Pact Supply Tracker`.

## Usage

- Click the barrel to copy today's Pact Supply waypoint route.
- Paste the copied route into chat.
- Hold `Alt` and drag the barrel to move it.
- Wait for the short cooldown before copying again.

## Building from source

The project targets Windows x64 and builds with **Visual Studio 2022+** and **CMake 3.24+**.

```powershell
git clone --recurse-submodules https://github.com/gustavotoigo-max/pact-supply-tracker.git
cd pact-supply-tracker
```

If you cloned without `--recurse-submodules`, initialize the dependencies manually:

```powershell
git submodule update --init --recursive
```

Dependencies are tracked as git submodules:

- [Nexus API](https://github.com/RaidcoreGG/Nexus-API)
- [Dear ImGui](https://github.com/ocornut/imgui)

The main repository stores only the submodule gitlinks and `.gitmodules`; it does not vendor the Nexus or ImGui source files.

### Visual Studio

1. Open the project folder in Visual Studio 2022 with `File > Open > Folder...`.
2. Select the `msvc-release` CMake preset.
3. Build the `gw2_nexus_addon` target.

### PowerShell

```powershell
powershell -ExecutionPolicy Bypass -File .\build.ps1 -Configuration Release
```

The release DLL is generated at:

```text
build/msvc-release/bin/Release/pact_supply_tracker.dll
```

## Project structure

- `src/addon.cpp` - addon entry point, Nexus load/unload hooks, ImGui rendering, clipboard copy, and day selection logic.
- `include/pact_supply_data.hpp` - Pact Supply route text by weekday.
- `include/shared.hpp` - shared addon metadata and Nexus API access.
- `include/resource.h` and `resources.rc` - embedded PNG resource IDs and declarations.
- `barrel.png` and `barrel_hover.png` - source images embedded into the DLL at build time.
- `build.ps1` - helper script for configuring and building Debug or Release presets.

## Credits

- Author: **Nahar.5349**
- Built on the [Nexus](https://raidcore.gg/Nexus) addon framework by Raidcore.

> *Guild Wars 2 and all associated trademarks are property of ArenaNet / NCSoft. This is an unofficial, fan-made addon.*

