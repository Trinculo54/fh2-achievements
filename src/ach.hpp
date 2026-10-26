// Achievements: Forza Horizon 2's 88 Xbox achievements, earned offline.
//
// On Xbox One the game never decided achievements itself: it sent gameplay events to Xbox Live
// (EtwPlus.dll's EtxEventWrite) and Xbox Live's rules unlocked them. This mod rebuilds those
// rules from three read-only sources:
//   - the newest save revision the game has finished writing (SaveState, ProfileData and
//     PlayerDatabase), read when the game starts and again after each autosave: that's what
//     gives credit for progress made before the mod was installed;
//   - the profile's named stats as the game changes them (SetStat), so stat-based ones unlock
//     straight away instead of at the next autosave;
//   - the gameplay events themselves, for the ones the save doesn't record (wins in a given car,
//     skills in a given car, a skill chain on Storm Island). Those count from when the mod is
//     installed, kept in its own file.
// It never writes to the save. Unlocks go to fh2-mods\Achievements\<user>.ini.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ach {

// ---- what the rules look at -------------------------------------------------------------

// From the game's database (media\Stripped\gamedbRC.slt), read once.
struct GameData {
    bool ok = false;
    std::map<int, int> champEventBucket;   // mainland championship race -> its car type (CarBucket)
    std::set<int> mainlandBuckets;         // every car type with a mainland championship
    int mainlandCampaigns = 0, islandCampaigns = 0;
    std::set<int> gauntletCampaigns;       // Storm Island's Gauntlet (IronMan) championships
    std::map<int, int> bucketList;         // bucket list challenge -> list
    std::set<int> porsches;                // car ids made by Porsche
    // Each championship, showcase and road trip race has its weather set (AnthemEventRaces:
    // CustomWeather, WeatherPreset, WeatherPrecipitationLevel, WeatherEnvironmentWetness).
    std::set<int> rainEvents;              // it rains (precipitation above 0)
    std::set<int> wetEvents;               // it rains, or the roads are wet
    std::set<int> stormEvents;             // a storm (presets StormB*, StormTropical*)
};

// From the newest finished save revision.
struct SaveFacts {
    bool ok = false;
    std::string revision;                                  // its folder name
    std::string pdbSha;                                    // PlayerDatabase's SHA-256 (from the commit file)
    std::map<std::string, double> stats;                   // ProfileData: "Main/XP" -> value
    std::map<std::string, std::map<std::string, std::string>> objects;   // SaveState: id -> property -> value
    std::map<std::string, std::string> objectType;         // SaveState: id -> type
    // PlayerDatabase
    std::set<int> champsCompleted, champsWon, islandChampsCompleted, islandChampsWon;
    std::map<int, int> eventPlace;                         // event -> best place (1 = won)
    std::vector<int> garage;                               // car id per garage slot
    int photos = 0;                                        // Horizon Promo cars photographed
    std::map<int, std::pair<bool, bool>> bucket;           // challenge -> (solo done, co-op done)
    std::string dbNotes;                                   // PlayerDatabase tables that couldn't be read
};

// From this session's hooks.
struct LiveFacts {
    std::map<std::string, double> stats;                   // stats the game set since it started
    std::set<std::string> boardsSmashed;                   // reward board ids smashed since it started
};

// Event-based progress, kept in the unlock file.
struct Counters {
    std::map<std::string, int64_t> v;
    int64_t get(const std::string& k) const {
        auto it = v.find(k);
        return it == v.end() ? 0 : it->second;
    }
    void add(const std::string& k, int64_t d) { v[k] += d; }
    void raise(const std::string& k, int64_t x) {
        auto& r = v[k];
        if (x > r) r = x;
    }
};

struct Facts {
    const GameData& gd;
    const SaveFacts& save;
    const LiveFacts& live;
    const Counters& ctr;

    double stat(const std::string& name) const;                         // live, else save, else 0
    std::string prop(const std::string& obj, const std::string& name) const;   // SaveState, "" if missing
    bool flag(const std::string& obj, const std::string& name) const { return prop(obj, name) == "true"; }
};

// ---- the achievements ----------------------------------------------------------------------

enum class Group { Base, StormIsland, Porsche };
enum class Track {
    Save,      // worked out from the save and stats (counts what was done before the mod)
    Events,    // counted from gameplay events since the mod was installed
    Online,    // needs online play (still tracked where a stat exists)
    Pending,   // not tracked yet
};

struct Progress {
    double have = 0, need = 1;
    bool done() const { return have >= need; }
};

struct Def {
    const char* id;
    const char* name;
    const char* desc;
    int gamerscore;
    Group group;
    Track track;
    const char* unit;                       // "roads", "boards"...: for the progress text
    Progress (*eval)(const Facts&);         // nullptr: nothing to measure yet
};

const std::vector<Def>& defs();

// ---- events --------------------------------------------------------------------------------

// One telemetry event, decoded from its EVENT_DATA_DESCRIPTORs. f[0..2] are the common fields,
// the user and the session; the event's own fields start at f[3].
struct EvField {
    uint8_t type = 0xFF;
    uint32_t size = 0;
    int64_t i = 0;
    double d = 0;
    std::string s;
};
struct Event {
    uint16_t id = 0;
    std::string name;
    std::vector<EvField> f;
    int64_t i(size_t k) const { return k < f.size() ? f[k].i : 0; }
    double d(size_t k) const { return k < f.size() ? f[k].d : 0; }
    const std::string& s(size_t k) const {
        static const std::string empty;
        return k < f.size() ? f[k].s : empty;
    }
    int64_t world() const { return f.empty() ? 0 : f.back().i; }   // 542 mainland, 549 Storm Island
};

// Event ids (the ETX descriptor's low 16 bits) this mod uses.
namespace ev {
constexpr uint16_t AnthemRaceEnd = 6, CarPurchased = 19, DistanceDrivenDelta = 33, IslandRewardBoardSmashed = 54,
                   IslandRoadsDriven = 55, IslandSkillChainCompleted = 57, RaceEncounterEnd = 97, RaceStartParts = 101,
                   RewardBoardSmashed = 103, RivalsEnd = 104, RoadTripCreated = 109, SkillAchieved = 115,
                   SkillChainCompleted = 116, WristbandGained = 129, XPGained = 131;
}
constexpr int64_t kWorldIsland = 549;

// Folds one event into the counters (and the live facts, for boards). Returns true if anything changed.
bool count_event(const Event& e, const GameData& gd, Counters& c, LiveFacts& live);
// Counts from the first save read, for rules that add what events count after it (kept with the
// counters, so taken once). Returns true if one was taken.
bool take_baselines(const SaveFacts& save, Counters& c);

// ---- the save ------------------------------------------------------------------------------

// The profile's save folder: <azimuth>\data\titles\1A96545B\saves\player-1\User_<id>, the one
// written to most recently if there are several.
// Optionally: how many profiles there are, and when the chosen one's revisions were last written
// (a FILETIME as 64 bits).
bool find_user_dir(const std::wstring& azimuthDir, std::wstring& userDir, std::wstring& user, std::string& why,
                   int* profiles = nullptr, uint64_t* written = nullptr);
// Its revision folder names, newest first.
std::vector<std::wstring> list_revisions(const std::wstring& userDir);
// Reads one revision. Its files are checked against its commit manifest first, so a revision
// the game is still writing is refused (try again later). `scratch` is a folder of ours for the
// copy of the database SQLite reads. With `prev` (the last revision read), an unchanged database
// isn't read again.
bool read_revision(const std::wstring& revisionDir, const std::wstring& scratch, const SaveFacts* prev, SaveFacts& out,
                   std::string& why);
bool read_game_data(const std::wstring& gameDir, GameData& out, std::string& why);

// ---- the in-game page (ui.cpp) -------------------------------------------------------------

// The Achievements page, as the platform's set_page_xaml takes it: XAML of the mod's own design.
struct PageXaml {
    std::string resources, content, help;
};
// The page: its sections on the left, the selected one's achievements on the right. `f` is null
// until a save has been read (then there's no progress to show); `unlocked` is id -> when.
PageXaml page_xaml(const Facts* f, const std::map<std::string, std::string>& unlocked);
// The page's sections, in order (slot 0 is the selected one; the first is the recent unlocks).
std::vector<std::string> section_titles();
// "" if every achievement is in exactly one section, otherwise what's wrong.
std::string section_check();
// The page's slots (FH2Mod_Slot_<n> in its XAML).
constexpr int kSlotSelected = 0;   // the section list's SelectedIndex: -1 while the focus is elsewhere
constexpr int kSlotPane     = 1;   // where the focus is: 0 the sections, 1 the shown one's achievements
constexpr int kSlotSection  = 2;   // the section shown: the last one selected

// ---- SQLite (Windows' own winsqlite3.dll) ----------------------------------------------------

struct Sql;
Sql* sql_open_readonly(const std::wstring& path, std::string& why);
void sql_close(Sql* db);
// Runs `query`; `row` gets each row's columns (NULL as 0). Returns false (with why) on an error.
using SqlRow = std::vector<int64_t>;
bool sql_ints(Sql* db, const char* query, const std::function<void(const SqlRow&)>& row, std::string& why);

}  // namespace ach
