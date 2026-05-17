#include "ui.h"
#include <commctrl.h>
#include <string>

#pragma comment(lib, "comctl32.lib")

bool RegisterMainWindowClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DiskAnalyzerMainWindow";

    return RegisterClassExW(&wc) != 0;
}

HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow)
{
    INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
    InitCommonControlsEx(&icex);

    HWND hwnd = CreateWindowExW(
        0,
        L"DiskAnalyzerMainWindow",
        L"Disk Analyzer",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        620,
        420,
        nullptr,
        nullptr,
        hInstance,
        nullptr);

    if (!hwnd)
        return nullptr;

    CreateWindowW(L"STATIC", L"Select drive:", WS_VISIBLE | WS_CHILD, 20, 20, 100, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"COMBOBOX", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST, 120, 18, 220, 200, hwnd, reinterpret_cast<HMENU>(ID_COMBO_DISKS), hInstance, nullptr);
    CreateWindowW(L"BUTTON", L"Analyze", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 360, 18, 100, 24, hwnd, reinterpret_cast<HMENU>(ID_BUTTON_ANALYZE), hInstance, nullptr);
    CreateWindowW(L"STATIC", L"Status:", WS_VISIBLE | WS_CHILD, 20, 60, 100, 20, hwnd, nullptr, hInstance, nullptr);
    CreateWindowW(L"STATIC", L"Loading disks...", WS_VISIBLE | WS_CHILD, 80, 60, 460, 20, hwnd, reinterpret_cast<HMENU>(ID_STATUS), hInstance, nullptr);

    HWND hListView = CreateWindowW(WC_LISTVIEW, nullptr,
        WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | WS_BORDER,
        20, 100, 560, 250, hwnd, reinterpret_cast<HMENU>(ID_LISTVIEW_RESULTS), hInstance, nullptr);

    ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

    LVCOLUMNW col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    col.cx = 240;
    col.pszText = const_cast<LPWSTR>(L"Test");
    ListView_InsertColumn(hListView, 0, &col);
    col.cx = 280;
    col.pszText = const_cast<LPWSTR>(L"Result");
    ListView_InsertColumn(hListView, 1, &col);

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);
    return hwnd;
}

void PopulateDiskCombo(HWND combo, const std::vector<DiskInfo>& disks)
{
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < disks.size(); ++i)
    {
        int index = SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(disks[i].name.c_str()));
        SendMessageW(combo, CB_SETITEMDATA, index, static_cast<LPARAM>(i));
    }
    if (!disks.empty())
    {
        SendMessageW(combo, CB_SETCURSEL, 0, 0);
    }
}

void ShowAnalysisResults(HWND listView, const std::vector<DiskTestResult>& results)
{
    ListView_DeleteAllItems(listView);
    for (size_t i = 0; i < results.size(); ++i)
    {
        LVITEMW item = {};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = const_cast<LPWSTR>(results[i].testName.c_str());
        ListView_InsertItem(listView, &item);

        ListView_SetItemText(listView, static_cast<int>(i), 1, const_cast<LPWSTR>(results[i].result.c_str()));
    }
}
