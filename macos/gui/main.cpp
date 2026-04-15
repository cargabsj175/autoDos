// main.cpp — AutoDOS GUI for macOS (SDL2 + ImGui)
//
// AutoDOS - Original by makuka97 (https://github.com/makuka97)
// macOS port by cargabsj175 (vibe coding approach)
//
// Graphical user interface using SDL2 and ImGui for drag & drop game management.
// This is part of the macOS port - the original Windows version uses Win32 API.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include "autodos.h"
#include "platform.h"
#include "nlohmann/json.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <chrono>

namespace fs = std::filesystem;
using json = nlohmann::json;

// ── Library entry ─────────────────────────────────────────────────────────────
struct LibEntry {
    std::string id;
    std::string title;
    std::string zipPath;
    std::string confPath;
    std::string source;
    float       confidence = 0.0f;
};

// ── Result from worker thread ─────────────────────────────────────────────────
struct ImportResult {
    bool        ok = false;
    std::string error;
    LibEntry    entry;
    AutoDOS::AnalyzeResult analyzeResult;
};

// ── Globals ───────────────────────────────────────────────────────────────────
static std::string g_appDir;
static std::string g_dbPath;
static std::string g_libPath;
static std::string g_dosboxPath;

static std::vector<LibEntry>  g_library;
static std::mutex             g_queueMtx;
static std::queue<std::string> g_queue;
static std::atomic<bool>      g_working{false};

static std::string g_status = "Ready - Drop a DOS game zip to add";
static int         g_selectedIndex = -1;
static bool        g_showDeleteConfirm = false;
static int         g_deleteConfirmIndex = -1;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string makeId() {
    static std::atomic<int> n{0};
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return "g" + std::to_string(now) + "_" + std::to_string(n++);
}

static void setStatus(const std::string& msg) {
    g_status = msg;
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

// ── Worker thread ─────────────────────────────────────────────────────────────

static void workerThread(std::string zipPath) {
    try {
        setStatus("Analyzing: " + fs::path(zipPath).filename().string() + "...");

        auto* res = new ImportResult();
        res->analyzeResult = AutoDOS::analyze(zipPath, g_dbPath);

        if (!res->analyzeResult.success) {
            res->ok    = false;
            res->error = res->analyzeResult.error.empty()
                         ? "Could not identify game executable"
                         : res->analyzeResult.error;
            
            // Post back to main thread via a simple mechanism
            std::lock_guard<std::mutex> lk(g_queueMtx);
            // We'll use a separate results queue in real impl
            delete res;
            setStatus("Error: " + res->error);
            return;
        }

        std::string stem    = fs::path(zipPath).stem().string();
        std::string gameDir = g_appDir + "/games/" + stem;
        Platform::createDirectories(gameDir);

        setStatus("Extracting: " + stem + "...");

        if (!AutoDOS::extractZip(zipPath, gameDir)) {
            setStatus("Extraction failed: " + stem);
            delete res;
            return;
        }

        std::string confPath = g_appDir + "/games/" + stem + ".conf";
        AutoDOS::writeDosboxConf(zipPath, gameDir, res->analyzeResult);

        std::string tempConf = zipPath.substr(0, zipPath.rfind('.')) + ".conf";
        if (fs::exists(tempConf)) {
            Platform::copyFile(tempConf, confPath);
            Platform::remove(tempConf);
        }

        std::string title = res->analyzeResult.title.empty()
                            ? stem : res->analyzeResult.title;

        res->ok               = true;
        res->entry.id         = makeId();
        res->entry.title      = title;
        res->entry.zipPath    = zipPath;
        res->entry.confPath   = confPath;
        res->entry.source     = res->analyzeResult.source;
        res->entry.confidence = res->analyzeResult.confidence;

        // Add to library (thread-safe in this simple version)
        {
            std::lock_guard<std::mutex> lk(g_queueMtx);
            bool dup = false;
            for (const auto& e : g_library)
                if (e.zipPath == zipPath) { dup = true; break; }

            if (!dup) {
                g_library.push_back(res->entry);
                saveLibrary();

                if (res->analyzeResult.source == "scored") {
                    res->analyzeResult.title = title;
                    AutoDOS::addToDatabase(g_dbPath, res->analyzeResult);
                }

                std::string src = (res->entry.source == "database") ? "DB"
                    : "Auto " + std::to_string((int)(res->entry.confidence*100)) + "%";
                setStatus("Added: " + res->entry.title + "  [" + src + "]");
            } else {
                setStatus("Already in library: " + res->entry.title);
            }
        }

        delete res;

    } catch (const std::exception& ex) {
        setStatus(std::string("Error: ") + ex.what());
    } catch (...) {
        setStatus("Unknown error during import");
    }

    g_working = false;
}

static void enqueueZip(const std::string& zipPath) {
    for (const auto& e : g_library) {
        if (e.zipPath == zipPath) {
            setStatus("Already in library: " + e.title);
            return;
        }
    }

    if (g_working) {
        setStatus("Already processing another file...");
        return;
    }

    g_working = true;
    std::thread(workerThread, zipPath).detach();
}

static void launchSelected() {
    if (g_selectedIndex < 0 || g_selectedIndex >= (int)g_library.size()) return;
    const auto& e = g_library[g_selectedIndex];
    if (!fs::exists(e.confPath)) { setStatus("Config missing — re-import"); return; }
    if (!Platform::exists(g_dosboxPath)) { setStatus("DOSBox not found"); return; }
    
    setStatus("Launching: " + e.title);
    if (!Platform::launchApp(g_dosboxPath, e.confPath)) {
        setStatus("Failed to launch DOSBox");
    }
}

static void deleteSelected() {
    if (g_selectedIndex < 0 || g_selectedIndex >= (int)g_library.size()) return;
    
    g_deleteConfirmIndex = g_selectedIndex;
    g_showDeleteConfirm = true;
}

static void confirmDelete() {
    if (g_deleteConfirmIndex < 0 || g_deleteConfirmIndex >= (int)g_library.size()) return;
    
    std::string title    = g_library[g_deleteConfirmIndex].title;
    std::string confPath = g_library[g_deleteConfirmIndex].confPath;
    
    Platform::remove(confPath);
    Platform::removeAll(confPath.substr(0, confPath.rfind('.')));
    
    g_library.erase(g_library.begin() + g_deleteConfirmIndex);
    saveLibrary();
    
    g_selectedIndex = -1;
    g_showDeleteConfirm = false;
    g_deleteConfirmIndex = -1;
    
    setStatus("Removed: " + title);
}

// ── macOS file drop via AppleEvents (simplified - use file dialog instead) ───

static std::string openFileDialog(SDL_Window* window) {
    // Simple file dialog - in production, use macOS native dialog
    // For now, return empty (user must drag to dock or use CLI)
    return "";
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Initialize paths
    g_appDir     = Platform::appDataDir();
    g_dbPath     = g_appDir + "/games.json";
    g_libPath    = g_appDir + "/library.json";
    
    // Find DOSBox - check multiple possible locations
    std::string exeDir = Platform::exeDir();
    
    // Inside .app bundle: Contents/MacOS -> check Contents/dosbox/
    if (exeDir.find(".app/Contents/MacOS") != std::string::npos) {
        std::string appContents = exeDir.substr(0, exeDir.find(".app/Contents/MacOS") + strlen(".app"));
        g_dosboxPath = appContents + "/Contents/dosbox/DOSBox Staging.app/Contents/MacOS/dosbox";
    } else {
        // Development/build directory
        g_dosboxPath = exeDir + "/dosbox/dosbox";
    }
    
    // Fallback: check if dosbox is in PATH
    if (!Platform::exists(g_dosboxPath)) {
        // Try common alternatives
        std::vector<std::string> alternatives = {
            "/Applications/DOSBox Staging.app/Contents/MacOS/dosbox",
            "/usr/local/bin/dosbox",
            "/opt/homebrew/bin/dosbox"
        };
        for (const auto& alt : alternatives) {
            if (Platform::exists(alt)) {
                g_dosboxPath = alt;
                break;
            }
        }
    }

    Platform::createDirectories(g_appDir);
    Platform::createDirectories(g_appDir + "/games");

    if (!Platform::exists(g_dbPath)) {
        std::string src = Platform::exeDir() + "/games.json";
        if (Platform::exists(src)) {
            Platform::copyFile(src, g_dbPath);
        }
    }

    loadLibrary();

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    if (TTF_Init() < 0) {
        SDL_Log("TTF_Init failed: %s", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "AutoDOS for macOS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        600, 700,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI
    );
    if (!window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = (g_appDir + "/imgui.ini").c_str();

    ImGui::StyleColorsDark();
    
    // Customize colors
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg]      = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
    colors[ImGuiCol_Header]        = ImVec4(0.31f, 0.78f, 0.47f, 0.6f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.31f, 0.78f, 0.47f, 0.8f);
    colors[ImGuiCol_HeaderActive]  = ImVec4(0.31f, 0.78f, 0.47f, 1.0f);
    colors[ImGuiCol_Button]        = ImVec4(0.31f, 0.78f, 0.47f, 0.4f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.31f, 0.78f, 0.47f, 0.6f);
    colors[ImGuiCol_ButtonActive]  = ImVec4(0.31f, 0.78f, 0.47f, 0.8f);
    colors[ImGuiCol_FrameBg]       = ImVec4(0.11f, 0.11f, 0.14f, 1.0f);
    colors[ImGuiCol_Text]          = ImVec4(0.86f, 0.86f, 0.86f, 1.0f);

    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);

    // Main loop
    bool done = false;
    SDL_Event event;
    while (!done) {
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            
            if (event.type == SDL_QUIT)
                done = true;
            else if (event.type == SDL_DROPFILE) {
                char* droppedPath = event.drop.file;
                if (droppedPath) {
                    std::string path = droppedPath;
                    std::string ext = Platform::getExtension(path);
                    if (ext == "ZIP" || ext == "7Z" || ext == "RAR") {
                        enqueueZip(path);
                    } else {
                        setStatus("Unsupported file type: " + ext);
                    }
                    SDL_free(droppedPath);
                }
            }
            else if (event.type == SDL_WINDOWEVENT) {
                if (event.window.event == SDL_WINDOWEVENT_CLOSE &&
                    event.window.windowID == SDL_GetWindowID(window))
                    done = true;
            }
        }

        // Start ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui UI
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("AutoDOS", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar);

        // Header
        ImGui::Text("AutoDOS for macOS");
        ImGui::Separator();
        ImGui::Spacing();

        // Game list
        if (ImGui::BeginChild("GameList", ImVec2(0, -120), true)) {
            if (g_library.empty()) {
                ImGui::TextDisabled("No games in library. Drag & drop a DOS game zip here.");
            } else {
                for (int i = 0; i < (int)g_library.size(); i++) {
                    const auto& e = g_library[i];
                    std::string label = e.title;
                    if (e.source == "scored")
                        label += "  [" + std::to_string((int)(e.confidence*100)) + "%]";
                    
                    bool isSelected = (i == g_selectedIndex);
                    if (ImGui::Selectable(label.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                        g_selectedIndex = i;
                        if (ImGui::IsMouseDoubleClicked(0)) {
                            launchSelected();
                        }
                    }
                }
            }
        }
        ImGui::EndChild();

        // Buttons
        ImGui::Spacing();
        if (ImGui::Button("Launch", ImVec2(100, 30))) {
            launchSelected();
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove", ImVec2(100, 30))) {
            deleteSelected();
        }

        // Status bar
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - 30);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%s", g_status.c_str());

        ImGui::End();

        // Delete confirmation modal
        if (g_showDeleteConfirm) {
            ImGui::OpenPopup("Confirm Delete");
        }
        
        if (ImGui::BeginPopupModal("Confirm Delete", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Remove this game from library?\n");
            if (g_deleteConfirmIndex >= 0 && g_deleteConfirmIndex < (int)g_library.size()) {
                ImGui::Text("\"%s\"", g_library[g_deleteConfirmIndex].title.c_str());
            }
            ImGui::Spacing();
            
            if (ImGui::Button("Yes", ImVec2(80, 30))) {
                confirmDelete();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("No", ImVec2(80, 30))) {
                g_showDeleteConfirm = false;
                g_deleteConfirmIndex = -1;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::EndPopup();
        }

        // Render
        ImGui::Render();
        SDL_RenderSetScale(renderer, io.DisplayFramebufferScale.x, io.DisplayFramebufferScale.y);
        SDL_SetRenderDrawColor(renderer, 18, 18, 24, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // Cleanup
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
