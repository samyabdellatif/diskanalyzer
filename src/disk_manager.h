#pragma once

#include <string>
#include <vector>

struct DiskInfo
{
    std::wstring name;
    std::wstring path;
    std::wstring serialNumber;
};

class DiskManager
{
public:
    static std::vector<DiskInfo> LoadDisks();
};
