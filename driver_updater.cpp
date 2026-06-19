#include "driver_updater.h"
#include "resource.h"
#include <windows.h>
#include <wuapi.h>
#include <comdef.h>
#include <newdev.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// ============================================================
//  UTILITIES
// ============================================================

std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] >= L'A' && result[i] <= L'Z')
            result[i] = result[i] + 32;
    }
    return result;
}

std::vector<std::wstring> TokenizeHardwareId(const std::wstring& hwId) {
    std::vector<std::wstring> tokens;
    std::wstring lower = ToLower(hwId);
    std::wstring token;
    for (wchar_t c : lower) {
        if (c == L'\\' || c == L'&') {
            if (token.length() >= 4) tokens.push_back(token);
            token.clear();
        } else {
            token += c;
        }
    }
    if (token.length() >= 4) tokens.push_back(token);
    return tokens;
}

// ============================================================
//  STAGE 1 — LOCAL DRIVER STORE REINSTALL
//  Only called for devices that genuinely have no driver
//  (status == "Problem" / "Failed Install" / "Needs Restart").
//  Uses a hidden message-only HWND to satisfy the API requirement.
// ============================================================

std::wstring TryLocalDriverStoreInstall(const std::vector<std::wstring>& hardwareIds) {
    if (hardwareIds.empty()) return L"";

    // UpdateDriverForPlugAndPlayDevicesW requires a valid HWND — NULL causes
    // ERROR_INVALID_PARAMETER (0x57) on every call. Create a hidden message-only
    // window on this thread to serve as the parent handle.
    HWND hMsgWnd = CreateWindowExW(0, L"STATIC", NULL, 0, 0, 0, 0, 0,
                                   HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);

    std::wstring result;

    for (const std::wstring& hwId : hardwareIds) {
        BOOL rebootRequired = FALSE;

        BOOL ok = UpdateDriverForPlugAndPlayDevicesW(
            hMsgWnd,        // FIX: valid HWND, not NULL
            hwId.c_str(),
            NULL,           // NULL = search DriverStore automatically
            INSTALLFLAG_FORCE,
            &rebootRequired
        );

        if (ok) {
            result = rebootRequired ? L"Installed (Reboot Required)"
                                    : L"Installed from DriverStore";
            break;
        }

        DWORD err = GetLastError();
        if (err != ERROR_NO_MORE_ITEMS && err != 0xE000020B) {
            // Real failure on this ID — report it but keep trying less-specific IDs
            std::wstringstream wss;
            wss << L"DriverStore Error (0x" << std::hex << err << L") on " << hwId;
            result = wss.str(); // will be overwritten if a later ID succeeds
        }
    }

    if (hMsgWnd) DestroyWindow(hMsgWnd);
    return result; // empty = nothing found in DriverStore, fall through to WU
}

// ============================================================
//  STAGE 2 — WINDOWS UPDATE AGENT CATALOG SEARCH
//  Only runs if Stage 1 found nothing.
// ============================================================

bool IsUpdateCompatible(IUpdate* pUpdate, const std::vector<std::wstring>& hwTokens) {
    if (!pUpdate) return false;

    auto anyTokenIn = [&](const std::wstring& haystack) -> bool {
        for (const std::wstring& tok : hwTokens)
            if (haystack.find(tok) != std::wstring::npos) return true;
        return false;
    };

    ICategoryCollection* pCategories = nullptr;
    if (SUCCEEDED(pUpdate->get_Categories(&pCategories)) && pCategories) {
        LONG count = 0;
        pCategories->get_Count(&count);
        for (LONG i = 0; i < count; ++i) {
            ICategory* pCategory = nullptr;
            if (SUCCEEDED(pCategories->get_Item(i, &pCategory)) && pCategory) {
                BSTR bstrName = nullptr;
                if (SUCCEEDED(pCategory->get_Name(&bstrName)) && bstrName) {
                    bool match = anyTokenIn(ToLower(bstrName));
                    SysFreeString(bstrName);
                    pCategory->Release();
                    if (match) { pCategories->Release(); return true; }
                } else {
                    pCategory->Release();
                }
            }
        }
        pCategories->Release();
    }

    BSTR bstrTitle = nullptr;
    if (SUCCEEDED(pUpdate->get_Title(&bstrTitle)) && bstrTitle) {
        bool match = anyTokenIn(ToLower(bstrTitle));
        SysFreeString(bstrTitle);
        if (match) return true;
    }

    return false;
}

std::wstring TryWindowsUpdateInstall(const std::vector<std::wstring>& hardwareIds) {
    std::vector<std::wstring> allTokens;
    for (const std::wstring& hwId : hardwareIds) {
        auto t = TokenizeHardwareId(hwId);
        allTokens.insert(allTokens.end(), t.begin(), t.end());
    }

    HRESULT hr;
    IUpdateSession* pSession = nullptr;
    hr = CoCreateInstance(__uuidof(UpdateSession), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pSession));
    if (FAILED(hr)) return L"Failed (WU Session: 0x" + std::to_wstring(hr) + L")";

    IUpdateSearcher* pSearcher = nullptr;
    hr = pSession->CreateUpdateSearcher(&pSearcher);
    if (FAILED(hr)) { pSession->Release(); return L"Failed (WU Searcher)"; }

    BSTR queryStr = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    ISearchResult* pSearchResult = nullptr;
    hr = pSearcher->Search(queryStr, &pSearchResult);
    SysFreeString(queryStr);
    pSearcher->Release();

    if (FAILED(hr)) { pSession->Release(); return L"Failed (WU Search: 0x" + std::to_wstring(hr) + L")"; }

    IUpdateCollection* pAllUpdates = nullptr;
    pSearchResult->get_Updates(&pAllUpdates);
    LONG totalUpdates = 0;
    pAllUpdates->get_Count(&totalUpdates);

    IUpdateCollection* pMatched = nullptr;
    CoCreateInstance(__uuidof(UpdateCollection), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pMatched));

    for (LONG i = 0; i < totalUpdates; ++i) {
        IUpdate* pUpdate = nullptr;
        if (SUCCEEDED(pAllUpdates->get_Item(i, &pUpdate)) && pUpdate) {
            if (IsUpdateCompatible(pUpdate, allTokens)) {
                LONG idx = 0;
                pMatched->Add(pUpdate, &idx);
            }
            pUpdate->Release();
        }
    }
    pAllUpdates->Release();
    pSearchResult->Release();

    LONG matchedCount = 0;
    pMatched->get_Count(&matchedCount);
    if (matchedCount == 0) {
        pMatched->Release();
        pSession->Release();
        return L"Not Found (No WU Driver)";
    }

    IUpdateDownloader* pDownloader = nullptr;
    hr = pSession->CreateUpdateDownloader(&pDownloader);
    if (FAILED(hr)) { pMatched->Release(); pSession->Release(); return L"Failed (WU Downloader)"; }

    pDownloader->put_Updates(pMatched);
    IDownloadResult* pDlResult = nullptr;
    HRESULT dlHr = pDownloader->Download(&pDlResult);
    if (pDlResult) pDlResult->Release();
    pDownloader->Release();

    if (FAILED(dlHr)) {
        pMatched->Release(); pSession->Release();
        return L"Failed (WU Download: 0x" + std::to_wstring(dlHr) + L")";
    }

    std::wstring finalStatus = L"Updated via WU";
    IUpdateInstaller* pInstaller = nullptr;
    hr = pSession->CreateUpdateInstaller(&pInstaller);
    if (SUCCEEDED(hr)) {
        pInstaller->put_Updates(pMatched);
        IInstallationResult* pInstResult = nullptr;
        HRESULT instHr = pInstaller->Install(&pInstResult);
        if (SUCCEEDED(instHr) && pInstResult) {
            OperationResultCode resCode;
            pInstResult->get_ResultCode(&resCode);
            if (resCode != orcSucceeded && resCode != orcSucceededWithErrors) {
                std::wstringstream wss;
                wss << L"WU Install Failed (Code: " << resCode << L")";
                finalStatus = wss.str();
            }
            pInstResult->Release();
        } else {
            finalStatus = L"WU Install Exception (0x" + std::to_wstring(instHr) + L")";
        }
        pInstaller->Release();
    }

    pMatched->Release();
    pSession->Release();
    return finalStatus;
}

// ============================================================
//  ORCHESTRATOR
//  KEY FIX: Only attempt DriverStore reinstall for devices that
//  actually have a problem. Devices with status "OK" only go
//  through WU to check for a newer version — they do NOT get
//  pushed through UpdateDriverForPlugAndPlayDevicesW.
// ============================================================

std::wstring ProcessDeviceUpdate(const std::vector<std::wstring>& hardwareIds,
                                 const std::wstring& currentAction) {
    if (hardwareIds.empty()) return L"No Hardware ID";

    // Only broken/missing devices go through the DriverStore reinstall path.
    // "Fix / Update" = device has a problem flag (CM_PROB_*).
    // "Check for Update" = device is working fine, only check WU for a newer version.
    if (currentAction == L"Fix / Update") {
        std::wstring localResult = TryLocalDriverStoreInstall(hardwareIds);
        if (!localResult.empty()) return localResult;
    }

    // For all devices: check WU catalog for a newer/missing driver
    return TryWindowsUpdateInstall(hardwareIds);
}

// ============================================================
//  THREAD ENTRY POINT
// ============================================================

DWORD WINAPI UpdateDriversThread(LPVOID lpParam) {
    UpdateThreadParams* params = (UpdateThreadParams*)lpParam;
    HWND hWnd = params->hWnd;

    HRESULT comHr = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    PostMessage(hWnd, WM_INSTALL_START, 0, 0);

    int processed = 0;

    for (DeviceInfo* device : params->deviceList) {
        if (device->action == L"Check for Update" || device->action == L"Fix / Update") {
            if (device->hardwareIds.empty()) {
                device->status = L"No Hardware ID";
                device->action = L"Cannot Update";
                PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
                processed++;
                continue;
            }

            device->status = L"Searching...";
            device->action = L"Processing...";
            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);

            // Pass currentAction so the orchestrator knows whether to try DriverStore
            std::wstring result = ProcessDeviceUpdate(device->hardwareIds, device->action);

            device->status = result;
            if (result == L"Installed from DriverStore" ||
                result == L"Installed (Reboot Required)" ||
                result == L"Updated via WU") {
                device->action = L"Done";
            } else if (result == L"Not Found (No WU Driver)") {
                device->action = L"Up to Date";
            } else {
                device->action = L"Error";
            }

            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
        }
        processed++;
    }

    if (SUCCEEDED(comHr)) CoUninitialize();

    for (DeviceInfo* device : params->deviceList) delete device;
    delete params;

    PostMessage(hWnd, WM_INSTALL_COMPLETE, 0, 0);
    return 0;
}
