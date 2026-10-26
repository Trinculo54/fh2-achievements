// The 88 achievements and how each is worked out. See ach.hpp for where the facts come from.
//
// Numbers checked against the game's data (24 Nov 2015 build):
//  - Wristbands follow total XP (Main/XP), from media\GameTunableSettings\PlayerXPLevels.xml:
//    Yellow 14 levels x 15,000, then 15 levels x 20,000 each for Green, Blue, Pink, Orange,
//    Purple, Red, Turquoise, Bronze, Silver and Gold. So Green starts at 210,000, Pink at
//    810,000, Purple at 1,410,000 and Gold at 2,910,000. (The game's WristbandGained event fired
//    exactly when a test save crossed 510,000, Blue.)
//  - 168 mainland championships and 30 on Storm Island (Campaigns.CareerLocationId 0 and 1); six
//    of the island ones are Gauntlets (race mode IronMan).
//  - 150 mainland reward boards (tracks\anthem\ribbon_00\GameObjs.xml: 50 fast travel and 100 XP,
//    saved as discount_board_travel_### and discount_board_xp_a/b/c_###) and 25 Storm Island
//    bonus boards (the DLC's test03 GameObjs.xml: 25 upgrade discount boards, saved as
//    island_discount_board_upgrades_###). A smashed board's State is Collected (OnSmashed,
//    0x14050F910). Once Storm Island's races have run, the save also lists 25
//    discount_board_upgrades_### without the island_ prefix: copies of the island's boards that
//    those routes keep hidden, which never become Collected, so they aren't counted. 10 mainland
//    barn finds and one on Storm Island (pdlc_barn_find_01).
//  - Main/NumSkillTokens is the total Skill Points earned. Buying a perk doesn't lower it; the
//    balance left to spend is SaveState skills.CurrentSkillTokensTotal. The 25 perks cost 65.
//  - Bucket lists 1-3 are the mainland's (15 challenges each: The Horizon, The Second Horizon and
//    Beat Playground Games), 4-5 Storm Island's (8), 6 the Porsche one (10). The mainland bucket
//    list achievements count the first two only.
//  - Names, descriptions and Gamerscore are the achievements' own text on Xbox Live.
//  - Showcase "Train in Vain" is event 92; the Horizon Finale is 201, Storm Island's 937.
//  - Car ids: Fiesta '81 2158, Galant VR-4 1381, Metro 6R4 2147, Ranger T6 2145, #7 SST 2143;
//    Porsche 911 Turbo 3.3 265, 914/6 266, 944 Turbo 268, 959 269, Carrera GT 292, 911 Turbo S
//    2289, 918 Spyder 2290, Cayman GTS 2293, Macan Turbo 2295, 911 GT3 RS 4.0 2297.
//  - Skills: "Clean Racing" is the FastAndClean skill (RaceFeats strings IDS_FastAndClean1-4).
//  - Storm Island has 50 roads: its IslandRoadsDriven event (sent with the roads found and the
//    total, 0x1409C1010) reported "all found" at 50.
//  - Weather: every championship, showcase and road trip race has its weather set in
//    AnthemEventRaces. 86 of the mainland's 672 championship races are in the rain, 35 of Storm
//    Island's 89; the storms are presets StormB (mainland) and StormTropical (Storm Island). The
//    save's Career_EventResults says which events were finished and the best place.
//  - Head-to-Head: RaceEncounterEnd (sent at 0x1409CD6F2) has [5] 1 for a win and [6] the
//    rareSpawn flag, which is the bounty Drivatar (a bounty win logged [6] 1).
//  - Road Trip: RoadTripCreated is sent for the RoadTripDestinationSelected message, which only
//    the destination screen (0x140E896B0) posts, as it sets CareerState.UserSelectedNextLocation.
//  - Rally parts: the game checks a car for Storm Island at 0x140E5C959 by its springs and
//    dampers level 4, tyre level 5 and transmission level 4 (the Rally parts in
//    List_Upgrade*.Level); RaceStartParts sends those same three at each race start.
//  - Showdowns are wager races, launched from a Car Meet into a network session (the car meet
//    states' targets are WagerRace, OnlineFreeRoam, OnlineRoadTrip...), so they need online play.
//    Rivals races ghosts from the leaderboards, also online; RivalsEnd has [7] car, [10] credits.
#include "ach.hpp"

#include <algorithm>
#include <cstdlib>

namespace ach {

double Facts::stat(const std::string& name) const {
    auto l = live.stats.find(name);
    if (l != live.stats.end()) return l->second;
    auto s = save.stats.find(name);
    return s == save.stats.end() ? 0 : s->second;
}

std::string Facts::prop(const std::string& obj, const std::string& name) const {
    auto o = save.objects.find(obj);
    if (o == save.objects.end()) return {};
    auto p = o->second.find(name);
    return p == o->second.end() ? std::string{} : p->second;
}

namespace {

Progress P(double have, double need) { return {std::min(have, need), need}; }
Progress Flag(bool b) { return {b ? 1.0 : 0.0, 1}; }

constexpr double kGreenXP = 210000, kPinkXP = 810000, kPurpleXP = 1410000, kGoldXP = 2910000;
constexpr int kTrainInVain = 92, kHorizonFinale = 201, kIslandFinale = 937;
constexpr int kMainlandRoads = 315, kIslandRoads = 50;
// The totals the achievements ask for, as the stock game database has them. Fixed here rather than
// counted from the database the mod reads, because other mods change that database: one that adds
// a championship or a challenge would otherwise put "all of them" out of reach.
constexpr int kMainlandChamps = 168, kIslandChamps = 30, kGauntlets = 6, kCarTypes = 28;
constexpr int kIslandChallenges = 8, kPorscheChallenges = 10, kMainlandBoards = 150, kIslandBoards = 25;

bool won_event(const Facts& f, int event) {
    auto it = f.save.eventPlace.find(event);
    return (it != f.save.eventPlace.end() && it->second == 1) || f.ctr.get("won." + std::to_string(event)) > 0;
}

// Reward boards smashed: SaveState's DiscountBoardSaveState objects, plus any smashed this session.
// The mainland's are the fast travel and XP boards; the island's have the island_ prefix.
int boards(const Facts& f, bool island) {
    int n = 0;
    for (const auto& [id, type] : f.save.objectType) {
        if (type != "DiscountBoardSaveState") continue;
        const bool counts = island ? id.rfind("island_", 0) == 0
                                   : id.rfind("discount_board_travel_", 0) == 0 || id.rfind("discount_board_xp_", 0) == 0;
        if (counts && (f.prop(id, "State") == "Collected" || f.live.boardsSmashed.count(id))) ++n;
    }
    return n;
}

bool barn_restored(const Facts& f, const std::string& id) {
    const std::string state = f.prop(id, "State");
    return state == "Collected" || state == "Restored";
}

// The 10 mainland barn finds; Storm Island's is pdlc_barn_find_01.
int barns_restored(const Facts& f) {
    int n = 0;
    for (int i = 1; i <= 10; ++i) {
        char id[32];
        std::snprintf(id, sizeof id, "barn_find_%02d", i);
        if (barn_restored(f, id)) ++n;
    }
    return n;
}

// Bucket list challenges done, solo or co-op, in the given lists.
struct Bucket {
    int solo = 0, coop = 0, total = 0;
};
Bucket bucket(const Facts& f, std::initializer_list<int> lists) {
    Bucket b;
    for (const auto& [ch, list] : f.gd.bucketList) {
        if (std::find(lists.begin(), lists.end(), list) == lists.end()) continue;
        ++b.total;
        auto it = f.save.bucket.find(ch);
        if (it == f.save.bucket.end()) continue;
        b.solo += it->second.first;
        b.coop += it->second.second;
    }
    return b;
}

int car_types_won(const Facts& f) {
    std::set<int> types;
    for (const auto& [event, bucket] : f.gd.champEventBucket)
        if (won_event(f, event)) types.insert(bucket);
    return static_cast<int>(types.size());
}

Progress wristband(const Facts& f, double xp) { return P(f.stat("Main/XP"), xp); }

// ---- the rules, one per achievement -------------------------------------------------------

#define RULE(name) Progress name(const Facts& f)

RULE(r_festival) { return Flag(f.flag("iestate", "IntroducedFestival")); }
RULE(r_sisteron) { return Flag(f.flag("iestate", "IntroducedSisteron")); }
RULE(r_nice) { return Flag(f.flag("iestate", "IntroducedNice")); }
RULE(r_saintmartin) { return Flag(f.flag("iestate", "IntroducedSaintMartin")); }
RULE(r_castelletto) { return Flag(f.flag("iestate", "IntroducedCastelletto")); }
RULE(r_montellino) { return Flag(f.flag("iestate", "IntroducedMontellino")); }
RULE(r_sangiovanni) { return Flag(f.flag("iestate", "IntroducedSanGimignano")); }   // San Giovanni's working name
RULE(r_wanderlust) { return P(f.stat("CompareStats/RoadTripEventsCompleted"), 1); }
RULE(r_yellow) { return Flag(f.flag("iestate", "IntroducedFestival") || f.stat("Main/XP") > 0); }
RULE(r_green) { return wristband(f, kGreenXP); }
RULE(r_pink) { return wristband(f, kPinkXP); }
RULE(r_purple) { return wristband(f, kPurpleXP); }
RULE(r_gold) { return wristband(f, kGoldXP); }
RULE(r_first_race) { return P(f.stat("CompareStats/CarChampionshipEventsWon"), 1); }
RULE(r_number_one) {
    return P(std::max<double>(f.save.champsWon.size(), f.stat("CompareStats/CarChampionshipsWon")), 1);
}
RULE(r_champs25) { return P(f.save.champsCompleted.size(), 25); }
RULE(r_champs100) { return P(f.save.champsCompleted.size(), 100); }
RULE(r_champs_all) { return P(f.save.champsCompleted.size(), kMainlandChamps); }
RULE(r_champion) { return Flag(f.flag("CareerState", "WonFirstFinal") || won_event(f, kHorizonFinale)); }
RULE(r_new_car) {
    // Bought a car since the mod was installed, or the garage holds more than the starting car
    // and all restored barn finds (every other way into the garage is a purchase).
    const int restored = barns_restored(f) + barn_restored(f, "pdlc_barn_find_01");
    return Flag(f.ctr.get("bought") > 0 || static_cast<int>(f.save.garage.size()) > restored + 1);
}
RULE(r_cars50) { return P(std::max<double>(f.save.garage.size(), f.stat("CompareStats/CarsInGarage")), 50); }
RULE(r_cars175) { return P(std::max<double>(f.save.garage.size(), f.stat("CompareStats/CarsInGarage")), 175); }
RULE(r_skill5) { return P(f.stat("Main/NumSkillTokens"), 5); }
RULE(r_skill65) { return P(f.stat("Main/NumSkillTokens"), 65); }
RULE(r_clean100) { return P(f.ctr.get("skill.FastAndClean"), 100); }
RULE(r_boards25) { return P(boards(f, false), 25); }
RULE(r_boards_all) { return P(boards(f, false), kMainlandBoards); }
RULE(r_roads) { return P(f.stat("CompareStats/RoadsDiscovered"), kMainlandRoads); }
RULE(r_barns10) { return P(barns_restored(f), 10); }
RULE(r_photos) {
    return P(std::max<double>(f.save.photos, f.stat("CompareStats/PhotoChallengesCompleted")), 100);
}
// The first two Horizon Bucket Lists (1 and 2, 15 challenges each); the third, Beat Playground
// Games, came later and the achievements were reworded to leave it out.
RULE(r_bucket1) { return P(bucket(f, {1, 2}).solo, 1); }
RULE(r_bucket15) { return P(bucket(f, {1, 2}).solo, 15); }
RULE(r_bucket30) { return P(bucket(f, {1, 2}).solo, 30); }
RULE(r_train) { return Flag(won_event(f, kTrainInVain)); }
// Weather, from the races' set weather: any finished in the rain (the save's results, or since
// the mod was installed); events won in a storm.
RULE(r_rain) {
    for (const auto& [event, place] : f.save.eventPlace)
        if (f.gd.rainEvents.count(event)) return Flag(true);
    return Flag(f.ctr.get("rain.done") > 0);
}
RULE(r_storm_wins) {
    int n = 0;
    for (int event : f.gd.stormEvents) n += won_event(f, event);
    return P(n, 5);
}
RULE(r_widow_maker) { return Flag(f.ctr.get("win.wet.265") > 0); }
// Choosing the Road Trip's next stop: the game's RoadTripCreated event, sent only from the
// destination screen (RoadTripDestinationSelect) when a destination is picked there.
RULE(r_horizon_leader) { return Flag(f.ctr.get("roadtrip.chosen") > 0); }
// Bounty Head-to-Heads won: RaceEncounterEnd with its rareSpawn flag, since the mod was installed,
// added to the ones before: the save's count of the bounty-win line when the mod first read it.
// The line doesn't play on every bounty win (a test win left it at 1), so it can only undercount.
RULE(r_wild_drivatar) {
    const int64_t said = std::atoll(f.prop("DIALOGUE_BountyWin", "PlayCount").c_str());
    return P(static_cast<double>(std::max(said, f.ctr.get("h2h.bounty.base") + f.ctr.get("h2h.bounty.won"))), 3);
}

RULE(r_car_types) {
    return P(car_types_won(f), kCarTypes);
}
// Online play: still counted from the profile's stats, in case online play comes back.
RULE(r_rivals_credits) { return P(f.stat("CompareStats/CreditsEarnedFromRivals"), 1000000); }
RULE(r_cup_car) { return P(static_cast<double>(f.ctr.get("rivals.cr.2297")), 25000); }
RULE(r_mp20) { return P(f.stat("MiscStats/OnlineEventsFinished"), 20); }
RULE(r_mp200) { return P(f.stat("MiscStats/OnlineEventsFinished"), 200); }
RULE(r_team50) { return P(f.stat("MiscStats/OnlineTeamRacesWon"), 50); }
RULE(r_online_trip1) { return P(f.stat("CompareStats/OnlineRoadTripDestsCompleted"), 1); }
RULE(r_online_trip25) { return P(f.stat("CompareStats/OnlineRoadTripDestsCompleted"), 25); }
RULE(r_coop25) { return P(bucket(f, {1, 2}).coop, 25); }
RULE(r_pg100) {
    return P(f.stat("MiscStats/OnlineInfectedGamesEntered") + f.stat("MiscStats/OnlineKingGamesEntered"), 100);
}

// Storm Island
RULE(r_island_arrive) { return Flag(f.prop("islandiestate", "IEState") == "Complete" || f.flag("islandiestate", "HasAwardedInitialCar")); }
RULE(r_island_won1) { return P(f.save.islandChampsWon.size(), 1); }
RULE(r_island_won5) { return P(f.save.islandChampsWon.size(), 5); }
RULE(r_island_all) { return P(f.save.islandChampsCompleted.size(), kIslandChamps); }
// The game's own test for a Storm Island-ready car (before a Storm Island race it suggests the
// Rally parts otherwise): Rally springs and dampers (level 4), Rally tyres (5), Rally
// transmission (4). RaceStartParts sends those three levels at the start of each race.
RULE(r_ready_to_bounce) { return Flag(f.ctr.get("island.rally.start") > 0); }
RULE(r_island_champion) { return Flag(f.flag("CareerState", "WonFirstIslandFinal") || won_event(f, kIslandFinale)); }
RULE(r_gauntlet1) {
    int n = 0;
    for (int c : f.gd.gauntletCampaigns) n += f.save.islandChampsCompleted.count(c);
    return P(n, 1);
}
RULE(r_gauntlet_all) {
    int n = 0;
    for (int c : f.gd.gauntletCampaigns) n += f.save.islandChampsWon.count(c);
    return P(n, kGauntlets);
}
RULE(r_island_roads) {
    return P(f.ctr.get("island.roads.all") ? kIslandRoads : f.stat("Island/RoadsDiscovered"), kIslandRoads);
}
RULE(r_island_boards) {
    return P(boards(f, true), kIslandBoards);
}
RULE(r_island_bucket1) { return P(bucket(f, {4, 5}).solo, 1); }
RULE(r_island_bucket_all) { return P(bucket(f, {4, 5}).solo, kIslandChallenges); }
RULE(r_island_coop_all) { return P(bucket(f, {4, 5}).coop, kIslandChallenges); }
RULE(r_island_barn) { return Flag(barn_restored(f, "pdlc_barn_find_01")); }
RULE(r_island_chain) { return P(f.ctr.get("chain.island.best"), 100000); }
RULE(r_fiesta) { return P(f.ctr.get("win.car.2158"), 1); }
RULE(r_galant) { return P(f.ctr.get("win.car.1381"), 1); }
RULE(r_metro) { return P(f.ctr.get("win.car.2147"), 1); }
RULE(r_ranger) { return P(f.ctr.get("win.car.2145"), 1); }
RULE(r_sst) { return P(f.ctr.get("win.car.2143"), 1); }

// Porsche
RULE(r_porsche_owned) {
    for (int car : f.save.garage)
        if (f.gd.porsches.count(car)) return Flag(true);
    return Flag(false);
}
RULE(r_porsche_bucket5) { return P(bucket(f, {6}).solo, 5); }
RULE(r_porsche_bucket_all) { return P(bucket(f, {6}).solo, kPorscheChallenges); }
RULE(r_porsche40) { return P(f.ctr.get("done.porsche"), 40); }
RULE(r_914) { return P(f.ctr.get("done.car.266"), 4); }
RULE(r_macan) { return P(f.ctr.get("done.car.2295"), 4); }
RULE(r_carrera) { return P(f.ctr.get("done.car.292"), 4); }
RULE(r_918) { return P(f.ctr.get("done.car.2290"), 4); }
RULE(r_959) { return P(f.ctr.get("done.car.269"), 4); }
RULE(r_same_porsche) {
    int64_t best = 0;
    for (int car : f.gd.porsches) best = std::max(best, f.ctr.get("done.car." + std::to_string(car)));
    return P(best, 10);
}
RULE(r_cayman_drifts) { return P(f.ctr.get("skill.Drift.2293"), 20); }
RULE(r_turbo_s_near_miss) { return P(f.ctr.get("skill.NearMiss.2289"), 30); }
RULE(r_944_clean) { return P(f.ctr.get("skill.FastAndClean.268"), 20); }

#undef RULE

using G = Group;
using T = Track;

const std::vector<Def> kDefs = {
    // ---- Forza Horizon 2: 50, 1000G ----
    {"welcome_festival", "Welcome to Horizon Europe", "Arrive at the Horizon Festival.", 10, G::Base, T::Save, nullptr, r_festival},
    {"welcome_sisteron", "Welcome to Sisteron", "Arrive at Sisteron for the first time.", 25, G::Base, T::Save, nullptr, r_sisteron},
    {"welcome_nice", "Welcome to Nice", "Arrive at Nice for the first time.", 25, G::Base, T::Save, nullptr, r_nice},
    {"welcome_saintmartin", "Welcome to Saint-Martin", "Arrive at Saint-Martin for the first time.", 25, G::Base, T::Save, nullptr, r_saintmartin},
    {"welcome_castelletto", "Welcome to Castelletto", "Arrive at Castelletto for the first time.", 25, G::Base, T::Save, nullptr, r_castelletto},
    {"welcome_montellino", "Welcome to Montellino", "Arrive at Montellino for the first time.", 25, G::Base, T::Save, nullptr, r_montellino},
    {"welcome_sangiovanni", "Welcome to San Giovanni", "Arrive at San Giovanni for the first time.", 25, G::Base, T::Save, nullptr, r_sangiovanni},
    {"horizon_leader", "Horizon Leader", "Choose where the Road Trip goes for the first time.", 20, G::Base, T::Events, nullptr, r_horizon_leader},
    {"wanderlust", "Wanderlust", "Complete a Road Trip.", 10, G::Base, T::Save, "road trips", r_wanderlust},
    {"wristband_yellow", "Yellow Wristband", "Obtain the Yellow Wristband.", 10, G::Base, T::Save, nullptr, r_yellow},
    {"wristband_green", "Green Wristband", "Obtain the Green Wristband.", 10, G::Base, T::Save, "XP", r_green},
    {"wristband_pink", "Pink Wristband", "Obtain the Pink Wristband.", 20, G::Base, T::Save, "XP", r_pink},
    {"wristband_purple", "Purple Wristband", "Obtain the Purple Wristband.", 30, G::Base, T::Save, "XP", r_purple},
    {"wristband_gold", "Gold Wristband", "Obtain the Gold Wristband.", 30, G::Base, T::Save, "XP", r_gold},
    {"first_of_many", "First of Many", "Win your first Championship race.", 10, G::Base, T::Save, "races", r_first_race},
    {"number_one", "Number One", "Win a Championship.", 10, G::Base, T::Save, "championships", r_number_one},
    {"horizon_fan", "Horizon Fan", "Complete 25 Championships.", 20, G::Base, T::Save, "championships", r_champs25},
    {"horizon_enthusiast", "Horizon Enthusiast", "Complete 100 Championships.", 25, G::Base, T::Save, "championships", r_champs100},
    {"all_your_race", "All Your Race Are Belong To Us 2.0", "Complete all 168 Championships.", 50, G::Base, T::Save, "championships", r_champs_all},
    {"horizon_champion", "Horizon Champion", "Become the Horizon Champion.", 50, G::Base, T::Save, nullptr, r_champion},
    {"new_car_smell", "That New Car Smell", "Buy your first car.", 10, G::Base, T::Save, nullptr, r_new_car},
    {"car_lover", "Car Lover", "Have 50 Cars in your Garage.", 20, G::Base, T::Save, "cars", r_cars50},
    {"worthy_sultan", "Worthy of a Sultan", "Have 175 Cars in your Garage.", 40, G::Base, T::Save, "cars", r_cars175},
    {"license_to_skill", "License to Skill", "Earn 5 Skill Points.", 10, G::Base, T::Save, "skill points", r_skill5},
    {"stone_cold_skiller", "Stone Cold Skiller", "Earn 65 Skill Points.", 20, G::Base, T::Save, "skill points", r_skill65},
    {"fast_and_clean", "Fast and Clean", "Earn the Clean Racing Skill 100 times.", 10, G::Base, T::Events, "times", r_clean100},
    {"summer_sale", "Summer Sale", "Smash 25 Reward Boards.", 10, G::Base, T::Save, "boards", r_boards25},
    {"smash_happy", "Smash Happy", "Collect all Reward Boards.", 20, G::Base, T::Save, "boards", r_boards_all},
    {"human_gps", "Human GPS", "Drive down every road in the game.", 10, G::Base, T::Save, "roads", r_roads},
    {"storage_hunter", "Storage Hunter", "Restore 10 Barn Finds.", 20, G::Base, T::Save, "barn finds", r_barns10},
    {"photographer", "Photographer", "Take photos of 100 different cars for Horizon Promo.", 20, G::Base, T::Save, "cars", r_photos},
    {"one_down", "One Down!", "Complete a Solo Bucket List challenge from the first two Horizon Bucket Lists.", 10, G::Base, T::Save, "challenges", r_bucket1},
    {"kick_the_bucket", "Kick the Bucket", "Scratch 15 items off the first two Horizon Bucket Lists.", 10, G::Base, T::Save, "challenges", r_bucket15},
    {"rest_in_peace", "Rest in Peace", "Complete 30 Solo Bucket List Challenges from the first two Horizon Bucket Lists.", 20, G::Base, T::Save, "challenges", r_bucket30},
    {"rain_master", "Rain Master", "Complete your first race in the rain.", 10, G::Base, T::Save, nullptr, r_rain},
    {"train_in_vain", "The Train's in Vain", "Beat the Train in a Horizon Showcase Event.", 10, G::Base, T::Save, nullptr, r_train},
    {"multi_disciplined", "Multi-Disciplined", "Win an Event in every type of car.", 10, G::Base, T::Save, "car types", r_car_types},
    {"wild_drivatar", "A Wild Drivatar Appears!", "Win 3 Head-to-Head Races against Drivatars with a Bounty on them.", 10, G::Base, T::Save, "wins", r_wild_drivatar},
    {"we_meet_again", "We Meet Again!", "Win a Showdown at every Car Meet.", 20, G::Base, T::Online, nullptr, nullptr},
    {"friends_with_benefits", "Friends with Benefits", "Earn a total of 1,000,000 Cr from Rivals.", 10, G::Base, T::Online, "Cr", r_rivals_credits},
    {"super_meet_boy", "Super Meet Boy", "Grab a livery, grab a tuning setup, buy a car, and enter a Showdown at any Car Meet.", 10, G::Base, T::Online, nullptr, nullptr},
    {"bragging_rights", "Bragging Rights", "Beat Friends or Club members on any 20 Speed Cameras.", 10, G::Base, T::Online, nullptr, nullptr},
    {"social_caterpillar", "Social Caterpillar", "Compete in 20 Multiplayer Events.", 10, G::Base, T::Online, "events", r_mp20},
    {"social_butterfly", "Social Butterfly", "Compete in 200 Multiplayer Events.", 50, G::Base, T::Online, "events", r_mp200},
    {"team_player", "Team Player", "Win 50 Team Events.", 30, G::Base, T::Online, "wins", r_team50},
    {"loyal", "Loyal", "Earn 500,000 XP for your Club.", 25, G::Base, T::Online, nullptr, nullptr},
    {"on_the_road_again", "On the Road Again", "Complete an Online Road Trip location.", 10, G::Base, T::Online, "locations", r_online_trip1},
    {"well_travelled", "Well Travelled", "Complete 25 Online Road Trip locations.", 30, G::Base, T::Online, "locations", r_online_trip25},
    {"yes_we_can", "Yes We Can", "Complete 25 Bucket List items co-operatively from the first two Horizon Bucket Lists.", 30, G::Base, T::Online, "challenges", r_coop25},
    {"you_are_pg", "You Are Playground Games", "Complete 100 Playground Games Events.", 40, G::Base, T::Online, "events", r_pg100},

    // ---- Storm Island: 23, 500G ----
    {"welcome_storm_island", "Welcome to Storm Island", "Arrive on Storm Island.", 20, G::StormIsland, T::Save, nullptr, r_island_arrive},
    {"into_the_jungle", "Into the Jungle", "Win your first Storm Island Championship.", 20, G::StormIsland, T::Save, "championships", r_island_won1},
    {"high_five", "High Five", "Win 5 Storm Island Championships.", 20, G::StormIsland, T::Save, "championships", r_island_won5},
    {"go_back", "We Have to Go Back!", "Complete every Storm Island Championship.", 50, G::StormIsland, T::Save, "championships", r_island_all},
    {"storm_island_champion", "Storm Island Champion", "Win the Storm Island Finale.", 50, G::StormIsland, T::Save, nullptr, r_island_champion},
    {"survive_gauntlet", "Survive the Gauntlet", "Complete your first Gauntlet Event.", 20, G::StormIsland, T::Save, "Gauntlets", r_gauntlet1},
    {"gauntlet_runner", "Gauntlet Runner", "Win every Gauntlet Event.", 30, G::StormIsland, T::Save, "Gauntlets", r_gauntlet_all},
    {"island_explorer", "Island Explorer", "Drive every Road on Storm Island.", 25, G::StormIsland, T::Save, "roads", r_island_roads},
    {"smash_em_all", "Gotta Smash Em All!", "Collect every Storm Island Bonus Board.", 25, G::StormIsland, T::Save, "boards", r_island_boards},
    {"tick", "Tick!", "Complete your first Storm Island Bucket List Challenge.", 10, G::StormIsland, T::Save, "challenges", r_island_bucket1},
    {"another_life", "See You in Another Life!", "Complete every Storm Island Bucket List Challenge.", 25, G::StormIsland, T::Save, "challenges", r_island_bucket_all},
    {"castaways", "Castaways", "Complete every Co-op Storm Island Bucket List Challenge.", 30, G::StormIsland, T::Online, "challenges", r_island_coop_all},
    {"tropical_treasure", "Tropical Treasure", "Find and restore a Barn Find on Storm Island.", 25, G::StormIsland, T::Save, nullptr, r_island_barn},
    {"skills_for_thrills", "Skills for Thrills", "Bank a Skill Chain of 100,000 points on Storm Island.", 20, G::StormIsland, T::Events, "points", r_island_chain},
    {"storm_breaker", "Storm Breaker", "Win 5 events in stormy conditions.", 20, G::StormIsland, T::Save, "events", r_storm_wins},
    {"ready_to_bounce", "Ready to Bounce", "Start a race with Off-Road tires, suspension, and transmission on Storm Island.", 10, G::StormIsland, T::Events, nullptr, r_ready_to_bounce},
    {"fiesta_fun", "Fiesta Fun", "Win an event in the '81 Ford Fiesta.", 10, G::StormIsland, T::Events, nullptr, r_fiesta},
    {"galant_great", "Galant Great", "Win an Event in the Mitsubishi Galant.", 10, G::StormIsland, T::Events, nullptr, r_galant},
    {"metro_master", "Metro Master", "Win an event in the MG Metro R4.", 10, G::StormIsland, T::Events, nullptr, r_metro},
    {"ranger_rock_star", "Ranger Rock Star", "Win an event in the Ford Ranger.", 10, G::StormIsland, T::Events, nullptr, r_ranger},
    {"stadium_star", "Stadium Star", "Win an Event in the Robby Gordon #7 SST.", 10, G::StormIsland, T::Events, nullptr, r_sst},
    {"meet_and_beat", "Meet and Beat", "Visit the Storm Island Car Meet and win 3 Showdowns.", 20, G::StormIsland, T::Online, nullptr, nullptr},
    {"horizon_5_0", "Horizon 5-0", "Complete 50 Online Events on Storm Island.", 30, G::StormIsland, T::Online, nullptr, nullptr},

    // ---- Porsche Expansion: 15, 500G ----
    {"porsche_owners_club", "Porsche Owners Club", "Obtain your first Porsche.", 25, G::Porsche, T::Save, nullptr, r_porsche_owned},
    {"porsche_rookie", "Porsche Rookie", "Tick off 5 items on the Porsche Bucket List", 25, G::Porsche, T::Save, "challenges", r_porsche_bucket5},
    {"porsche_pro", "Porsche Pro", "Complete the Porsche Bucket List", 50, G::Porsche, T::Save, "challenges", r_porsche_bucket_all},
    {"horse_power", "Horse Power", "Complete 40 events in Porsches.", 100, G::Porsche, T::Events, "events", r_porsche40},
    {"air_cooled", "Air Cooled Competitor", "Complete 4 events in the 1970 Porsche 914/6.", 30, G::Porsche, T::Events, "events", r_914},
    {"cajun_kickin", "Cajun Kickin'", "Complete 4 events in the 2015 Porsche Macan Turbo.", 30, G::Porsche, T::Events, "events", r_macan},
    {"listen_to_it_roar", "Listen to it Roar!", "Complete 4 events in the 2003 Porsche Carrera GT.", 30, G::Porsche, T::Events, "events", r_carrera},
    {"ring_wraith", "Ring Wraith", "Complete 4 events in the 2014 Porsche 918 Spyder.", 30, G::Porsche, T::Events, "events", r_918},
    {"show_and_display", "Show and Display", "Complete 4 events in the 1987 Porsche 959.", 30, G::Porsche, T::Events, "events", r_959},
    {"this_is_my_porsche", "This Is My Porsche", "Complete 10 events in the same Porsche.", 50, G::Porsche, T::Events, "events", r_same_porsche},
    {"animal", "Animal!", "Pull off 20 Drifts in the 2015 Porsche Cayman GTS.", 20, G::Porsche, T::Events, "drifts", r_cayman_drifts},
    {"call_911", "Call 911", "Pull off 30 Near Miss Skills in the Porsche 911 Turbo S.", 20, G::Porsche, T::Events, "near misses", r_turbo_s_near_miss},
    {"cup_car", "Cup Car", "Earn 25,000 Credits in Rivals Mode with the Porsche 911 GT3 RS 4.0.", 20, G::Porsche, T::Online, "Cr", r_cup_car},
    {"no_airbag", "No Airbag Required", "Earn 20 Clean Racer Skills in the 1989 Porsche 944 Turbo.", 20, G::Porsche, T::Events, "skills", r_944_clean},
    {"widow_maker", "Widow Maker", "Win a race in the wet in a 1982 Porsche 911 Turbo 3.3.", 20, G::Porsche, T::Events, nullptr, r_widow_maker},
};

}  // namespace

const std::vector<Def>& defs() { return kDefs; }

// ---- events -----------------------------------------------------------------------------------

bool take_baselines(const SaveFacts& save, Counters& c) {
    if (!save.ok || c.v.count("h2h.bounty.base")) return false;
    int64_t said = 0;
    auto o = save.objects.find("DIALOGUE_BountyWin");
    if (o != save.objects.end()) {
        auto p = o->second.find("PlayCount");
        if (p != o->second.end()) said = std::atoll(p->second.c_str());
    }
    c.v["h2h.bounty.base"] = said;
    return true;
}

bool count_event(const Event& e, const GameData& gd, Counters& c, LiveFacts& live) {
    switch (e.id) {
        case ev::CarPurchased:
            c.raise("bought", 1);
            return true;
        case ev::RewardBoardSmashed:      // [3] board id ("discount_board_xp_a_047"), [4] kind, [5] boards so far
        case ev::IslandRewardBoardSmashed:
            if (e.s(3).empty()) return false;
            return live.boardsSmashed.insert(e.s(3)).second;
        case ev::SkillAchieved: {         // [3] skill ("Drift"), [4] grade, [5] score, [6] car
            const std::string skill = e.s(3);
            if (skill.empty()) return false;
            const int64_t car = e.i(6);
            if (skill == "FastAndClean") c.add("skill.FastAndClean", 1);
            if ((skill == "Drift" && car == 2293) || (skill == "NearMiss" && car == 2289) ||
                (skill == "FastAndClean" && car == 268))
                c.add("skill." + skill + "." + std::to_string(car), 1);
            return skill == "FastAndClean" || car == 2293 || car == 2289 || car == 268;
        }
        case ev::SkillChainCompleted:     // [3] score, [4] skills in the chain, [5] multiplier
            if (e.world() != kWorldIsland) return false;
            c.raise("chain.island.best", e.i(3));
            return true;
        case ev::IslandSkillChainCompleted:
            c.raise("chain.island.best", e.i(3));
            return true;
        case ev::IslandRoadsDriven:       // [3] roads found, [4] all found, [5] percent
            if (!e.i(4)) return false;
            c.raise("island.roads.all", 1);
            return true;
        case ev::RaceEncounterEnd:        // [3] difficulty, [5] 1 won / 2 lost, [6] rareSpawn (the bounty)
            if (e.i(5) != 1) return false;
            c.add("h2h.won", 1);
            if (e.i(6) != 0) c.add("h2h.bounty.won", 1);
            return true;
        case ev::RaceStartParts:          // [3] track, [4] springs and dampers, [5] tyres, [6] transmission levels
            if (e.world() != kWorldIsland || e.i(4) != 4 || e.i(5) != 5 || e.i(6) != 4) return false;
            c.raise("island.rally.start", 1);
            return true;
        case ev::RoadTripCreated:         // a destination picked on the Road Trip screen
            c.raise("roadtrip.chosen", 1);
            return true;
        case ev::RivalsEnd:               // [3] event, [4] track, [7] car, [10] credits won, [11] place
            if (e.i(7) != 2297 || e.i(10) <= 0) return false;
            c.add("rivals.cr.2297", e.i(10));
            return true;
        case ev::AnthemRaceEnd: {
            // [8] car, [10] time, [11] place (1 = won), [12] event, [14] championship, [18]
            // credits, [20] world. The game sends it from the race-end handler (0x1409CD9C0) and
            // always passes 0 as [9]; the check below is only a guard.
            if (e.f.size() < 13 || e.i(9) != 0) return false;
            const int64_t car = e.i(8), place = e.i(11), event = e.i(12);
            const std::string cs = std::to_string(car);
            if (place == 1) {
                c.raise("won." + std::to_string(event), 1);
                c.add("win.car." + cs, 1);
                if (gd.wetEvents.count(static_cast<int>(event))) c.add("win.wet." + cs, 1);
            }
            if (gd.rainEvents.count(static_cast<int>(event))) c.raise("rain.done", 1);
            if (gd.porsches.count(static_cast<int>(car))) {
                c.add("done.car." + cs, 1);
                c.add("done.porsche", 1);
            }
            return true;
        }
        default:
            return false;
    }
}

}  // namespace ach
