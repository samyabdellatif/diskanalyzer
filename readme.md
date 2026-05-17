# Disk Analyzer

Disk Analyzer is a native Windows utility written in C++ that uses Win32 APIs, SetupAPI, and built-in Windows commands to enumerate physical disks, run diagnostics, and display health and performance results.

## Features

- Enumerates physical disks, including internal and external drives
- Shows friendly disk names, model, serial, and physical device path
- Runs SMART availability checks via `DeviceIoControl`
- Gathers disk performance samples from `IOCTL_DISK_PERFORMANCE`
- Executes `WinSAT disk` and parses disk benchmark metrics
- Uses `wmic` and `fsutil` to collect additional disk info
- Displays results in a lightweight native Win32 GUI
- Requests administrator privileges via embedded application manifest

## Build

Required:

- Windows 10/11 with Windows SDK
- CMake
- Visual Studio / MSVC toolset

Build steps:

```powershell
cd C:\Users\Samy\gitRepos\diskanalyzer
cmake -S . -B build
cmake --build build --config Release
```

## Run

After building, run the executable from `build\Release\DiskAnalyzer.exe`.

## Notes

- The app uses `build/` for generated build files.
- A `.gitignore` is included to exclude the build output.
