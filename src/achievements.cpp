// Achievements: the mod. Hooks, the pop-ups, the unlock file and the worker thread that ties them
// together; the rules are in rules.cpp, the save in save_reader.cpp. See ach.hpp for the idea.
//
// Game addresses are for the 24 Nov 2015 build Azimuth runs; the platform checks each hooked
// byte against the exe on disk, and the two functions called directly are checked here.
//  - Telemetry events: EtwPlus.dll's EtxEventWrite through its import slot, game+0x1DD0060
//    (event descriptor in rcx: id at +0, name at +0x10, field types at +0x20, field count in the
//    low byte of +0x28; the EVENT_DATA_DESCRIPTORs are the 5th argument).
//  - Profile stats: SetStat (0x1402F7940) at 0x1402F7977, just after its "stats enabled" check:
//    rsi = the stat's name ("Main/XP"), rbp = its new value (type at +0x10: 3 int32, 9 float;
//    value at +8).
//  - HUD notifications: HUDMinorNotificationComponent's constructor (0x14073E0B1, rbx = it) and
//    destructor (0x14073F046, rbx = it) keep a list of them; TemplateName is the std::string at
//    +0x128+0x18. Show (0x140755330) takes the component and three display strings by value
//    (main text, subtext, icon name; it releases them itself), made with 0x140323310
//    (out, const char* text, -1). The icon "Achievement" is the HUD's own trophy.
//  - A tick on the game thread to show them from: AchievementsModule's update (vtable +0x20,
//    0x1403FCD60, about 60 a second), hooked at 0x1403FCD84.
#include "ach.hpp"
#include "fh2mod.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>

using namespace ach;

namespace {

const FH2ModApi* g_api;
FH2Mod* g_mod;
uint8_t* g_base;
std::wstring g_azimuth, g_gameDir, g_dir, g_iniPath;

namespace rva {
constexpr uintptr_t EtxEventWriteSlot = 0x1DD0060;
constexpr uintptr_t SetStatHook = 0x2F7977;
constexpr uintptr_t NotifCtor = 0x73E0B1, NotifDtor = 0x73F046;
constexpr uintptr_t AchModUpdate = 0x3FCD84;
constexpr uintptr_t MakeDisplayString = 0x323310, ShowNotif = 0x755330;
}  // namespace rva

const uint8_t kMakeDisplayStringBytes[] = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7, 0x44, 0x24, 0x28, 0xFE, 0xFF, 0xFF, 0xFF};
const uint8_t kShowNotifBytes[] = {0x4C, 0x89, 0x4C, 0x24, 0x20, 0x4C, 0x89, 0x44, 0x24, 0x18, 0x48, 0x89, 0x54, 0x24, 0x10};

// ---- log: fh2-mods\Achievements\Achievements.log ---------------------------------------------

std::mutex g_logMu;
std::string g_logBuf;
std::wstring g_logPath;
ULONGLONG g_t0;
std::atomic<size_t> g_logBytes{0};

void logf(const char* fmt, ...) {
    if (g_logBytes.load(std::memory_order_relaxed) > (8u << 20)) return;
    char b[2048];
    SYSTEMTIME st;
    GetLocalTime(&st);
    int n = std::snprintf(b, sizeof b, "%02d:%02d:%02d ", st.wHour, st.wMinute, st.wSecond);
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(b + n, sizeof b - n - 2, fmt, ap);
    va_end(ap);
    size_t len = std::min<size_t>(n + std::max(m, 0), sizeof b - 2);
    b[len++] = '\n';
    g_logBytes += len;
    std::lock_guard<std::mutex> lk(g_logMu);
    g_logBuf.append(b, len);
}

void flush_log() {
    std::string s;
    {
        std::lock_guard<std::mutex> lk(g_logMu);
        s.swap(g_logBuf);
    }
    if (s.empty() || g_logPath.empty()) return;
    HANDLE h = CreateFileW(g_logPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0;
    WriteFile(h, s.data(), static_cast<DWORD>(s.size()), &w, nullptr);
    CloseHandle(h);
}

void plog(const std::string& s) { g_api->log(g_mod, s.c_str()); }

std::string narrow(const std::wstring& w) {
    std::string s;
    for (wchar_t c : w) s.push_back(c < 128 ? static_cast<char>(c) : '?');
    return s;
}

bool sread(const void* p, void* out, size_t n) {
    if (reinterpret_cast<uintptr_t>(p) < 0x10000) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), p, out, n, &got) && got == n;
}
template <class T> T srd(const void* p, T def = T{}) {
    T v;
    return sread(p, &v, sizeof v) ? v : def;
}
// MSVC 2012 std::string: buffer or pointer at +0, size at +0x10, capacity at +0x18.
std::string msvc_str(const void* s) {
    auto b = static_cast<const uint8_t*>(s);
    const uint64_t size = srd<uint64_t>(b + 0x10, ~0ull), cap = srd<uint64_t>(b + 0x18, 0);
    if (size > 512 || cap < size) return {};
    const char* p = cap >= 16 ? srd<const char*>(b) : reinterpret_cast<const char*>(b);
    std::string r(size, '\0');
    if (size && !sread(p, r.data(), size)) return {};
    return r;
}

bool code_matches(uintptr_t rva, const uint8_t* bytes, size_t n) {
    uint8_t have[32];
    return n <= sizeof have && sread(g_base + rva, have, n) && std::memcmp(have, bytes, n) == 0;
}

std::string now_utc() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    char b[32];
    std::snprintf(b, sizeof b, "%04d-%02d-%02d %02d:%02d:%02d UTC", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return b;
}

// ---- settings: fh2-mods\Achievements.ini -----------------------------------------------------

std::atomic<bool> g_popupsOn{true};

void on_setting(void*, const char* key, int value) {
    if (std::strcmp(key, "Popups") == 0) {
        g_popupsOn = value != 0;
        WritePrivateProfileStringW(L"Achievements", L"Popups", value ? L"1" : L"0", g_iniPath.c_str());
    }
}

// ---- events: hooked import, decoded off the game's back --------------------------------------

struct EDD {   // EVENT_DATA_DESCRIPTOR
    uint64_t ptr;
    uint32_t size;
    uint32_t reserved;
};
using EtxWriteFn = uint32_t (*)(const uint8_t*, const void*, uint64_t, uint32_t, const EDD*);
EtxWriteFn o_etx;

std::mutex g_evMu;
std::deque<Event> g_events;
std::atomic<bool> g_ready{false};   // the player is in the game world (pop-ups can show)
std::atomic<int> g_evCount[256];

bool wanted(uint16_t id) {
    switch (id) {
        case ev::AnthemRaceEnd:
        case ev::CarPurchased:
        case ev::RewardBoardSmashed:
        case ev::IslandRewardBoardSmashed:
        case ev::SkillAchieved:
        case ev::SkillChainCompleted:
        case ev::IslandSkillChainCompleted:
        case ev::IslandRoadsDriven:
        case ev::RaceEncounterEnd:
        case ev::RaceStartParts:
        case ev::RivalsEnd:
        case ev::RoadTripCreated:
            return true;
        default:
            return false;
    }
}
// Events logged (the first 25 of each): ones whose fields aren't understood yet but would let more
// achievements be tracked, and the newest counted ones, to check them against play.
bool research(uint16_t id) {
    switch (id) {
        case 7:    // AnthemRaceStart
        case 11: case 12: case 45: case 46:   // barn finds
        case 14: case 47:                     // bucket list challenge ends
        case 17: case 18:                     // car meets
        case 20: case 21: case 50: case 51:   // championships
        case 34: case 41: case 49:            // DroveToDestination, champions
        case ev::RaceEncounterEnd:
        case ev::RaceStartParts:
        case ev::RivalsEnd:
        case ev::RoadTripCreated:
        case ev::WristbandGained:
        case 108:  // RoadsDriven
        case 113:  // ShowcaseRaceEnd
            return true;
        default:
            return false;
    }
}

Event decode(const uint8_t* desc, uint32_t n, const EDD* data) {
    Event e;
    e.id = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(desc));
    const char* name = *reinterpret_cast<const char* const*>(desc + 0x10);
    if (name) e.name.assign(name, strnlen(name, 64));
    const uint8_t* types = *reinterpret_cast<const uint8_t* const*>(desc + 0x20);
    const uint32_t nf = *reinterpret_cast<const uint32_t*>(desc + 0x28) & 0xFF;
    n = std::min<uint32_t>(n, 40);
    e.f.resize(n);
    for (uint32_t k = 0; k < n; ++k) {
        EvField& f = e.f[k];
        f.type = (types && k < nf) ? types[k] : 0xFF;
        f.size = data[k].size;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(data[k].ptr);
        if (!p) continue;
        if (k >= 3 && f.type == 0x00 && f.size % 2 == 0) {
            for (uint32_t c = 0; c < f.size / 2 && c < 128; ++c) {
                wchar_t w;
                std::memcpy(&w, p + 2 * c, 2);
                if (!w) break;
                f.s.push_back(w < 128 ? static_cast<char>(w) : '?');
            }
        } else if (f.size == 4) {
            int32_t i;
            std::memcpy(&i, p, 4);
            f.i = i;
            if (f.type == 0x09) {   // a float; 0x0B is a 4-byte bool (RaceEncounterEnd's rareSpawn)
                float x;
                std::memcpy(&x, p, 4);
                f.d = x;
            } else {
                f.d = i;
            }
        } else if (f.size == 8) {
            std::memcpy(&f.i, p, 8);
            f.d = static_cast<double>(f.i);
        } else if (f.size == 2) {
            uint16_t v;
            std::memcpy(&v, p, 2);
            f.i = v;
            f.d = v;
        } else if (f.size == 1) {
            f.i = *p;
            f.d = *p;
        }
    }
    return e;
}

std::string describe(const Event& e) {
    std::string s = e.name + " (" + std::to_string(e.id) + "):";
    char b[64];
    for (size_t k = 3; k < e.f.size(); ++k) {
        const EvField& f = e.f[k];
        if (!f.s.empty() || f.type == 0x00) std::snprintf(b, sizeof b, " [%zu] \"%.40s\"", k, f.s.c_str());
        else if (f.type == 0x09) std::snprintf(b, sizeof b, " [%zu] %g", k, f.d);
        else std::snprintf(b, sizeof b, " [%zu] %lld", k, static_cast<long long>(f.i));
        s += b;
    }
    return s;
}

uint32_t etx_detour(const uint8_t* desc, const void* prov, uint64_t handle, uint32_t n, const EDD* data) {
    const uint16_t id = static_cast<uint16_t>(*reinterpret_cast<const uint32_t*>(desc));
    switch (id) {
        case ev::XPGained:
        case ev::DistanceDrivenDelta:
        case ev::SkillChainCompleted:
        case ev::SkillAchieved:
        case ev::RewardBoardSmashed:
        case 7:   // AnthemRaceStart
            g_ready = true;
            break;
    }
    const int seen = ++g_evCount[id & 0xFF];
    if (research(id) && seen <= 25) logf("event %s", describe(decode(desc, n, data)).c_str());
    if (wanted(id)) {
        Event e = decode(desc, n, data);
        std::lock_guard<std::mutex> lk(g_evMu);
        if (g_events.size() < 10000) g_events.push_back(std::move(e));
    }
    return o_etx(desc, prov, handle, n, data);
}

// ---- live stats: SetStat ----------------------------------------------------------------------

std::mutex g_statMu;
std::map<std::string, double> g_liveStats;
std::atomic<uint32_t> g_statVersion{0};

void on_set_stat(FH2Context* c, void*) {
    const char* name = reinterpret_cast<const char*>(c->rsi);
    const uint8_t* v = reinterpret_cast<const uint8_t*>(c->rbp);
    if (!name || !v) return;
    const int32_t type = *reinterpret_cast<const int32_t*>(v + 0x10);
    double value;
    if (type == 3) value = *reinterpret_cast<const int32_t*>(v + 8);
    else if (type == 9) value = *reinterpret_cast<const float*>(v + 8);
    else return;
    const std::string key(name, strnlen(name, 96));
    std::lock_guard<std::mutex> lk(g_statMu);
    auto [it, added] = g_liveStats.try_emplace(key, value);
    if (added || it->second != value) {
        it->second = value;
        ++g_statVersion;
    }
}

// ---- HUD notifications ------------------------------------------------------------------------

std::mutex g_compMu;
std::vector<uint8_t*> g_comps;   // live HUDMinorNotificationComponents, oldest first
std::atomic<ULONGLONG> g_firstCompTick{0};
// The constructor, destructor and tick hooks all made and on: without the destructor the list
// would keep components the game has freed, so pop-ups need all three.
std::atomic<bool> g_popupHooks{false};

void on_notif_ctor(FH2Context* c, void*) {
    if (!g_popupHooks) return;
    std::lock_guard<std::mutex> lk(g_compMu);
    g_comps.push_back(reinterpret_cast<uint8_t*>(c->rbx));
    if (!g_firstCompTick) g_firstCompTick = GetTickCount64();
}
void on_notif_dtor(FH2Context* c, void*) {
    std::lock_guard<std::mutex> lk(g_compMu);
    auto it = std::find(g_comps.begin(), g_comps.end(), reinterpret_cast<uint8_t*>(c->rbx));
    if (it != g_comps.end()) g_comps.erase(it);
}

// The HUD's "road discovered" notification (its template draws an icon and two lines; the
// first one made, which lives as long as the HUD), else the oldest.
uint8_t* pick_component() {
    std::lock_guard<std::mutex> lk(g_compMu);
    for (uint8_t* c : g_comps)
        if (msvc_str(c + 0x128 + 0x18) == "RoadDiscovery") return c;
    return g_comps.empty() ? nullptr : g_comps.front();
}

struct Popup {
    std::string main, sub;
};
std::mutex g_popMu;
std::deque<Popup> g_popups;
ULONGLONG g_nextPopup = 0;   // game thread only
bool g_callsChecked = false, g_callsOk = false;

void queue_popup(const std::string& main, const std::string& sub) {
    logf("pop-up queued: %s / %s", main.c_str(), sub.c_str());
    std::lock_guard<std::mutex> lk(g_popMu);
    g_popups.push_back({main, sub});
}

// Runs on the game thread about 60 times a second.
void on_tick(FH2Context*, void*) {
    if (!g_popupsOn || !g_ready || !g_popupHooks) return;
    const ULONGLONG now = GetTickCount64();
    if (now < g_nextPopup) return;
    Popup p;
    {
        std::lock_guard<std::mutex> lk(g_popMu);
        if (g_popups.empty()) return;
        p = g_popups.front();
    }
    if (!g_callsChecked) {
        g_callsChecked = true;
        g_callsOk = code_matches(rva::MakeDisplayString, kMakeDisplayStringBytes, sizeof kMakeDisplayStringBytes) &&
                    code_matches(rva::ShowNotif, kShowNotifBytes, sizeof kShowNotifBytes);
        if (!g_callsOk) logf("the game's notification code isn't what this mod expects: no pop-ups");
    }
    if (!g_callsOk) return;
    uint8_t* comp = pick_component();
    if (!comp) return;
    {
        std::lock_guard<std::mutex> lk(g_popMu);
        if (!g_popups.empty()) g_popups.pop_front();
    }
    auto make = reinterpret_cast<void** (*)(void**, const char*, int)>(g_base + rva::MakeDisplayString);
    auto show = reinterpret_cast<void (*)(void*, void**, void**, void**)>(g_base + rva::ShowNotif);
    void *a = nullptr, *b = nullptr, *icon = nullptr;
    make(&a, p.main.c_str(), -1);
    make(&b, p.sub.c_str(), -1);
    make(&icon, "Achievement", -1);
    show(comp, &a, &b, &icon);   // takes all three by value: it releases them itself
    g_nextPopup = now + 5500;
    logf("pop-up shown: %s / %s", p.main.c_str(), p.sub.c_str());
}

// ---- the unlock file: fh2-mods\Achievements\<user>.ini -----------------------------------------

struct Store {
    std::map<std::string, std::string> unlocked;   // id -> when
    Counters ctr;
    bool existed = false;
    bool writable = true;    // false: the file is there but couldn't be read, so it's never overwritten
    bool backedUp = false;   // copied to <user>.ini.bak before this session's first write
};

bool read_text(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) && size.QuadPart < (16 << 20);
    if (ok) {
        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD got = 0;
        ok = out.empty() || (ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(h);
    return ok;
}

enum class StoreRead { Ok, Missing, Unreadable, Damaged };

StoreRead read_store(const std::wstring& path, Store& s) {
    s = Store{};
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return StoreRead::Missing;
    s.existed = true;
    std::string text;
    bool got = false;   // another program (a virus scanner, a backup) may have it open for a moment
    for (int i = 0; i < 10 && !(got = read_text(path, text)); ++i) Sleep(300);
    if (!got) return StoreRead::Unreadable;
    if (text.find("[unlocked]") == std::string::npos) return StoreRead::Damaged;   // emptied by a crash, say
    std::istringstream in(text);
    std::string line, section;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') {
            section = line;
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (section == "[unlocked]") s.unlocked[k] = v;
        else if (section == "[counters]") s.ctr.v[k] = std::strtoll(v.c_str(), nullptr, 10);
    }
    return StoreRead::Ok;
}

// The profile's unlock file. One that's there but can't be read is left alone for the session; a
// damaged one is kept as .damaged and replaced by the backup from the start of the last session.
void load_store(const std::wstring& path, Store& s) {
    const StoreRead r = read_store(path, s);
    if (r == StoreRead::Ok || r == StoreRead::Missing) return;
    if (r == StoreRead::Unreadable) {
        s = Store{};
        s.existed = true;
        s.writable = false;
        logf("couldn't open %s: it's left as it is, and this session's achievements aren't saved; restart the game "
             "to try again",
             narrow(path).c_str());
        return;
    }
    CopyFileW(path.c_str(), (path + L".damaged").c_str(), FALSE);
    Store bak;
    if (read_store(path + L".bak", bak) == StoreRead::Ok) {
        s = std::move(bak);
        s.backedUp = true;   // keep the good backup
        logf("%s was damaged (kept as .damaged): using the backup from the start of the last session",
             narrow(path).c_str());
    } else {
        s = Store{};
        s.existed = true;
        logf("%s was damaged (kept as .damaged) and there's no backup: starting it again from the save",
             narrow(path).c_str());
    }
}

bool save_store(const std::wstring& path, Store& s) {
    if (!s.writable) return true;
    std::ostringstream o;
    o << "; Achievements unlocked offline (Achievements mod). Delete this file to start over.\n[unlocked]\n";
    for (const auto& [k, v] : s.unlocked) o << k << '=' << v << '\n';
    o << "\n; Progress counted from gameplay events since the mod was installed.\n[counters]\n";
    for (const auto& [k, v] : s.ctr.v) o << k << '=' << v << '\n';
    const std::string text = o.str();
    const std::wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    const bool ok = WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &w, nullptr) && w == text.size() &&
                    FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok) return false;
    if (!s.backedUp) {   // the file as the session found it, in case a crash damages the new one
        if (s.existed) CopyFileW(path.c_str(), (path + L".bak").c_str(), FALSE);
        s.backedUp = true;
    }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

// ---- the in-game page (platform API 8) ------------------------------------------------------

bool g_pageOn = false;   // the platform has pages, and ours was added

std::string g_tileText;   // as last set
int g_tileBand = -1;      // the wristband the tile shows, as last set
void push_page(const Facts* f, const Store& s) {
    if (!g_pageOn) return;
    const PageXaml x = page_xaml(f, s.unlocked);
    if (!g_api->set_page_xaml(g_mod, "List", x.resources.c_str(), x.content.c_str(), x.help.c_str())) logf("the page's XAML was refused");
    // the Gamerscore on the pause menu's Achievements tile: "340 / 2,000G"
    int got = 0, all = 0;
    for (const Def& d : defs()) {
        all += d.gamerscore;
        if (s.unlocked.count(d.id)) got += d.gamerscore;
    }
    auto thousands = [](int v) {
        std::string t = std::to_string(v);
        for (int i = static_cast<int>(t.size()) - 3; i > 0; i -= 3) t.insert(static_cast<size_t>(i), ",");
        return t;
    };
    const std::string text = thousands(got) + " / " + thousands(all) + "G";
    // the tile's text and colours came with platform 2.7.0's API 8
    if (g_api->size < offsetof(FH2ModApi, set_page_tile_colors) + sizeof(void*)) return;
    if (text != g_tileText) {
        if (g_api->set_page_tile_text(g_mod, "List", text.c_str())) g_tileText = text;
        else logf("the tile's text was refused");
    }
    // ... in the colour of a wristband: the game's own, from media\GameTunableSettings\
    // PlayerXPLevels.xml (each band's ColourNum, and the TextColour on it). A band per tenth of
    // the Gamerscore you can earn offline; Gold for all of it.
    struct Band {
        const char* name;
        uint32_t back, text;
    };
    static const Band kBands[] = {
        {"Yellow", 0xFFFCFF14, 0xFF444444}, {"Green", 0xFF50AE36, 0xFFFEFEFE},  {"Blue", 0xFF3667DB, 0xFFFEFEFE},
        {"Pink", 0xFFF05A77, 0xFFFEFEFE},   {"Orange", 0xFFDD791F, 0xFFFEFEFE}, {"Purple", 0xFF7533B3, 0xFFFEFEFE},
        {"Red", 0xFFCE4134, 0xFFFEFEFE},    {"Turquoise", 0xFF3BDABA, 0xFF444444}, {"Bronze", 0xFF9C5932, 0xFFFEFEFE},
        {"Silver", 0xFFABB8C7, 0xFF444444}, {"Gold", 0xFFC2B336, 0xFFFEFEFE},
    };
    int offGot = 0, offAll = 0;
    for (const Def& d : defs()) {
        if (d.track == Track::Online) continue;
        offAll += d.gamerscore;
        if (s.unlocked.count(d.id)) offGot += d.gamerscore;
    }
    const int band = offGot >= offAll ? 10 : std::min(9, offAll ? offGot * 10 / offAll : 0);
    if (band != g_tileBand) {
        if (g_api->set_page_tile_colors(g_mod, "List", kBands[band].back, kBands[band].text)) {
            g_tileBand = band;
            logf("the Achievements tile is %s (%d of the %dG you can earn offline)", kBands[band].name, offGot, offAll);
        } else {
            logf("the tile's colours were refused");
        }
    }
}

// The section list's selection: the section to show. (It goes to -1 when the focus moves to the
// cards: the shown section stays.)
void on_page_slot(void*, const char* key, int slot, int value) {
    if (slot == kSlotSelected && value >= 0) g_api->set_page_slot(g_mod, key, kSlotSection, value);
}

// A on a section: into its achievements. B there: back to the section. B on the sections leaves.
// The pane changes first, so the sections stop (or start) taking the focus before it moves.
int on_page_button(void*, const char* key, int button) {
    const int pane = g_api->get_page_slot(g_mod, key, kSlotPane), sec = g_api->get_page_slot(g_mod, key, kSlotSection);
    if (button == FH2_BUTTON_A) {
        if (pane != 0) return FH2_PAGE_HANDLED;
        g_api->set_page_slot(g_mod, key, kSlotPane, 1);
        if (!g_api->focus_page_element(g_mod, key, ("Ach_Card_" + std::to_string(sec) + "_0").c_str())) {
            g_api->set_page_slot(g_mod, key, kSlotPane, 0);
            return FH2_PAGE_HANDLED;
        }
        return FH2_PAGE_HANDLED_SELECT;
    }
    if (button == FH2_BUTTON_B && pane == 1) {
        g_api->set_page_slot(g_mod, key, kSlotPane, 0);
        g_api->focus_page_element(g_mod, key, sec == 0 ? "FirstItem" : ("Ach_Sec_" + std::to_string(sec)).c_str());
        return FH2_PAGE_HANDLED_BACK;
    }
    // LB and RB: the section before or after, wherever the focus is (on the cards, it stays on them)
    if (button == FH2_BUTTON_LB || button == FH2_BUTTON_RB) {
        const int next = sec + (button == FH2_BUTTON_RB ? 1 : -1);
        if (next < 0 || next >= static_cast<int>(section_titles().size())) return FH2_PAGE_HANDLED;
        if (pane == 0) {   // the section list follows the focus, and the cards follow it (on_page_slot)
            g_api->focus_page_element(g_mod, key, next == 0 ? "FirstItem" : ("Ach_Sec_" + std::to_string(next)).c_str());
        } else {           // the cards: the next section's, from the top
            g_api->set_page_slot(g_mod, key, kSlotSection, next);
            g_api->focus_page_element(g_mod, key, ("Ach_Card_" + std::to_string(next) + "_0").c_str());
        }
        return FH2_PAGE_HANDLED_SELECT;
    }
    return FH2_PAGE_IGNORED;
}

// fh2-mods\Achievements\Achievements.txt: the same list as a text file.
void write_list(const std::wstring& path, const Facts& f, const Store& s) {
    int got = 0, gs = 0, gsAll = 0;
    for (const Def& d : defs()) {
        gsAll += d.gamerscore;
        if (s.unlocked.count(d.id)) {
            ++got;
            gs += d.gamerscore;
        }
    }
    std::ostringstream o;
    o << "Forza Horizon 2 achievements: " << got << " of " << defs().size() << " unlocked, " << gs << "G of " << gsAll
      << "G\n";
    const char* groups[] = {"Forza Horizon 2", "Storm Island", "Porsche Expansion"};
    for (int g = 0; g < 3; ++g) {
        o << "\n== " << groups[g] << " ==\n";
        for (const Def& d : defs()) {
            if (static_cast<int>(d.group) != g) continue;
            const bool done = s.unlocked.count(d.id) > 0;
            char head[160];
            std::snprintf(head, sizeof head, "[%c] %-36s %3dG  ", done ? 'x' : ' ', d.name, d.gamerscore);
            o << head << d.desc;
            if (done) {
                o << "  (unlocked " << s.unlocked.at(d.id) << ")";
            } else {
                if (d.eval && d.unit) {
                    const Progress p = d.eval(f);
                    char pr[96];
                    std::snprintf(pr, sizeof pr, "  [%.0f / %.0f %s]", p.have, p.need, d.unit);
                    o << pr;
                }
                if (d.track == Track::Online) o << "  (online only)";
            }
            o << '\n';
        }
    }
    std::ofstream(path.c_str(), std::ios::binary) << o.str();
}

// ---- the worker -----------------------------------------------------------------------------

FH2Hook* hook(const char* name, uintptr_t at, FH2InstructionFn fn) {
    FH2Hook* h = g_api->hook_instruction(g_mod, name, g_base + at, fn, nullptr);
    if (!h) logf("couldn't hook %s (see platform.log)", name);
    return h;
}

DWORD WINAPI worker(LPVOID) {
    g_base = static_cast<uint8_t*>(g_api->wait_for_code(0xFFFFFFFFu));
    if (!g_base) {
        logf("the game's code never became ready");
        flush_log();
        return 0;
    }
    int made = 0;
    if (g_api->hook_pointer(g_mod, "EtxEventWrite (import slot)", reinterpret_cast<void**>(g_base + rva::EtxEventWriteSlot),
                            reinterpret_cast<void*>(&etx_detour), reinterpret_cast<void**>(&o_etx)))
        ++made;
    else
        logf("couldn't hook the telemetry import (see platform.log): no event-based achievements");
    made += hook("SetStat (after its enabled check)", rva::SetStatHook, &on_set_stat) != nullptr;
    FH2Hook* ctor = hook("HUDMinorNotificationComponent constructor", rva::NotifCtor, &on_notif_ctor);
    FH2Hook* dtor = hook("HUDMinorNotificationComponent destructor", rva::NotifDtor, &on_notif_dtor);
    FH2Hook* tick = hook("AchievementsModule update (pop-up tick)", rva::AchModUpdate, &on_tick);
    made += (ctor != nullptr) + (dtor != nullptr) + (tick != nullptr);
    g_popupHooks = ctor && dtor && tick;
    if (!g_api->enable_hooks(g_mod, nullptr)) {
        // which one is in platform.log; the pop-ups need all three, so check them one by one
        g_popupHooks = g_popupHooks && g_api->enable_hooks(g_mod, ctor) && g_api->enable_hooks(g_mod, dtor) &&
                       g_api->enable_hooks(g_mod, tick);
    }
    if (!g_popupHooks) {
        for (FH2Hook* h : {ctor, dtor, tick})
            if (h) g_api->disable_hooks(g_mod, h);
        logf("the HUD notification hooks aren't all in: no pop-ups (achievements still unlock)");
    }
    logf("%d of 5 hooks made", made);
    plog(std::to_string(made) + " of 5 hooks made; logging to fh2-mods\\Achievements\\Achievements.log");

    GameData gd;
    std::string why;
    auto load_game_data = [&]() {
        if (!read_game_data(g_gameDir, gd, why)) return false;
        logf("game data: %zu championship races in %zu car types, %d + %d championships, %zu Gauntlets, %zu bucket list "
             "challenges, %zu Porsches",
             gd.champEventBucket.size(), gd.mainlandBuckets.size(), gd.mainlandCampaigns, gd.islandCampaigns,
             gd.gauntletCampaigns.size(), gd.bucketList.size(), gd.porsches.size());
        return true;
    };
    // Without the game data, race ends can't be counted (their weather, car type and Porsches come
    // from it), so they wait while it's tried again every 30 seconds, for up to 10 minutes.
    const ULONGLONG gdStart = GetTickCount64();
    ULONGLONG gdTried = gdStart;
    bool gdGaveUp = false;
    std::deque<Event> heldRaces;
    if (!load_game_data()) logf("%s; trying again every 30 seconds", why.c_str());

    FILETIME startFt;
    GetSystemTimeAsFileTime(&startFt);
    const uint64_t started = (static_cast<uint64_t>(startFt.dwHighDateTime) << 32) | startFt.dwLowDateTime;
    std::wstring userDir, user;
    int profiles = 0;
    uint64_t written = 0;
    while (!find_user_dir(g_azimuth, userDir, user, why, &profiles, &written)) {
        static bool told = false;
        if (!told) logf("%s; waiting for one", why.c_str());
        told = true;
        flush_log();
        Sleep(5000);
    }
    // With more than one profile, the one being played is only certain once the game saves to it:
    // until then the events wait in their queue and nothing is worked out, so no progress goes to the
    // wrong profile.
    bool sure = profiles <= 1 || written > started;
    if (!sure) logf("%d profiles: waiting for the game to save, to know which one is played", profiles);
    std::wstring storePath = g_dir + L"\\" + user + L".ini";
    const std::wstring listPath = g_dir + L"\\Achievements.txt";
    Store store;
    load_store(storePath, store);
    logf("profile %s: %zu achievements unlocked before%s", narrow(user).c_str(), store.unlocked.size(),
         store.existed ? "" : " (first run)");

    SaveFacts save;
    LiveFacts live;
    bool firstEval = true, storeDirty = false, storeUrgent = false, dirty = false, listDirty = false, pageDirty = true;
    uint32_t statVersion = ~0u;
    ULONGLONG lastScan = 0, lastFlush = 0, lastStoreWrite = 0, lastList = 0, lastPage = 0;
    std::map<std::string, ULONGLONG> refused;   // revision -> when it was last refused
    std::string notedDb;                        // the PlayerDatabase note last logged

    for (;;) {
        const ULONGLONG now = GetTickCount64();
        // 1. A newer finished save revision? The newest one may still be being written: then
        // keep what we have and try it again in a few seconds (or, at the start, use the one before).
        if (now - lastScan > 5000) {
            lastScan = now;
            // Another profile being saved to now (an install with two, such as one for online play):
            // follow it, with its own unlock file, so its save and its progress aren't missed.
            std::wstring nextDir, nextUser;
            std::string whyNot;
            const bool foundUser = find_user_dir(g_azimuth, nextDir, nextUser, whyNot, &profiles, &written);
            if (foundUser && nextUser != user) {
                if (storeDirty && !save_store(storePath, store)) logf("couldn't write %s", narrow(storePath).c_str());
                storeDirty = false;
                logf("profile changed: %s -> %s", narrow(user).c_str(), narrow(nextUser).c_str());
                user = nextUser;
                userDir = nextDir;
                storePath = g_dir + L"\\" + user + L".ini";
                store = Store{};
                load_store(storePath, store);
                logf("profile %s: %zu achievements unlocked before%s", narrow(user).c_str(), store.unlocked.size(),
                     store.existed ? "" : " (first run)");
                save = SaveFacts{};
                // stats and boards seen live may be the other profile's: its save says the rest
                {
                    std::lock_guard<std::mutex> lk(g_statMu);
                    g_liveStats.clear();
                }
                live = LiveFacts{};
                refused.clear();
                firstEval = true;
                dirty = false;
                listDirty = pageDirty = true;
            }
            if (!sure && foundUser && written > started) {
                sure = true;
                dirty = true;
                logf("the game saved to %s: that's the profile being played", narrow(user).c_str());
            }
            for (const std::wstring& rev : list_revisions(userDir)) {
                const std::string r = narrow(rev);
                if (save.ok && r == save.revision) break;
                auto it = refused.find(r);
                if (it != refused.end() && now - it->second < 15000) {
                    if (save.ok) break;
                    continue;
                }
                SaveFacts next;
                if (read_revision(userDir + L"\\revisions\\" + rev, g_dir + L"\\cache", &save, next, why)) {
                    save = std::move(next);
                    dirty = true;
                    if (take_baselines(save, store.ctr)) {
                        storeDirty = true;
                        logf("bounty wins before the mod, from the save: %lld",
                             static_cast<long long>(store.ctr.get("h2h.bounty.base")));
                    }
                    logf("read save revision %s: %zu stats, %zu objects, %zu cars, %zu championships done", r.c_str(),
                         save.stats.size(), save.objects.size(), save.garage.size(), save.champsCompleted.size());
                    if (save.dbNotes != notedDb) {
                        notedDb = save.dbNotes;
                        if (!notedDb.empty()) logf("save tables not read (counted as empty): %s", notedDb.c_str());
                    }
                    break;
                }
                if (it == refused.end()) logf("save revision %s not used yet: %s", r.c_str(), why.c_str());
                refused[r] = now;
                if (save.ok) break;
            }
        }
        // 2. events, once the profile is certain; race ends once the game data is in (or can't be had)
        if (!gd.ok && !gdGaveUp && now - gdTried > 30000) {
            gdTried = now;
            if (load_game_data()) {
                dirty = true;
            } else if (now - gdStart > 600000) {
                gdGaveUp = true;
                logf("%s; giving up: race ends are counted without weather, car types or Porsches this session",
                     why.c_str());
            }
        }
        std::deque<Event> evs;
        if (sure) {
            std::lock_guard<std::mutex> lk(g_evMu);
            evs.swap(g_events);
        }
        const bool gdReady = gd.ok || gdGaveUp;
        if (gdReady && !heldRaces.empty()) {
            evs.insert(evs.begin(), heldRaces.begin(), heldRaces.end());
            heldRaces.clear();
        }
        for (const Event& e : evs) {
            if (!gdReady && e.id == ev::AnthemRaceEnd) {
                if (heldRaces.size() < 5000) heldRaces.push_back(e);
                continue;
            }
            if (count_event(e, gd, store.ctr, live)) {
                dirty = storeDirty = true;
                // the frequent ones can wait for the next write; anything else is written at once,
                // as the game is ended without warning
                if (e.id != ev::SkillAchieved && e.id != ev::SkillChainCompleted && e.id != ev::IslandSkillChainCompleted)
                    storeUrgent = true;
            }
        }
        // 3. live stats
        if (g_statVersion != statVersion) {
            statVersion = g_statVersion;
            std::lock_guard<std::mutex> lk(g_statMu);
            live.stats = g_liveStats;
            dirty = true;
        }
        // 4. the rules
        if (dirty && save.ok && sure) {
            dirty = false;
            Facts f{gd, save, live, store.ctr};
            std::vector<const Def*> fresh;
            int gs = 0;
            for (const Def& d : defs()) {
                if (!d.eval || store.unlocked.count(d.id)) continue;
                if (!d.eval(f).done()) continue;
                store.unlocked[d.id] = now_utc();
                fresh.push_back(&d);
                gs += d.gamerscore;
                logf("unlocked: %s (%dG)", d.name, d.gamerscore);
            }
            if (!fresh.empty()) {
                storeDirty = storeUrgent = true;
                char sub[128];
                if (firstEval && (!store.existed || fresh.size() >= 3)) {
                    std::snprintf(sub, sizeof sub, "%zu unlocked from your save - %dG", fresh.size(), gs);
                    queue_popup("ACHIEVEMENTS UNLOCKED", sub);
                } else if (fresh.size() >= 4) {
                    std::snprintf(sub, sizeof sub, "%zu unlocked - %dG", fresh.size(), gs);
                    queue_popup("ACHIEVEMENTS UNLOCKED", sub);
                } else {
                    for (const Def* d : fresh) {
                        std::snprintf(sub, sizeof sub, "%s - %dG", d->name, d->gamerscore);
                        queue_popup("ACHIEVEMENT UNLOCKED", sub);
                    }
                }
            }
            if (firstEval) {
                firstEval = false;
                if (!store.existed) storeDirty = true;   // so a first run with nothing unlocked still counts as run
            }
            listDirty = pageDirty = true;
            if (!fresh.empty()) lastList = lastPage = 0;   // write them now
        }
        // The page: kept up to date for when it's opened (it's written then, from these rows).
        // Before a save has been read, it has what's unlocked, without progress.
        if (pageDirty && now - lastPage > 2000) {
            if (save.ok) {
                const Facts f{gd, save, live, store.ctr};
                push_page(&f, store);
            } else {
                push_page(nullptr, store);
            }
            pageDirty = false;
            lastPage  = now;
        }
        if (listDirty && save.ok && now - lastList > 10000) {
            write_list(listPath, Facts{gd, save, live, store.ctr}, store);
            listDirty = false;
            lastList = now;
        }
        if (storeDirty && (storeUrgent || now - lastStoreWrite > 3000)) {
            if (save_store(storePath, store)) storeDirty = false;
            else logf("couldn't write %s", narrow(storePath).c_str());
            storeUrgent = false;   // after a failed write, the next try is in 3 seconds
            lastStoreWrite = now;
        }
        // The game world is up a minute after the HUD's notifications are made, at the latest.
        if (!g_ready && g_firstCompTick && now - g_firstCompTick > 60000) g_ready = true;
        if (now - lastFlush > 1000) {
            flush_log();
            lastFlush = now;
        }
        Sleep(250);
    }
}

}  // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (!api || api->api_version < 7) return 1;
    g_api = api;
    g_t0 = GetTickCount64();
    // settings, with a commented file the first time
    g_iniPath = std::wstring(api->azimuth_dir) + L"\\fh2-mods\\Achievements.ini";
    if (GetFileAttributesW(g_iniPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        std::ofstream(g_iniPath.c_str(), std::ios::binary)
            << "; Achievements (fh2-mods\\Achievements.ini). Delete this file to get the defaults back.\r\n"
               "[Achievements]\r\n"
               "; 0 = no pop-up when you unlock an achievement (also under Pause > Settings > MODS)\r\n"
               "Popups=1\r\n"
               "; 0 = don't check GitHub for a newer version\r\n"
               "CheckUpdates=1\r\n";
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "Achievements";
    info.version          = ACH_VERSION;
    info.github_repo      = "Trinculo54/fh2-achievements";
    info.check_updates    = GetPrivateProfileIntW(L"Achievements", L"CheckUpdates", 1, g_iniPath.c_str());
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.description      = "Forza Horizon 2's 88 achievements, earned offline, with the game's own pop-up. Progress "
                            "already in your save counts. The pause menu's ACHIEVEMENTS tile lists them all.";
    g_mod = api->register_mod(&info);
    if (!g_mod) return 1;

    g_azimuth = api->azimuth_dir;
    g_gameDir = api->game_dir ? api->game_dir : L"";
    g_dir     = g_azimuth + L"\\fh2-mods\\Achievements";
    CreateDirectoryW(g_dir.c_str(), nullptr);
    CreateDirectoryW((g_dir + L"\\cache").c_str(), nullptr);
    g_logPath = g_dir + L"\\Achievements.log";
    HANDLE h = CreateFileW(g_logPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
    logf("Achievements %s", ACH_VERSION);

    g_popupsOn = GetPrivateProfileIntW(L"Achievements", L"Popups", 1, g_iniPath.c_str()) != 0;
    FH2Setting s{};
    s.size        = sizeof s;
    s.key         = "Popups";
    s.label       = "Achievement pop-ups";
    s.description = "Show the game's notification when you unlock an achievement.";
    s.value       = g_popupsOn ? 1 : 0;
    s.on_change   = &on_setting;
    api->add_setting(g_mod, &s);

    // The page: on the pause menu's ACHIEVEMENTS tile (and the Storm Island tab's), of the mod's
    // own design. Its XAML comes from the worker.
    if (api->api_version >= 8) {
        if (const std::string bad = section_check(); !bad.empty()) logf("the page's sections are wrong: %s", bad.c_str());
        FH2Page p{};
        p.size        = sizeof p;
        p.key         = "List";
        p.title       = "Achievements";
        p.label       = "Achievements list";
        p.description = "Every achievement, unlocked or not, with your progress towards the locked ones.";
        p.tile        = "Achievements";
        p.on_button   = &on_page_button;
        p.on_slot     = &on_page_slot;
        g_pageOn      = api->add_page(g_mod, &p) != 0;
        if (g_pageOn) {
            Store none;
            push_page(nullptr, none);   // until the profile's been read: nothing unlocked
        }
    }

    if (HANDLE t = CreateThread(nullptr, 0, worker, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
