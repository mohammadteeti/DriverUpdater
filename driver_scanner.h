#ifndef DRIVER_SCANNER_H
#define DRIVER_SCANNER_H

#define INITGUID
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <string>
#include <vector>

struct DeviceInfo {
    std::wstring deviceName;
    std::wstring className;
    std::wstring hardwareId;
    std::wstring driverVersion;
    std::wstring driverDate;
    std::wstring status;
    std::wstring action;
    std::wstring downloadUrl; // Populated by updater
};

// Thread-safe scanner entry point
DWORD WINAPI ScanHardwareThread(LPVOID lpParam);

#endif // DRIVER_SCANNER_H