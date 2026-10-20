// SQLite, built into the mod (third_party/sqlite). Up to 1.0.0 it used Windows' own copy,
// System32\winsqlite3.dll, which Wine and Proton don't have, so on Linux no save could be read
// and nothing ever unlocked. Read-only use.
#include "ach.hpp"

#include <windows.h>

#include "sqlite3.h"

namespace ach {

namespace {

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

struct Sql {
    sqlite3* db = nullptr;
};

Sql* sql_open_readonly(const std::wstring& path, std::string& why) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(utf8(path).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        why = db ? sqlite3_errmsg(db) : "can't open the database";
        if (db) sqlite3_close(db);
        return nullptr;
    }
    return new Sql{db};
}

void sql_close(Sql* s) {
    if (!s) return;
    sqlite3_close(s->db);
    delete s;
}

bool sql_ints(Sql* s, const char* query, const std::function<void(const SqlRow&)>& row, std::string& why) {
    sqlite3_stmt* st = nullptr;
    if (!s || sqlite3_prepare_v2(s->db, query, -1, &st, nullptr) != SQLITE_OK) {
        why = s ? sqlite3_errmsg(s->db) : "no database";
        return false;
    }
    const int cols = sqlite3_column_count(st);
    SqlRow r(cols);
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        for (int c = 0; c < cols; ++c) r[c] = sqlite3_column_int64(st, c);
        row(r);
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) {
        why = sqlite3_errmsg(s->db);
        return false;
    }
    return true;
}

}  // namespace ach
