# FastFile

A lightweight Windows tray app for creating empty files with a hotkey. Built with C++ and Win32 for Windows 10/11 x64. No installer or extra runtime required.

## Usage

1. Run `FastFile.exe`.
2. Open a folder in File Explorer or select the desktop.
3. Press **Ctrl+Alt+N** and choose a file type.
4. Rename the new file. Existing files are never overwritten.

Click the tray icon to change the hotkey, manage file types or enable **Start with Windows**. Right-click it to exit. Startup is off by default.

Any extension is supported, including compound extensions such as `tar.gz`. Files are always empty: choosing `docx` does not create a valid Word document. Search results, archives and virtual folders are not supported.

Settings are saved in `%LOCALAPPDATA%\FastFile\settings.ini`. If you move the EXE, save settings again to update its startup path.

## Build

Requires CMake 3.20+ and Visual Studio 2022 with the C++ desktop tools:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
```

