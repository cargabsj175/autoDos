#include "autodos.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "nlohmann/json.hpp"

#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct LibraryEntry {
    std::string id;
    std::string title;
    std::string zipPath;
    std::string confPath;
    std::string source;
    float confidence = 0.0f;
};

struct AppState {
    std::string dataDir;
    std::string dbPath;
    std::string libraryPath;
    std::string imguiIniPath;
    std::string dosboxPath = "dosbox";
    std::string status = "Drop a DOS zip or type a path to import.";
    std::string inputPath;
    std::vector<LibraryEntry> library;
    int selected = -1;
};

static AppState* g_state = nullptr;

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

static std::string makeId() {
    static int counter = 0;
    return std::to_string(std::time(nullptr)) + "_" + std::to_string(counter++);
}

static void ensureDatabase(const AppState& state) {
    std::error_code ec;
    fs::create_directories(fs::path(state.dbPath).parent_path(), ec);
    if (ec) return;
    if (fs::exists(state.dbPath)) return;

    std::vector<fs::path> candidates = {
        fs::path(exeDir()) / "games.json",
        fs::current_path() / "src" / "games.json",
        fs::current_path() / "games.json"
    };

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            fs::copy_file(candidate, state.dbPath, fs::copy_options::overwrite_existing);
            return;
        }
    }
}

static void saveLibrary(const AppState& state) {
    json arr = json::array();
    for (const auto& e : state.library) {
        arr.push_back({
            {"id", e.id},
            {"title", e.title},
            {"zipPath", e.zipPath},
            {"confPath", e.confPath},
            {"source", e.source},
            {"confidence", e.confidence},
        });
    }

    std::error_code ec;
    fs::create_directories(fs::path(state.libraryPath).parent_path(), ec);
    if (ec) return;
    std::ofstream f(state.libraryPath);
    if (f.is_open()) f << arr.dump(2);
}

static void loadLibrary(AppState& state) {
    state.library.clear();
    std::ifstream f(state.libraryPath);
    if (!f.is_open()) return;

    try {
        json arr;
        f >> arr;
        for (const auto& item : arr) {
            LibraryEntry e;
            e.id = item.value("id", "");
            e.title = item.value("title", "");
            e.zipPath = item.value("zipPath", "");
            e.confPath = item.value("confPath", "");
            e.source = item.value("source", "");
            e.confidence = item.value("confidence", 0.0f);
            if (!e.id.empty()) state.library.push_back(e);
        }
    } catch (...) {
        state.status = "Could not read library.json.";
    }
}

static bool isArchivePath(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(c));
    return ext == ".zip";
}

static bool alreadyImported(const AppState& state, const std::string& zipPath) {
    for (const auto& e : state.library) {
        if (e.zipPath == zipPath) return true;
    }
    return false;
}

static void importZip(AppState& state, const std::string& zipPath) {
    if (zipPath.empty()) {
        state.status = "Enter a ZIP path or drop one onto the window.";
        return;
    }
    if (!fs::exists(zipPath)) {
        state.status = "File not found: " + zipPath;
        return;
    }
    if (!isArchivePath(zipPath)) {
        state.status = "Only ZIP files are supported by this port right now.";
        return;
    }
    if (alreadyImported(state, zipPath)) {
        state.status = "Already in library: " + fs::path(zipPath).filename().string();
        return;
    }

    ensureDatabase(state);
    auto result = AutoDOS::analyze(zipPath, state.dbPath);
    if (!result.success) {
        state.status = "Analyze failed: " + result.error;
        return;
    }

    fs::path zip = zipPath;
    std::string stem = zip.stem().string();
    fs::path gameDir = fs::path(state.dataDir) / "games" / stem;
    fs::path confPath = fs::path(state.dataDir) / "games" / (stem + ".conf");

    std::error_code ec;
    fs::create_directories(gameDir, ec);
    if (ec) {
        state.status = "Could not create data directory: " + gameDir.string();
        return;
    }
    if (!AutoDOS::extractZip(zipPath, gameDir.string())) {
        state.status = "Extraction failed: " + stem;
        return;
    }
    if (!AutoDOS::writeDosboxConf(zipPath, gameDir.string(), result)) {
        state.status = "Could not write DOSBox config.";
        return;
    }

    LibraryEntry entry;
    entry.id = makeId();
    entry.title = result.title.empty() ? stem : result.title;
    entry.zipPath = zipPath;
    entry.confPath = confPath.string();
    entry.source = result.source;
    entry.confidence = result.confidence;
    state.library.push_back(entry);
    state.selected = static_cast<int>(state.library.size()) - 1;

    if (result.source == "scored" || result.source == "batch") {
        AutoDOS::AnalyzeResult dbResult = result;
        dbResult.title = entry.title;
        AutoDOS::addToDatabase(state.dbPath, dbResult);
    }

    saveLibrary(state);
    state.status = "Imported: " + entry.title;
}

static void launchSelected(AppState& state) {
    if (state.selected < 0 || state.selected >= static_cast<int>(state.library.size())) {
        state.status = "Select a game first.";
        return;
    }

    const auto& entry = state.library[state.selected];
    if (!fs::exists(entry.confPath)) {
        state.status = "Config missing: " + entry.confPath;
        return;
    }

    if (!AutoDOS::launchDosBox(state.dosboxPath, entry.confPath)) {
        state.status = "Failed to launch DOSBox.";
        return;
    }
    state.status = "Launching: " + entry.title;
}

static void removeSelected(AppState& state) {
    if (state.selected < 0 || state.selected >= static_cast<int>(state.library.size())) {
        state.status = "Select a game first.";
        return;
    }

    LibraryEntry entry = state.library[state.selected];
    fs::remove(entry.confPath);
    fs::remove_all(fs::path(entry.confPath).replace_extension(""));
    state.library.erase(state.library.begin() + state.selected);
    if (state.library.empty()) state.selected = -1;
    else if (state.selected >= static_cast<int>(state.library.size())) state.selected = static_cast<int>(state.library.size()) - 1;
    saveLibrary(state);
    state.status = "Removed: " + entry.title;
}

static void dropCallback(GLFWwindow*, int count, const char** paths) {
    if (!g_state) return;
    for (int i = 0; i < count; ++i) {
        importZip(*g_state, paths[i]);
    }
}

static void setupStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
}

int main() {
    AppState state;
    if (const char* dataDir = std::getenv("AUTODOS_DATA_DIR")) {
        state.dataDir = dataDir;
    } else {
        state.dataDir = (fs::path(homeDir()) / ".local" / "share" / "autodos").string();
    }
    state.dbPath = (fs::path(state.dataDir) / "games.json").string();
    state.libraryPath = (fs::path(state.dataDir) / "library.json").string();
    state.imguiIniPath = (fs::path(state.dataDir) / "imgui.ini").string();
    if (const char* dosbox = std::getenv("AUTODOS_DOSBOX")) state.dosboxPath = dosbox;

    ensureDatabase(state);
    loadLibrary(state);
    g_state = &state;

    if (!glfwInit()) {
        std::cerr << "Could not initialize GLFW\n";
        return 1;
    }

    const char* glslVersion = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(760, 560, "AutoDOS Linux", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        std::cerr << "Could not create GLFW window\n";
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    glfwSetDropCallback(window, dropCallback);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = state.imguiIniPath.c_str();

    setupStyle();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glslVersion);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(static_cast<float>(width), static_cast<float>(height)));

        ImGui::Begin("AutoDOS", nullptr,
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoSavedSettings);

        ImGui::TextUnformatted("AutoDOS Linux");
        ImGui::TextDisabled("Drop ZIP files, import them, then launch with DOSBox.");
        ImGui::Separator();

        char pathBuf[4096] = {};
        std::snprintf(pathBuf, sizeof(pathBuf), "%s", state.inputPath.c_str());
        ImGui::SetNextItemWidth(-110.0f);
        if (ImGui::InputText("##zip_path", pathBuf, sizeof(pathBuf))) {
            state.inputPath = pathBuf;
        }
        ImGui::SameLine();
        if (ImGui::Button("Import", ImVec2(96.0f, 0.0f))) {
            importZip(state, state.inputPath);
        }

        ImGui::Spacing();
        ImGui::BeginChild("library", ImVec2(0, -78.0f), true);
        if (state.library.empty()) {
            ImGui::TextDisabled("No games imported yet.");
        }
        for (int i = 0; i < static_cast<int>(state.library.size()); ++i) {
            const auto& entry = state.library[i];
            std::string label = entry.title;
            if (entry.source == "scored") {
                label += "  [" + std::to_string(static_cast<int>(entry.confidence * 100.0f)) + "%]";
            }
            if (ImGui::Selectable(label.c_str(), state.selected == i)) {
                state.selected = i;
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s", entry.zipPath.c_str(), entry.confPath.c_str());
            }
        }
        ImGui::EndChild();

        bool hasSelection = state.selected >= 0 && state.selected < static_cast<int>(state.library.size());
        if (!hasSelection) ImGui::BeginDisabled();
        if (ImGui::Button("Launch", ImVec2(110.0f, 0.0f))) launchSelected(state);
        ImGui::SameLine();
        if (ImGui::Button("Remove", ImVec2(110.0f, 0.0f))) removeSelected(state);
        if (!hasSelection) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Reload", ImVec2(110.0f, 0.0f))) {
            loadLibrary(state);
            state.status = "Library reloaded.";
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", state.status.c_str());

        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(0.09f, 0.10f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
