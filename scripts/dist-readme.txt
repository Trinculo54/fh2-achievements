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

  - Forza Horizon 2 running through Azimuth (from Azimuth 1.0.3, the
    platform is the DWrite.dll in this zip).


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       DWrite.dll                    next to azimuth.exe
       fh2-mods\Achievements.dll
3. Start the game. Achievements you've already earned in your save
   unlock on the first launch, with one pop-up for all of them.

d3dcompiler_47.dll and DWrite.dll are the same file: the FH2 Mod
Platform, the shared base for FH2 mods
(https://github.com/Trinculo54/FH2-Mod-Platform). Azimuth 1.0.3 loads
DWrite.dll, older versions d3dcompiler_47.dll. It loads every mod in
fh2-mods. Each mod's zip ships the platform it was built with, and older
ones only have d3dcompiler_47.dll, so extract this zip last, or extract
it again after installing a mod with an older platform. The list and the
tile need platform 2.7.0 or newer (this zip has 2.8.0). If the title
screen says "Old FH2 Mod Platform: reinstall Achievements", another
mod's zip put an older platform back. With a platform older than 2.6.0
(WheresMyBuddy 1.7.0's zip has 2.5.0), Achievements doesn't start at
all, and its log says it "needs FH2 Mod Platform 2.6.0 or newer".
Either way, extract this zip again.


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
co-op Bucket Lists). They're listed as ONLINE ONLY. Online play works
again through FH2MP, and 13 of them are tracked there; Bragging Rights,
Loyal, Horizon 5-0 and Cup Car count from the moment the mod is
installed, and online races in a Porsche count for the Porsche ones too.
The three that need Showdowns can't be earned yet, and Yes We Can,
Castaways and Horizon 5-0 only move if FH2MP offers co-op Bucket Lists
and online races on Storm Island. With FH2MP installed, leave "Load when
playing online" On (the MODS tab > Installed mods > Achievements), or
nothing is counted online.


Files
~~~~~

  fh2-mods\Achievements.ini     settings (Popups=1, CheckUpdates=1)
  fh2-mods\Achievements\        per profile: what you've unlocked and
                                when (User_<id>.ini, with a copy from the
                                start of the session in User_<id>.ini.bak,
                                used only if the .ini is damaged), the list
                                as text (Achievements.txt) and a log

Delete your profile's User_<id>.ini to start over.

When a newer version is out, the title screen says so after
"PRESS (A) TO START".


If it doesn't work
~~~~~~~~~~~~~~~~~~

  - fh2-mods\Achievements\Achievements.log says "needs FH2 Mod Platform
    2.6.0 or newer": another mod's zip put an older platform back.
    Extract this zip again.
  - No Achievements.log at all, or one that wasn't written at this
    launch (check its time): the platform isn't loading Achievements.
    Check that d3dcompiler_47.dll and DWrite.dll are both next to
    azimuth.exe (Azimuth 1.0.3 only loads DWrite.dll), and
    Achievements.dll is in fh2-mods. The first line of
    fh2-mods\platform.log says which platform version is running. If
    fh2-mods\platform-standby.log is from your last launch, an older
    platform from another mod's zip ran instead of this one: extract
    this zip again. (That file stays once it's written, so check its
    date.)
  - The tile does nothing and there's no MODS tab: these come through
    the mediaoverride folder. With Extra Options installed, keep
    EnableMediaOverrides=1 in FH2_ExtraOptions.ini, and check that the
    second line of FH2_ExtraOptions.log starts with OK. If it starts
    with REFUSED (for example "unsupported Azimuth build") or DISABLED,
    Extra Options changed nothing, so nothing reads mediaoverride:
    install the Extra Options release made for your Azimuth version, or
    remove its winmm.dll so the platform reads the folder itself.
  - On Linux (Wine or Proton) with Azimuth 1.0.3, if
    fh2-mods\platform.log never appears, add dwrite to the DLL override:
    WINEDLLOVERRIDES="winmm,dwrite=n,b"


Uninstall
~~~~~~~~~

Delete fh2-mods\Achievements.dll, fh2-mods\Achievements.ini and the
fh2-mods\Achievements folder. Delete d3dcompiler_47.dll, DWrite.dll and
fh2-mods too if no other FH2 mods use them.


Thanks
~~~~~~

To Laur1sXd, for the fixes to progress read from the save (pull request
#2 on GitHub), which 1.0.2 builds on.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
