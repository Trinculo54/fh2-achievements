// Reading the save, read-only. Azimuth keeps each profile's save as numbered revision folders:
//
//   <azimuth>\data\titles\1A96545B\saves\player-1\User_<id>\revisions\<16 hex digits>\
//       commit             "Azimuth complete save revision v2", "blobs N", then one
//                          "<sha256> <size> <name>" line per file in data\
//       data\SaveState     XML: <Object Id=".." Type=".."> holding <Property Name=".." Value=".."/>
//       data\ProfileData   "ssmc", u32 payload size, u32 root count, then nodes: u32 name length,
//                          name, 20 00 00 00 00, u32 type, value (type 15 = group: u32 child count;
//                          0/1 byte, 3/7 int32, 4 uint64, 9 float, 12 u32 length + UTF-16)
//       data\PlayerDatabase  8-byte header, then a SQLite database: the profile's tables
//       data\BinarySaveState, data\VersionFlags  (not used)
//
// (The formats are as documented by AnthemSaveEditor, github.com/qiangqiang101/AnthemSaveEditor.)
// The game writes a new revision every few minutes and keeps the last two. A revision is only
// read once every file matches its commit line, so a half-written one is never used.
#include "ach.hpp"

#include <windows.h>
#include <bcrypt.h>

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <fstream>
#include <sstream>

namespace ach {

namespace {

std::string narrow(const std::wstring& w) {
    std::string s;
    for (wchar_t c : w) s.push_back(c < 128 ? static_cast<char>(c) : '?');
    return s;
}

bool read_file(const std::wstring& path, std::string& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(h, &size) && size.QuadPart < (256ll << 20);
    if (ok) {
        out.resize(static_cast<size_t>(size.QuadPart));
        DWORD got = 0;
        ok = out.empty() || (ReadFile(h, out.data(), static_cast<DWORD>(out.size()), &got, nullptr) && got == out.size());
    }
    CloseHandle(h);
    return ok;
}

bool write_file(const std::wstring& path, const char* data, size_t n) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    DWORD w = 0;
    bool ok = WriteFile(h, data, static_cast<DWORD>(n), &w, nullptr) && w == n;
    CloseHandle(h);
    return ok;
}

std::string sha256_hex(const std::string& data) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    unsigned char out[32];
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0) return hex;
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
        if (BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(data.data())), static_cast<ULONG>(data.size()), 0) == 0 &&
            BCryptFinishHash(hash, out, sizeof out, 0) == 0) {
            static const char* d = "0123456789abcdef";
            for (unsigned char c : out) {
                hex.push_back(d[c >> 4]);
                hex.push_back(d[c & 15]);
            }
        }
        BCryptDestroyHash(hash);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return hex;
}

// ---- SaveState ------------------------------------------------------------------------------

std::string xml_unescape(std::string s) {
    static const std::pair<const char*, const char*> kEnt[] = {
        {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&amp;", "&"}};
    if (s.find('&') == std::string::npos) return s;
    for (const auto& [from, to] : kEnt) {
        size_t p = 0;
        while ((p = s.find(from, p)) != std::string::npos) {
            s.replace(p, std::strlen(from), to);
            p += std::strlen(to);
        }
    }
    return s;
}

// The value of attribute `name` inside the tag text [b, e).
bool attr(const std::string& t, size_t b, size_t e, const char* name, std::string& out) {
    const std::string key = std::string(name) + "=\"";
    size_t p = t.find(key, b);
    if (p == std::string::npos || p >= e) return false;
    p += key.size();
    size_t q = t.find('"', p);
    if (q == std::string::npos || q > e) return false;
    out = xml_unescape(t.substr(p, q - p));
    return true;
}

bool parse_savestate(const std::string& t, SaveFacts& out, std::string& why) {
    std::string cur;
    size_t p = 0;
    int objects = 0;
    while ((p = t.find('<', p)) != std::string::npos) {
        const size_t e = t.find('>', p);
        if (e == std::string::npos) break;
        if (t.compare(p, 8, "<Object ") == 0) {
            std::string id, type;
            if (attr(t, p, e, "Id", id) && attr(t, p, e, "Type", type)) {
                cur = id;
                out.objectType[id] = type;
                out.objects[id];
                ++objects;
            }
        } else if (t.compare(p, 10, "<Property ") == 0 && !cur.empty()) {
            std::string name, value;
            if (attr(t, p, e, "Name", name) && attr(t, p, e, "Value", value)) out.objects[cur][name] = value;
        } else if (t.compare(p, 9, "</Object>") == 0) {
            cur.clear();
        }
        p = e + 1;
    }
    if (!objects) why = "SaveState: no objects";
    return objects > 0;
}

// ---- ProfileData ----------------------------------------------------------------------------

struct Reader {
    const std::string& d;
    size_t p, end;
    bool ok = true;
    uint32_t u32() {
        if (p + 4 > end) {
            ok = false;
            return 0;
        }
        uint32_t v;
        std::memcpy(&v, d.data() + p, 4);
        p += 4;
        return v;
    }
    template <class T> T get() {
        if (p + sizeof(T) > end) {
            ok = false;
            return T{};
        }
        T v;
        std::memcpy(&v, d.data() + p, sizeof v);
        p += sizeof v;
        return v;
    }
    void skip(size_t n) {
        if (p + n > end) ok = false;
        else p += n;
    }
};

bool profile_node(Reader& r, const std::string& parent, SaveFacts& out, int depth) {
    if (depth > 16) return false;
    const uint32_t len = r.u32();
    if (!r.ok || len > 256 || r.p + len > r.end) return false;
    const std::string name = r.d.substr(r.p, len);
    r.skip(len);
    r.skip(5);   // 20 00 00 00 00
    const uint32_t type = r.u32();
    if (!r.ok) return false;
    const std::string path = parent.empty() ? name : parent + "/" + name;
    double v = 0;
    switch (type) {
        case 15: {
            const uint32_t n = r.u32();
            if (n > 100000) return false;
            for (uint32_t i = 0; i < n && r.ok; ++i)
                if (!profile_node(r, path, out, depth + 1)) return false;
            return r.ok;
        }
        case 0:
        case 1: v = r.get<uint8_t>(); break;
        case 3:
        case 7: v = r.get<int32_t>(); break;
        case 4: v = static_cast<double>(r.get<uint64_t>()); break;
        case 9: v = r.get<float>(); break;
        case 12: {
            const uint32_t chars = r.u32();
            if (chars > 65536) return false;
            r.skip(2ull * chars);
            return r.ok;   // text values aren't used
        }
        default: return false;
    }
    if (r.ok) out.stats[path] = v;
    return r.ok;
}

bool parse_profile(const std::string& d, SaveFacts& out, std::string& why) {
    if (d.size() < 12 || d.compare(0, 4, "ssmc") != 0) {
        why = "ProfileData: no 'ssmc' signature";
        return false;
    }
    uint32_t payload;
    std::memcpy(&payload, d.data() + 4, 4);
    if (8ull + payload > d.size()) {
        why = "ProfileData: bad size";
        return false;
    }
    Reader r{d, 8, 8 + static_cast<size_t>(payload)};
    const uint32_t roots = r.u32();
    for (uint32_t i = 0; i < roots && r.ok; ++i)
        if (!profile_node(r, "", out, 0)) {
            why = "ProfileData: unexpected layout";
            return false;
        }
    return r.ok;
}

// ---- PlayerDatabase -------------------------------------------------------------------------

bool read_player_db(const std::string& data, const std::wstring& scratch, SaveFacts& out, std::string& why) {
    if (data.size() < 24 || data.compare(8, 16, std::string("SQLite format 3\0", 16)) != 0) {
        why = "PlayerDatabase: no SQLite database after its header";
        return false;
    }
    CreateDirectoryW(scratch.c_str(), nullptr);
    const std::wstring copy = scratch + L"\\save.sqlite";
    if (!write_file(copy, data.data() + 8, data.size() - 8)) {
        why = "can't write " + narrow(copy);
        return false;
    }
    Sql* db = sql_open_readonly(copy, why);
    if (!db) return false;
    std::string err;
    auto q = [&](const char* sql, const std::function<void(const SqlRow&)>& row) {
        std::string e;
        if (!sql_ints(db, sql, row, e) && err.empty()) err = std::string(sql) + ": " + e;
    };
    q("SELECT CampaignId, Won FROM Career_ChampionshipsCompleted", [&](const SqlRow& r) {
        out.champsCompleted.insert(static_cast<int>(r[0]));
        if (r[1]) out.champsWon.insert(static_cast<int>(r[0]));
    });
    q("SELECT CampaignId FROM Career_ChampionshipsWon", [&](const SqlRow& r) { out.champsWon.insert(static_cast<int>(r[0])); });
    q("SELECT CampaignId, Won FROM IslandCareer_ChampionshipsCompleted", [&](const SqlRow& r) {
        out.islandChampsCompleted.insert(static_cast<int>(r[0]));
        if (r[1]) out.islandChampsWon.insert(static_cast<int>(r[0]));
    });
    q("SELECT CampaignId FROM IslandCareer_ChampionshipsWon",
      [&](const SqlRow& r) { out.islandChampsWon.insert(static_cast<int>(r[0])); });
    q("SELECT EventId, Place FROM Career_EventResults", [&](const SqlRow& r) {
        if (r[1] > 0) out.eventPlace[static_cast<int>(r[0])] = static_cast<int>(r[1]);
    });
    q("SELECT CarId FROM Career_Garage", [&](const SqlRow& r) { out.garage.push_back(static_cast<int>(r[0])); });
    q("SELECT UnlockId FROM PerkUnlock", [&](const SqlRow& r) { out.perkUnlocks.insert(static_cast<int>(r[0])); });
    q("SELECT count(*) FROM PhotoCaptures", [&](const SqlRow& r) { out.photos = static_cast<int>(r[0]); });
    q("SELECT ChallengeId, Completed, CompletedCoop FROM BucketListProgress",
      [&](const SqlRow& r) { out.bucket[static_cast<int>(r[0])] = {r[1] != 0, r[2] != 0}; });
    sql_close(db);
    DeleteFileW(copy.c_str());
    if (!err.empty()) {
        why = "PlayerDatabase: " + err;
        return false;
    }
    return true;
}

}  // namespace

// ---- finding the save -----------------------------------------------------------------------

bool find_user_dir(const std::wstring& azimuthDir, std::wstring& userDir, std::wstring& user, std::string& why) {
    const std::wstring base = azimuthDir + L"\\data\\titles\\1A96545B\\saves\\player-1";
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((base + L"\\User_*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        why = "no save found in " + narrow(base);
        return false;
    }
    FILETIME best{};
    bool found = false;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        // only the profiles themselves (User_ and the hex id), not copies such as "User_<id>.fresh-..."
        const wchar_t* id = fd.cFileName + 5;
        if (!*id || std::wcsspn(id, L"0123456789ABCDEFabcdef") != std::wcslen(id)) continue;
        WIN32_FILE_ATTRIBUTE_DATA a;
        const std::wstring rev = base + L"\\" + fd.cFileName + L"\\revisions";
        if (!GetFileAttributesExW(rev.c_str(), GetFileExInfoStandard, &a)) continue;
        if (!found || CompareFileTime(&a.ftLastWriteTime, &best) > 0) {
            best = a.ftLastWriteTime;
            userDir = base + L"\\" + fd.cFileName;
            user = fd.cFileName;
            found = true;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    if (!found) why = "no User_ folder with revisions in " + narrow(base);
    return found;
}

std::vector<std::wstring> list_revisions(const std::wstring& userDir) {
    std::vector<std::pair<uint64_t, std::wstring>> revs;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((userDir + L"\\revisions\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.') continue;
            wchar_t* end = nullptr;
            const uint64_t n = wcstoull(fd.cFileName, &end, 16);
            if (end && !*end) revs.emplace_back(n, fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    std::sort(revs.begin(), revs.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<std::wstring> out;
    for (auto& r : revs) out.push_back(r.second);
    return out;
}

bool read_revision(const std::wstring& revDir, const std::wstring& scratch, const SaveFacts* prev, SaveFacts& out,
                   std::string& why) {
    out = SaveFacts{};
    std::string commit;
    if (!read_file(revDir + L"\\commit", commit)) {
        why = "no commit file yet";
        return false;
    }
    // "<sha256> <size> <name>" per file
    std::map<std::string, std::pair<std::string, uint64_t>> manifest;
    std::istringstream in(commit);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (n++ == 0) {
            if (line.rfind("Azimuth complete save revision", 0) != 0) {
                why = "unknown commit format";
                return false;
            }
            continue;
        }
        std::istringstream ls(line);
        std::string sha, name;
        uint64_t size = 0;
        if (ls >> sha >> size >> name && sha.size() == 64) manifest[name] = {sha, size};
    }
    std::string files[3];
    const char* names[3] = {"SaveState", "ProfileData", "PlayerDatabase"};
    for (int i = 0; i < 3; ++i) {
        auto it = manifest.find(names[i]);
        if (it == manifest.end()) {
            why = std::string(names[i]) + " isn't in the commit file";
            return false;
        }
        // The database (6 MB) changes far less often than the save is written: when its hash is
        // the one already read, its tables are taken from the last read instead.
        if (i == 2 && prev && prev->ok && prev->pdbSha == it->second.first) {
            out.pdbSha = prev->pdbSha;
            continue;
        }
        if (!read_file(revDir + L"\\data\\" + std::wstring(names[i], names[i] + std::strlen(names[i])), files[i]) ||
            files[i].size() != it->second.second || sha256_hex(files[i]) != it->second.first) {
            why = std::string(names[i]) + " doesn't match the commit file (still being written?)";
            return false;
        }
        if (i == 2) out.pdbSha = it->second.first;
    }
    if (!parse_savestate(files[0], out, why) || !parse_profile(files[1], out, why)) return false;
    if (files[2].empty()) {
        out.champsCompleted = prev->champsCompleted;
        out.champsWon = prev->champsWon;
        out.islandChampsCompleted = prev->islandChampsCompleted;
        out.islandChampsWon = prev->islandChampsWon;
        out.perkUnlocks = prev->perkUnlocks;
        out.eventPlace = prev->eventPlace;
        out.garage = prev->garage;
        out.photos = prev->photos;
        out.bucket = prev->bucket;
    } else if (!read_player_db(files[2], scratch, out, why)) {
        return false;
    }
    out.revision = narrow(revDir.substr(revDir.find_last_of(L'\\') + 1));
    out.ok = true;
    return true;
}

// ---- the game's database --------------------------------------------------------------------

bool read_game_data(const std::wstring& gameDir, GameData& out, std::string& why) {
    out = GameData{};
    // The platform's copy (with every mod's database changes) if there is one, else the game's.
    std::wstring path = gameDir + L"\\mediaoverride\\Stripped\\gamedbRC.slt";
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) path = gameDir + L"\\media\\Stripped\\gamedbRC.slt";
    Sql* db = sql_open_readonly(path, why);
    if (!db) {
        why = "game database (" + narrow(path) + "): " + why;
        return false;
    }
    std::string err;
    auto q = [&](const char* sql, const std::function<void(const SqlRow&)>& row) {
        std::string e;
        if (!sql_ints(db, sql, row, e) && err.empty()) err = std::string(sql) + ": " + e;
    };
    q("SELECT e.AnthemEventId, c.CarBucket FROM AnthemEvents e JOIN Campaigns c ON c.Id = e.CampaignId "
      "WHERE e.EventType = 0 AND c.CareerLocationId = 0",
      [&](const SqlRow& r) {
          out.champEventBucket[static_cast<int>(r[0])] = static_cast<int>(r[1]);
          out.mainlandBuckets.insert(static_cast<int>(r[1]));
      });
    q("SELECT CareerLocationId, count(*) FROM Campaigns GROUP BY CareerLocationId", [&](const SqlRow& r) {
        if (r[0] == 0) out.mainlandCampaigns = static_cast<int>(r[1]);
        if (r[0] == 1) out.islandCampaigns = static_cast<int>(r[1]);
    });
    q("SELECT DISTINCT e.CampaignId FROM AnthemEvents e JOIN AnthemEventRaces r ON r.AnthemEventId = e.AnthemEventId "
      "JOIN RaceModes m ON m.id = r.RaceModeId WHERE m.Enum = 'IronMan'",
      [&](const SqlRow& r) { out.gauntletCampaigns.insert(static_cast<int>(r[0])); });
    q("SELECT ChallengeId, ListId FROM BucketListChallenges",
      [&](const SqlRow& r) { out.bucketList[static_cast<int>(r[0])] = static_cast<int>(r[1]); });
    q("SELECT Id FROM Data_Car WHERE MediaName LIKE 'POR!_%' ESCAPE '!'",
      [&](const SqlRow& r) { out.porsches.insert(static_cast<int>(r[0])); });
    q("SELECT Id, Cost FROM Perks", [&](const SqlRow& r) {
        out.perkCosts[static_cast<int>(r[0])] = static_cast<int>(r[1]);
    });
    // the weather each event's race is run in (WeatherPreset 7, 8: StormB; 11-13: StormTropical)
    q("SELECT AnthemEventId, max(WeatherPrecipitationLevel > 0), max(WeatherPrecipitationLevel > 0 OR WeatherEnvironmentWetness > 0), "
      "max(WeatherPreset IN (7, 8, 11, 12, 13)) FROM AnthemEventRaces WHERE CustomWeather = 1 GROUP BY AnthemEventId",
      [&](const SqlRow& r) {
          const int ev = static_cast<int>(r[0]);
          if (r[1]) out.rainEvents.insert(ev);
          if (r[2]) out.wetEvents.insert(ev);
          if (r[3]) out.stormEvents.insert(ev);
      });
    sql_close(db);
    if (!err.empty()) {
        why = "game database: " + err;
        return false;
    }
    out.ok = true;
    return true;
}

}  // namespace ach
