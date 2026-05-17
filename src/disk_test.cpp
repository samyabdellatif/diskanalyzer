#include "disk_test.h"
#include <windows.h>
#include <winioctl.h>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

static std::wstring NormalizeCommandOutput(const std::wstring& output)
{
    std::wstring normalized;
    normalized.reserve(output.size());
    for (size_t i = 0; i < output.size(); ++i)
    {
        if (output[i] == L'\r')
            continue;
        normalized.push_back(output[i]);
    }
    return normalized;
}

static bool RunCommandCaptureOutput(const std::wstring& command, std::wstring& output)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hRead = nullptr;
    HANDLE hWrite = nullptr;
    if (!CreatePipe(&hRead, &hWrite, &sa, 0))
        return false;
    if (!SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::wstring commandLine = command;
    if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(hRead);
        CloseHandle(hWrite);
        return false;
    }

    CloseHandle(hWrite);

    std::string buffer;
    char readBuffer[4096];
    DWORD bytesRead = 0;
    while (ReadFile(hRead, readBuffer, static_cast<DWORD>(sizeof(readBuffer) - 1), &bytesRead, nullptr) && bytesRead > 0)
    {
        readBuffer[bytesRead] = '\0';
        buffer.append(readBuffer, bytesRead);
    }

    CloseHandle(hRead);
    WaitForSingleObject(pi.hProcess, 120000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    int length = MultiByteToWideChar(CP_OEMCP, 0, buffer.c_str(), -1, nullptr, 0);
    if (length > 0)
    {
        output.resize(length - 1);
        MultiByteToWideChar(CP_OEMCP, 0, buffer.c_str(), -1, output.data(), length);
    }
    else
    {
        output.clear();
    }

    output = NormalizeCommandOutput(output);
    return true;
}

static bool GetPhysicalDiskNumberFromDevicePath(const std::wstring& devicePath, DWORD& diskNumber)
{
    HANDLE hDisk = CreateFileW(devicePath.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDisk == INVALID_HANDLE_VALUE)
        return false;

    STORAGE_DEVICE_NUMBER deviceNumber = {};
    DWORD returned = 0;
    bool success = DeviceIoControl(hDisk, IOCTL_STORAGE_GET_DEVICE_NUMBER, nullptr, 0,
        &deviceNumber, static_cast<DWORD>(sizeof(deviceNumber)), &returned, nullptr) == TRUE;
    CloseHandle(hDisk);

    if (!success)
        return false;

    diskNumber = deviceNumber.DeviceNumber;
    return true;
}

static std::wstring FindDriveLetterOnDisk(const std::wstring& devicePath)
{
    DWORD diskNumber = 0;
    if (!GetPhysicalDiskNumberFromDevicePath(devicePath, diskNumber))
        return {};

    DWORD drives = GetLogicalDrives();
    if (drives == 0)
        return {};

    for (int letter = 0; letter < 26; ++letter)
    {
        if (!(drives & (1u << letter)))
            continue;

        wchar_t volumePath[8] = {};
        swprintf_s(volumePath, L"\\.\\%c:", L'A' + letter);

        HANDLE hVolume = CreateFileW(volumePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr, OPEN_EXISTING, 0, nullptr);
        if (hVolume == INVALID_HANDLE_VALUE)
            continue;

        BYTE buffer[sizeof(VOLUME_DISK_EXTENTS) + 16 * sizeof(DISK_EXTENT)] = {};
        DWORD returned = 0;
        if (DeviceIoControl(hVolume, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0,
            buffer, static_cast<DWORD>(sizeof(buffer)), &returned, nullptr))
        {
            auto* extents = reinterpret_cast<VOLUME_DISK_EXTENTS*>(buffer);
            if (extents->NumberOfDiskExtents > 0 && extents->NumberOfDiskExtents <= 16)
            {
                for (DWORD index = 0; index < extents->NumberOfDiskExtents; ++index)
                {
                    if (extents->Extents[index].DiskNumber == diskNumber)
                    {
                        CloseHandle(hVolume);
                        return std::wstring(1, L'A' + letter);
                    }
                }
            }
        }

        CloseHandle(hVolume);
    }

    return {};
}

static bool AddParsedWinSATResults(const std::wstring& winsatOutput, std::vector<DiskTestResult>& results)
{
    std::wistringstream stream(winsatOutput);
    std::wstring line;
    bool found = false;

    auto trim = [](std::wstring text) {
        while (!text.empty() && std::iswspace(text.back()))
            text.pop_back();
        while (!text.empty() && std::iswspace(text.front()))
            text.erase(text.begin());
        return text;
    };

    while (std::getline(stream, line))
    {
        if (line.empty())
            continue;

        line = trim(line);
        if (line.rfind(L"Disk  Random 16.0 Read", 0) == 0 || line.rfind(L"Disk Random 16.0 Read", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Random 16.0 Read";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
        else if (line.rfind(L"Disk  Sequential 64.0 Read", 0) == 0 || line.rfind(L"Disk Sequential 64.0 Read", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Sequential 64.0 Read";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
        else if (line.rfind(L"Disk  Sequential 64.0 Write", 0) == 0 || line.rfind(L"Disk Sequential 64.0 Write", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Sequential 64.0 Write";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
        else if (line.rfind(L"Latency: 95th Percentile", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Latency 95th Percentile";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
        else if (line.rfind(L"Latency: Maximum", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Latency Maximum";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
        else if (line.rfind(L"Average Read Time with Sequential Writes", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Avg Read Time Seq Writes";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
        else if (line.rfind(L"Average Read Time with Random Writes", 0) == 0)
        {
            DiskTestResult metric;
            metric.testName = L"WinSAT Avg Read Time Random Writes";
            metric.result = trim(line.substr(line.find_first_of(L"0123456789")));
            results.push_back(metric);
            found = true;
        }
    }

    return found;
}

static bool RunWinSATCommand(const std::wstring& driveLetter, std::wstring& result)
{
    if (driveLetter.empty())
    {
        result = L"No mounted drive found on this physical disk for WinSAT.";
        return false;
    }

    std::wstring command = L"cmd.exe /C winsat disk -drive " + driveLetter;
    if (!RunCommandCaptureOutput(command, result) || result.empty())
    {
        result = L"WinSAT not available, returned no output, or failed.";
        return false;
    }

    return true;
}

static bool QueryDiskPerformance(HANDLE diskHandle, std::vector<DiskTestResult>& results)
{
    DISK_PERFORMANCE before = {};
    DISK_PERFORMANCE after = {};
    DWORD returned = 0;

    if (!DeviceIoControl(diskHandle, IOCTL_DISK_PERFORMANCE, nullptr, 0, &before, sizeof(before), &returned, nullptr))
        return false;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    if (!DeviceIoControl(diskHandle, IOCTL_DISK_PERFORMANCE, nullptr, 0, &after, sizeof(after), &returned, nullptr))
        return false;

    ULONGLONG beforeReadBytes = (static_cast<ULONGLONG>(static_cast<ULONG>(before.BytesRead.HighPart)) << 32) |
        static_cast<ULONGLONG>(before.BytesRead.LowPart);
    ULONGLONG afterReadBytes = (static_cast<ULONGLONG>(static_cast<ULONG>(after.BytesRead.HighPart)) << 32) |
        static_cast<ULONGLONG>(after.BytesRead.LowPart);
    ULONGLONG readBytes = afterReadBytes - beforeReadBytes;

    ULONGLONG beforeWriteBytes = (static_cast<ULONGLONG>(static_cast<ULONG>(before.BytesWritten.HighPart)) << 32) |
        static_cast<ULONGLONG>(before.BytesWritten.LowPart);
    ULONGLONG afterWriteBytes = (static_cast<ULONGLONG>(static_cast<ULONG>(after.BytesWritten.HighPart)) << 32) |
        static_cast<ULONGLONG>(after.BytesWritten.LowPart);
    ULONGLONG writeBytes = afterWriteBytes - beforeWriteBytes;

    ULONGLONG beforeReadTime = (static_cast<ULONGLONG>(static_cast<ULONG>(before.ReadTime.HighPart)) << 32) |
        static_cast<ULONGLONG>(before.ReadTime.LowPart);
    ULONGLONG afterReadTime = (static_cast<ULONGLONG>(static_cast<ULONG>(after.ReadTime.HighPart)) << 32) |
        static_cast<ULONGLONG>(after.ReadTime.LowPart);
    ULONGLONG readTime = afterReadTime - beforeReadTime;

    ULONGLONG beforeWriteTime = (static_cast<ULONGLONG>(static_cast<ULONG>(before.WriteTime.HighPart)) << 32) |
        static_cast<ULONGLONG>(before.WriteTime.LowPart);
    ULONGLONG afterWriteTime = (static_cast<ULONGLONG>(static_cast<ULONG>(after.WriteTime.HighPart)) << 32) |
        static_cast<ULONGLONG>(after.WriteTime.LowPart);
    ULONGLONG writeTime = afterWriteTime - beforeWriteTime;

    if (readBytes == 0 && writeBytes == 0 && readTime == 0 && writeTime == 0)
    {
        DiskTestResult perfSample;
        perfSample.testName = L"Disk performance sample";
        perfSample.result = L"No activity captured from IOCTL_DISK_PERFORMANCE during the sample interval. Use WinSAT for accurate performance metrics.";
        results.push_back(perfSample);
        return true;
    }

    DiskTestResult perfRead;
    perfRead.testName = L"Disk read bytes (sample)";
    perfRead.result = std::to_wstring(readBytes) + L" bytes";
    results.push_back(perfRead);

    DiskTestResult perfWrite;
    perfWrite.testName = L"Disk write bytes (sample)";
    perfWrite.result = std::to_wstring(writeBytes) + L" bytes";
    results.push_back(perfWrite);

    DiskTestResult readTimeResult;
    readTimeResult.testName = L"Disk read time (sample)";
    readTimeResult.result = std::to_wstring(readTime / 10000) + L" ms";
    results.push_back(readTimeResult);

    DiskTestResult writeTimeResult;
    writeTimeResult.testName = L"Disk write time (sample)";
    writeTimeResult.result = std::to_wstring(writeTime / 10000) + L" ms";
    results.push_back(writeTimeResult);

    return true;
}

static bool QuerySmartSupport(HANDLE diskHandle, std::vector<DiskTestResult>& results)
{
    DWORD returned = 0;
    DWORD version = 0;
    if (DeviceIoControl(diskHandle, SMART_GET_VERSION, nullptr, 0, &version, sizeof(version), &returned, nullptr))
    {
        DiskTestResult smartResult;
        smartResult.testName = L"SMART support";
        smartResult.result = L"Supported";
        results.push_back(smartResult);
        return true;
    }

    DiskTestResult smartResult;
    smartResult.testName = L"SMART support";
    smartResult.result = L"Not available";
    results.push_back(smartResult);
    return false;
}

static std::wstring EscapeWmicDeviceId(const std::wstring& devicePath)
{
    std::wstring escaped;
    escaped.reserve(devicePath.size() * 2);
    for (wchar_t c : devicePath)
    {
        if (c == L'\\')
            escaped += L"\\\\";
        else
            escaped.push_back(c);
    }
    return escaped;
}

static void QueryWmicDiskDriveInfo(const std::wstring& devicePath, std::vector<DiskTestResult>& results)
{
    std::wstring command = L"cmd.exe /C wmic diskdrive get Caption,Model,Manufacturer,Partitions,Size,SerialNumber,Status,DeviceID /format:list";
    std::wstring output;
    if (RunCommandCaptureOutput(command, output) && !output.empty())
    {
        DiskTestResult wmicTest;
        wmicTest.testName = L"WMIC diskdrive info";
        wmicTest.result = output;
        results.push_back(wmicTest);
    }
    else
    {
        DiskTestResult wmicTest;
        wmicTest.testName = L"WMIC diskdrive info";
        wmicTest.result = L"Failed or unavailable.";
        results.push_back(wmicTest);
    }
}

static void QueryFsutilInfo(std::vector<DiskTestResult>& results)
{
    std::wstring output;
    if (RunCommandCaptureOutput(L"cmd.exe /C fsutil fsinfo drives", output) && !output.empty())
    {
        DiskTestResult fsutilTest;
        fsutilTest.testName = L"FSUTIL drives";
        fsutilTest.result = output;
        results.push_back(fsutilTest);
    }
    else
    {
        DiskTestResult fsutilTest;
        fsutilTest.testName = L"FSUTIL drives";
        fsutilTest.result = L"Failed or unavailable.";
        results.push_back(fsutilTest);
    }
}

std::vector<DiskTestResult> AnalyzeDisk(const DiskInfo& disk)
{
    std::vector<DiskTestResult> results;

    DiskTestResult pathTest;
    pathTest.testName = L"Device Path";
    pathTest.result = disk.path;
    results.push_back(pathTest);

    DiskTestResult nameTest;
    nameTest.testName = L"Disk Name";
    nameTest.result = disk.name;
    results.push_back(nameTest);

    DISK_GEOMETRY_EX geometry = {};
    HANDLE hDisk = CreateFileW(disk.path.c_str(), GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hDisk == INVALID_HANDLE_VALUE)
    {
        DiskTestResult failTest;
        failTest.testName = L"Open Physical Disk";
        failTest.result = L"Failed to access physical disk.";
        results.push_back(failTest);
        return results;
    }

    DiskTestResult openTest;
    openTest.testName = L"Open Physical Disk";
    openTest.result = L"Accessible";
    results.push_back(openTest);

    if (DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX, nullptr, 0,
        &geometry, static_cast<DWORD>(sizeof(geometry)), nullptr, nullptr))
    {
        DiskTestResult sizeTest;
        sizeTest.testName = L"Physical Disk Size";
        sizeTest.result = std::to_wstring(geometry.DiskSize.QuadPart / (1024ULL * 1024ULL)) + L" MB";
        results.push_back(sizeTest);
    }
    else
    {
        DiskTestResult sizeTest;
        sizeTest.testName = L"Physical Disk Size";
        sizeTest.result = L"Unable to query size.";
        results.push_back(sizeTest);
    }

    QuerySmartSupport(hDisk, results);
    QueryDiskPerformance(hDisk, results);
    QueryWmicDiskDriveInfo(disk.path, results);
    QueryFsutilInfo(results);

    std::wstring driveLetter = FindDriveLetterOnDisk(disk.path);
    std::wstring winsatResult;
    if (RunWinSATCommand(driveLetter, winsatResult))
    {
        if (!AddParsedWinSATResults(winsatResult, results))
        {
            DiskTestResult winsatTest;
            winsatTest.testName = L"WinSAT disk assessment";
            winsatTest.result = L"Drive " + driveLetter + L" assessment completed. Raw output may not have parsed metrics.";
            results.push_back(winsatTest);

            DiskTestResult rawOutputTest;
            rawOutputTest.testName = L"WinSAT raw output";
            rawOutputTest.result = winsatResult;
            results.push_back(rawOutputTest);
        }
    }
    else
    {
        DiskTestResult winsatTest;
        winsatTest.testName = L"WinSAT disk assessment";
        winsatTest.result = winsatResult;
        results.push_back(winsatTest);
    }

    CloseHandle(hDisk);
    return results;
}
