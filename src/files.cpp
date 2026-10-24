#include "app.h"

CreatedFile CreateEmptyFile(const std::wstring& directory, const std::wstring& extension) {
    CreatedFile result;
    for (unsigned i = 1; i <= 10000; ++i) {
        result.name = L"New file" + (i == 1 ? std::wstring() : L" (" + std::to_wstring(i) + L")") + L"." + extension;
        result.path = directory + L"\\" + result.name;
        HANDLE file = CreateFileW(result.path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
            result.error = ERROR_SUCCESS;
            return result;
        }
        result.error = GetLastError();
        if (result.error != ERROR_FILE_EXISTS && result.error != ERROR_ALREADY_EXISTS) return result;
    }
    return result;
}
