#include "app.h"

void Error(const std::wstring& message, HWND parent) {
    MessageBoxW(parent ? parent : owner, message.c_str(), L"FastFile", MB_OK | MB_ICONWARNING);
}

std::wstring SystemError(DWORD code) {
    wchar_t* buffer = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring result = buffer ? buffer : L"Unknown error.";
    LocalFree(buffer);
    return result;
}

std::wstring KnownFolder(REFKNOWNFOLDERID id) {
    PWSTR path = nullptr;
    if (FAILED(SHGetKnownFolderPath(id, 0, nullptr, &path))) return {};
    std::wstring result(path);
    CoTaskMemFree(path);
    return result;
}

