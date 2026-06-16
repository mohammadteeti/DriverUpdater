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
# 🚀 Quick Start

1. **Launch the Application**

   * Right-click **DriverUpdater.exe**
   * Select **Run as Administrator**

2. **Scan Your Hardware**

   * Click **Scan Hardware** to begin device detection.
   * The device list will automatically clear and populate with detected hardware.
   * Monitor scan progress in real time using the integrated progress bar.

3. **Review Results**

   * Examine detected devices and their current status.
   * Identify missing, outdated, or problematic drivers.

4. **Update Drivers**

   * Click **Update All** to initiate the automated background update process.
   * Driver installation tasks are executed asynchronously to maintain UI responsiveness.

---

# 📸 Application Screenshot

*DriverUpdater main interface displaying hardware detection, device status monitoring, and automated driver management capabilities.*

---

# 🏗️ Technical Architecture

## Core Components

### Main Application (`main.cpp`)

* Native Windows desktop application built with the **Win32 API**
* Window creation, message processing, and event handling
* ListView-based device management interface
* Thread-safe UI communication using `PostMessage`
* Real-time status and progress updates

### Driver Scanner (`driver_scanner.cpp`)

* Hardware enumeration using Windows **SetupAPI**
* Registry inspection through **Advapi32**
* Device health and status analysis via **Configuration Manager**
* Dedicated background scanning thread for non-blocking operation

### Driver Updater (`driver_updater.cpp`)

* Automated update orchestration using native **64-bit PowerShell**
* Safe access to system utilities through `Wow64DisableWow64FsRedirection`
* Background execution managed by worker threads
* Responsive and asynchronous installation workflow

---

## Threading Model

| Thread             | Responsibility                                         |
| ------------------ | ------------------------------------------------------ |
| **Main Thread**    | UI rendering, user interaction, and message processing |
| **Scanner Thread** | Hardware detection and device enumeration              |
| **Update Thread**  | Driver download and installation tasks                 |
| **Message System** | Thread communication via `PostMessage`                 |

---

# 🔧 Technical Details

## Key Windows APIs

### SetupAPI

* `SetupDiGetClassDevs`
* `SetupDiEnumDeviceInfo`

### Configuration Manager

* `CM_Get_DevNode_Status`

### Registry API

* `RegOpenKeyExW`
* `RegQueryValueExW`
* `RegCloseKey`

### Common Controls

* ListView
* Progress Bar
* Static Text

### Threading

* `CreateThread`
* `PostMessage`

---

## Linked System Libraries

| Library        | Purpose                                   |
| -------------- | ----------------------------------------- |
| `setupapi.lib` | Hardware enumeration and device discovery |
| `newdev.lib`   | Device installation and update support    |
| `comctl32.lib` | Windows common controls                   |
| `advapi32.lib` | Registry and advanced system services     |
| `winhttp.lib`  | HTTP networking backend                   |
| `cfgmgr32.lib` | Configuration Manager integration         |
| `user32.lib`   | Core Windows UI functionality             |
| `gdi32.lib`    | Graphics and rendering support            |

---

# 🐛 Troubleshooting

## Error: `0x80040154 - Class not registered`

**Possible Causes**

* Application compiled using MinGW or a 32-bit toolchain.
* Missing administrator privileges.

**Solution**

* Build the project using **MSVC x64** as documented.
* Launch the application using **Run as Administrator**.

---

## Application Won't Start

**Verify**

* Windows 10 (Build 1809+) or newer.
* 64-bit Windows installation.
* Required system components are available.

---

## Scan Shows No Devices

**Check**

* Administrator privileges are enabled.
* Devices are visible in Windows Device Manager.
* Hardware detection services are functioning normally.

---

# 📝 Contributing

Contributions are welcome and greatly appreciated.

Feel free to submit Pull Requests for:

* Bug fixes
* Performance improvements
* UI enhancements
* Code refactoring
* Documentation updates

---

# 📄 License

This project is licensed under the **MIT License**. See the `LICENSE` file for additional details.

---

<div align="center">

**Built with ❤️ using Native Windows APIs and Modern C++**

*By Eng. Mohammad J. Teeti*

</div>
