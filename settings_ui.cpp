#include "settings_ui.h"
#include "resource.h"
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <memory>

namespace guard {
namespace {

struct Editor {
    HWND sheet = nullptr;
    HWND displays = nullptr;
    HWND applications = nullptr;
    HWND general = nullptr;
    Settings draft;
    bool startup = false;
    bool populating = false;
    bool dirty = false;
    std::wstring path;
    std::vector<Monitor> monitors;
    std::vector<std::wstring> destinations;
    ApplySettings apply;
};

std::unique_ptr<Editor> editor;

void Changed(HWND page) {
    if (editor->populating) return;
    editor->dirty = true;
    PropSheet_Changed(GetParent(page), page);
}

std::wstring DisplayNumber(const Monitor& monitor) {
    const auto position = monitor.device.find(L"DISPLAY");
    return position == std::wstring::npos ? L"?" : monitor.device.substr(position + 7);
}

std::wstring DisplayLabel(const Monitor& monitor) {
    return DisplayNumber(monitor) + L" - " + monitor.name + (monitor.primary ? L" (main)" : L"");
}

void FillDestinations() {
    const auto combo = GetDlgItem(editor->displays, IDC_DESTINATION);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    editor->destinations = {L""};
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Automatic - most visible / nearest display"));
    int selected = 0;
    for (const auto& monitor : editor->monitors) {
        if (monitor.blocked || monitor.identities.empty() || monitor.identities.front().empty()) continue;
        if (Contains(monitor.identities, editor->draft.destination)) selected = static_cast<int>(editor->destinations.size());
        editor->destinations.push_back(monitor.identities.front());
        const auto label = DisplayLabel(monitor);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
    }
    if (!editor->draft.destination.empty() && selected == 0) {
        selected = static_cast<int>(editor->destinations.size());
        editor->destinations.push_back(editor->draft.destination);
        SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Saved display unavailable - automatic fallback"));
    }
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

void RefreshDisplayPage() {
    editor->populating = true;
    editor->monitors = ReadMonitors();
    ApplyDisplayRules(editor->monitors, editor->draft);
    const auto list = GetDlgItem(editor->displays, IDC_MONITORS);
    ListView_DeleteAllItems(list);
    unsigned allowed = 0;
    for (size_t index = 0; index < editor->monitors.size(); ++index) {
        const auto& monitor = editor->monitors[index];
        auto label = DisplayLabel(monitor);
        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(index);
        item.pszText = label.data();
        ListView_InsertItem(list, &item);
        auto resolution = std::to_wstring(Width(monitor.bounds)) + L" x " + std::to_wstring(Height(monitor.bounds));
        ListView_SetItemText(list, item.iItem, 1, resolution.data());
        auto state = std::wstring(monitor.blocked ? L"Blocked" : L"Allowed");
        ListView_SetItemText(list, item.iItem, 2, state.data());
        ListView_SetCheckState(list, item.iItem, monitor.blocked);
        if (!monitor.blocked) ++allowed;
    }
    SetDlgItemTextW(editor->displays, IDC_DISPLAY_NOTE, allowed ? L"Checked displays are kept clear. Audio and display settings are unchanged."
                                                             : L"Leave at least one display available before applying these rules.");
    FillDestinations();
    InvalidateRect(GetDlgItem(editor->displays, IDC_MONITOR_MAP), nullptr, TRUE);
    editor->populating = false;
}

void DrawMap(const DRAWITEMSTRUCT& draw) {
    FillRect(draw.hDC, &draw.rcItem, GetSysColorBrush(COLOR_WINDOW));
    if (editor->monitors.empty()) return;
    RECT desktop = editor->monitors.front().bounds;
    for (const auto& monitor : editor->monitors) UnionRect(&desktop, &desktop, &monitor.bounds);
    const double scale = std::min(static_cast<double>(Width(draw.rcItem) - 24) / std::max(1L, Width(desktop)),
                                  static_cast<double>(Height(draw.rcItem) - 18) / std::max(1L, Height(desktop)));
    const int originX = draw.rcItem.left + static_cast<int>((Width(draw.rcItem) - Width(desktop) * scale) / 2);
    const int originY = draw.rcItem.top + static_cast<int>((Height(draw.rcItem) - Height(desktop) * scale) / 2);
    const auto font = reinterpret_cast<HFONT>(SendMessageW(editor->displays, WM_GETFONT, 0, 0));
    const auto oldFont = SelectObject(draw.hDC, font);
    SetBkMode(draw.hDC, TRANSPARENT);
    for (const auto& monitor : editor->monitors) {
        RECT box = {originX + static_cast<int>((monitor.bounds.left - desktop.left) * scale) + 2,
                    originY + static_cast<int>((monitor.bounds.top - desktop.top) * scale) + 2,
                    originX + static_cast<int>((monitor.bounds.right - desktop.left) * scale) - 2,
                    originY + static_cast<int>((monitor.bounds.bottom - desktop.top) * scale) - 2};
        const auto background = CreateSolidBrush(monitor.blocked ? RGB(247, 225, 222) : RGB(223, 235, 245));
        FillRect(draw.hDC, &box, background);
        DeleteObject(background);
        FrameRect(draw.hDC, &box, GetSysColorBrush(COLOR_3DSHADOW));
        SetTextColor(draw.hDC, monitor.blocked ? RGB(135, 37, 28) : RGB(25, 60, 90));
        const auto text = DisplayNumber(monitor) + (monitor.blocked ? L"\nBlocked" : L"\nAllowed");
        RECT textBox = box;
        textBox.top += std::max(0L, (Height(box) - 36) / 2);
        DrawTextW(draw.hDC, text.c_str(), -1, &textBox, DT_CENTER | DT_NOPREFIX);
    }
    SelectObject(draw.hDC, oldFont);
}

void FillApplications() {
    const auto list = GetDlgItem(editor->applications, IDC_APP_LIST);
    SendMessageW(list, LB_RESETCONTENT, 0, 0);
    for (const auto& name : editor->draft.applications) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
    EnableWindow(GetDlgItem(editor->applications, IDC_REMOVE_APP), FALSE);
}

void AddApplication(const std::wstring& input) {
    const auto name = ApplicationName(input);
    if (!ValidApplication(name)) {
        MessageBoxW(editor->sheet, L"Enter an executable name such as vlc.exe, or choose an executable with Browse.", L"Application rule", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (!Contains(editor->draft.applications, name)) {
        if (editor->draft.applications.size() >= MaxRules) {
            const auto message = L"You can save up to " + std::to_wstring(MaxRules) + L" application rules. Remove an existing rule before adding another.";
            MessageBoxW(editor->sheet, message.c_str(), L"Application rule limit", MB_OK | MB_ICONINFORMATION);
            return;
        }
        editor->draft.applications.push_back(name);
        std::sort(editor->draft.applications.begin(), editor->draft.applications.end());
        FillApplications();
        Changed(editor->applications);
    }
    SetDlgItemTextW(editor->applications, IDC_APP_NAME, L"");
}

void BrowseApplication() {
    wchar_t path[32768]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = editor->sheet;
    dialog.lpstrFilter = L"Applications (*.exe)\0*.exe\0\0";
    dialog.lpstrFile = path;
    dialog.nMaxFile = static_cast<DWORD>(std::size(path));
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    dialog.lpstrTitle = L"Choose an application";
    if (GetOpenFileNameW(&dialog)) AddApplication(path);
}

void ChooseRunningApplication() {
    const auto applications = RunningApplications();
    const auto menu = CreatePopupMenu();
    if (applications.empty()) AppendMenuW(menu, MF_GRAYED, 0, L"No accessible applications found");
    for (size_t index = 0; index < applications.size(); ++index) AppendMenuW(menu, MF_STRING, index + 1, applications[index].c_str());
    RECT button{};
    GetWindowRect(GetDlgItem(editor->applications, IDC_RUNNING_APP), &button);
    const auto chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, button.left, button.bottom, 0, editor->sheet, nullptr);
    DestroyMenu(menu);
    if (chosen && chosen <= applications.size()) AddApplication(applications[chosen - 1]);
}

bool Apply() {
    if (!editor->dirty) return true;
    if (editor->general) {
        BOOL valid = FALSE;
        const auto interval = GetDlgItemInt(editor->general, IDC_INTERVAL, &valid, FALSE);
        if (!valid || interval < 1 || interval > 30) {
            MessageBoxW(editor->sheet, L"Choose a check interval between 1 and 30 seconds.", L"Check interval", MB_OK | MB_ICONWARNING);
            return false;
        }
        editor->draft.intervalSeconds = interval;
    }
    auto currentMonitors = ReadMonitors();
    ApplyDisplayRules(currentMonitors, editor->draft);
    if (!currentMonitors.empty() && std::all_of(currentMonitors.begin(), currentMonitors.end(), [](const Monitor& monitor) { return monitor.blocked; })) {
        MessageBoxW(editor->sheet, L"At least one connected display must remain allowed. Uncheck a display on the Displays tab.", L"No destination available", MB_OK | MB_ICONWARNING);
        return false;
    }
    std::wstring error;
    if (!editor->apply(editor->draft, editor->startup, error)) {
        MessageBoxW(editor->sheet, error.c_str(), L"Could not save settings", MB_OK | MB_ICONERROR);
        return false;
    }
    editor->dirty = false;
    return true;
}

INT_PTR CommonNotification(HWND page, LPARAM parameter) {
    const auto notification = reinterpret_cast<NMHDR*>(parameter);
    if (notification->code == PSN_APPLY) {
        SetWindowLongPtrW(page, DWLP_MSGRESULT, Apply() ? PSNRET_NOERROR : PSNRET_INVALID_NOCHANGEPAGE);
        return TRUE;
    }
    return FALSE;
}

INT_PTR CALLBACK DisplaysProcedure(HWND page, UINT message, WPARAM parameter, LPARAM extra) {
    if (message == WM_INITDIALOG) {
        editor->displays = page;
        const auto list = GetDlgItem(page, IDC_MONITORS);
        ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        SetWindowTheme(list, L"Explorer", nullptr);
        RECT bounds{};
        GetClientRect(list, &bounds);
        const wchar_t* headings[] = {L"Display", L"Resolution", L"Rule"};
        const int widths[] = {static_cast<int>(Width(bounds) * 64 / 100), static_cast<int>(Width(bounds) * 21 / 100), static_cast<int>(Width(bounds) * 15 / 100 - 4)};
        for (int index = 0; index < 3; ++index) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT | LVCF_WIDTH;
            column.pszText = const_cast<wchar_t*>(headings[index]);
            column.cx = widths[index];
            ListView_InsertColumn(list, index, &column);
        }
        RefreshDisplayPage();
        return TRUE;
    }
    if (message == WM_DRAWITEM && parameter == IDC_MONITOR_MAP) { DrawMap(*reinterpret_cast<DRAWITEMSTRUCT*>(extra)); return TRUE; }
    if (message == WM_COMMAND && !editor->populating) {
        if (LOWORD(parameter) == IDC_REFRESH) RefreshDisplayPage();
        if (LOWORD(parameter) == IDC_CLEAR_DISPLAYS) {
            editor->draft.blockedDisplays.clear();
            editor->draft.destination.clear();
            RefreshDisplayPage();
            Changed(page);
        }
        if (LOWORD(parameter) == IDC_DESTINATION && HIWORD(parameter) == CBN_SELCHANGE) {
            const auto selected = SendDlgItemMessageW(page, IDC_DESTINATION, CB_GETCURSEL, 0, 0);
            if (selected >= 0 && static_cast<size_t>(selected) < editor->destinations.size()) editor->draft.destination = editor->destinations[selected];
            Changed(page);
        }
    }
    if (message == WM_NOTIFY) {
        const auto notification = reinterpret_cast<NMHDR*>(extra);
        if (notification->idFrom == IDC_MONITORS && notification->code == LVN_ITEMCHANGED && !editor->populating) {
            const auto change = reinterpret_cast<NMLISTVIEW*>(extra);
            if (change->iItem >= 0 && static_cast<size_t>(change->iItem) < editor->monitors.size() &&
                ((change->uNewState ^ change->uOldState) & LVIS_STATEIMAGEMASK)) {
                const auto monitor = editor->monitors[change->iItem];
                const bool blocked = ListView_GetCheckState(notification->hwndFrom, change->iItem) != FALSE;
                for (const auto& identity : monitor.identities) {
                    auto& identities = editor->draft.blockedDisplays;
                    identities.erase(std::remove_if(identities.begin(), identities.end(), [&](const auto& value) { return _wcsicmp(value.c_str(), identity.c_str()) == 0; }), identities.end());
                    if (blocked && !identity.empty()) identities.push_back(identity);
                }
                if (blocked && Contains(monitor.identities, editor->draft.destination)) editor->draft.destination.clear();
                RefreshDisplayPage();
                ListView_SetItemState(notification->hwndFrom, change->iItem, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                Changed(page);
            }
        }
        return CommonNotification(page, extra);
    }
    return FALSE;
}

INT_PTR CALLBACK ApplicationsProcedure(HWND page, UINT message, WPARAM parameter, LPARAM extra) {
    if (message == WM_INITDIALOG) {
        editor->applications = page;
        CheckRadioButton(page, IDC_ALL_APPS, IDC_ONLY_APPS, editor->draft.onlyListed ? IDC_ONLY_APPS : IDC_ALL_APPS);
        SendDlgItemMessageW(page, IDC_APP_NAME, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Executable name, e.g. vlc.exe"));
        FillApplications();
        return TRUE;
    }
    if (message == WM_COMMAND) {
        const auto control = LOWORD(parameter);
        if (control == IDC_ALL_APPS || control == IDC_ONLY_APPS) { editor->draft.onlyListed = control == IDC_ONLY_APPS; Changed(page); }
        if (control == IDC_ADD_APP) { wchar_t name[512]{}; GetDlgItemTextW(page, IDC_APP_NAME, name, static_cast<int>(std::size(name))); AddApplication(name); }
        if (control == IDC_BROWSE_APP) BrowseApplication();
        if (control == IDC_RUNNING_APP) ChooseRunningApplication();
        if (control == IDC_APP_LIST && HIWORD(parameter) == LBN_SELCHANGE) EnableWindow(GetDlgItem(page, IDC_REMOVE_APP), TRUE);
        if (control == IDC_REMOVE_APP) {
            const auto selected = SendDlgItemMessageW(page, IDC_APP_LIST, LB_GETCURSEL, 0, 0);
            if (selected >= 0 && static_cast<size_t>(selected) < editor->draft.applications.size()) {
                editor->draft.applications.erase(editor->draft.applications.begin() + selected);
                FillApplications();
                Changed(page);
            }
        }
    }
    if (message == WM_NOTIFY) return CommonNotification(page, extra);
    return FALSE;
}

INT_PTR CALLBACK GeneralProcedure(HWND page, UINT message, WPARAM parameter, LPARAM extra) {
    if (message == WM_INITDIALOG) {
        editor->populating = true;
        editor->general = page;
        CheckDlgButton(page, IDC_STARTUP, editor->startup ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(page, IDC_PARTIAL, editor->draft.partialOverlap ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(page, IDC_RESIZE, editor->draft.resizeOversized ? BST_CHECKED : BST_UNCHECKED);
        SendDlgItemMessageW(page, IDC_INTERVAL_SPIN, UDM_SETRANGE32, 1, 30);
        SetDlgItemInt(page, IDC_INTERVAL, editor->draft.intervalSeconds, FALSE);
        SetDlgItemTextW(page, IDC_CONFIG_LOCATION, editor->path.c_str());
        editor->populating = false;
        return TRUE;
    }
    if (message == WM_COMMAND && !editor->populating) {
        const auto control = LOWORD(parameter);
        if (control == IDC_STARTUP) editor->startup = IsDlgButtonChecked(page, control) == BST_CHECKED;
        if (control == IDC_PARTIAL) editor->draft.partialOverlap = IsDlgButtonChecked(page, control) == BST_CHECKED;
        if (control == IDC_RESIZE) editor->draft.resizeOversized = IsDlgButtonChecked(page, control) == BST_CHECKED;
        if (control == IDC_STARTUP || control == IDC_PARTIAL || control == IDC_RESIZE || (control == IDC_INTERVAL && HIWORD(parameter) == EN_CHANGE)) Changed(page);
    }
    if (message == WM_NOTIFY) return CommonNotification(page, extra);
    return FALSE;
}

}

void ShowSettings(HINSTANCE instance, HWND owner, HICON icon, const Settings& settings, bool startup,
                  const std::wstring& path, ApplySettings apply) {
    if (editor && IsWindow(editor->sheet)) {
        ShowWindow(editor->sheet, SW_RESTORE);
        SetForegroundWindow(editor->sheet);
        return;
    }
    editor = std::make_unique<Editor>();
    editor->draft = settings;
    editor->startup = startup;
    editor->path = path;
    editor->apply = std::move(apply);
    PROPSHEETPAGEW pages[3]{};
    const UINT templates[] = {IDD_DISPLAYS, IDD_APPLICATIONS, IDD_GENERAL};
    const wchar_t* titles[] = {L"Displays", L"Applications", L"General"};
    const DLGPROC procedures[] = {DisplaysProcedure, ApplicationsProcedure, GeneralProcedure};
    for (unsigned index = 0; index < 3; ++index) {
        pages[index].dwSize = sizeof(PROPSHEETPAGEW);
        pages[index].dwFlags = PSP_USETITLE;
        pages[index].hInstance = instance;
        pages[index].pszTemplate = MAKEINTRESOURCEW(templates[index]);
        pages[index].pszTitle = titles[index];
        pages[index].pfnDlgProc = procedures[index];
    }
    PROPSHEETHEADERW header{};
    header.dwSize = sizeof(header);
    header.dwFlags = PSH_PROPSHEETPAGE | PSH_MODELESS | PSH_NOCONTEXTHELP | PSH_USEHICON;
    header.hwndParent = nullptr;
    header.hInstance = instance;
    header.hIcon = icon;
    header.pszCaption = L"Window Guard";
    header.nPages = 3;
    header.ppsp = pages;
    SetLastError(ERROR_SUCCESS);
    const auto result = PropertySheetW(&header);
    if (result <= 0) {
        const auto error = L"The settings window could not be created. Windows error: " + std::to_wstring(GetLastError());
        MessageBoxW(owner, error.c_str(), L"Window Guard", MB_OK | MB_ICONERROR);
        editor.reset();
        return;
    }
    editor->sheet = reinterpret_cast<HWND>(result);
    const auto allowed = Destination({0, 0, 1, 1}, editor->monitors, settings);
    RECT bounds{};
    if (allowed && GetWindowRect(editor->sheet, &bounds)) {
        SetWindowPos(editor->sheet, nullptr, allowed->work.left + (Width(allowed->work) - Width(bounds)) / 2,
                     allowed->work.top + (Height(allowed->work) - Height(bounds)) / 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    }
    ShowWindow(editor->sheet, SW_SHOW);
    SetForegroundWindow(editor->sheet);
}

bool SettingsMessage(MSG& message) {
    if (!editor || !IsWindow(editor->sheet)) return false;
    const auto handled = PropSheet_IsDialogMessage(editor->sheet, &message) != FALSE;
    if (!PropSheet_GetCurrentPageHwnd(editor->sheet)) CloseSettings();
    return handled;
}

void CloseSettings() {
    if (editor && IsWindow(editor->sheet)) DestroyWindow(editor->sheet);
    editor.reset();
}

}
