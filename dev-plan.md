# Disk Analyzer Development Plan

## Overview
Disk Analyzer is a lightweight Windows utility written in C++ using only native Windows APIs and libraries. It will discover installed disks (internal and external), provide a minimal GUI for user interaction, run disk tests, and display disk status and health.

## Goals
- Collect information for all installed disks
- Support internal and external disks
- Provide a simple native Windows GUI
- Allow disk selection and test execution via buttons
- Display test results and disk health information
- Use no external dependencies

## Architecture
1. Core disk management
   - Disk enumeration
   - Disk property retrieval
   - Health and status analysis
   - Test execution
2. GUI layer
   - Native Windows window(s)
   - Disk list control
   - Buttons for actions
   - Result display area
3. Integration
   - Connect GUI actions to disk manager functions
   - Update UI based on selected disk and test results

## Components

### 1. Disk Enumeration and Info Gathering
- Use Windows SetupAPI, WMI, or DeviceIoControl for disk detection
- Identify physical disks and removable disks
- Gather disk metadata:
  - disk name / device path
  - model and manufacturer
  - size and partition information
  - serial number
  - bus type / removable flag
- Build a data structure to represent each disk

### 2. Disk Health and Status Analysis
- Query SMART data via `DeviceIoControl` and `SMART_RCV_DRIVE_DATA`
- Extract health indicators such as:
  - overall health status
  - temperature
  - reallocated sectors count
  - pending sectors
  - read/write error rates
- If SMART is unavailable, derive simple status from disk presence and Windows drive state

### 3. Disk Tests
- Define core tests to run on a selected disk:
  - SMART status check
  - quick read test for target disk sectors
  - optional surface or file-based access test
- Implement tests using native Windows I/O APIs
- Report results as pass/fail plus details

### 4. GUI Implementation
- Create a native Win32 application using `WinMain`, `CreateWindowEx`, and the Windows message loop
- Layout controls:
  - ListBox or ListView for disk selection
  - Buttons: `Refresh`, `Run Health Test`, `Show Details`
  - Static text or edit control to show selected disk details and test output
- Handle user actions:
  - refresh disk list
  - select disk from the list
  - run tests on the selected disk
  - display results
- Keep UI responsive by using worker threads for long-running tests if needed

### 5. Project and Build
- Use Visual Studio solution / project or a simple MSVC makefile setup
- Keep source files organized:
  - `main.cpp` for app entry and GUI setup
  - `disk_manager.cpp/h` for enumeration and health logic
  - `disk_test.cpp/h` for test execution
  - `ui.cpp/h` for UI helpers and control callbacks
- Use only native Windows headers (`windows.h`, `setupapi.h`, `winioctl.h`, etc.)

## Implementation Steps

### Step 1: Setup Project
- Create a new C++ Windows desktop application project
- Configure target to use the Windows SDK only
- Add source files and headers

### Step 2: Implement Disk Enumeration
- Write code to enumerate disks with native APIs
- Populate a list of disk objects
- Verify detection of internal and external disks

### Step 3: Implement Disk Info Retrieval
- Add functions to fetch disk metadata and size
- Test information retrieval in a console or log output

### Step 4: Implement SMART and Health Analysis
- Integrate SMART query support for supported drives
- Parse basic health attributes
- Add fallback status when SMART is unavailable

### Step 5: Build the GUI Skeleton
- Create the main window and controls
- Display the disk list and allow selection
- Wire up basic refresh action

### Step 6: Link GUI and Disk Logic
- Populate the UI list from disk enumeration
- Show selected disk details when chosen
- Enable or disable buttons based on disk selection

### Step 7: Add Test Execution and Result Display
- Implement button handling for running tests
- Execute health checks and format results
- Display output in the UI

### Step 8: Polish and QA
- Validate behavior for internal, external, and unresponsive disks
- Ensure UI remains responsive during operations
- Confirm there are no external dependencies
- Test on Windows 10/11

## File Outline
- `dev-plan.md` — implementation plan
- `main.cpp` — application entry, message loop, window creation
- `disk_manager.h` / `disk_manager.cpp` — disk enumeration and info functions
- `disk_test.h` / `disk_test.cpp` — health analysis and test logic
- `ui.h` / `ui.cpp` — Win32 control helpers and event handling
- Optional: `resource.h` / `.rc` for icons, strings, and layout

## Notes
- Keep the UI minimal and native
- Prioritize correctness and low resource usage
- Avoid non-Windows libraries entirely
- Focus on essential disk health features first, with additional tests as optional enhancements
