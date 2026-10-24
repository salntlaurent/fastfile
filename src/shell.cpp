#include "app.h"
namespace {
bool menuOpen = false;
std::unique_ptr<Target> pending;
std::wstring pendingName;
unsigned renameAttempts = 0;
}
std::wstring ClassName(HWND window) {
    wchar_t name[128]{};
    GetClassNameW(window, name, 128);
    return name;
}

std::unique_ptr<Target> FromDispatch(IDispatch* dispatch, HWND root, bool desktop) {
    Com<IServiceProvider> service;
    Com<IShellBrowser> browser;
    auto result = std::make_unique<Target>();
    if (FAILED(dispatch->QueryInterface(IID_IServiceProvider, reinterpret_cast<void**>(service.out()))) ||
        FAILED(service->QueryService(SID_STopLevelBrowser, IID_IShellBrowser, reinterpret_cast<void**>(browser.out()))) ||
        FAILED(browser->QueryActiveShellView(result->view.out())) ||
        FAILED(result->view->GetWindow(&result->viewWindow))) return {};
    if (!desktop && (!IsWindowVisible(result->viewWindow) || GetAncestor(result->viewWindow, GA_ROOT) != root)) return {};
    Com<IFolderView> folderView;
    Com<IPersistFolder2> folder;
    if (FAILED(result->view->QueryInterface(IID_IFolderView, reinterpret_cast<void**>(folderView.out()))) ||
        FAILED(folderView->GetFolder(IID_IPersistFolder2, reinterpret_cast<void**>(folder.out())))) return {};
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(folder->GetCurFolder(&pidl))) return {};
    wchar_t path[32768]{};
    bool filesystem = SHGetPathFromIDListEx(pidl, path, 32768, GPFIDL_DEFAULT) != FALSE;
    CoTaskMemFree(pidl);
    result->path = desktop ? KnownFolder(FOLDERID_Desktop) : (filesystem ? path : L"");
    if (result->path.empty()) return {};
    DWORD attributes = GetFileAttributesW(result->path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return {};
    result->root = root;
    return result;
}

std::unique_ptr<Target> Resolve(HWND root) {
    if (!IsWindow(root)) return {};
    auto cls = ClassName(root);
    bool desktop = cls == L"Progman" || cls == L"WorkerW";
    if (!desktop && cls != L"CabinetWClass" && cls != L"ExploreWClass") return {};
    Com<IShellWindows> windows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER, IID_IShellWindows, reinterpret_cast<void**>(windows.out())))) return {};
    if (desktop) {
        VARIANT location{}, rootLocation{};
        location.vt = VT_I4;
        location.lVal = CSIDL_DESKTOP;
        LONG handle = 0;
        Com<IDispatch> dispatch;
        if (SUCCEEDED(windows->FindWindowSW(&location, &rootLocation, SWC_DESKTOP, &handle, SWFO_NEEDDISPATCH, dispatch.out())) && dispatch)
            return FromDispatch(dispatch.p, root, true);
        return {};
    }
    long count = 0;
    windows->get_Count(&count);
    std::unique_ptr<Target> selected;
    for (long i = 0; i < count; ++i) {
        VARIANT index{};
        index.vt = VT_I4;
        index.lVal = i;
        Com<IDispatch> dispatch;
        if (FAILED(windows->Item(index, dispatch.out())) || !dispatch) continue;
        auto candidate = FromDispatch(dispatch.p, root, false);
        if (candidate) {
            if (selected && selected->viewWindow != candidate->viewWindow) return {};
            selected = std::move(candidate);
        }
    }
    return selected;
}

bool SameTarget(const Target& a, const Target& b) {
    return a.root == b.root && a.viewWindow == b.viewWindow && _wcsicmp(a.path.c_str(), b.path.c_str()) == 0;
}

void StopRename(bool warn) {
    KillTimer(owner, 1);
    pending.reset();
    if (warn) Error(L"The file was created, but could not enter rename mode. Select it and press F2.");
}

void RenameTick() {
    if (!pending) return;
    auto current = Resolve(pending->root);
    if (!current || !SameTarget(*pending, *current) || GetForegroundWindow() != pending->root) { StopRename(true); return; }
    Com<IFolderView> folderView;
    Com<IShellFolder> folder;
    PIDLIST_RELATIVE child = nullptr;
    HRESULT result = pending->view->QueryInterface(IID_IFolderView, reinterpret_cast<void**>(folderView.out()));
    if (SUCCEEDED(result)) result = folderView->GetFolder(IID_IShellFolder, reinterpret_cast<void**>(folder.out()));
    if (SUCCEEDED(result)) result = folder->ParseDisplayName(nullptr, nullptr, const_cast<wchar_t*>(pendingName.c_str()), nullptr, &child, nullptr);
    if (SUCCEEDED(result)) result = pending->view->SelectItem(child, SVSI_EDIT | SVSI_DESELECTOTHERS | SVSI_ENSUREVISIBLE | SVSI_FOCUSED);
    CoTaskMemFree(child);
    if (SUCCEEDED(result)) StopRename(false);
    else if (++renameAttempts >= 10) StopRename(true);
}

void CreateNewFile(std::unique_ptr<Target> target, const std::wstring& extension) {
    auto current = Resolve(target->root);
    if (!current || !SameTarget(*target, *current)) { Error(L"The folder or tab has changed. Open the menu again."); return; }
    auto created = CreateEmptyFile(target->path, extension);
    if (created.error != ERROR_SUCCESS) { Error(L"Could not create the file.\n" + SystemError(created.error)); return; }
    SHChangeNotify(SHCNE_CREATE, SHCNF_PATHW, created.path.c_str(), nullptr);
    SetForegroundWindow(target->root);
    pending = std::move(target);
    pendingName = created.name;
    renameAttempts = 0;
    if (!SetTimer(owner, 1, 100, nullptr)) StopRename(true);
}

void FileMenu() {
    if (menuOpen || pending) return;
    auto target = Resolve(GetForegroundWindow());
    if (!target) return;
    HMENU menu = CreatePopupMenu();
    for (size_t i = 0; i < settings.extensions.size(); ++i) {
        auto label = L"." + settings.extensions[i];
        size_t pos = 0;
        while ((pos = label.find(L'&', pos)) != std::wstring::npos) { label.insert(pos, 1, L'&'); pos += 2; }
        AppendMenuW(menu, MF_STRING, i + 1, label.c_str());
    }
    POINT point{};
    GetCursorPos(&point);
    menuOpen = true;
    SetForegroundWindow(owner);
    UINT command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, point.x, point.y, owner, nullptr);
    PostMessageW(owner, WM_NULL, 0, 0);
    DestroyMenu(menu);
    menuOpen = false;
    if (command && command <= settings.extensions.size()) CreateNewFile(std::move(target), settings.extensions[command - 1]);
    else if (GetForegroundWindow() == owner) SetForegroundWindow(target->root);
}

