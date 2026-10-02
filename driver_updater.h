#ifndef DRIVER_UPDATER_H
#define DRIVER_UPDATER_H

// Set Windows version to at least Windows 10 to avoid warnings
#ifndef _WIN32_WINNT
#define _WIN32_WINNT   0x0A00
#endif
#ifndef WINVER
#define WINVER         0x0A00
#endif
#ifndef NTDDI_VERSION
#define NTDDI_VERSION  0x0A000000
#endif

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include "driver_scanner.h"

struct UpdateThreadParams {
    HWND hWnd;
    std::vector<DeviceInfo*> deviceList;
};

// Asynchronous updater thread
DWORD WINAPI UpdateDriversThread(LPVOID lpParam);

#endif // DRIVER_UPDATER_H