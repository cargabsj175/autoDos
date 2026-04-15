#include "autodos.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;

struct Options {
    std::string dbPath;
    std::string dataDir;
    std::string dosboxPath = "dosbox";
    bool launchAfterImport = false;
    bool dbPathExplicit = false;
};

static std::string homeDir() {
    const char* home = std::getenv("HOME");
    return home ? home : ".";
}

static std::string exeDir() {
    char buf[4096] = {};
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return fs::current_path().string();
    buf[len] = '\0';
    return fs::path(buf).parent_path().string();
}

static Options defaultOptions() {
    Options opt;
    opt.dataDir = (fs::path(homeDir()) / ".local" / "share" / "autodos").string();
    opt.dbPath = (fs::path(opt.dataDir) / "games.json").string();

    if (const char* dosbox = std::getenv("AUTODOS_DOSBOX")) {
        opt.dosboxPath = dosbox;
    }
    return opt;
}

static void printUsage(const char* argv0) {
    std::cout
        << "AutoDOS Linux CLI\n\n"
        << "Usage:\n"
        << "  " << argv0 << " analyze <game.zip> [options]\n"
        << "  " << argv0 << " import <game.zip> [options]\n"
        << "  " << argv0 << " launch <dosbox.conf> [options]\n\n"
        << "Options:\n"
        << "  --db <path>        games.json path\n"
        << "  --data-dir <path>  data directory for extracted games\n"
        << "  --dosbox <path>    DOSBox executable, default: dosbox\n"
        << "  --launch           launch after import\n";
}

static bool parseOptions(int argc, char** argv, int start, Options& opt) {
    for (int i = start; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--db" && i + 1 < argc) {
            opt.dbPath = argv[++i];
            opt.dbPathExplicit = true;
        } else if (arg == "--data-dir" && i + 1 < argc) {
            opt.dataDir = argv[++i];
            if (!opt.dbPathExplicit) {
                opt.dbPath = (fs::path(opt.dataDir) / "games.json").string();
            }
        } else if (arg == "--dosbox" && i + 1 < argc) {
            opt.dosboxPath = argv[++i];
        } else if (arg == "--launch") {
            opt.launchAfterImport = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }
    }
    return true;
}

static void ensureDatabase(const Options& opt) {
    fs::create_directories(fs::path(opt.dbPath).parent_path());
    if (fs::exists(opt.dbPath)) return;

    std::vector<fs::path> candidates = {
        fs::path(exeDir()) / "games.json",
        fs::current_path() / "src" / "games.json",
        fs::current_path() / "games.json"
    };

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            fs::copy_file(candidate, opt.dbPath, fs::copy_options::overwrite_existing);
            return;
        }
    }
}

static void printAnalyzeResult(const AutoDOS::AnalyzeResult& result) {
    if (!result.success) {
        std::cout << "Analyze failed: " << result.error << "\n";
        return;
    }

    std::cout
        << "Title: " << (result.title.empty() ? "(unknown)" : result.title) << "\n"
        << "Exe: " << result.exe << "\n"
        << "Work dir: " << (result.workDir.empty() ? "." : result.workDir) << "\n"
        << "Type: " << result.gameType << "\n"
        << "Source: " << result.source << "\n"
        << "Confidence: " << static_cast<int>(result.confidence * 100.0f) << "%\n"
        << "Cycles: " << (result.cycles.empty() ? "max limit 80000" : result.cycles) << "\n"
        << "Memsize: " << result.memsize << "\n"
        << "CD mount: " << (result.cdMount ? "yes" : "no") << "\n";
}

static int analyzeCommand(const std::string& zipPath, Options& opt) {
    ensureDatabase(opt);
    auto result = AutoDOS::analyze(zipPath, opt.dbPath);
    printAnalyzeResult(result);
    return result.success ? 0 : 1;
}

static int importCommand(const std::string& zipPath, Options& opt) {
    ensureDatabase(opt);

    auto result = AutoDOS::analyze(zipPath, opt.dbPath);
    if (!result.success) {
        std::cerr << "Analyze failed: " << result.error << "\n";
        return 1;
    }

    fs::path zip = zipPath;
    std::string stem = zip.stem().string();
    fs::path gameDir = fs::path(opt.dataDir) / "games" / stem;
    fs::path confPath = fs::path(opt.dataDir) / "games" / (stem + ".conf");

    fs::create_directories(gameDir);
    if (!AutoDOS::extractZip(zipPath, gameDir.string())) {
        std::cerr << "Extraction failed\n";
        return 1;
    }

    if (!AutoDOS::writeDosboxConf(zipPath, gameDir.string(), result)) {
        std::cerr << "Could not write DOSBox conf\n";
        return 1;
    }

    if (result.source == "scored" || result.source == "batch") {
        AutoDOS::AnalyzeResult dbResult = result;
        dbResult.title = result.title.empty() ? stem : result.title;
        AutoDOS::addToDatabase(opt.dbPath, dbResult);
    }

    std::cout << "Imported: " << (result.title.empty() ? stem : result.title) << "\n";
    std::cout << "Game dir: " << gameDir << "\n";
    std::cout << "Conf: " << confPath << "\n";

    if (opt.launchAfterImport) {
        if (!AutoDOS::launchDosBox(opt.dosboxPath, confPath.string())) {
            std::cerr << "Failed to launch DOSBox\n";
            return 1;
        }
        std::cout << "Launched with: " << opt.dosboxPath << "\n";
    }

    return 0;
}

static int launchCommand(const std::string& confPath, Options& opt) {
    if (!fs::exists(confPath)) {
        std::cerr << "Conf not found: " << confPath << "\n";
        return 1;
    }
    if (!AutoDOS::launchDosBox(opt.dosboxPath, confPath)) {
        std::cerr << "Failed to launch DOSBox\n";
        return 1;
    }
    std::cout << "Launched with: " << opt.dosboxPath << "\n";
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage(argv[0]);
        return argc == 1 ? 0 : 1;
    }

    std::string command = argv[1];
    std::string target = argv[2];
    Options opt = defaultOptions();
    if (!parseOptions(argc, argv, 3, opt)) return 1;

    try {
        if (command == "analyze") return analyzeCommand(target, opt);
        if (command == "import") return importCommand(target, opt);
        if (command == "launch") return launchCommand(target, opt);
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    std::cerr << "Unknown command: " << command << "\n";
    printUsage(argv[0]);
    return 1;
}
