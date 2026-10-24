#include "app.h"
namespace {
std::vector<std::wstring> draft;
constexpr int List = 101, Extension = 102, Add = 103, Change = 104, Remove = 105,
    Up = 106, Down = 107, Hotkey = 108, Startup = 109, Save = 110;
}
int Scale(int value) {
    HDC dc = GetDC(nullptr);
    int dpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(nullptr, dc);
    return MulDiv(value, dpi, 96);
}

HWND Control(HWND parent, const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h, int id) {
    HWND control = CreateWindowExW((style & WS_BORDER) ? WS_EX_CLIENTEDGE : 0, cls, text, WS_CHILD | WS_VISIBLE | style,
        Scale(x), Scale(y), Scale(w), Scale(h), parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    return control;
}

void RefreshList(int selected) {
    HWND list = GetDlgItem(settingsWindow, List);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& extension : draft) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(extension.c_str()));
    if (!draft.empty()) {
        selected = std::max(0, std::min(selected, static_cast<int>(draft.size()) - 1));
        SendMessageW(list, LB_SETCURSEL, selected, 0);
        SetDlgItemTextW(settingsWindow, Extension, draft[selected].c_str());
    } else SetDlgItemTextW(settingsWindow, Extension, L"");
}

void SaveSettings() {
    WORD chord = static_cast<WORD>(SendDlgItemMessageW(settingsWindow, Hotkey, HKM_GETHOTKEY, 0, 0));
    BYTE flags = HIBYTE(chord);
    Settings next;
    next.key = LOBYTE(chord);
    next.modifiers = ((flags & HOTKEYF_CONTROL) ? MOD_CONTROL : 0) | ((flags & HOTKEYF_ALT) ? MOD_ALT : 0) | ((flags & HOTKEYF_SHIFT) ? MOD_SHIFT : 0);
    next.extensions = draft;
    if (!next.key || !next.modifiers || next.extensions.empty()) { Error(L"Choose a hotkey with Ctrl, Alt or Shift and add at least one file type.", settingsWindow); return; }
    bool changed = !registered || next.key != settings.key || next.modifiers != settings.modifiers;
    int candidateId = hotkeyId == 1 ? 2 : 1;
    if (changed && !RegisterHotKey(owner, candidateId, next.modifiers | MOD_NOREPEAT, next.key)) { Error(L"This hotkey is unavailable. Choose another combination.", settingsWindow); return; }
    bool oldStartup = StartupEnabled();
    bool newStartup = IsDlgButtonChecked(settingsWindow, Startup) == BST_CHECKED;
    if (!SetStartup(newStartup)) {
        if (changed) UnregisterHotKey(owner, candidateId);
        Error(L"Could not update startup settings.", settingsWindow);
        return;
    }
    if (!Persist(next)) {
        SetStartup(oldStartup);
        if (changed) UnregisterHotKey(owner, candidateId);
        Error(L"Could not save settings.", settingsWindow);
        return;
    }
    if (changed) {
        if (registered) UnregisterHotKey(owner, hotkeyId);
        hotkeyId = candidateId;
        registered = true;
    }
    settings = std::move(next);
    DestroyWindow(settingsWindow);
}

LRESULT CALLBACK SettingsProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_CREATE) {
        settingsWindow = window;
        draft = settings.extensions;
        Control(window, L"STATIC", L"Hotkey:", 0, 16, 16, 180, 20, 0);
        HWND hotkey = Control(window, HOTKEY_CLASSW, L"", WS_TABSTOP | WS_BORDER, 16, 40, 340, 28, Hotkey);
        WORD modifiers = ((settings.modifiers & MOD_CONTROL) ? HOTKEYF_CONTROL : 0) | ((settings.modifiers & MOD_ALT) ? HOTKEYF_ALT : 0) | ((settings.modifiers & MOD_SHIFT) ? HOTKEYF_SHIFT : 0);
        SendMessageW(hotkey, HKM_SETHOTKEY, MAKEWORD(settings.key, modifiers), 0);
        Control(window, L"STATIC", L"File types (empty files):", 0, 16, 82, 380, 22, 0);
        Control(window, L"LISTBOX", L"", WS_TABSTOP | WS_BORDER | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT, 16, 108, 220, 180, List);
        Control(window, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 248, 108, 174, 28, Extension);
        SendDlgItemMessageW(window, Extension, EM_SETLIMITTEXT, 65, 0);
        Control(window, L"BUTTON", L"Add", WS_TABSTOP, 248, 144, 174, 28, Add);
        Control(window, L"BUTTON", L"Edit", WS_TABSTOP, 248, 180, 174, 28, Change);
        Control(window, L"BUTTON", L"Remove", WS_TABSTOP, 248, 216, 174, 28, Remove);
        Control(window, L"BUTTON", L"Up", WS_TABSTOP, 248, 252, 82, 28, Up);
        Control(window, L"BUTTON", L"Down", WS_TABSTOP, 340, 252, 82, 28, Down);
        Control(window, L"BUTTON", L"Start with Windows", WS_TABSTOP | BS_AUTOCHECKBOX, 16, 304, 400, 26, Startup);
        CheckDlgButton(window, Startup, StartupEnabled() ? BST_CHECKED : BST_UNCHECKED);
        Control(window, L"BUTTON", L"Save", WS_TABSTOP | BS_DEFPUSHBUTTON, 200, 346, 108, 30, Save);
        Control(window, L"BUTTON", L"Cancel", WS_TABSTOP, 320, 346, 102, 30, IDCANCEL);
        RefreshList(0);
        return 0;
    }
    if (message == WM_COMMAND) {
        int id = LOWORD(wparam);
        int selection = static_cast<int>(SendDlgItemMessageW(window, List, LB_GETCURSEL, 0, 0));
        if (id == List && HIWORD(wparam) == LBN_SELCHANGE && selection >= 0) SetDlgItemTextW(window, Extension, draft[selection].c_str());
        if (id == Add || id == Change) {
            if (id == Change && selection < 0) return 0;
            wchar_t buffer[128]{};
            GetDlgItemTextW(window, Extension, buffer, 128);
            std::wstring value(buffer);
            if (!Normalize(value)) { Error(L"Enter an extension of up to 64 characters without spaces or \\ / : * ? \" < > |.", window); return 0; }
            for (size_t i = 0; i < draft.size(); ++i) if (draft[i] == value && !(id == Change && static_cast<int>(i) == selection)) { Error(L"This file type is already in the list.", window); return 0; }
            if (id == Add) {
                if (draft.size() >= 100) { Error(L"You can add up to 100 file types.", window); return 0; }
                draft.push_back(value);
                selection = static_cast<int>(draft.size()) - 1;
            } else draft[selection] = value;
            RefreshList(selection);
        }
        if (id == Remove && selection >= 0) { draft.erase(draft.begin() + selection); RefreshList(selection); }
        if ((id == Up || id == Down) && selection >= 0) {
            int next = selection + (id == Up ? -1 : 1);
            if (next >= 0 && next < static_cast<int>(draft.size())) { std::swap(draft[selection], draft[next]); RefreshList(next); }
        }
        if (id == Save || id == IDOK) SaveSettings();
        if (id == IDCANCEL) DestroyWindow(window);
        return 0;
    }
    if (message == WM_CLOSE) { DestroyWindow(window); return 0; }
    if (message == WM_DESTROY) { settingsWindow = nullptr; return 0; }
    return DefWindowProcW(window, message, wparam, lparam);
}

void OpenSettings() {
    if (!settingsWindow) {
        RECT bounds{0, 0, Scale(440), Scale(396)};
        AdjustWindowRectEx(&bounds, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, WS_EX_CONTROLPARENT);
        settingsWindow = CreateWindowExW(WS_EX_CONTROLPARENT, L"FastFile.Settings", L"FastFile - Settings", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    }
    ShowWindow(settingsWindow, SW_SHOW);
    SetForegroundWindow(settingsWindow);
}

