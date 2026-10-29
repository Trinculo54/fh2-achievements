<h1 align="center" id="title">Achievements</h1>

A mod for **Forza Horizon 2** on PC that brings back its 88 achievements. On Xbox they came from Xbox Live; running through Azimuth, the game has nowhere to send them, and the pause menu's ACHIEVEMENTS tile does nothing. With this mod you earn them offline as you play, each one with the game's own achievement pop-up, and the tile opens a full list of them in the game's own style.

What you've already done counts: the first time you launch, everything your save shows you've earned unlocks at once. The game's files and your save are never changed.

## Features

**All 88.** Forza Horizon 2's 50 (1,000G), Storm Island's 23 (500G) and the Porsche Expansion's 15 (500G), with their names, descriptions and Gamerscore as they were on Xbox Live.

**The game's pop-up.** Unlocking one shows the game's own HUD notification with its trophy, as Xbox did: "ACHIEVEMENT UNLOCKED, Rain Master - 10G". Four or more at once are rolled into one. There's a switch for them under Pause > Settings > MODS.

**The list.** Pause > ACHIEVEMENTS (and the Storm Island tab's ACHIEVEMENTS tile) opens a page in the style of the game's menus: sections down the left (your most recent unlocks, the festival and road trips, wristbands and skills, racing, cars, exploration, online, Storm Island, the Porsche Expansion), and the selected section's achievements on the right, each with its trophy, description, Gamerscore and a progress bar towards the locked ones. A goes into a section, B comes back out, and LB and RB step through the sections.

**The tile.** The ACHIEVEMENTS tile shows your Gamerscore, and takes the colour of the wristband you'd be wearing: the game's own wristband colours, a band for every tenth of the Gamerscore you can earn offline, through to Gold for all of it.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth (from Azimuth 1.0.3, the platform is the `DWrite.dll` in this zip). Download the latest zip from the [Releases](../../releases) page, close Azimuth, and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `d3dcompiler_47.dll` and `DWrite.dll` next to `azimuth.exe`, and `Achievements.dll` inside a `fh2-mods` folder. Start the game.

### Updates

When a newer version is out, the title screen says so next to "PRESS (A) TO START". The mod asks GitHub at most every six hours; `CheckUpdates=0` in `fh2-mods\Achievements.ini` turns that off. The version you have is in the first line of `fh2-mods\Achievements\Achievements.log`.

### Using it with other FH2 mods

`d3dcompiler_47.dll` and `DWrite.dll` are the same file: the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), the shared base for FH2 mods. Azimuth 1.0.3 loads `DWrite.dll`, older versions `d3dcompiler_47.dll`, and where both load only one of them runs. It loads every mod in the `fh2-mods` folder and lets them hook the game without stepping on each other. Each mod's zip ships the platform it was built with, and older ones only have `d3dcompiler_47.dll`. So extract Achievements last, or extract it again after installing a mod with an older platform. Achievements needs platform 2.7.0 or newer for its list and the tile (its zip has 2.8.0). If another mod's zip puts an older platform back, the title screen says "Old FH2 Mod Platform: reinstall Achievements" under "PRESS (A) TO START". With a platform older than 2.6.0 (WheresMyBuddy 1.7.0's zip has 2.5.0), Achievements doesn't start at all, and its log says it "needs FH2 Mod Platform 2.6.0 or newer". Either way, extract this zip again. [WreckingBall](https://github.com/Trinculo54/WreckingBall), [WheresMyBuddy](https://github.com/Trinculo54/WheresMyBuddy) and [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) are built on it too.

**Playing with a racing wheel.** [fh2wheel](https://github.com/iamAzoDx/fh2wheel) ships its own `D3DCOMPILER_47.dll`, and only one can sit next to `azimuth.exe`. Install fh2wheel first, then extract this zip and let its `d3dcompiler_47.dll` overwrite fh2wheel's. Keep `fh2wheel.dll` and `fh2wheel-config.exe` next to `azimuth.exe`: the platform loads fh2wheel itself. Azimuth 1.0.3 doesn't load any `d3dcompiler_47.dll`, so fh2wheel's own loader doesn't run there, but the platform's `DWrite.dll` still loads `fh2wheel.dll`.

### On Linux (Wine or Proton)

From 1.0.1 it works as it does on Windows. 1.0.0 read your save with Windows' built-in SQLite (`winsqlite3.dll`), which Wine and Proton don't have, so on Linux it never unlocked anything and its log said "winsqlite3.dll isn't available". Achievements you earned in the meantime are in your save, and 1.0.1 unlocks them on its first launch. On Azimuth 1.0.3 the platform is `DWrite.dll`, and Wine has its own copy of that: if `fh2-mods\platform.log` never appears, add `dwrite` to the DLL override, `WINEDLLOVERRIDES="winmm,dwrite=n,b"`. Getting FH2 mods running on Linux in general is covered in the platform's [Linux notes](https://github.com/Trinculo54/FH2-Mod-Platform/blob/main/docs/linux.md).

### Uninstall

Delete `fh2-mods\Achievements.dll`, `fh2-mods\Achievements.ini` and the `fh2-mods\Achievements` folder. Delete `d3dcompiler_47.dll`, `DWrite.dll` and `fh2-mods` too if no other FH2 mods use them.

## What counts

Most achievements are worked out from your save (read, never written): championships and their results, the road trip, wristbands from your XP, cars in your garage, skill points, reward and bonus boards, roads driven, barn finds, Horizon Promo photos, bucket list challenges, Storm Island and the Porsche Expansion's own progress, and each race's weather. Everything you did before installing counts towards those.

The rest count from the moment the mod is installed, from what happens as you play, because the save doesn't keep what they need:

- **Forza Horizon 2:** Horizon Leader (choosing where the Road Trip goes) and Fast and Clean (Clean Racing skills).
- **Storm Island:** Skills for Thrills, Ready to Bounce, and the five car achievements (Fiesta Fun, Galant Great, Metro Master, Ranger Rock Star, Stadium Star).
- **Porsche Expansion:** the event and skill ones: Horse Power, This Is My Porsche, the five "4 events in" cars, Animal!, Call 911, No Airbag Required and Widow Maker.

A Wild Drivatar Appears! does both: bounty wins from before the mod come from the save, and new ones are counted as you win them.

**Online play.** 16 achievements need online play: multiplayer events, clubs, Rivals, Showdowns at Car Meets, Online Road Trips, co-op Bucket List challenges and Storm Island's online events. The list shows them as ONLINE ONLY. Online play works again through FH2MP, and 13 of them are tracked when you play there. Most come from your save, so online play from before the mod counts too; Bragging Rights, Loyal, Horizon 5-0 and Cup Car count from the moment the mod is installed. Online races in a Porsche count towards Horse Power, This Is My Porsche and the five "4 events in" cars too, as they did on Xbox. We Meet Again!, Super Meet Boy and Meet and Beat can't be earned yet: they need Showdowns at Car Meets, which FH2MP doesn't run. Yes We Can and Castaways need co-op Bucket Lists, and Horizon 5-0 online events on Storm Island, so those only move if FH2MP offers them. Where FH2MP is installed, Pause > Settings > MODS > Installed mods > Achievements has a "Load when playing online" switch: leave it On, or nothing is counted in online sessions.

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

If `Achievements.log` says it "needs FH2 Mod Platform 2.6.0 or newer", another mod's zip put an older platform back: extract this zip again. If there's no `Achievements.log` at all, or it wasn't written at this launch (check its time), the platform isn't loading Achievements: check that `d3dcompiler_47.dll` and `DWrite.dll` are both next to `azimuth.exe` (Azimuth 1.0.3 only loads `DWrite.dll`), and `Achievements.dll` is in `fh2-mods`. The first line of `platform.log` says which platform version is running. If `fh2-mods\platform-standby.log` is from your last launch, an older platform from another mod's zip ran instead of this one: extract this zip again. (That file stays once it's written, so check its date.)

The list, the tile and the MODS tab come from files the platform gives the game through the `mediaoverride` folder. Platform 2.7.3 and newer make the game read that folder when Extra Options' `winmm.dll` isn't there; with Extra Options, its `EnableMediaOverrides=1` in `FH2_ExtraOptions.ini` does it, so keep that on. The second line of `FH2_ExtraOptions.log` should start with `OK`. If it starts with `REFUSED` (for example "unsupported Azimuth build") or `DISABLED`, Extra Options changed nothing, so nothing reads the folder: install the Extra Options release made for your Azimuth version, or remove its `winmm.dll` so the platform reads the folder itself. In that case `platform.log` says "media overrides: Extra Options is here, so mediaoverride is left to it" and "game code is ready (Extra Options made no patches this time)".

## Build from source

Built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake; the platform is a git submodule.

```bash
git clone --recursive https://github.com/Trinculo54/fh2-achievements
./scripts/build.sh
```

`dist/` then holds the zip's contents. GitHub Actions does the same on every push, and a tag like `v1.0.0` attaches the zip to a release. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`; bump it before tagging.

How each achievement is worked out, and the game addresses the mod uses, are in the comments at the top of `src/rules.cpp` and `src/achievements.cpp`.

## Thanks

To [Laur1sXd](https://github.com/Laur1sXd), for the fixes to progress read from the save in [#2](https://github.com/Trinculo54/fh2-achievements/pull/2), which 1.0.2 builds on.

## License

GPLv3. It includes [SQLite](https://sqlite.org) (`third_party/sqlite`), which is in the public domain. Unofficial fan-made mod; not affiliated with Playground Games, Turn 10, Microsoft or the Azimuth developers.
