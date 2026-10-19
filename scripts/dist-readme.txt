Achievements
============

Forza Horizon 2's 88 achievements, earned offline on PC (Azimuth), with
the game's own achievement pop-up. What you've already done in your save
counts. The pause menu's ACHIEVEMENTS tile, which did nothing on PC, opens
the full list, and shows your Gamerscore in the colour of a wristband.
The game's files and your save are never changed.

Project page and source: https://github.com/Trinculo54/fh2-achievements


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth.
  - FH2ML, the Forza Horizon 2 mod loader:
    https://github.com/Refilewski/FH2ML
  - Forza Horizon 2 Extra Options.


Install
~~~~~~~

With the FH2ML Manager: Install mod, and pick this zip.

Or by hand:
1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       mods\achievements\mod.json
       mods\achievements\Achievements.dll
3. Start the game. Achievements you've already earned in your save
   unlock on the first launch, with one pop-up for all of them.

Coming from Achievements 1.0.0, from before FH2ML? Let the FH2ML Manager
remove the old mod loader when it offers to, before you start the game,
so the two versions don't both run. The first launch of this version
copies your settings and unlocks over; the old files are left as they
were.


In the game
~~~~~~~~~~~

Pause > ACHIEVEMENTS (and the Storm Island tab's ACHIEVEMENTS tile) opens
the list: sections on the left, their achievements on the right, with
your progress towards the locked ones. A goes into a section, B back out,
LB and RB move between sections. The tile shows your Gamerscore, and its
colour is the wristband you'd be on: a band for every tenth of the
Gamerscore you can earn offline, Gold for all of it.

Pause > Settings > MODS has a switch for the pop-ups.


What counts
~~~~~~~~~~~

Most achievements are worked out from your save, so everything you did
before installing counts. These count from the moment the mod is
installed, because the save doesn't keep what they need: Horizon Leader,
Fast and Clean, Skills for Thrills, Ready to Bounce, the five Storm Island
car achievements (Fiesta Fun, Galant Great, Metro Master, Ranger Rock
Star, Stadium Star) and the Porsche Expansion's event and skill ones.

16 achievements need online play (multiplayer, clubs, Rivals, Showdowns,
co-op Bucket Lists). They're listed as ONLINE ONLY.


Files
~~~~~

FH2ML\data\achievements\ in your Azimuth folder holds what's yours, and
is kept when you update or uninstall the mod:

  Achievements.ini      settings (Popups=1)
  User_<id>.ini         per profile: what you've unlocked and when
  Achievements.txt      the list as text, with your progress
  Achievements.log      what the mod did this launch (FH2ML's own log
                        is FH2ML\logs\FH2ML.log)

Delete your profile's User_<id>.ini to start over.

When a newer version is out, the title screen says so after
"PRESS (A) TO START". To stop the mod checking, set
"updateChecks": false for "achievements" under "mods" in
FH2ML\config\FH2ML.json.


Uninstall
~~~~~~~~~

Uninstall it in the FH2ML Manager, or delete the mods\achievements
folder. Your unlocks stay in FH2ML\data\achievements; delete that folder
too if you want them gone.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
