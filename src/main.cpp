#include <windows.h>
#include <commctrl.h>
#include <thread>
#include <vector>
#include <mutex>
#include "disk_manager.h"
#include "disk_test.h"
#include "ui.h"

constexpr UINT WM_APP_DISKS_LOADED = WM_APP + 1;
constexpr UINT WM_APP_ANALYSIS_COMPLETE = WM_APP + 2;

static std::vector<DiskInfo> g_disks;
static std::mutex g_disksMutex;
static UIHandles g_handles = {};

LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        INITCOMMONCONTROLSEX icex = { sizeof(icex), ICC_LISTVIEW_CLASSES };
        InitCommonControlsEx(&icex);

        CreateWindowW(L"STATIC", L"Select drive:", WS_VISIBLE | WS_CHILD, 20, 20, 100, 20, hwnd, reinterpret_cast<HMENU>(nullptr), reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance, nullptr);
        g_handles.hComboDisks = CreateWindowW(L"COMBOBOX", nullptr, WS_VISIBLE | WS_CHILD | WS_BORDER | CBS_DROPDOWNLIST,
            120, 18, 220, 200, hwnd, reinterpret_cast<HMENU>(MAKEINTRESOURCEW(ID_COMBO_DISKS)), reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance, nullptr);
        g_handles.hButtonAnalyze = CreateWindowW(L"BUTTON", L"Analyze", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            360, 18, 100, 24, hwnd, reinterpret_cast<HMENU>(MAKEINTRESOURCEW(ID_BUTTON_ANALYZE)), reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance, nullptr);
        CreateWindowW(L"STATIC", L"Status:", WS_VISIBLE | WS_CHILD, 20, 60, 100, 20, hwnd, reinterpret_cast<HMENU>(nullptr), reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance, nullptr);
        g_handles.hStatus = CreateWindowW(L"STATIC", L"Loading disks...", WS_VISIBLE | WS_CHILD, 80, 60, 460, 20, hwnd, reinterpret_cast<HMENU>(MAKEINTRESOURCEW(ID_STATUS)), reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance, nullptr);

        g_handles.hResults = CreateWindowW(WC_LISTVIEW, nullptr,
            WS_VISIBLE | WS_CHILD | WS_BORDER | WS_HSCROLL | WS_VSCROLL | LVS_REPORT | LVS_SHOWSELALWAYS,
            20, 100, 560, 250, hwnd, reinterpret_cast<HMENU>(MAKEINTRESOURCEW(ID_LISTVIEW_RESULTS)), reinterpret_cast<LPCREATESTRUCT>(lParam)->hInstance, nullptr);
        ListView_SetExtendedListViewStyle(g_handles.hResults, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | LVS_EX_DOUBLEBUFFER);

        LVCOLUMNW col = {};
        col.mask = LVCF_TEXT | LVCF_WIDTH;
        col.cx = 220;
        col.pszText = const_cast<LPWSTR>(L"Test");
        ListView_InsertColumn(g_handles.hResults, 0, &col);
        col.cx = 340;
        col.pszText = const_cast<LPWSTR>(L"Result");
        ListView_InsertColumn(g_handles.hResults, 1, &col);

        EnableWindow(g_handles.hButtonAnalyze, FALSE);
        SetWindowTextW(g_handles.hStatus, L"Loading disks...");

        std::thread loadThread([hwnd]() {
            auto disks = DiskManager::LoadDisks();
            {
                std::scoped_lock lock(g_disksMutex);
                g_disks = std::move(disks);
            }
            PostMessageW(hwnd, WM_APP_DISKS_LOADED, 0, 0);
        });
        loadThread.detach();
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BUTTON_ANALYZE)
        {
            int selected = static_cast<int>(SendMessageW(g_handles.hComboDisks, CB_GETCURSEL, 0, 0));
            std::vector<DiskInfo> disksCopy;
            {
                std::scoped_lock lock(g_disksMutex);
                disksCopy = g_disks;
            }

            if (selected >= 0 && selected < static_cast<int>(disksCopy.size()))
            {
                EnableWindow(g_handles.hButtonAnalyze, FALSE);
                SetWindowTextW(g_handles.hStatus, L"Running tests...");

                DiskInfo selectedDisk = disksCopy[selected];
                std::thread analysisThread([hwnd, selectedDisk]() {
                    auto results = AnalyzeDisk(selectedDisk);
                    auto* data = new std::vector<DiskTestResult>(std::move(results));
                    PostMessageW(hwnd, WM_APP_ANALYSIS_COMPLETE, reinterpret_cast<WPARAM>(data), 0);
                });
                analysisThread.detach();
            }
        }
        return 0;
    case WM_APP_DISKS_LOADED:
    {
        std::vector<DiskInfo> disksCopy;
        {
            std::scoped_lock lock(g_disksMutex);
            disksCopy = g_disks;
        }
        PopulateDiskCombo(g_handles.hComboDisks, disksCopy);
        if (!disksCopy.empty())
        {
            SetWindowTextW(g_handles.hStatus, L"Select a disk and click Analyze.");
            EnableWindow(g_handles.hButtonAnalyze, TRUE);
        }
        else
        {
            SetWindowTextW(g_handles.hStatus, L"No disks detected.");
        }
        return 0;
    }
    case WM_APP_ANALYSIS_COMPLETE:
    {
        auto* results = reinterpret_cast<std::vector<DiskTestResult>*>(wParam);
        if (results)
        {
            ShowAnalysisResults(g_handles.hResults, *results);
            delete results;
        }
        SetWindowTextW(g_handles.hStatus, L"Analysis complete.");
        EnableWindow(g_handles.hButtonAnalyze, TRUE);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DiskAnalyzerMainWindow";

    if (!RegisterClassExW(&wc))
        return 0;

    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Disk Analyzer", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 620, 420,
        nullptr, nullptr, hInstance, nullptr);

    if (!hwnd)
        return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    MSG msg = {};
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    return static_cast<int>(msg.wParam);
}
