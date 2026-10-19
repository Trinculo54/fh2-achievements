<h1 align="center" id="title">Achievements</h1>

A mod for **Forza Horizon 2** on PC that brings back its 88 achievements. On Xbox they came from Xbox Live; running through Azimuth, the game has nowhere to send them, and the pause menu's ACHIEVEMENTS tile does nothing. With this mod you earn them offline as you play, each one with the game's own achievement pop-up, and the tile opens a full list of them in the game's own style.

What you've already done counts: the first time you launch, everything your save shows you've earned unlocks at once. The game's files and your save are never changed.

## Features

**All 88.** Forza Horizon 2's 50 (1,000G), Storm Island's 23 (500G) and the Porsche Expansion's 15 (500G), with their names, descriptions and Gamerscore as they were on Xbox Live.

**The game's pop-up.** Unlocking one shows the game's own HUD notification with its trophy, as Xbox did: "ACHIEVEMENT UNLOCKED, Rain Master - 10G". Four or more at once are rolled into one. There's a switch for them under Pause > Settings > MODS.

**The list.** Pause > ACHIEVEMENTS (and the Storm Island tab's ACHIEVEMENTS tile) opens a page in the style of the game's menus: sections down the left (your most recent unlocks, the festival and road trips, wristbands and skills, racing, cars, exploration, online, Storm Island, the Porsche Expansion), and the selected section's achievements on the right, each with its trophy, description, Gamerscore and a progress bar towards the locked ones. A goes into a section, B comes back out, and LB and RB step through the sections.

**The tile.** The ACHIEVEMENTS tile shows your Gamerscore, and takes the colour of the wristband you'd be wearing: the game's own wristband colours, a band for every tenth of the Gamerscore you can earn offline, through to Gold for all of it.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth, with [FH2ML](https://github.com/Refilewski/FH2ML), the Forza Horizon 2 mod loader, and Forza Horizon 2 Extra Options. Download the latest zip from the [Releases](../../releases) page, then either:

- in the FH2ML Manager, choose **Install mod** and pick the zip, or
- close Azimuth and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `mod.json` and `Achievements.dll` in `mods\achievements`.

Start the game.

**Coming from Achievements 1.0.0, from before FH2ML?** Let the FH2ML Manager remove the old mod loader when it offers to, before you start the game, so the two versions don't both run. The first launch of this version copies your settings and unlocks into `FH2ML\data\achievements`; the old files are left as they were.

### Updates

When a newer version is out, the title screen says so next to "PRESS (A) TO START". FH2ML asks GitHub at most every six hours; to turn that off for this mod, set `"updateChecks": false` for `"achievements"` under `"mods"` in `FH2ML\config\FH2ML.json`. The version you have is in the first line of `FH2ML\data\achievements\Achievements.log`.

### Using it with other FH2 mods

Achievements is an FH2ML mod. FH2ML loads every mod in the `mods` folder and lets them hook the game, change its text and database and add to its menus without stepping on each other, so FH2ML mods can be installed in any order. Each one keeps its own folder in `mods`, and its settings in `FH2ML\data`.

**Playing with a racing wheel.** Achievements doesn't put anything next to `azimuth.exe`, so [fh2wheel](https://github.com/iamAzoDx/fh2wheel) installs as its own instructions say.

## What counts

Most achievements are worked out from your save (read, never written): championships and their results, the road trip, wristbands from your XP, cars in your garage, skill points, reward and bonus boards, roads driven, barn finds, Horizon Promo photos, bucket list challenges, Storm Island and the Porsche Expansion's own progress, and each race's weather. Everything you did before installing counts towards those.

The rest count from the moment the mod is installed, from what happens as you play, because the save doesn't keep what they need:

- **Forza Horizon 2:** Horizon Leader (choosing where the Road Trip goes) and Fast and Clean (Clean Racing skills).
- **Storm Island:** Skills for Thrills, Ready to Bounce, and the five car achievements (Fiesta Fun, Galant Great, Metro Master, Ranger Rock Star, Stadium Star).
- **Porsche Expansion:** the event and skill ones: Horse Power, This Is My Porsche, the five "4 events in" cars, Animal!, Call 911, No Airbag Required and Widow Maker.

A Wild Drivatar Appears! does both: bounty wins from before the mod come from the save, and new ones are counted as you win them.

**Online play.** 16 achievements need online play: multiplayer events, clubs, Rivals, Showdowns at Car Meets, Online Road Trips, co-op Bucket List challenges and Storm Island's online events. The list shows them as ONLINE ONLY. The ones with a stat in your profile are still tracked, in case online play comes back.

## Settings

`FH2ML\data\achievements\Achievements.ini` (in your Azimuth folder), created the first time you launch:

| Setting | Default | What it does |
|---|---|---|
| `Popups` | `1` | `0` turns the pop-ups off. Also under Pause > Settings > MODS. |

Update checks are FH2ML's: see [Updates](#updates).

## Files

`FH2ML\data\achievements\`, which FH2ML keeps when you update or uninstall the mod, holds the settings above and, for each profile:

- `User_<id>.ini`: what you've unlocked and when, and the counts the mod keeps for the achievements it counts as you play. Delete it to start over.
- `Achievements.txt`: the whole list as a text file, with your progress.
- `Achievements.log`: what the mod did this launch. A working launch reads your save ("read save revision ...") within a few seconds of the game starting. If the tile does nothing or nothing unlocks, look here, and in `FH2ML\logs\FH2ML.log`.

## Build from source

Built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake, against FH2ML's v4 mod API (`third_party/fh2ml/FH2ML_v4.h`, MIT; FH2ML provides the services, so there's nothing else to build or link).

```bash
git clone https://github.com/Trinculo54/fh2-achievements
cd fh2-achievements
./scripts/build.sh
```

`dist/` then holds the zip's contents: `mods/achievements/` with `mod.json`, `Achievements.dll`, `README.txt` and `LICENSE.txt`. GitHub Actions does the same on every push, and a tag like `v1.1.0` attaches the zip to a release. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`, and the DLL and `mod.json` both take it from there; bump it before tagging.

How each achievement is worked out, and the game addresses the mod uses, are in the comments at the top of `src/rules.cpp` and `src/achievements.cpp`.

## License

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games, Turn 10, Microsoft or the Azimuth developers.
