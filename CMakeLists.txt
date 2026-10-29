cmake_minimum_required(VERSION 3.20)
# Bump VERSION before tagging a release (the tag build checks they match).
project(achievements VERSION 1.0.2 LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
if(NOT WIN32)
    message(FATAL_ERROR "Targets Windows. On Linux use -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake")
endif()

add_compile_definitions(_WIN32_WINNT=0x0A00 WIN32_LEAN_AND_MEAN NOMINMAX)
if(MSVC)
    message(FATAL_ERROR "Build with llvm-mingw (clang).")
else()
    add_compile_options(-Wall -Wno-missing-field-initializers -fms-extensions)
    add_compile_options("$<$<CONFIG:Release>:-O2;-ffunction-sections;-fdata-sections>")
    add_link_options("$<$<CONFIG:Release>:-Wl,--gc-sections;-s>")
    add_link_options(-static)
endif()

# The FH2 Mod Platform (git submodule): fh2mod.h.
add_subdirectory(platform)
configure_file(cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/fh2_version.h)

# SQLite (public domain, third_party/sqlite/VERSION.txt), to read the save and the game
# database. Windows has its own (winsqlite3.dll), but Wine and Proton don't.
set(SQLITE_SRC third_party/sqlite/sqlite3.c)
set_source_files_properties(${SQLITE_SRC} PROPERTIES
    COMPILE_OPTIONS "-w;-Os"
    COMPILE_DEFINITIONS "SQLITE_OMIT_LOAD_EXTENSION;SQLITE_DEFAULT_MEMSTATUS=0")

# The mod: fh2-mods\Achievements.dll
add_library(achievements SHARED
    src/achievements.cpp
    src/ui.cpp
    src/rules.cpp
    src/save_reader.cpp
    src/sqlite.cpp
    src/version.rc
    ${SQLITE_SRC})
target_include_directories(achievements PRIVATE src third_party/sqlite ${CMAKE_BINARY_DIR}/generated)
target_compile_definitions(achievements PRIVATE ACH_VERSION="${PROJECT_VERSION}")
target_link_libraries(achievements PRIVATE fh2mod_api bcrypt)
set_target_properties(achievements PROPERTIES OUTPUT_NAME Achievements PREFIX "")
