#pragma once
// autodos.h — AutoDOS core library public interface
// Drop any DOS game zip. AutoDOS figures out the rest.

#include <string>
#include <vector>

namespace AutoDOS {

// ── Result from analyze() ─────────────────────────────────────────────────────

struct AnalyzeResult {
    bool        success     = false;
    std::string exe;          // relative path inside extracted zip e.g. "DOOM.EXE"
    std::string workDir;      // subdirectory to cd into e.g. "DAGGER"
    std::string gameType;     // SIMPLE | INSTALLED | CD_BASED | BATCH_LAUNCHER | COMPLEX
    std::string source;       // "database" | "scored" | "batch"
    float       confidence  = 0.0f;
    std::string error;

    // From games.json database entry (may be empty for scored results)
    std::string title;
    std::string cycles;       // e.g. "max limit 35000" or "65000"
    int         memsize     = 16;
    bool        ems         = true;
    bool        xms         = true;
    bool        cdMount     = false;
};

// ── Game library entry ────────────────────────────────────────────────────────

struct GameEntry {
    std::string id;
    std::string title;
    std::string zipPath;
    std::string confPath;
    std::string exe;
    std::string source;    // "database" | "scored"
    bool        cdMount  = false;
};

// ── Core API ──────────────────────────────────────────────────────────────────

// Analyze a zip file and return launch configuration
AnalyzeResult analyze(const std::string& zipPath, const std::string& dbPath);

// Extract zip to outDir
bool extractZip(const std::string& zipPath, const std::string& outDir);

// Write a dosbox.conf using the analyze result
bool writeDosboxConf(const std::string& zipPath,
                     const std::string& extractedDir,
                     const AnalyzeResult& result);

// Launch DOSBox with the conf
bool launchDosBox(const std::string& dosboxPath, const std::string& confPath);

// Fingerprint a filename for database lookup
std::string fingerprint(const std::string& filename);

// Add a game to games.json
bool addToDatabase(const std::string& dbPath, const AnalyzeResult& result);

} // namespace AutoDOS
