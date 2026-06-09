#ifndef DRIVER_UPDATER_H
#define DRIVER_UPDATER_H

#include <windows.h>
#include <winhttp.h>
#include <string>
#include <vector>
#include "driver_scanner.h"

// Move the struct definition here so main.cpp can see it!
struct UpdateThreadParams {
    HWND hWnd;
    std::vector<DeviceInfo*> deviceList;
};

// Asynchronous updater thread
DWORD WINAPI UpdateDriversThread(LPVOID lpParam);

#endif // DRIVER_UPDATER_H