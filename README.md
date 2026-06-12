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
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Usage Guide](#usage-guide)
- [Technical Architecture](#technical-architecture)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [License](#license)

## 🎯 Overview

**DriverUpdater** is a sophisticated native Windows utility built in C++ that streamlines driver management on your system. It automatically scans your machine for installed hardware devices, identifies outdated drivers, retrieves manufacturer-specific updates, and manages the installation process with minimal user intervention.

Whether you're maintaining a single workstation or managing a fleet of computers, DriverUpdater helps ensure optimal hardware compatibility and system performance.

### The Problem It Solves
- 🔍 **Manual driver hunting** - No more searching through manufacturer websites
- ⚠️ **System instability** - Outdated drivers can cause crashes and performance issues
- ⏰ **Time-consuming updates** - Bulk update management is tedious and error-prone
- 🖥️ **Hardware compatibility** - Automatically ensures compatible driver versions

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

## 🆕 Recent Updates

### Latest Changes (June 2026)
- ✅ **Fixed Screenshot Display** - Corrected image embedding in README for proper visualization
- ✅ **ListView Auto-Clear** - ListView now clears automatically at the start of each new scan for clean results
- ✅ **Enhanced Memory Management** - Improved device database cleanup and memory leak prevention
- ✅ **Better UI Responsiveness** - Optimized ListView operations and thread communication

## 💻 System Requirements

### Minimum Requirements
- **OS**: Windows 10 (Build 1809) or later
- **Architecture**: x86-64 (x64)
- **RAM**: 2 GB
- **Disk Space**: 500 MB free
- **Permissions**: Administrator privileges required

### Recommended Requirements
- **OS**: Windows 10 21H2 or Windows 11
- **RAM**: 4 GB or more
- **Disk Space**: 2 GB free
- **Internet**: Broadband connection for driver downloads

## 📥 Installation

### Option 1: Standalone Executable
1. Download the latest release from the [Releases](https://github.com/mohammadteeti/DriverUpdater/releases) page
2. Extract the archive to your desired location
3. Run `DriverUpdater.exe` with administrator privileges

```bash
# Right-click and select "Run as administrator" or use:
.\DriverUpdater.exe --admin
```

### Option 2: Build from Source
1. Clone the repository:
   ```bash
   git clone https://github.com/mohammadteeti/DriverUpdater.git
   cd DriverUpdater
   ```

2. Build using the provided Makefile:
   ```bash
   mingw32-make -f Makefile.win
   ```

---

## 🚀 Quick Start

1. **Launch the application** as Administrator
2. **Click "Scan Hardware"** to begin device detection
   - The ListView will clear and populate with detected devices
   - Progress bar shows scan progress
   - Status bar displays current operation
3. **Review the device list** for status and recommendations
4. **Click "Update All"** to begin driver updates (when implemented)


---

## 📸 Application Screenshot

![Screenshot](https://github.com/mohammadteeti/DriverUpdater/blob/master/image.PNG?raw=true)

*DriverUpdater main interface showing hardware scanning and device management features*

---

=======
![ScreenShot Here](https://github.com/mohammadteeti/DriverUpdater/blob/master/image.PNG)

=======

## 🏗️ Technical Architecture

### Core Components

**Main Application (main.cpp)**
- Windows native GUI using Win32 API
- Window message processing and event handling
- ListView management for device display
- Thread-safe UI updates via PostMessage

**Driver Scanner (driver_scanner.cpp)**
- Hardware detection using SetupDi* functions
- Registry queries for driver information
- Device status checking via Configuration Manager
- Background thread implementation for non-blocking scans

**Driver Updater (driver_updater.cpp)**
- Driver update orchestration
- Download management
- Installation handling
- Progress reporting

### Threading Model
- **Main Thread**: Handles all UI operations and window messages
- **Scanner Thread**: Performs hardware enumeration
- **Update Thread**: Handles driver downloads and installation
- **Message-Based Communication**: Threads communicate with main window via PostMessage

### Memory Management
- Device information stored in vector with proper cleanup
- Pointer-based data passing between threads
- Deep cloning for thread-safe data sharing
- Automatic cleanup in WM_DESTROY handler

## 🔧 Technical Details

### Key APIs Used
- **Windows Setup API**: SetupDiGetClassDevs, SetupDiEnumDeviceInfo
- **Configuration Manager**: CM_Get_DevNode_Status
- **Registry API**: RegOpenKeyEx, RegQueryValueEx
- **Common Controls**: ListView, Progress Bar, Static Text
- **Threading**: CreateThread, PostMessage

### Libraries
- `setupapi.lib` - Hardware enumeration
- `newdev.lib` - Device installation
- `comctl32.lib` - Common controls
- `winhttp.lib` - HTTP operations

## 🐛 Troubleshooting

### Application won't start
- Ensure you're running as Administrator
- Check that your Windows version is compatible (Windows 10 Build 1809+)
- Verify your system is x64 architecture

### Scan shows no devices
- Make sure Administrator privileges are granted
- Try restarting the application
- Check Device Manager to confirm your system has devices

### Driver updates not working
- Verify internet connectivity for driver downloads
- Check that target devices are compatible
- Ensure sufficient disk space

## 📝 Contributing

Contributions are welcome! Please feel free to submit a Pull Request with:
- Bug fixes
- Feature enhancements
- Documentation improvements
- Performance optimizations

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

**Built with ❤️ using native Windows APIs and C++ by Eng Mohammad J Teeti**

