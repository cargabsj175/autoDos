// main.cpp — AutoDOS GUI (Win32)
// Drop any DOS zip → AutoDOS finds the exe → DOSBox launches

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

#include "autodos.h"
#include "nlohmann/json.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "comdlg32.lib")

namespace fs = std::filesystem;
using json   = nlohmann::json;

// ── IDs ───────────────────────────────────────────────────────────────────────
#define IDC_LIST        1001
#define IDC_BTN_LAUNCH  1002
#define IDC_BTN_DELETE  1003
#define IDC_BTN_ADD     1004
#define IDC_STATUS      1005
#define IDM_LAUNCH      2001
#define IDM_DELETE      2002
#define WM_IMPORT_DONE  (WM_APP + 1)
#define WM_SET_STATUS   (WM_APP + 2)

// ── Colors ────────────────────────────────────────────────────────────────────
#define CLR_BG        RGB(18,  18,  24)
#define CLR_ITEM      RGB(28,  28,  36)
#define CLR_ITEM_SEL  RGB(32,  52,  40)
#define CLR_ACCENT    RGB(80,  200, 120)
#define CLR_TEXT      RGB(220, 220, 220)
#define CLR_TEXT_DIM  RGB(110, 110, 120)
#define CLR_DIVIDER   RGB(38,  38,  50)

// ── Library entry ─────────────────────────────────────────────────────────────
struct LibEntry {
    std::string id;
    std::string title;
    std::string zipPath;
    std::string confPath;
    std::string source;
    float       confidence = 0.0f;
};

// ── Result passed back from worker thread ─────────────────────────────────────
struct ImportResult {
    bool        ok = false;
    std::string error;
    LibEntry    entry;
    AutoDOS::AnalyzeResult analyzeResult;
};

// ── Globals ───────────────────────────────────────────────────────────────────
static HWND g_hwnd      = nullptr;
static HWND g_list      = nullptr;
static HWND g_status    = nullptr;
static HWND g_btnLaunch = nullptr;
static HWND g_btnDelete = nullptr;
static HWND g_btnAdd    = nullptr;

static std::string g_appDir;
static std::string g_dbPath;
static std::string g_libPath;
static std::string g_dosboxPath;

static std::vector<LibEntry>  g_library;  // main thread only
static std::mutex             g_queueMtx;
static std::queue<std::string> g_queue;
static std::atomic<bool>      g_working{false};

static HBRUSH g_hbrBg    = nullptr;
static HFONT  g_fontItem = nullptr;
static HFONT  g_fontSm   = nullptr;

// ── Helpers ───────────────────────────────────────────────────────────────────

static std::string appDataDir() {
    char p[MAX_PATH];
    SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, p);
    return std::string(p) + "\\AutoDOS";
}

static std::string exeDir() {
    char p[MAX_PATH];
    GetModuleFileNameA(nullptr, p, MAX_PATH);
    std::string s = p;
    return s.substr(0, s.rfind('\\'));
}

static std::string makeId() {
    static std::atomic<int> n{0};
    return "g" + std::to_string(GetTickCount64()) + "_" + std::to_string(n++);
}

// Safe status from any thread
static void postStatus(const std::string& msg) {
    char* copy = new char[msg.size() + 1];
    strcpy_s(copy, msg.size() + 1, msg.c_str());
    PostMessageA(g_hwnd, WM_SET_STATUS, 0, (LPARAM)copy);
}

// ── Library IO (main thread only) ────────────────────────────────────────────

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

// ── UI helpers (main thread only) ────────────────────────────────────────────

static void setStatus(const std::string& msg) {
    SetWindowTextA(g_status, msg.c_str());
}

static void refreshList() {
    SendMessage(g_list, LB_RESETCONTENT, 0, 0);
    for (const auto& e : g_library) {
        std::string label = e.title;
        if (e.source == "scored")
            label += "  [" + std::to_string((int)(e.confidence*100)) + "%]";
        SendMessageA(g_list, LB_ADDSTRING, 0, (LPARAM)label.c_str());
    }
    bool has = SendMessage(g_list, LB_GETCURSEL, 0, 0) != LB_ERR;
    EnableWindow(g_btnLaunch, has ? TRUE : FALSE);
    EnableWindow(g_btnDelete, has ? TRUE : FALSE);
}

// ── Worker thread — NO UI calls, only PostMessageA ───────────────────────────

static void workerThread(std::string zipPath) {
  try {
    postStatus("Analyzing: " + fs::path(zipPath).filename().string() + "...");

    auto* res = new ImportResult();
    res->analyzeResult = AutoDOS::analyze(zipPath, g_dbPath);

    if (!res->analyzeResult.success) {
        res->ok    = false;
        res->error = res->analyzeResult.error.empty()
                     ? "Could not identify game executable"
                     : res->analyzeResult.error;
        PostMessageA(g_hwnd, WM_IMPORT_DONE, 0, (LPARAM)res);
        return;
    }

    std::string stem    = fs::path(zipPath).stem().string();
    std::string gameDir = g_appDir + "\\games\\" + stem;
    fs::create_directories(gameDir);

    postStatus("Extracting: " + stem + "...");

    if (!AutoDOS::extractZip(zipPath, gameDir)) {
        res->ok    = false;
        res->error = "Extraction failed: " + stem;
        PostMessageA(g_hwnd, WM_IMPORT_DONE, 0, (LPARAM)res);
        return;
    }

    std::string confPath = g_appDir + "\\games\\" + stem + ".conf";
    AutoDOS::writeDosboxConf(zipPath, gameDir, res->analyzeResult);

    std::string tempConf = zipPath.substr(0, zipPath.rfind('.')) + ".conf";
    if (fs::exists(tempConf)) {
        fs::copy_file(tempConf, confPath, fs::copy_options::overwrite_existing);
        fs::remove(tempConf);
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

    PostMessageA(g_hwnd, WM_IMPORT_DONE, 0, (LPARAM)res);

  } catch (const std::exception& ex) {
    auto* err = new ImportResult();
    err->ok    = false;
    err->error = std::string(ex.what());
    PostMessageA(g_hwnd, WM_IMPORT_DONE, 0, (LPARAM)err);
  } catch (...) {
    auto* err = new ImportResult();
    err->ok    = false;
    err->error = "Unknown error during import";
    PostMessageA(g_hwnd, WM_IMPORT_DONE, 0, (LPARAM)err);
  }
}

// Start next queued job — main thread only
static void startNextJob() {
    std::lock_guard<std::mutex> lk(g_queueMtx);
    if (g_queue.empty()) {
        g_working = false;
        return;
    }
    std::string next = g_queue.front();
    g_queue.pop();
    std::thread(workerThread, next).detach();
}

// Called from main thread when a zip is dropped or browsed
static void enqueueZip(const std::string& zipPath) {
    for (const auto& e : g_library) {
        if (e.zipPath == zipPath) {
            setStatus("Already in library: " + e.title);
            return;
        }
    }

    std::lock_guard<std::mutex> lk(g_queueMtx);
    if (g_working) {
        g_queue.push(zipPath);
        setStatus("Queued: " + fs::path(zipPath).filename().string());
        return;
    }
    g_working = true;
    std::thread(workerThread, zipPath).detach();
}

// ── Launch / Delete ───────────────────────────────────────────────────────────

static void launchSelected() {
    int idx = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= (int)g_library.size()) return;
    const auto& e = g_library[idx];
    if (!fs::exists(e.confPath)) { setStatus("Conf missing — re-import"); return; }
    if (!AutoDOS::launchDosBox(g_dosboxPath, e.confPath)) { setStatus("Failed to launch DOSBox"); return; }
    setStatus("Launching: " + e.title);
}

static void deleteSelected() {
    int idx = (int)SendMessage(g_list, LB_GETCURSEL, 0, 0);
    if (idx < 0 || idx >= (int)g_library.size()) return;
    std::string title    = g_library[idx].title;
    std::string confPath = g_library[idx].confPath;
    std::string prompt   = "Remove \"" + title + "\" from library?";
    if (MessageBoxA(g_hwnd, prompt.c_str(), "AutoDOS", MB_YESNO|MB_ICONQUESTION) != IDYES) return;
    fs::remove(confPath);
    fs::remove_all(confPath.substr(0, confPath.rfind('.')));
    g_library.erase(g_library.begin() + idx);
    saveLibrary();
    refreshList();
    setStatus("Removed: " + title);
}

static void browseAndAdd() {
    char path[MAX_PATH] = {};
    OPENFILENAMEA ofn   = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = g_hwnd;
    ofn.lpstrFilter = "DOS Game Archives\0*.zip;*.7z;*.rar\0All Files\0*.*\0";
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    ofn.lpstrTitle  = "Select DOS Game Zip";
    if (GetOpenFileNameA(&ofn)) enqueueZip(path);
}

// ── Owner-draw list item ──────────────────────────────────────────────────────

static void drawItem(DRAWITEMSTRUCT* dis) {
    if (dis->itemID == (UINT)-1) return;
    HDC dc  = dis->hDC;
    RECT rc = dis->rcItem;
    bool sel = (dis->itemState & ODS_SELECTED) != 0;

    HBRUSH hbr = CreateSolidBrush(sel ? CLR_ITEM_SEL : CLR_ITEM);
    FillRect(dc, &rc, hbr); DeleteObject(hbr);

    if (sel) {
        HBRUSH ab = CreateSolidBrush(CLR_ACCENT);
        RECT bar  = {rc.left, rc.top, rc.left+3, rc.bottom};
        FillRect(dc, &bar, ab); DeleteObject(ab);
    }

    char text[256] = {};
    SendMessageA(dis->hwndItem, LB_GETTEXT, dis->itemID, (LPARAM)text);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, sel ? CLR_ACCENT : CLR_TEXT);
    SelectObject(dc, g_fontItem);
    RECT tr = {rc.left+14, rc.top, rc.right-10, rc.bottom};
    DrawTextA(dc, text, -1, &tr, DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS);

    HPEN pen = CreatePen(PS_SOLID, 1, CLR_DIVIDER);
    SelectObject(dc, pen);
    MoveToEx(dc, rc.left, rc.bottom-1, nullptr);
    LineTo(dc,   rc.right, rc.bottom-1);
    DeleteObject(pen);
}

// ── WndProc ───────────────────────────────────────────────────────────────────

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {

    case WM_CREATE:
        DragAcceptFiles(hwnd, TRUE);
        return 0;

    case WM_SIZE: {
        int W=LOWORD(lParam), H=HIWORD(lParam);
        int pad=10, btnH=32, btnW=90, barH=28;
        int listH = H - btnH - barH - pad*3;
        SetWindowPos(g_list,      nullptr, pad,        pad,   W-pad*2, listH, SWP_NOZORDER);
        int btnY = pad + listH + pad;
        SetWindowPos(g_btnLaunch, nullptr, pad,        btnY,  btnW, btnH, SWP_NOZORDER);
        SetWindowPos(g_btnAdd,    nullptr, pad+btnW+8, btnY,  btnW, btnH, SWP_NOZORDER);
        SetWindowPos(g_btnDelete, nullptr, W-btnW-pad, btnY,  btnW, btnH, SWP_NOZORDER);
        SetWindowPos(g_status,    nullptr, 0, H-barH,  W,    barH, SWP_NOZORDER);
        return 0;
    }

    case WM_DROPFILES: {
        HDROP drop = (HDROP)wParam;
        UINT count = DragQueryFileA(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; i++) {
            char path[MAX_PATH];
            DragQueryFileA(drop, i, path, MAX_PATH);
            std::string p = path;
            if (p.rfind('.') == std::string::npos) continue;
            std::string ext = p.substr(p.rfind('.')+1);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext=="zip"||ext=="7z"||ext=="rar") enqueueZip(p);
        }
        DragFinish(drop);
        return 0;
    }

    case WM_IMPORT_DONE: {
        // Main thread only — safe to touch g_library and UI
        auto* res = (ImportResult*)lParam;
        if (!res) { startNextJob(); return 0; }

        if (!res->ok) {
            setStatus("Error: " + res->error);
        } else {
            bool dup = false;
            for (const auto& e : g_library)
                if (e.zipPath == res->entry.zipPath) { dup = true; break; }

            if (!dup) {
                g_library.push_back(res->entry);
                saveLibrary();

                if (res->analyzeResult.source == "scored") {
                    res->analyzeResult.title = res->entry.title;
                    AutoDOS::addToDatabase(g_dbPath, res->analyzeResult);
                }

                refreshList();
                SendMessage(g_list, LB_SETCURSEL, g_library.size()-1, 0);
                EnableWindow(g_btnLaunch, TRUE);
                EnableWindow(g_btnDelete, TRUE);

                std::string src = (res->entry.source == "database") ? "DB"
                    : "Auto " + std::to_string((int)(res->entry.confidence*100)) + "%";
                setStatus("Added: " + res->entry.title + "  [" + src + "]");
            } else {
                setStatus("Already in library: " + res->entry.title);
            }
        }

        delete res;
        startNextJob();
        return 0;
    }

    case WM_SET_STATUS: {
        char* m = (char*)lParam;
        if (m) { setStatus(m); delete[] m; }
        return 0;
    }

    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id==IDC_BTN_LAUNCH||id==IDM_LAUNCH)       { launchSelected(); return 0; }
        if (id==IDC_BTN_DELETE||id==IDM_DELETE)       { deleteSelected(); return 0; }
        if (id==IDC_BTN_ADD)                           { browseAndAdd();   return 0; }
        if (id==IDC_LIST&&HIWORD(wParam)==LBN_DBLCLK) { launchSelected(); return 0; }
        if (id==IDC_LIST&&HIWORD(wParam)==LBN_SELCHANGE) {
            bool has = SendMessage(g_list,LB_GETCURSEL,0,0) != LB_ERR;
            EnableWindow(g_btnLaunch, has?TRUE:FALSE);
            EnableWindow(g_btnDelete, has?TRUE:FALSE);
        }
        return 0;
    }

    case WM_MEASUREITEM:
        ((MEASUREITEMSTRUCT*)lParam)->itemHeight = 40;
        return TRUE;

    case WM_DRAWITEM: {
        auto* dis = (DRAWITEMSTRUCT*)lParam;
        if (dis->CtlID == IDC_LIST) drawItem(dis);
        return TRUE;
    }

    case WM_CTLCOLORLISTBOX:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
        SetBkColor((HDC)wParam, CLR_BG);
        SetTextColor((HDC)wParam, CLR_TEXT_DIM);
        return (LRESULT)g_hbrBg;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(hwnd, &rc);
        FillRect((HDC)wParam, &rc, g_hbrBg);
        return 1;
    }

    case WM_CONTEXTMENU:
        if ((HWND)wParam == g_list) {
            HMENU m = CreatePopupMenu();
            AppendMenuA(m, MF_STRING, IDM_LAUNCH, "Launch");
            AppendMenuA(m, MF_STRING, IDM_DELETE, "Remove");
            TrackPopupMenu(m, TPM_RIGHTBUTTON,
                GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), 0, hwnd, nullptr);
            DestroyMenu(m);
        }
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ── WinMain ───────────────────────────────────────────────────────────────────

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    INITCOMMONCONTROLSEX icc = {sizeof(icc), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&icc);

    g_appDir     = appDataDir();
    g_dbPath     = g_appDir + "\\games.json";
    g_libPath    = g_appDir + "\\library.json";
    g_dosboxPath = exeDir()  + "\\dosbox\\dosbox.exe";

    fs::create_directories(g_appDir);
    fs::create_directories(g_appDir + "\\games");

    if (!fs::exists(g_dbPath)) {
        std::string src = exeDir() + "\\games.json";
        if (fs::exists(src)) fs::copy_file(src, g_dbPath);
    }

    loadLibrary();

    g_hbrBg   = CreateSolidBrush(CLR_BG);
    g_fontItem = CreateFontA(15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");
    g_fontSm   = CreateFontA(12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,"Segoe UI");

    WNDCLASSEXA wc   = {sizeof(wc)};
    wc.style         = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = g_hbrBg;
    wc.lpszClassName = "AutoDOS_Main";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExA(&wc);

    g_hwnd = CreateWindowExA(WS_EX_ACCEPTFILES,
        "AutoDOS_Main","AutoDOS",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,500,600,
        nullptr,nullptr,hInst,nullptr);

    g_list = CreateWindowExA(WS_EX_CLIENTEDGE,"LISTBOX",nullptr,
        WS_CHILD|WS_VISIBLE|WS_VSCROLL|LBS_NOTIFY|LBS_OWNERDRAWFIXED|LBS_HASSTRINGS,
        0,0,0,0,g_hwnd,(HMENU)IDC_LIST,hInst,nullptr);
    SendMessage(g_list, WM_SETFONT, (WPARAM)g_fontItem, TRUE);

    g_btnLaunch = CreateWindowA("BUTTON","Launch",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0,g_hwnd,(HMENU)IDC_BTN_LAUNCH,hInst,nullptr);
    SendMessage(g_btnLaunch, WM_SETFONT, (WPARAM)g_fontItem, TRUE);
    EnableWindow(g_btnLaunch, FALSE);

    g_btnAdd = CreateWindowA("BUTTON","Add Zip",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0,g_hwnd,(HMENU)IDC_BTN_ADD,hInst,nullptr);
    SendMessage(g_btnAdd, WM_SETFONT, (WPARAM)g_fontItem, TRUE);

    g_btnDelete = CreateWindowA("BUTTON","Remove",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,
        0,0,0,0,g_hwnd,(HMENU)IDC_BTN_DELETE,hInst,nullptr);
    SendMessage(g_btnDelete, WM_SETFONT, (WPARAM)g_fontItem, TRUE);
    EnableWindow(g_btnDelete, FALSE);

    g_status = CreateWindowA("STATIC",
        "Drop a DOS zip to add  |  Double-click to launch",
        WS_CHILD|WS_VISIBLE|SS_LEFT,
        0,0,0,0,g_hwnd,(HMENU)IDC_STATUS,hInst,nullptr);
    SendMessage(g_status, WM_SETFONT, (WPARAM)g_fontSm, TRUE);

    refreshList();
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessage(&msg,nullptr,0,0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    DeleteObject(g_hbrBg);
    DeleteObject(g_fontItem);
    DeleteObject(g_fontSm);
    return (int)msg.wParam;
}
