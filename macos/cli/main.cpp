// main.cpp — AutoDOS CLI for macOS
//
// AutoDOS - Original by makuka97 (https://github.com/makuka97)
// macOS port by cargabsj175 (vibe coding approach)
//
// Command-line interface for managing and launching DOS games on macOS.
// This is part of the macOS port - the original Windows version is in src/

#include "autodos.h"
#include "platform.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ── Globals ───────────────────────────────────────────────────────────────────

static std::string g_appDir;
static std::string g_dbPath;
static std::string g_libPath;
static std::string g_dosboxPath;

struct LibEntry {
    std::string id;
    std::string title;
    std::string zipPath;
    std::string confPath;
    std::string source;
    float       confidence = 0.0f;
};

static std::vector<LibEntry> g_library;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string makeId() {
    static int n = 0;
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "g" + std::to_string(now) + "_" + std::to_string(n++);
}

static void printBanner() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════╗\n";
    std::cout << "║          AutoDOS for macOS               ║\n";
    std::cout << "║     Drop any DOS game, play instantly    ║\n";
    std::cout << "╚══════════════════════════════════════════╝\n";
    std::cout << "\n";
}

static void printUsage() {
    std::cout << "Usage:\n";
    std::cout << "  autodos <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  list                  List all games in library\n";
    std::cout << "  add <zip_path>        Add a DOS game zip to library\n";
    std::cout << "  launch <game_id>      Launch a game from library\n";
    std::cout << "  remove <game_id>      Remove a game from library\n";
    std::cout << "  analyze <zip_path>    Analyze a zip without adding\n";
    std::cout << "  help                  Show this help message\n\n";
}

// ── Library IO ────────────────────────────────────────────────────────────────

static void saveLibrary() {
    json arr = json::array();
    for (const auto& e : g_library)
        arr.push_back({{"id",e.id},{"title",e.title},
                       {"zipPath",e.zipPath},{"confPath",e.confPath},
                       {"source",e.source},{"confidence",e.confidence}});
    std::ofstream f(g_libPath);
    if (f.is_open()) f << arr.dump(2);
}

static void loadLibrary() {
    g_library.clear();
    std::ifstream f(g_libPath);
    if (!f.is_open()) return;
    try {
        json arr; f >> arr;
        for (const auto& item : arr) {
            LibEntry e;
            e.id         = item.value("id","");
            e.title      = item.value("title","");
            e.zipPath    = item.value("zipPath","");
            e.confPath   = item.value("confPath","");
            e.source     = item.value("source","");
            e.confidence = item.value("confidence",0.0f);
            if (!e.id.empty()) g_library.push_back(e);
        }
    } catch(...) {}
}

// ── Commands ──────────────────────────────────────────────────────────────────

static void cmdList() {
    loadLibrary();
    if (g_library.empty()) {
        std::cout << "Library is empty. Add games with: autodos add <zip>\n";
        return;
    }

    std::cout << "Library (" << g_library.size() << " games):\n";
    std::cout << "─────────────────────────────────────────────\n";
    for (const auto& e : g_library) {
        std::string src = (e.source == "database") ? "DB" 
                          : "Auto " + std::to_string((int)(e.confidence*100)) + "%";
        std::cout << "  [" << e.id << "] " << e.title << " [" << src << "]\n";
    }
}

static void cmdAnalyze(const std::string& zipPath) {
    if (!fs::exists(zipPath)) {
        std::cerr << "Error: File not found: " << zipPath << "\n";
        return;
    }

    std::cout << "Analyzing: " << fs::path(zipPath).filename().string() << "\n";
    
    auto result = AutoDOS::analyze(zipPath, g_dbPath);
    
    if (!result.success) {
        std::cerr << "Error: " << result.error << "\n";
        return;
    }

    std::cout << "\nAnalysis Result:\n";
    std::cout << "─────────────────────────────────────────────\n";
    std::cout << "  Title:     " << (result.title.empty() ? "(unknown)" : result.title) << "\n";
    std::cout << "  Executable: " << result.exe << "\n";
    std::cout << "  Work Dir:  " << (result.workDir.empty() ? "(root)" : result.workDir) << "\n";
    std::cout << "  Source:    " << result.source << "\n";
    std::cout << "  Confidence: " << (int)(result.confidence*100) << "%\n";
    std::cout << "  Game Type: " << result.gameType << "\n";
    if (!result.cycles.empty())
        std::cout << "  Cycles:    " << result.cycles << "\n";
    std::cout << "  Memsize:   " << result.memsize << "MB\n";
    std::cout << "  CD Mount:  " << (result.cdMount ? "yes" : "no") << "\n";
}

static void cmdAdd(const std::string& zipPath) {
    if (!fs::exists(zipPath)) {
        std::cerr << "Error: File not found: " << zipPath << "\n";
        return;
    }

    // Check for duplicates
    loadLibrary();
    for (const auto& e : g_library) {
        if (e.zipPath == zipPath) {
            std::cout << "Already in library: " << e.title << "\n";
            return;
        }
    }

    std::cout << "Analyzing: " << fs::path(zipPath).filename().string() << "\n";
    auto result = AutoDOS::analyze(zipPath, g_dbPath);

    if (!result.success) {
        std::cerr << "Error: " << result.error << "\n";
        return;
    }

    std::string stem = fs::path(zipPath).stem().string();
    std::string gameDir = g_appDir + "/games/" + stem;
    Platform::createDirectories(gameDir);

    std::cout << "Extracting: " << stem << "\n";
    if (!AutoDOS::extractZip(zipPath, gameDir)) {
        std::cerr << "Error: Extraction failed\n";
        return;
    }

    std::string confPath = g_appDir + "/games/" + stem + ".conf";
    AutoDOS::writeDosboxConf(zipPath, gameDir, result);

    // Copy conf to game directory
    std::string tempConf = zipPath.substr(0, zipPath.rfind('.')) + ".conf";
    if (fs::exists(tempConf)) {
        Platform::copyFile(tempConf, confPath);
        Platform::remove(tempConf);
    }

    std::string title = result.title.empty() ? stem : result.title;

    LibEntry entry;
    entry.id         = makeId();
    entry.title      = title;
    entry.zipPath    = zipPath;
    entry.confPath   = confPath;
    entry.source     = result.source;
    entry.confidence = result.confidence;

    g_library.push_back(entry);
    saveLibrary();

    if (result.source == "scored") {
        result.title = title;
        AutoDOS::addToDatabase(g_dbPath, result);
    }

    std::string src = (result.source == "database") ? "DB" 
                    : "Auto " + std::to_string((int)(result.confidence*100)) + "%";
    std::cout << "Added: " << title << " [" << src << "]\n";
    std::cout << "ID: " << entry.id << "\n";
}

static void cmdLaunch(const std::string& gameId) {
    loadLibrary();

    for (const auto& e : g_library) {
        if (e.id == gameId) {
            if (!fs::exists(e.confPath)) {
                std::cerr << "Error: Config file missing - re-import game\n";
                return;
            }

            if (!Platform::exists(g_dosboxPath)) {
                std::cerr << "Error: DOSBox not found at: " << g_dosboxPath << "\n";
                std::cerr << "Install DOSBox or update path in config\n";
                return;
            }

            std::cout << "Launching: " << e.title << "\n";
            if (!Platform::launchApp(g_dosboxPath, e.confPath)) {
                std::cerr << "Error: Failed to launch DOSBox\n";
            }
            return;
        }
    }

    std::cerr << "Error: Game not found: " << gameId << "\n";
    std::cout << "Use 'autodos list' to see available games\n";
}

static void cmdRemove(const std::string& gameId) {
    loadLibrary();

    for (auto it = g_library.begin(); it != g_library.end(); ++it) {
        if (it->id == gameId) {
            std::string title = it->title;
            std::string confPath = it->confPath;

            Platform::remove(confPath);
            Platform::removeAll(confPath.substr(0, confPath.rfind('.')));

            g_library.erase(it);
            saveLibrary();

            std::cout << "Removed: " << title << "\n";
            return;
        }
    }

    std::cerr << "Error: Game not found: " << gameId << "\n";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Initialize paths
    g_appDir     = Platform::appDataDir();
    g_dbPath     = g_appDir + "/games.json";
    g_libPath    = g_appDir + "/library.json";
    g_dosboxPath = Platform::exeDir() + "/dosbox/dosbox";

    Platform::createDirectories(g_appDir);
    Platform::createDirectories(g_appDir + "/games");

    // Copy games.json if not exists
    if (!Platform::exists(g_dbPath)) {
        std::string src = Platform::exeDir() + "/games.json";
        if (Platform::exists(src)) {
            Platform::copyFile(src, g_dbPath);
        }
    }

    if (argc < 2) {
        printBanner();
        printUsage();
        return 1;
    }

    std::string cmd = argv[1];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printBanner();
        printUsage();
        return 0;
    }

    if (cmd == "list") {
        cmdList();
        return 0;
    }

    if (cmd == "analyze") {
        if (argc < 3) {
            std::cerr << "Error: Missing zip path\n";
            std::cout << "Usage: autodos analyze <zip_path>\n";
            return 1;
        }
        cmdAnalyze(argv[2]);
        return 0;
    }

    if (cmd == "add") {
        if (argc < 3) {
            std::cerr << "Error: Missing zip path\n";
            std::cout << "Usage: autodos add <zip_path>\n";
            return 1;
        }
        cmdAdd(argv[2]);
        return 0;
    }

    if (cmd == "launch") {
        if (argc < 3) {
            std::cerr << "Error: Missing game ID\n";
            std::cout << "Usage: autodos launch <game_id>\n";
            return 1;
        }
        cmdLaunch(argv[2]);
        return 0;
    }

    if (cmd == "remove") {
        if (argc < 3) {
            std::cerr << "Error: Missing game ID\n";
            std::cout << "Usage: autodos remove <game_id>\n";
            return 1;
        }
        cmdRemove(argv[2]);
        return 0;
    }

    std::cerr << "Unknown command: " << cmd << "\n";
    printUsage();
    return 1;
}
