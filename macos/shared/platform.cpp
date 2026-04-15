// platform.cpp — macOS/POSIX platform abstraction layer
//
// AutoDOS macOS Port
// Original by: makuka97 (https://github.com/makuka97)
// macOS port by: cargabsj175 (vibe coding approach)
//
// This file replaces Windows-specific APIs (CreateProcess, SHGetFolderPath, etc.)
// with POSIX/macOS equivalents. The original src/ code is completely untouched.

#include "platform.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fstream>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#include <ApplicationServices/ApplicationServices.h>
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace Platform {

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

// ── App data directory ────────────────────────────────────────────────────────

std::string appDataDir() {
#ifdef __APPLE__
    // Use ~/Library/Application Support/AutoDOS
    const char* home = getenv("HOME");
    if (!home) return "./AutoDOS";
    return std::string(home) + "/Library/Application Support/AutoDOS";
#else
    // Linux fallback
    const char* xdg = getenv("XDG_DATA_HOME");
    if (xdg) return std::string(xdg) + "/AutoDOS";
    const char* home = getenv("HOME");
    if (!home) return "./AutoDOS";
    return std::string(home) + "/.local/share/AutoDOS";
#endif
}

// ── Executable directory ──────────────────────────────────────────────────────

std::string exeDir() {
#ifdef __APPLE__
    char path[PATH_MAX];
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) == 0) {
        return dirname(path);
    }
    return ".";
#else
    char path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (count != -1) {
        path[count] = '\0';
        return dirname(path);
    }
    return ".";
#endif
}

// ── Current directory ─────────────────────────────────────────────────────────

std::string currentDir() {
    char buf[PATH_MAX];
    if (getcwd(buf, sizeof(buf))) {
        return std::string(buf);
    }
    return ".";
}

// ── File/directory operations ─────────────────────────────────────────────────

bool createDirectories(const std::string& path) {
    return fs::create_directories(path);
}

bool exists(const std::string& path) {
    return fs::exists(path);
}

bool copyFile(const std::string& from, const std::string& to) {
    try {
        fs::copy_file(from, to, fs::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

bool remove(const std::string& path) {
    try {
        return fs::remove(path);
    } catch (...) {
        return false;
    }
}

bool removeAll(const std::string& path) {
    try {
        return fs::remove_all(path);
    } catch (...) {
        return false;
    }
}

// ── Path helpers ──────────────────────────────────────────────────────────────

std::string getExtension(const std::string& filename) {
    std::string base = basename(filename);
    size_t dot = base.rfind('.');
    if (dot == std::string::npos) return "";
    return toUpper(base.substr(dot + 1));
}

std::string basename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string stem(const std::string& path) {
    std::string b = basename(path);
    size_t dot = b.rfind('.');
    return (dot == std::string::npos) ? b : b.substr(0, dot);
}

std::string normalizePath(const std::string& path) {
    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    return normalized;
}

// ── ZIP extraction (using miniz) ──────────────────────────────────────────────

bool extractZip(const std::string& zipPath, const std::string& outDir) {
    // This will be called from the core autodos.cpp
    // Implemented there with miniz
    return true; // placeholder - actual implementation in autodos.cpp
}

// ── Launch external application ───────────────────────────────────────────────

bool launchApp(const std::string& appPath, const std::string& confPath) {
    std::string cmd;
    
    // Find the .app bundle path if inside Contents/MacOS/
    size_t macosPos = appPath.find("/Contents/MacOS/");
    if (macosPos != std::string::npos) {
        std::string appBundle = appPath.substr(0, macosPos);
        std::string binaryName = appPath.substr(appPath.rfind('/') + 1);
        
        // Launch the binary directly with proper working directory
        // This ensures keyboard input works correctly (open -a can cause focus issues)
        cmd = "cd \"" + appBundle + "/Contents/MacOS\" && "
              "./" + binaryName + " -conf \"" + confPath + "\"";
    } else {
        // Direct binary path
        cmd = "\"" + appPath + "\" -conf \"" + confPath + "\"";
    }

    // Fork and exec
    pid_t pid = fork();
    if (pid == 0) {
        // Child process - launch in a new session to get proper terminal/input focus
        setsid();
        execl("/bin/bash", "bash", "-c", cmd.c_str(), nullptr);
        _exit(127);
    } else if (pid > 0) {
        return true;
    }
    return false;
}

} // namespace Platform
