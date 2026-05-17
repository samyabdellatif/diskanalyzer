#include "disk_manager.h"
#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
#include <vector>
#include <string>

static const GUID GUID_DEVINTERFACE_DISK =
{ 0x53f56307, 0xb6bf, 0x11d0, {0x94, 0xf2, 0x00, 0xa0, 0xc9, 0x1e, 0xfb, 0x8b} };

// Convert an ANSI string returned by device descriptors into a UTF-16 wide string.
static std::wstring ToWide(const char* text)
{
    if (text == nullptr || *text == '\0')
    {
        return L"";
    }

    int length = MultiByteToWideChar(CP_ACP, 0, text, -1, nullptr, 0);
    if (length <= 0)
    {
        return L"";
    }

    std::wstring result(length, L'\0');
    MultiByteToWideChar(CP_ACP, 0, text, -1, result.data(), length);
    result.resize(length - 1);
    return result;
}

// Read a registry property string from the device information set.
static std::wstring QueryDeviceProperty(HDEVINFO deviceInfoSet, PSP_DEVINFO_DATA devInfoData, DWORD property)
{
    wchar_t buffer[512] = {};
    DWORD requiredSize = 0;

    if (SetupDiGetDeviceRegistryPropertyW(deviceInfoSet, devInfoData, property, nullptr,
        reinterpret_cast<PBYTE>(buffer), static_cast<DWORD>(sizeof(buffer)), &requiredSize))
    {
        return buffer;
    }

    return L"";
}

// Query the storage descriptor for device model and serial number.
static void QueryStorageDescriptor(HANDLE deviceHandle, std::wstring& model, std::wstring& serial)
{
    STORAGE_PROPERTY_QUERY query = {};
    query.PropertyId = StorageDeviceProperty;
    query.QueryType = PropertyStandardQuery;

    BYTE descriptorBuffer[1024] = {};
    DWORD returned = 0;
    if (!DeviceIoControl(deviceHandle, IOCTL_STORAGE_QUERY_PROPERTY,
        &query, static_cast<DWORD>(sizeof(query)), descriptorBuffer,
        static_cast<DWORD>(sizeof(descriptorBuffer)), &returned, nullptr))
    {
        return;
    }

    auto descriptor = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(descriptorBuffer);
    if (descriptor->ProductIdOffset != 0 && descriptor->ProductIdOffset < returned)
    {
        model = ToWide(reinterpret_cast<char*>(descriptorBuffer + descriptor->ProductIdOffset));
    }
    if (descriptor->SerialNumberOffset != 0 && descriptor->SerialNumberOffset < returned)
    {
        serial = ToWide(reinterpret_cast<char*>(descriptorBuffer + descriptor->SerialNumberOffset));
    }
}

// Query the physical disk size from the disk geometry ioctl.
static bool QueryDiskSize(HANDLE deviceHandle, ULONGLONG& sizeBytes)
{
    DISK_GEOMETRY_EX geometry = {};
    DWORD returned = 0;
    if (!DeviceIoControl(deviceHandle, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
        nullptr, 0, &geometry, static_cast<DWORD>(sizeof(geometry)), &returned, nullptr))
    {
        return false;
    }

    sizeBytes = geometry.DiskSize.QuadPart;
    return true;
}

// Enumerate all physical disk devices and collect friendly names, model, serial, and device path.
std::vector<DiskInfo> DiskManager::LoadDisks()
{
    std::vector<DiskInfo> disks;
    HDEVINFO deviceInfoSet = SetupDiGetClassDevsW(&GUID_DEVINTERFACE_DISK, nullptr, nullptr,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (deviceInfoSet == INVALID_HANDLE_VALUE)
    {
        return disks;
    }

    SP_DEVICE_INTERFACE_DATA interfaceData = {};
    interfaceData.cbSize = sizeof(interfaceData);
    for (DWORD index = 0; SetupDiEnumDeviceInterfaces(deviceInfoSet, nullptr, &GUID_DEVINTERFACE_DISK, index, &interfaceData); ++index)
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, nullptr, 0, &requiredSize, nullptr);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            continue;

        std::vector<BYTE> detailBuffer(requiredSize);
        auto* detailData = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(detailBuffer.data());
        detailData->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);

        SP_DEVINFO_DATA devInfoData = {};
        devInfoData.cbSize = sizeof(devInfoData);
        if (!SetupDiGetDeviceInterfaceDetailW(deviceInfoSet, &interfaceData, detailData, requiredSize, nullptr, &devInfoData))
            continue;

        std::wstring devicePath = detailData->DevicePath;
        std::wstring friendlyName = QueryDeviceProperty(deviceInfoSet, &devInfoData, SPDRP_FRIENDLYNAME);
        if (friendlyName.empty())
        {
            friendlyName = QueryDeviceProperty(deviceInfoSet, &devInfoData, SPDRP_DEVICEDESC);
        }

        std::wstring model;
        std::wstring serial;
        HANDLE hDevice = CreateFileW(devicePath.c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDevice != INVALID_HANDLE_VALUE)
        {
            QueryStorageDescriptor(hDevice, model, serial);
            CloseHandle(hDevice);
        }

        DiskInfo disk;
        disk.name = friendlyName.empty() ? devicePath : friendlyName;
        if (!model.empty())
        {
            disk.name += L" (" + model + L")";
        }
        disk.path = devicePath;
        disk.serialNumber = serial;
        disks.push_back(disk);
    }

    SetupDiDestroyDeviceInfoList(deviceInfoSet);
    return disks;
}
