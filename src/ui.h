#pragma once

#include <windows.h>
#include <vector>
#include "disk_manager.h"
#include "disk_test.h"

struct UIHandles
{
    HWND hComboDisks;
    HWND hButtonAnalyze;
    HWND hStatus;
    HWND hResults;
};

constexpr UINT ID_COMBO_DISKS = 101;
constexpr UINT ID_BUTTON_ANALYZE = 102;
constexpr UINT ID_STATUS = 103;
constexpr UINT ID_LISTVIEW_RESULTS = 104;

bool RegisterMainWindowClass(HINSTANCE hInstance);
HWND CreateMainWindow(HINSTANCE hInstance, int nCmdShow);
void PopulateDiskCombo(HWND combo, const std::vector<DiskInfo>& disks);
void ShowAnalysisResults(HWND listView, const std::vector<DiskTestResult>& results);
