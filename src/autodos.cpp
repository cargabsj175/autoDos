// autodos.cpp — AutoDOS core library implementation
// Ported from autodos.js (JavaScript) to C++

#include "autodos.h"
#include "miniz.c"
#include "nlohmann/json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <regex>
#include <sstream>
#include <windows.h>

using json = nlohmann::json;

namespace AutoDOS {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    size_t end   = s.find_last_not_of(" \t\r\n");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

static std::string basename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

static std::string stemOf(const std::string& filename) {
    std::string b = basename(filename);
    size_t dot = b.rfind('.');
    return (dot == std::string::npos) ? b : b.substr(0, dot);
}

static std::string extOf(const std::string& filename) {
    std::string b = basename(filename);
    size_t dot = b.rfind('.');
    return (dot == std::string::npos) ? "" : toUpper(b.substr(dot + 1));
}

static std::string dirOf(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    size_t pos = normalized.rfind('/');
    if (pos == std::string::npos) return "";
    std::string dir = normalized.substr(0, pos);
    std::replace(dir.begin(), dir.end(), '/', '\\');
    return dir;
}

static bool endsWith(const std::string& s, const std::string& suffix) {
    if (suffix.size() > s.size()) return false;
    return s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// ── Blacklist ─────────────────────────────────────────────────────────────────

static const std::vector<std::string> EXE_BLACKLIST = {
    "setup", "install", "uninst", "uninstall", "patch", "update",
    "config", "cfg", "register", "readme", "read", "help",
    "directx", "dxsetup", "vcredist", "dotnet",
    "dos4gw", "cwsdpmi", "himemx", "emm386",
    "fixsave", "fix", "convert", "copy", "move"
};

static bool isBlacklisted(const std::string& stem) {
    std::string lower = toLower(stem);
    for (const auto& b : EXE_BLACKLIST) {
        if (lower == b) return true;
    }
    return false;
}

// ── ZIP entry ─────────────────────────────────────────────────────────────────

struct ZipEntry {
    std::string name;      // full path, normalized slashes
    int         depth = 0;
    std::string ext;
    std::string base;      // uppercase filename
    size_t      compSize = 0;
};

// ── ZIP reader ────────────────────────────────────────────────────────────────

static std::vector<ZipEntry> readZipEntries(const std::string& zipPath) {
    std::vector<ZipEntry> entries;

    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0))
        return entries;

    int count = (int)mz_zip_reader_get_num_files(&zip);
    for (int i = 0; i < count; i++) {
        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        std::string rawName = stat.m_filename;
        // Normalize backslashes
        std::replace(rawName.begin(), rawName.end(), '\\', '/');

        // Split into parts
        std::vector<std::string> parts;
        std::stringstream ss(rawName);
        std::string part;
        while (std::getline(ss, part, '/')) {
            if (!part.empty()) parts.push_back(part);
        }
        if (parts.empty()) continue;

        ZipEntry e;
        e.name     = rawName;
        e.depth    = (int)parts.size() - 1;
        e.base     = toUpper(parts.back());
        e.ext      = extOf(e.base);
        e.compSize = (size_t)stat.m_comp_size;
        entries.push_back(e);
    }

    mz_zip_reader_end(&zip);
    return entries;
}

// ── Normalize entries (strip single top-level wrapper) ───────────────────────

static std::vector<ZipEntry> normalizeEntries(std::vector<ZipEntry> entries) {
    if (entries.empty()) return entries;

    // Find top-level folders
    std::string firstTop;
    bool allSame = true;
    for (const auto& e : entries) {
        size_t slash = e.name.find('/');
        std::string top = (slash != std::string::npos) ? e.name.substr(0, slash) : "";
        if (firstTop.empty()) {
            firstTop = top;
        } else if (top != firstTop) {
            allSame = false;
            break;
        }
    }

    // Do NOT strip top-level folder — we need full relative paths
    // so the conf writer can correctly set the working directory.
    // e.g. mastori/ORION.EXE → dirOf → "mastori" → cd \mastori in conf
    (void)allSame; (void)firstTop;

    return entries;
}

// ── Fingerprint ───────────────────────────────────────────────────────────────

std::string fingerprint(const std::string& filename) {
    std::string name = basename(filename);
    // Remove extension
    size_t dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);

    name = toLower(name);

    // Strip DOSBOX_ prefix
    if (name.substr(0, 7) == "dosbox_" || name.substr(0, 7) == "dosbox-")
        name = name.substr(7);

    // Strip (region), [flags]
    name = std::regex_replace(name, std::regex(R"([\s\-_]*[\(\[][^\)\]]*[\)\]])"), "");

    // Strip version strings
    name = std::regex_replace(name, std::regex(R"([\s\-_]*v\d[\d\.]*)"), "");

    // Replace separators with space then collapse
    name = std::regex_replace(name, std::regex(R"(\s*[-:]\s*)"), " ");
    name = std::regex_replace(name, std::regex(R"([\s\-_:,'\.\!\?]+)"), "");

    // Strip leading article only if first WORD of original was an article
    // (prevents "theme" → strip "the" → "me")
    std::string orig = toLower(basename(filename));
    dot = orig.rfind('.');
    if (dot != std::string::npos) orig = orig.substr(0, dot);
    // Get first word
    size_t wordEnd = orig.find_first_of(" -_");
    std::string firstWord = (wordEnd != std::string::npos) ? orig.substr(0, wordEnd) : orig;
    if (firstWord == "the" || firstWord == "a" || firstWord == "an") {
        if (name.substr(0, firstWord.size()) == firstWord)
            name = name.substr(firstWord.size());
    }

    return name;
}

// ── Game type classifier ──────────────────────────────────────────────────────

static std::string classifyGameType(const std::vector<ZipEntry>& entries) {
    for (const auto& e : entries) {
        if (e.ext == "ISO" || e.ext == "CUE" || e.ext == "BIN" || e.ext == "MDF")
            return "CD_BASED";
    }
    for (const auto& e : entries) {
        if (e.ext == "BAT") {
            std::string lower = toLower(e.base);
            if (lower == "start.bat" || lower == "run.bat" ||
                lower == "go.bat"    || lower == "launch.bat" ||
                lower == "play.bat"  || lower == "game.bat")
                return "BATCH_LAUNCHER";
        }
    }
    for (const auto& e : entries) {
        std::string lower = toLower(e.base);
        if (lower == "install.exe" || lower == "setup.exe" ||
            lower == "install.bat" || lower == "setup.bat")
            return "INSTALLED";
    }
    if (entries.size() > 50) return "COMPLEX";
    return "SIMPLE";
}

// ── Exe scorer ────────────────────────────────────────────────────────────────

static float scoreExe(const ZipEntry& e, const std::string& zipBase, size_t maxCompSize) {
    std::string stem = toLower(stemOf(e.base));

    if (isBlacklisted(stem)) return 0.0f;

    float score = 0.0f;
    if      (e.ext == "EXE") score = 1.0f;
    else if (e.ext == "COM") score = 0.6f;
    else if (e.ext == "BAT") score = 0.7f;
    else return 0.0f;

    // Depth penalty
    score -= e.depth * 0.15f;
    if (score < 0.01f) score = 0.01f;

    // Name matches zip name
    std::string zipStem = toLower(stemOf(zipBase));
    if (stem == zipStem ||
        stem.find(zipStem) == 0 ||
        zipStem.find(stem) == 0) {
        score += 0.3f;
    }

    // Common game exe patterns
    if (stem == "game" || stem == "play" || stem == "start" ||
        stem == "run"  || stem == "main" || stem == "go") {
        score += 0.15f;
    }

    // Size bonus
    if (maxCompSize > 0) {
        score += ((float)e.compSize / (float)maxCompSize) * 0.25f;
    }

    return std::min(score, 1.5f);
}

// ── Database loader ───────────────────────────────────────────────────────────

static json loadDatabase(const std::string& dbPath) {
    std::ifstream f(dbPath);
    if (!f.is_open()) return json::object();
    json data;
    try {
        f >> data;
        return data.value("games", json::object());
    } catch (...) {
        return json::object();
    }
}

// ── Find ISO in extracted dir ─────────────────────────────────────────────────

static std::string findIsoInDir(const std::string& dir) {
    WIN32_FIND_DATAA fd = {};
    std::string pattern = dir + "\\*";
    HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return "";

    std::string found;
    do {
        std::string name = fd.cFileName;
        if (name == "." || name == "..") continue;
        std::string fullPath = dir + "\\" + name;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            found = findIsoInDir(fullPath);
            if (!found.empty()) break;
        } else {
            std::string ext = toUpper(extOf(name));
            if (ext == "ISO" || ext == "CUE" || ext == "BIN" || ext == "MDF") {
                found = fullPath;
                break;
            }
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
    return found;
}

// ── Main analyze function ─────────────────────────────────────────────────────

AnalyzeResult analyze(const std::string& zipPath, const std::string& dbPath) {
    AnalyzeResult result;

    // Read zip entries
    auto rawEntries = readZipEntries(zipPath);
    if (rawEntries.empty()) {
        result.error = "Zip is empty or unreadable";
        return result;
    }

    auto entries     = normalizeEntries(rawEntries);
    std::string fp   = fingerprint(zipPath);
    std::string gt   = classifyGameType(entries);
    result.gameType  = gt;

    // ── Layer 1: Database lookup ──────────────────────────────────────────────
    json db = loadDatabase(dbPath);

    json dbEntry;
    if (db.contains(fp)) {
        dbEntry = db[fp];
    } else {
        // Substring match
        for (auto it = db.begin(); it != db.end(); ++it) {
            if (fp.find(it.key()) != std::string::npos) {
                dbEntry = it.value();
                break;
            }
        }
    }

    if (!dbEntry.is_null()) {
        std::string exeName = toUpper(dbEntry.value("exe", ""));
        // Verify exe exists in zip
        for (const auto& e : entries) {
            if (e.base == exeName) {
                result.success    = true;
                result.exe        = e.name;
                result.workDir    = dbEntry.value("work_dir", "");
                result.source     = "database";
                result.confidence = 1.0f;
                result.title      = dbEntry.value("title", "");
                // cycles can be int or string in games.json
                if (dbEntry.contains("cycles")) {
                    auto& cyc = dbEntry["cycles"];
                    if (cyc.is_string())       result.cycles = cyc.get<std::string>();
                    else if (cyc.is_number())  result.cycles = std::to_string(cyc.get<int>());
                    else                       result.cycles = "max limit 80000";
                } else {
                    result.cycles = "max limit 80000";
                }
                result.memsize    = dbEntry.value("memsize", 16);
                result.ems        = dbEntry.value("ems", true);
                result.xms        = dbEntry.value("xms", true);
                result.cdMount    = dbEntry.value("cd_mount", false);
                return result;
            }
        }
    }

    // ── Layer 2: Batch launcher ───────────────────────────────────────────────
    if (gt == "BATCH_LAUNCHER") {
        for (const auto& e : entries) {
            if (e.ext == "BAT") {
                std::string lower = toLower(e.base);
                if (lower == "start.bat" || lower == "run.bat" || lower == "go.bat") {
                    result.success    = true;
                    result.exe        = e.name;
                    result.workDir    = dirOf(e.name);
                    result.source     = "batch";
                    result.confidence = 0.85f;
                    return result;
                }
            }
        }
    }

    // ── Layer 3: Scorer ───────────────────────────────────────────────────────
    std::vector<ZipEntry> exeEntries;
    for (const auto& e : entries) {
        if (e.ext == "EXE" || e.ext == "COM" || e.ext == "BAT")
            exeEntries.push_back(e);
    }

    size_t maxCompSize = 0;
    for (const auto& e : exeEntries) {
        if (e.compSize > maxCompSize) maxCompSize = e.compSize;
    }

    std::string zipBase = basename(zipPath);

    std::vector<std::pair<float, ZipEntry>> scored;
    for (const auto& e : exeEntries) {
        float s = scoreExe(e, zipBase, maxCompSize);
        if (s > 0.0f) scored.push_back({s, e});
    }

    if (scored.empty()) {
        result.error = "No executable files found in zip";
        return result;
    }

    std::sort(scored.begin(), scored.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    const auto& best = scored[0];
    result.success    = true;
    result.exe        = best.second.name;
    result.workDir    = dirOf(best.second.name);
    result.source     = "scored";
    result.confidence = std::min(best.first / 1.5f, 1.0f);

    // Auto-detect CD mount from game type
    if (gt == "CD_BASED") result.cdMount = true;

    return result;
}

// ── Extract zip ───────────────────────────────────────────────────────────────

bool extractZip(const std::string& zipPath, const std::string& outDir) {
    mz_zip_archive zip = {};
    if (!mz_zip_reader_init_file(&zip, zipPath.c_str(), 0)) return false;

    int count = (int)mz_zip_reader_get_num_files(&zip);

    for (int i = 0; i < count; i++) {
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

        mz_zip_archive_file_stat stat = {};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

        std::string name = stat.m_filename;

        // Normalize to backslash
        std::replace(name.begin(), name.end(), '/', '\\');

        // Skip dangerous paths
        if (name.find("..\\") != std::string::npos) continue;
        if (name.size() > 1 && name[1] == ':') continue;
        if (name.empty()) continue;

        std::string outPath = outDir + "\\" + name;

        // Create all parent directories using Windows API
        std::string dir = outPath.substr(0, outPath.rfind('\\'));
        // Walk up and create each level
        for (size_t pos = outDir.size(); pos < dir.size(); ) {
            pos = dir.find('\\', pos + 1);
            if (pos == std::string::npos) pos = dir.size();
            CreateDirectoryA(dir.substr(0, pos).c_str(), nullptr);
        }

        // Extract file — skip on failure, don't abort whole zip
        try {
            mz_zip_reader_extract_to_file(&zip, i, outPath.c_str(), 0);
        } catch (...) {
            // Skip this file
        }
    }

    mz_zip_reader_end(&zip);
    return true;  // always return true — partial extract is better than failure
}

// ── Write DOSBox conf ─────────────────────────────────────────────────────────

bool writeDosboxConf(const std::string& zipPath,
                     const std::string& extractedDir,
                     const AnalyzeResult& result) {
    std::string confPath = zipPath.substr(0, zipPath.rfind('.')) + ".conf";

    // Parse exe path
    std::string exeFull = result.exe;
    std::replace(exeFull.begin(), exeFull.end(), '/', '\\');
    size_t lastSlash = exeFull.rfind('\\');
    std::string exeName = (lastSlash != std::string::npos)
        ? exeFull.substr(lastSlash + 1)
        : exeFull;
    std::string exeSubDir = (lastSlash != std::string::npos)
        ? exeFull.substr(0, lastSlash)
        : "";

    // Use work_dir from database if set, otherwise derive from path
    std::string cdDir = result.workDir.empty() ? exeSubDir : result.workDir;

    std::string cycles  = result.cycles.empty() ? "max limit 80000" : result.cycles;
    int         memsize = result.memsize > 0 ? result.memsize : 16;

    // Build autoexec
    std::ostringstream autoexec;
    autoexec << "@echo off\r\n";
    autoexec << "mount C \"" << extractedDir << "\"\r\n";

    // CD mount for CD-based games
    if (result.cdMount) {
        std::string isoPath = findIsoInDir(extractedDir);
        if (!isoPath.empty()) {
            autoexec << "imgmount D \"" << isoPath << "\" -t iso\r\n";
        }
    }

    autoexec << "C:\r\n";
    if (!cdDir.empty()) {
        autoexec << "cd \\" << cdDir << "\r\n";
    }
    autoexec << exeName << "\r\n";
    autoexec << "exit\r\n";

    // Build conf
    std::ostringstream conf;
    conf << "[sdl]\r\n";
    conf << "fullscreen=true\r\n";
    conf << "fullresolution=desktop\r\n";
    conf << "output=openglnb\r\n";
    conf << "\r\n";
    conf << "[dosbox]\r\n";
    conf << "machine=svga_s3\r\n";
    conf << "memsize=" << memsize << "\r\n";
    conf << "\r\n";
    conf << "[cpu]\r\n";
    conf << "core=dynamic\r\n";
    conf << "cputype=pentium_slow\r\n";
    conf << "cycles=" << cycles << "\r\n";
    conf << "cycleup=500\r\n";
    conf << "cycledown=20\r\n";
    conf << "\r\n";
    conf << "[dos]\r\n";
    conf << "ems=" << (result.ems ? "true" : "false") << "\r\n";
    conf << "xms=" << (result.xms ? "true" : "false") << "\r\n";
    conf << "\r\n";
    conf << "[mixer]\r\n";
    conf << "rate=44100\r\n";
    conf << "blocksize=1024\r\n";
    conf << "prebuffer=20\r\n";
    conf << "\r\n";
    conf << "[render]\r\n";
    conf << "frameskip=0\r\n";
    conf << "aspect=true\r\n";
    conf << "\r\n";
    conf << "[autoexec]\r\n";
    conf << autoexec.str();
    conf << "\r\n";

    std::ofstream f(confPath);
    if (!f.is_open()) return false;
    f << conf.str();
    return true;
}

// ── Launch DOSBox ─────────────────────────────────────────────────────────────

bool launchDosBox(const std::string& dosboxPath, const std::string& confPath) {
    std::string cmd = "\"" + dosboxPath + "\" -conf \"" + confPath + "\"";
    STARTUPINFOA        si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    bool ok = CreateProcessA(
        nullptr,
        const_cast<char*>(cmd.c_str()),
        nullptr, nullptr, FALSE,
        DETACHED_PROCESS,
        nullptr,
        nullptr,
        &si, &pi
    );
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok;
}

// ── Add to database ───────────────────────────────────────────────────────────

bool addToDatabase(const std::string& dbPath, const AnalyzeResult& result) {
    // Load existing
    json data;
    std::ifstream fin(dbPath);
    if (fin.is_open()) {
        try { fin >> data; } catch (...) {}
        fin.close();
    }

    std::string key = fingerprint(result.title.empty() ? result.exe : result.title);
    if (key.empty()) return false;

    json& games = data["games"];

    // Don't overwrite manual entries
    if (games.contains(key) && games[key].value("source", "") == "manual")
        return false;

    games[key] = {
        {"title",         result.title},
        {"exe",           basename(result.exe)},
        {"cycles",        result.cycles.empty() ? "max limit 80000" : result.cycles},
        {"memsize",       result.memsize},
        {"ems",           result.ems},
        {"xms",           result.xms},
        {"cd_mount",      result.cdMount},
        {"work_dir",      result.workDir},
        {"install_first", false},
        {"source",        "autosync"},
    };

    data["_meta"]["games"] = games.size();

    std::ofstream fout(dbPath);
    if (!fout.is_open()) return false;
    fout << data.dump(2);
    return true;
}

} // namespace AutoDOS
