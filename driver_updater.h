#ifndef DRIVER_UPDATER_H
#define DRIVER_UPDATER_H

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
