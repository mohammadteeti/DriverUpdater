# 🚀 DriverUpdater

> Intelligent Driver Detection and Automated Update Management for Windows Systems

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub Repository](https://img.shields.io/badge/GitHub-DriverUpdater-blue)](https://github.com/mohammadteeti/DriverUpdater)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows-0078d4)](https://www.microsoft.com/en-us/windows)
[![Language: C++](https://img.shields.io/badge/Language-C%2B%2B-00599c)](https://en.wikipedia.org/wiki/C%2B%2B)

## 📋 Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Recent Updates](#recent-updates)
- [System Requirements](#system-requirements)
- [Building & Installation](#building--installation)
- [Quick Start](#quick-start)
- [Technical Architecture](#technical-architecture)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

---

## 🎯 Overview

**DriverUpdater** is a sophisticated native Windows utility built in C++ that streamlines driver management on your system. It automatically scans your machine for installed hardware devices, identifies outdated drivers, retrieves manufacturer-specific updates, and manages the installation process with minimal user intervention.

Whether you're maintaining a single workstation or managing a fleet of computers, DriverUpdater helps ensure optimal hardware compatibility and system performance.

### The Problem It Solves
- 🔍 **Manual driver hunting** - No more searching through manufacturer websites
- ⚠️ **System instability** - Outdated drivers can cause crashes and performance issues
- ⏰ **Time-consuming updates** - Bulk update management is tedious and error-prone
- 🖥️ **Hardware compatibility** - Automatically ensures compatible driver versions

---

## ✨ Key Features

### 🔎 Comprehensive Device Scanning
- Automatically detects all hardware devices on your system using Windows Setup API
- Identifies device status including problem detection
- Retrieves detailed device information (name, class, hardware ID)
- Displays current driver versions and installation dates
- ListView cleared on each new scan for clean results
- Support for GPU, chipset, network, storage, and peripheral devices

### 🔄 Device Status Detection
- Real-time device status monitoring
- Problem identification (requires restart, failed install, etc.)
- Device health assessment with action recommendations
- Support for Configuration Manager queries

### ⚙️ Automated Update Management
- One-click scan for all system devices
- Batch update processing for multiple drivers
- Progress tracking with visual feedback
- Background thread processing to keep UI responsive
- Safe device database management with proper memory cleanup

### 📊 Intelligent UI
- Clean, intuitive ListView interface
- Multi-column device information display:
  - Device Name
  - Device Class
  - Hardware ID
  - Driver Version
  - Status
  - Recommended Action
- Progress bar with real-time updates
- Status bar for operation feedback

### 🛡️ Safety & Reliability
- Proper memory management and resource cleanup
- Background thread worker pattern for non-blocking operations
- Device database cloning for thread-safe operations
- Handle leak prevention
- Administrator privilege enforcement via manifest

---

## 🆕 Recent Updates

### Latest Changes (June 2026)
- 🚀 **Native x64 Toolchain Migration** - Fully switched to the Microsoft Visual C++ (MSVC) architecture.
- 🌐 **WUA COM Engine Optimization** - Streamlined the PowerShell background pipeline to run inside native 64-bit wrappers for clean interaction with the Windows Update Agent.
- ✅ **Fixed Screenshot Display** - Corrected image embedding in README for proper visualization
- ✅ **ListView Auto-Clear** - ListView now clears automatically at the start of each new scan for clean results
- ✅ **Enhanced Memory Management** - Improved device database cleanup and memory leak prevention
- ✅ **Better UI Responsiveness** - Optimized ListView operations and thread communication

---

## 💻 System Requirements

### Minimum Requirements
- **OS**: Windows 10 (Build 1809) or later
- **Architecture**: x86-64 (x64 Native)
- **RAM**: 2 GB
- **Disk Space**: 500 MB free
- **Permissions**: Administrator privileges required (UAC Elevated)

---

## 📥 Building & Installation

### ⚠️ IMPORTANT: Compiler Toolchain Warning
> 🛑 **DO NOT USE DEV-C++ OR 32-BIT GCC/MINGW BUILDERS.**
> This application communicates directly with the Windows Update Agent (WUA) via COM interfaces. Windows actively blocks 32-bit processes from instantiating the 64-bit Windows Update subsystem, resulting in `0x80040154 (Class not registered)` errors. You **must** build this project natively as a 64-bit application using the official Microsoft Visual Studio tools.

### Prerequisites
1. Download and install the free **Visual Studio Community** or **Build Tools for Visual Studio**.
2. During installation, select the **Desktop development with C++** workload.

### Step-by-Step Compilation Guide

#### 1. Open the Correct Environment
Do not use a standard Command Prompt or PowerShell terminal. Open your Windows Start Menu, then search for and launch:
```text
x64 Native Tools Command Prompt for VS 2022
```
2. Clone and Navigate to the Repository
```Bash
git clone [https://github.com/mohammadteeti/DriverUpdater.git](https://github.com/mohammadteeti/DriverUpdater.git)

cd DriverUpdater
```
3. Compile the Application Resources
Compile the system application definitions and UAC manifest using the Microsoft Resource Compiler (rc.exe):

```DOS
rc.exe app.rc
```
Note: This generates a compiled binary resource file named app.res.

4. Compile the 64-Bit Binary via MSVC
Execute the following complete command using cl.exe. This enforces native Unicode support (/DUNICODE) and links the explicit structural system libraries needed for registry mapping and device management:

```DOS
cl.exe /EHsc /DUNICODE /D_UNICODE main.cpp driver_scanner.cpp driver_updater.cpp app.res /Fe:DriverUpdater.exe urlmon.lib comctl32.lib setupapi.lib newdev.lib winhttp.lib cfgmgr32.lib user32.lib gdi32.lib advapi32.lib ole32.lib oleaut32.lib
```
🚀 Quick Start
Launch the application by right-clicking DriverUpdater.exe and selecting "Run as Administrator".

Click "Scan Hardware" to begin device detection.

The ListView will clear and populate with detected devices.

Progress bar tracks system discovery live.

Review the device list for status errors and recommendations.

Click "Update All" to trigger the background automated installation script.

📸 Application Screenshot
DriverUpdater main interface showing hardware scanning and device management features

🏗️ Technical Architecture
Core Components
Main Application (main.cpp)

Windows native GUI using Win32 API

Window message processing and event handling

ListView management for device display

Thread-safe UI updates via PostMessage

Driver Scanner (driver_scanner.cpp)

Hardware detection using SetupDi* functions

Registry queries via advapi32 for structural driver profiles

Device status checking via Configuration Manager

Background thread implementation for non-blocking scans

Driver Updater (driver_updater.cpp)

Background script orchestration targeting native 64-bit powershell.exe

Bypasses file system redirection (Wow64DisableWow64FsRedirection) to access native system tools safely

Asynchronous updates managed cleanly via worker threads

Threading Model
Main Thread: Handles all UI operations and window messages

Scanner Thread: Performs hardware enumeration

Update Thread: Handles driver downloads and installation

Message-Based Communication: Threads communicate with main window via PostMessage

🔧 Technical Details
Key APIs Used
Windows Setup API: SetupDiGetClassDevs, SetupDiEnumDeviceInfo

Configuration Manager: CM_Get_DevNode_Status

Registry API: RegOpenKeyExW, RegQueryValueExW, RegCloseKey

Common Controls: ListView, Progress Bar, Static Text

Threading: CreateThread, PostMessage

Explicit Link Libraries
setupapi.lib - Hardware enumeration hooks

newdev.lib - Core device installation extensions

comctl32.lib - Windows visual common controls framework

advapi32.lib - Advanced Registry services API mapping

winhttp.lib - HTTP backend management

🐛 Troubleshooting
Error: 0x80040154 Class not registered
Ensure you did not compile with MinGW or Dev-C++ 32-bit profiles. Follow the MSVC x64 build steps precisely.

Ensure you launched the executable with Administrator privileges.

Application won't start
Check that your Windows version is compatible (Windows 10 Build 1809+)

Verify your system is x64 architecture

Scan shows no devices
Make sure Administrator privileges are granted

Check Device Manager to confirm your system has active devices

📝 Contributing
Contributions are welcome! Please feel free to submit a Pull Request with bug fixes, performance optimizations, or UI updates.

📄 License
This project is licensed under the MIT License - see the LICENSE file for details.

Built with ❤️ using native Windows APIs and C++ by Eng Mohammad J Teeti