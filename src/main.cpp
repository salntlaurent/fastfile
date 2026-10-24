#include "app.h"
HINSTANCE instance;
HWND owner = nullptr, settingsWindow = nullptr;
HFONT font = nullptr;
Settings settings;
int hotkeyId = 1;
bool registered = false;
namespace {
UINT taskbarMessage;
NOTIFYICONDATAW tray{};
}
void AddTray() {
    tray = {};
    tray.cbSize = sizeof(tray);
    tray.hWnd = owner;
    tray.uID = 1;
    tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    tray.uCallbackMessage = TrayMessage;
    tray.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    lstrcpyW(tray.szTip, L"FastFile - Create files");
    Shell_NotifyIconW(NIM_ADD, &tray);
}

LRESULT CALLBACK OwnerProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == taskbarMessage) { AddTray(); return 0; }
    if (message == WM_HOTKEY) { FileMenu(); return 0; }
    if (message == WM_TIMER && wparam == 1) { RenameTick(); return 0; }
    if (message == ShowSettings) { OpenSettings(); return 0; }
    if (message == TrayMessage) {
        if (lparam == WM_LBUTTONUP || lparam == NIN_KEYSELECT) OpenSettings();
        if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
            HMENU menu = CreatePopupMenu();
            AppendMenuW(menu, MF_STRING, 1, L"Settings");
            AppendMenuW(menu, MF_STRING, 2, L"Exit");
            POINT point{};
            GetCursorPos(&point);
            SetForegroundWindow(window);
            UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, window, nullptr);
            DestroyMenu(menu);
            PostMessageW(window, WM_NULL, 0, 0);
            if (command == 1) OpenSettings();
            if (command == 2) DestroyWindow(window);
        }
        return 0;
    }
    if (message == WM_DESTROY) {
        StopRename(false);
        if (settingsWindow) DestroyWindow(settingsWindow);
        if (registered) UnregisterHotKey(window, hotkeyId);
        Shell_NotifyIconW(NIM_DELETE, &tray);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

int WINAPI wWinMain(HINSTANCE currentInstance, HINSTANCE, PWSTR, int) {
    instance = currentInstance;
    HANDLE mutex = CreateMutexW(nullptr, FALSE, L"Local\\FastFile.SingleInstance");
    if (!mutex) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = nullptr;
        for (unsigned attempt = 0; attempt < 40 && !existing; ++attempt) {
            existing = FindWindowW(L"FastFile.Owner", nullptr);
            if (!existing) Sleep(50);
        }
        if (existing) { DWORD process = 0; GetWindowThreadProcessId(existing, &process); AllowSetForegroundWindow(process); PostMessageW(existing, ShowSettings, 0, 0); }
        CloseHandle(mutex);
        return 0;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) { CloseHandle(mutex); return 1; }
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES};
    InitCommonControlsEx(&controls);
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0);
    font = CreateFontIndirectW(&metrics.lfMessageFont);
    LoadSettings();
    taskbarMessage = RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSEXW cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = instance;
    cls.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    cls.hIconSm = cls.hIcon;
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    cls.lpfnWndProc = OwnerProc;
    cls.lpszClassName = L"FastFile.Owner";
    RegisterClassExW(&cls);
    cls.lpfnWndProc = SettingsProc;
    cls.lpszClassName = L"FastFile.Settings";
    RegisterClassExW(&cls);
    owner = CreateWindowExW(WS_EX_TOOLWINDOW, L"FastFile.Owner", L"FastFile", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (owner) {
        ShowWindow(owner, SW_HIDE);
        AddTray();
        registered = RegisterHotKey(owner, hotkeyId, settings.modifiers | MOD_NOREPEAT, settings.key) != FALSE;
        if (!registered) { OpenSettings(); Error(L"This hotkey is already in use. Choose another combination in Settings.", settingsWindow); }
        MSG message{};
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            if (settingsWindow && IsDialogMessageW(settingsWindow, &message)) continue;
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    DeleteObject(font);
    CoUninitialize();
    CloseHandle(mutex);
    return owner ? 0 : 1;
}
