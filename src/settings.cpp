#include "app.h"

namespace { std::wstring configPath; }

bool Normalize(std::wstring& value) {
    while (!value.empty() && iswspace(value.front())) value.erase(value.begin());
    while (!value.empty() && iswspace(value.back())) value.pop_back();
    if (!value.empty() && value.front() == L'.') value.erase(value.begin());
    if (value.empty() || value.size() > 64 || value.front() == L'.' || value.back() == L'.') return false;
    for (auto& c : value) {
        if (c < 32 || iswspace(c) || std::wstring(L"\\/:*?\"<>|").find(c) != std::wstring::npos) return false;
        c = static_cast<wchar_t>(towlower(c));
    }
    return value.find(L"..") == std::wstring::npos;
}

std::wstring ReadValue(const wchar_t* key, const wchar_t* fallback) {
    wchar_t value[256]{};
    GetPrivateProfileStringW(L"FastFile", key, fallback, value, 256, configPath.c_str());
    return value;
}

void LoadSettings() {
    auto directory = KnownFolder(FOLDERID_LocalAppData) + L"\\FastFile";
    CreateDirectoryW(directory.c_str(), nullptr);
    configPath = directory + L"\\settings.ini";
    settings.modifiers = GetPrivateProfileIntW(L"FastFile", L"Modifiers", MOD_CONTROL | MOD_ALT, configPath.c_str());
    settings.key = GetPrivateProfileIntW(L"FastFile", L"Key", 'N', configPath.c_str());
    if ((settings.modifiers & ~(MOD_CONTROL | MOD_ALT | MOD_SHIFT)) || !settings.modifiers || settings.key < 0x20 || settings.key > 0xFE) {
        settings.modifiers = MOD_CONTROL | MOD_ALT;
        settings.key = 'N';
    }
    int count = GetPrivateProfileIntW(L"FastFile", L"Count", -1, configPath.c_str());
    if (count > 0 && count <= 100) {
        std::vector<std::wstring> loaded;
        for (int i = 0; i < count; ++i) {
            auto value = ReadValue((L"Type" + std::to_wstring(i)).c_str(), L"");
            if (Normalize(value) && std::find(loaded.begin(), loaded.end(), value) == loaded.end()) loaded.push_back(value);
        }
        if (!loaded.empty()) settings.extensions = std::move(loaded);
    }
}

bool Persist(const Settings& next) {
    std::wstring text = L"\xFEFF[FastFile]\r\nModifiers=" + std::to_wstring(next.modifiers) + L"\r\nKey=" + std::to_wstring(next.key)
        + L"\r\nCount=" + std::to_wstring(next.extensions.size()) + L"\r\n";
    for (size_t i = 0; i < next.extensions.size(); ++i) text += L"Type" + std::to_wstring(i) + L"=" + next.extensions[i] + L"\r\n";
    auto temporary = configPath + L".tmp";
    HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0, bytes = static_cast<DWORD>(text.size() * sizeof(wchar_t));
    bool ok = WriteFile(file, text.data(), bytes, &written, nullptr) && written == bytes;
    if (ok) ok = FlushFileBuffers(file) != FALSE;
    CloseHandle(file);
    if (ok) ok = MoveFileExW(temporary.c_str(), configPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) DeleteFileW(temporary.c_str());
    return ok;
}

constexpr auto RunKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

bool StartupEnabled() {
    DWORD bytes = 0;
    return RegGetValueW(HKEY_CURRENT_USER, RunKey, L"FastFile", RRF_RT_REG_SZ, nullptr, nullptr, &bytes) == ERROR_SUCCESS;
}

bool SetStartup(bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, RunKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) != ERROR_SUCCESS) return false;
    LSTATUS status;
    if (enabled) {
        wchar_t path[32768]{};
        if (!GetModuleFileNameW(nullptr, path, 32768)) { RegCloseKey(key); return false; }
        std::wstring command = L"\"" + std::wstring(path) + L"\"";
        status = RegSetValueExW(key, L"FastFile", 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()), static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    } else {
        status = RegDeleteValueW(key, L"FastFile");
        if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
    }
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

