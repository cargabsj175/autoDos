#pragma once
// platform.h — Cross-platform abstraction layer for macOS/POSIX

#include <string>
#include <vector>

namespace Platform {

// Get application data directory (e.g., ~/Library/Application Support/AutoDOS)
std::string appDataDir();

// Get directory of the running executable
std::string exeDir();

// Get current working directory
std::string currentDir();

// Extract zip file to output directory
bool extractZip(const std::string& zipPath, const std::string& outDir);

// Launch an external application (e.g., DOSBox) with arguments
bool launchApp(const std::string& appPath, const std::string& confPath);

// Create directories recursively
bool createDirectories(const std::string& path);

// Check if file/directory exists
bool exists(const std::string& path);

// Copy file
bool copyFile(const std::string& from, const std::string& to);

// Remove file or directory
bool remove(const std::string& path);
bool removeAll(const std::string& path);

// Get file extension (uppercase)
std::string getExtension(const std::string& filename);

// Get filename without path
std::string basename(const std::string& path);

// Get filename without extension
std::string stem(const std::string& path);

// Convert path separators to platform native
std::string normalizePath(const std::string& path);

} // namespace Platform
