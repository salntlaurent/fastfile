#pragma once
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <exdisp.h>
#include <shlguid.h>
#include <servprov.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <memory>

template<class T> struct Com {
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T** out() { return &p; }
    T* operator->() const { return p; }
    explicit operator bool() const { return p != nullptr; }
    Com() = default;
    Com(const Com&) = delete;
    Com& operator=(const Com&) = delete;
};

struct Settings {
    UINT modifiers = MOD_CONTROL | MOD_ALT;
    UINT key = 'N';
    std::vector<std::wstring> extensions{L"txt", L"md", L"py", L"json", L"html"};
};

struct Target {
    HWND root = nullptr;
    HWND viewWindow = nullptr;
    std::wstring path;
    Com<IShellView> view;
};

struct CreatedFile {
    std::wstring name;
    std::wstring path;
    DWORD error = ERROR_SUCCESS;
};

CreatedFile CreateEmptyFile(const std::wstring& directory, const std::wstring& extension);

extern HINSTANCE instance;
extern HWND owner;
extern HWND settingsWindow;
extern HFONT font;
extern Settings settings;
extern int hotkeyId;
extern bool registered;
constexpr UINT TrayMessage = WM_APP + 1, ShowSettings = WM_APP + 2;
void Error(const std::wstring& message, HWND parent = nullptr);
std::wstring SystemError(DWORD code);
std::wstring KnownFolder(REFKNOWNFOLDERID id);
bool Normalize(std::wstring& value);
void LoadSettings();
bool Persist(const Settings& next);
bool StartupEnabled();
bool SetStartup(bool enabled);
void FileMenu();
void RenameTick();
void StopRename(bool warn);
void OpenSettings();
LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
