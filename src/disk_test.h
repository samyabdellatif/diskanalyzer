#pragma once

#include <string>
#include <vector>
#include "disk_manager.h"

struct DiskTestResult
{
    std::wstring testName;
    std::wstring result;
};

std::vector<DiskTestResult> AnalyzeDisk(const DiskInfo& disk);
