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
    std::wstring hardwareId;              // First (most specific) hardware ID - used for display
    std::vector<std::wstring> hardwareIds; // FIX #2: All hardware IDs from the multi-string property
    std::wstring driverVersion;
    std::wstring driverDate;
    std::wstring status;
    std::wstring action;
    std::wstring downloadUrl; // Populated by updater
};

// Thread-safe scanner entry point
DWORD WINAPI ScanHardwareThread(LPVOID lpParam);

#endif // DRIVER_SCANNER_H
