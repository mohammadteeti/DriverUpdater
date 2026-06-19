#include "driver_scanner.h"
#include "resource.h"

// Helper to safely get a single registry string property
std::wstring GetDeviceProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, DWORD property) {
    std::wstring result;
    DWORD reqSize = 0;
    DWORD dataType = 0;

    SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, property, &dataType, NULL, 0, &reqSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && reqSize > 0) {
        std::vector<BYTE> buffer(reqSize);
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, property, &dataType, buffer.data(), reqSize, NULL)) {
            result = std::wstring(reinterpret_cast<wchar_t*>(buffer.data()));
        }
    }
    return result;
}

// FIX #2: Read a REG_MULTI_SZ property and return ALL strings it contains.
// SPDRP_HARDWAREID is a multi-string: several IDs ranked by specificity,
// separated by null characters and terminated by a double-null.
// The original code only captured the first one, causing WU catalog misses.
std::vector<std::wstring> GetDevicePropertyMultiString(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, DWORD property) {
    std::vector<std::wstring> results;
    DWORD reqSize = 0;
    DWORD dataType = 0;

    SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, property, &dataType, NULL, 0, &reqSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && reqSize > 0) {
        std::vector<BYTE> buffer(reqSize);
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, property, &dataType, buffer.data(), reqSize, NULL)) {
            const wchar_t* p = reinterpret_cast<wchar_t*>(buffer.data());
            // Walk each null-separated sub-string until we hit the double-null terminator
            while (p && *p) {
                results.push_back(std::wstring(p));
                p += wcslen(p) + 1;
            }
        }
    }
    return results;
}

// Helper to extract Driver Version and Date from Registry
void GetDriverVersionAndDate(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, std::wstring& outVersion, std::wstring& outDate) {
    std::wstring driverKey = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_DRIVER);
    if (driverKey.empty()) {
        outVersion = L"N/A";
        outDate    = L"N/A";
        return;
    }

    std::wstring regPath = L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + driverKey;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t versionBuf[256] = {0};
        DWORD size = sizeof(versionBuf);
        if (RegQueryValueExW(hKey, L"DriverVersion", NULL, NULL, (LPBYTE)versionBuf, &size) == ERROR_SUCCESS) {
            outVersion = versionBuf;
        } else {
            outVersion = L"N/A";
        }

        wchar_t dateBuf[256] = {0};
        size = sizeof(dateBuf);
        if (RegQueryValueExW(hKey, L"DriverDate", NULL, NULL, (LPBYTE)dateBuf, &size) == ERROR_SUCCESS) {
            outDate = dateBuf;
        } else {
            outDate = L"N/A";
        }
        RegCloseKey(hKey);
    } else {
        outVersion = L"N/A";
        outDate    = L"N/A";
    }
}

DWORD WINAPI ScanHardwareThread(LPVOID lpParam) {
    HWND hWnd = (HWND)lpParam;
    PostMessage(hWnd, WM_SCAN_START, 0, 0);

    HDEVINFO hDevInfo = SetupDiGetClassDevsW(NULL, NULL, NULL, DIGCF_ALLCLASSES | DIGCF_PRESENT);
    if (hDevInfo == INVALID_HANDLE_VALUE) {
        PostMessage(hWnd, WM_SCAN_COMPLETE, 0, 0);
        return 1;
    }

    SP_DEVINFO_DATA devInfoData;
    devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
    DWORD deviceIndex = 0;

    while (SetupDiEnumDeviceInfo(hDevInfo, deviceIndex, &devInfoData)) {
        DeviceInfo* pInfo = new DeviceInfo();

        pInfo->deviceName = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_DEVICEDESC);
        pInfo->className  = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_CLASS);

        // FIX #2: Read ALL hardware IDs from the multi-string property.
        // Store the full list in hardwareIds and keep the first (most specific)
        // in hardwareId for display/matching purposes.
        pInfo->hardwareIds = GetDevicePropertyMultiString(hDevInfo, devInfoData, SPDRP_HARDWAREID);
        if (!pInfo->hardwareIds.empty()) {
            pInfo->hardwareId = pInfo->hardwareIds[0]; // first = most specific, shown in UI
        }

        GetDriverVersionAndDate(hDevInfo, devInfoData, pInfo->driverVersion, pInfo->driverDate);

        // Check Device Status via Configuration Manager
        ULONG status = 0, problemNumber = 0;
        if (CM_Get_DevNode_Status(&status, &problemNumber, devInfoData.DevInst, 0) == CR_SUCCESS) {
            if (status & DN_HAS_PROBLEM) {
                if (problemNumber == CM_PROB_NEED_RESTART)   pInfo->status = L"Needs Restart";
                else if (problemNumber == CM_PROB_FAILED_INSTALL) pInfo->status = L"Failed Install";
                else                                          pInfo->status = L"Problem";
                pInfo->action = L"Fix / Update";
            } else {
                pInfo->status = L"OK";
                pInfo->action = L"Check for Update";
            }
        } else {
            pInfo->status = L"Unknown";
            pInfo->action = L"Scan";
        }

        // Send to UI thread. UI thread will delete pInfo after use.
        PostMessage(hWnd, WM_SCAN_UPDATE, 0, (LPARAM)pInfo);

        deviceIndex++;
        Sleep(10);
    }

    SetupDiDestroyDeviceInfoList(hDevInfo); // Prevent handle leak
    PostMessage(hWnd, WM_SCAN_COMPLETE, 0, 0);
    return 0;
}
