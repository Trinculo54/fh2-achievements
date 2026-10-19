// SQLite from Windows itself (System32\winsqlite3.dll, in every Windows 10 and 11), as
// FH2ML does for its database patches, so the mod doesn't carry a copy. Read-only use.
#include "ach.hpp"

#include <windows.h>

#include <mutex>

namespace ach {

namespace {

constexpr int SQLITE_OK = 0, SQLITE_ROW = 100, SQLITE_DONE = 101, SQLITE_OPEN_READONLY = 1;

struct Api {
    int (*open_v2)(const char*, void**, int, const char*) = nullptr;
    int (*close)(void*) = nullptr;
    int (*prepare_v2)(void*, const char*, int, void**, const char**) = nullptr;
    int (*step)(void*) = nullptr;
    int (*finalize)(void*) = nullptr;
    int (*column_count)(void*) = nullptr;
    int64_t (*column_int64)(void*, int) = nullptr;
    const char* (*errmsg)(void*) = nullptr;
    bool ok = false;
};

const Api& api() {
    static Api a;
    static std::once_flag once;
    std::call_once(once, [] {
        HMODULE h = LoadLibraryExW(L"winsqlite3.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!h) return;
        auto get = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(reinterpret_cast<void*>(GetProcAddress(h, name)));
            return fn != nullptr;
        };
        a.ok = get(a.open_v2, "sqlite3_open_v2") && get(a.close, "sqlite3_close") && get(a.prepare_v2, "sqlite3_prepare_v2") &&
               get(a.step, "sqlite3_step") && get(a.finalize, "sqlite3_finalize") &&
               get(a.column_count, "sqlite3_column_count") && get(a.column_int64, "sqlite3_column_int64") &&
               get(a.errmsg, "sqlite3_errmsg");
    });
    return a;
}

std::string utf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

}  // namespace

struct Sql {
    void* db = nullptr;
};

Sql* sql_open_readonly(const std::wstring& path, std::string& why) {
    const Api& a = api();
    if (!a.ok) {
        why = "winsqlite3.dll isn't available";
        return nullptr;
    }
    void* db = nullptr;
    if (a.open_v2(utf8(path).c_str(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        why = db ? a.errmsg(db) : "can't open the database";
        if (db) a.close(db);
        return nullptr;
    }
    return new Sql{db};
}

void sql_close(Sql* s) {
    if (!s) return;
    api().close(s->db);
    delete s;
}

bool sql_ints(Sql* s, const char* query, const std::function<void(const SqlRow&)>& row, std::string& why) {
    const Api& a = api();
    void* st = nullptr;
    if (!s || a.prepare_v2(s->db, query, -1, &st, nullptr) != SQLITE_OK) {
        why = s ? a.errmsg(s->db) : "no database";
        return false;
    }
    const int cols = a.column_count(st);
    SqlRow r(cols);
    int rc;
    while ((rc = a.step(st)) == SQLITE_ROW) {
        for (int c = 0; c < cols; ++c) r[c] = a.column_int64(st, c);
        row(r);
    }
    a.finalize(st);
    if (rc != SQLITE_DONE) {
        why = a.errmsg(s->db);
        return false;
    }
    return true;
}

}  // namespace ach
