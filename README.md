<h1 align="center" id="title">Achievements</h1>

A mod for **Forza Horizon 2** on PC that brings back its 88 achievements. On Xbox they came from Xbox Live; running through Azimuth, the game has nowhere to send them, and the pause menu's ACHIEVEMENTS tile does nothing. With this mod you earn them offline as you play, each one with the game's own achievement pop-up, and the tile opens a full list of them in the game's own style.

What you've already done counts: the first time you launch, everything your save shows you've earned unlocks at once. The game's files and your save are never changed.

## Features

**All 88.** Forza Horizon 2's 50 (1,000G), Storm Island's 23 (500G) and the Porsche Expansion's 15 (500G), with their names, descriptions and Gamerscore as they were on Xbox Live.

**The game's pop-up.** Unlocking one shows the game's own HUD notification with its trophy, as Xbox did: "ACHIEVEMENT UNLOCKED, Rain Master - 10G". Four or more at once are rolled into one. There's a switch for them under Pause > Settings > MODS.

**The list.** Pause > ACHIEVEMENTS (and the Storm Island tab's ACHIEVEMENTS tile) opens a page in the style of the game's menus: sections down the left (your most recent unlocks, the festival and road trips, wristbands and skills, racing, cars, exploration, online, Storm Island, the Porsche Expansion), and the selected section's achievements on the right, each with its trophy, description, Gamerscore and a progress bar towards the locked ones. A goes into a section, B comes back out, and LB and RB step through the sections.

**The tile.** The ACHIEVEMENTS tile shows your Gamerscore, and takes the colour of the wristband you'd be wearing: the game's own wristband colours, a band for every tenth of the Gamerscore you can earn offline, through to Gold for all of it.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth. Download the latest zip from the [Releases](../../releases) page, close Azimuth, and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `d3dcompiler_47.dll` next to `azimuth.exe` and `Achievements.dll` inside a `fh2-mods` folder. Start the game.

### Updates

When a newer version is out, the title screen says so next to "PRESS (A) TO START". The mod asks GitHub at most every six hours; `CheckUpdates=0` in `fh2-mods\Achievements.ini` turns that off. The version you have is in the first line of `fh2-mods\Achievements\Achievements.log`.

### Using it with other FH2 mods

`d3dcompiler_47.dll` is the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), the shared base for FH2 mods. It loads every mod in the `fh2-mods` folder and lets them hook the game without stepping on each other. Every mod built on it ships the same file, so you can install them in any order and let them overwrite it. Achievements needs platform 2.7.0 or newer for its list and the tile (its zip has 2.7.1); [WreckingBall](https://github.com/Trinculo54/WreckingBall), [WheresMyBuddy](https://github.com/Trinculo54/WheresMyBuddy) and [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) are built on it too.

**Playing with a racing wheel.** [fh2wheel](https://github.com/iamAzoDx/fh2wheel) ships its own `D3DCOMPILER_47.dll`, and only one can sit next to `azimuth.exe`. Install fh2wheel first, then extract this zip and let its `d3dcompiler_47.dll` overwrite fh2wheel's. Keep `fh2wheel.dll` and `fh2wheel-config.exe` next to `azimuth.exe`: the platform loads fh2wheel itself.

### On Linux (Wine or Proton)

From 1.0.1 it works as it does on Windows. 1.0.0 read your save with Windows' built-in SQLite (`winsqlite3.dll`), which Wine and Proton don't have, so on Linux it never unlocked anything and its log said "winsqlite3.dll isn't available". Achievements you earned in the meantime are in your save, and 1.0.1 unlocks them on its first launch. Getting FH2 mods running on Linux in general is covered in the platform's [Linux notes](https://github.com/Trinculo54/FH2-Mod-Platform/blob/main/docs/linux.md).

## What counts

Most achievements are worked out from your save (read, never written): championships and their results, the road trip, wristbands from your XP, cars in your garage, skill points, reward and bonus boards, roads driven, barn finds, Horizon Promo photos, bucket list challenges, Storm Island and the Porsche Expansion's own progress, and each race's weather. Everything you did before installing counts towards those.

The rest count from the moment the mod is installed, from what happens as you play, because the save doesn't keep what they need:

- **Forza Horizon 2:** Horizon Leader (choosing where the Road Trip goes) and Fast and Clean (Clean Racing skills).
- **Storm Island:** Skills for Thrills, Ready to Bounce, and the five car achievements (Fiesta Fun, Galant Great, Metro Master, Ranger Rock Star, Stadium Star).
- **Porsche Expansion:** the event and skill ones: Horse Power, This Is My Porsche, the five "4 events in" cars, Animal!, Call 911, No Airbag Required and Widow Maker.

A Wild Drivatar Appears! does both: bounty wins from before the mod come from the save, and new ones are counted as you win them.

**Online play.** 16 achievements need online play: multiplayer events, clubs, Rivals, Showdowns at Car Meets, Online Road Trips, co-op Bucket List challenges and Storm Island's online events. The list shows them as ONLINE ONLY. The ones with a stat in your profile are still tracked, in case online play comes back.

## Settings

`fh2-mods\Achievements.ini`, created the first time you launch:

| Setting | Default | What it does |
|---|---|---|
| `Popups` | `1` | `0` turns the pop-ups off. Also under Pause > Settings > MODS. |
| `CheckUpdates` | `1` | `0` stops the mod checking GitHub for a newer version. |

## Files

`fh2-mods\Achievements\` holds, for each profile:

- `User_<id>.ini`: what you've unlocked and when, and the counts the mod keeps for the achievements it counts as you play. Delete it to start over.
- `User_<id>.ini.bak`: the same file as it was when the session started. The mod only uses it if the `.ini` is damaged (after a crash or a power cut, say).
- `Achievements.txt`: the whole list as a text file, with your progress.
- `Achievements.log`: what the mod did this launch. A working launch reads your save ("read save revision ...") within a few seconds of the game starting. If the tile does nothing or nothing unlocks, look here, and in `fh2-mods\platform.log`.

## Build from source

Built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake; the platform is a git submodule.

```bash
git clone --recursive https://github.com/Trinculo54/fh2-achievements
./scripts/build.sh
```

`dist/` then holds the zip's contents. GitHub Actions does the same on every push, and a tag like `v1.0.0` attaches the zip to a release. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`; bump it before tagging.

How each achievement is worked out, and the game addresses the mod uses, are in the comments at the top of `src/rules.cpp` and `src/achievements.cpp`.

## License

GPLv3. It includes [SQLite](https://sqlite.org) (`third_party/sqlite`), which is in the public domain. Unofficial fan-made mod; not affiliated with Playground Games, Turn 10, Microsoft or the Azimuth developers.
