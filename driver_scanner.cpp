#include "driver_scanner.h"
#include "resource.h"

// Helper to safely get registry string property
std::wstring GetDeviceProperty(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, DWORD property) {
    std::wstring result;
    DWORD reqSize = 0;
    DWORD dataType = 0;

    // First call to get required size
    SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, property, &dataType, NULL, 0, &reqSize);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && reqSize > 0) {
        std::vector<BYTE> buffer(reqSize);
        if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData, property, &dataType, buffer.data(), reqSize, NULL)) {
            result = std::wstring(reinterpret_cast<wchar_t*>(buffer.data()));
        }
    }
    return result;
}

// Helper to extract Driver Version and Date from Registry
void GetDriverVersionAndDate(HDEVINFO hDevInfo, SP_DEVINFO_DATA& devInfoData, std::wstring& outVersion, std::wstring& outDate) {
    std::wstring driverKey = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_DRIVER);
    if (driverKey.empty()) {
        outVersion = L"N/A";
        outDate = L"N/A";
        return;
    }

    std::wstring regPath = L"SYSTEM\\CurrentControlSet\\Control\\Class\\" + driverKey;
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, regPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t versionBuf[256] = {0};
        DWORD size = sizeof(versionBuf);
        if (RegQueryValueExW(hKey, L"DriverVersion", NULL, NULL, (LPBYTE)versionBuf, &size) == ERROR_SUCCESS) {
            outVersion = versionBuf;
        }
        
        wchar_t dateBuf[256] = {0};
        size = sizeof(dateBuf);
        if (RegQueryValueExW(hKey, L"DriverDate", NULL, NULL, (LPBYTE)dateBuf, &size) == ERROR_SUCCESS) {
            outDate = dateBuf;
        }
        RegCloseKey(hKey);
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
    int totalDevices = 0; // In real app, you might count first or update progress dynamically

    while (SetupDiEnumDeviceInfo(hDevInfo, deviceIndex, &devInfoData)) {
        DeviceInfo* pInfo = new DeviceInfo();
        
        pInfo->deviceName = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_DEVICEDESC);
        pInfo->className = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_CLASS);
        pInfo->hardwareId = GetDeviceProperty(hDevInfo, devInfoData, SPDRP_HARDWAREID);
        
        GetDriverVersionAndDate(hDevInfo, devInfoData, pInfo->driverVersion, pInfo->driverDate);

        // Check Device Status via Configuration Manager
        ULONG status = 0, problemNumber = 0;
        if (CM_Get_DevNode_Status(&status, &problemNumber, devInfoData.DevInst, 0) == CR_SUCCESS) {
            if (status & DN_HAS_PROBLEM) {
                if (problemNumber == CM_PROB_NEED_RESTART) pInfo->status = L"Needs Restart";
                else if (problemNumber == CM_PROB_FAILED_INSTALL) pInfo->status = L"Failed Install";
                else pInfo->status = L"Problem";
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
        // Simulate slight delay to allow UI to process messages and show progress
        Sleep(10); 
    }

    SetupDiDestroyDeviceInfoList(hDevInfo); // CRITICAL: Prevent handle leak
    PostMessage(hWnd, WM_SCAN_COMPLETE, 0, 0);
    return 0;
}