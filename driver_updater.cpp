// driver_updater.cpp
#include "driver_updater.h"
#include "resource.h"
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

bool IsUpdateCompatible(IUpdate* pUpdate, const std::vector<std::wstring>& deviceHwIds) {
    if (!pUpdate) return false;

    // Build tokens: full IDs + split parts
    std::vector<std::wstring> searchTokens;
    for (const std::wstring& hwId : deviceHwIds) {
        searchTokens.push_back(ToLower(hwId));
        auto parts = TokenizeHardwareId(hwId);
        searchTokens.insert(searchTokens.end(), parts.begin(), parts.end());
    }

    auto anyTokenIn = [&](const std::wstring& haystack) -> bool {
        std::wstring lowerHaystack = ToLower(haystack);
        for (const std::wstring& tok : searchTokens) {
            if (lowerHaystack.find(tok) != std::wstring::npos)
                return true;
        }
        return false;
    };

    // Check categories
    ICategoryCollection* pCategories = nullptr;
    if (SUCCEEDED(pUpdate->get_Categories(&pCategories)) && pCategories) {
        LONG count = 0;
        pCategories->get_Count(&count);
        for (LONG i = 0; i < count; ++i) {
            ICategory* pCategory = nullptr;
            if (SUCCEEDED(pCategories->get_Item(i, &pCategory)) && pCategory) {
                BSTR bstrName = nullptr;
                if (SUCCEEDED(pCategory->get_Name(&bstrName)) && bstrName) {
                    bool match = anyTokenIn(bstrName);
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

    // Check title
    BSTR bstrTitle = nullptr;
    if (SUCCEEDED(pUpdate->get_Title(&bstrTitle)) && bstrTitle) {
        bool match = anyTokenIn(bstrTitle);
        SysFreeString(bstrTitle);
        if (match) return true;
    }

    return false;
}

// ============================================================
//  STAGE 1 — LOCAL DRIVER STORE REINSTALL
//  Now logs detailed errors to the console.
// ============================================================

std::wstring TryLocalDriverStoreInstall(const std::vector<std::wstring>& hardwareIds) {
    if (hardwareIds.empty()) return L"No Hardware ID";

    std::wcout << L"[DriverStore] Trying to install from local DriverStore..." << std::endl;

    HWND hMsgWnd = CreateWindowExW(0, L"STATIC", NULL, 0, 0, 0, 0, 0,
                                   HWND_MESSAGE, NULL, GetModuleHandleW(NULL), NULL);

    if (!hMsgWnd) {
        std::wcout << L"[DriverStore] Failed to create message window." << std::endl;
        return L"Error (no window)";
    }

    std::wstring result;

    for (size_t i = 0; i < hardwareIds.size(); ++i) {
        const std::wstring& hwId = hardwareIds[i];
        std::wcout << L"[DriverStore] Trying HWID: " << hwId << std::endl;

        BOOL rebootRequired = FALSE;
        BOOL ok = UpdateDriverForPlugAndPlayDevicesW(
            hMsgWnd,
            hwId.c_str(),
            NULL,           // NULL = search DriverStore
            INSTALLFLAG_FORCE,
            &rebootRequired
        );

        if (ok) {
            result = rebootRequired ? L"Installed (Reboot Required)"
                                    : L"Installed from DriverStore";
            std::wcout << L"[DriverStore] Success: " << result << std::endl;
            break;
        }

        DWORD err = GetLastError();
        std::wcout << L"[DriverStore] Error 0x" << std::hex << err << L" for HWID: " << hwId << std::endl;

        // If error is not "no more items" and not "no driver selected", we might have a real failure.
        if (err != ERROR_NO_MORE_ITEMS && err != 0xE000020B) {
            std::wstringstream wss;
            wss << L"DriverStore Error (0x" << std::hex << err << L") on " << hwId;
            result = wss.str();
            std::wcout << L"[DriverStore] Stopping because of non‑retryable error." << std::endl;
            break;
        }
        // Otherwise continue to next HWID
    }

    if (hMsgWnd) DestroyWindow(hMsgWnd);

    if (result.empty()) {
        std::wcout << L"[DriverStore] No driver found for any hardware ID." << std::endl;
    }
    return result;
}

// ============================================================
//  STAGE 2 — WINDOWS UPDATE AGENT CATALOG SEARCH (fallback)
//  Logs the number of driver updates found and whether any match.
// ============================================================

std::wstring TryWindowsUpdateInstall(const std::vector<std::wstring>& hardwareIds) {
    if (hardwareIds.empty()) return L"No Hardware ID";

    std::wcout << L"[WU] Querying Windows Update for driver updates..." << std::endl;
    for (const auto& id : hardwareIds) {
        std::wcout << L"[WU]   Device HWID: " << id << std::endl;
    }

    HRESULT hr;
    IUpdateSession* pSession = nullptr;
    hr = CoCreateInstance(__uuidof(UpdateSession), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pSession));
    if (FAILED(hr)) {
        std::wcout << L"[WU] Failed to create UpdateSession (0x" << std::hex << hr << L")" << std::endl;
        return L"Failed (WU Session)";
    }

    IUpdateSearcher* pSearcher = nullptr;
    hr = pSession->CreateUpdateSearcher(&pSearcher);
    if (FAILED(hr)) {
        pSession->Release();
        std::wcout << L"[WU] Failed to create UpdateSearcher (0x" << std::hex << hr << L")" << std::endl;
        return L"Failed (WU Searcher)";
    }

    BSTR queryStr = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    ISearchResult* pSearchResult = nullptr;
    hr = pSearcher->Search(queryStr, &pSearchResult);
    SysFreeString(queryStr);
    pSearcher->Release();

    if (FAILED(hr)) {
        pSession->Release();
        std::wcout << L"[WU] Search failed (0x" << std::hex << hr << L")" << std::endl;
        return L"Failed (WU Search)";
    }

    IUpdateCollection* pAllUpdates = nullptr;
    pSearchResult->get_Updates(&pAllUpdates);
    LONG totalUpdates = 0;
    pAllUpdates->get_Count(&totalUpdates);
    std::wcout << L"[WU] Total driver updates available: " << totalUpdates << std::endl;

    IUpdateCollection* pMatched = nullptr;
    CoCreateInstance(__uuidof(UpdateCollection), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pMatched));

    for (LONG i = 0; i < totalUpdates; ++i) {
        IUpdate* pUpdate = nullptr;
        if (SUCCEEDED(pAllUpdates->get_Item(i, &pUpdate)) && pUpdate) {
            if (IsUpdateCompatible(pUpdate, hardwareIds)) {
                LONG idx = 0;
                pMatched->Add(pUpdate, &idx);
                BSTR title = nullptr;
                if (SUCCEEDED(pUpdate->get_Title(&title)) && title) {
                    std::wcout << L"[WU] Matched update: " << title << std::endl;
                    SysFreeString(title);
                }
            }
            pUpdate->Release();
        }
    }
    pAllUpdates->Release();
    pSearchResult->Release();

    LONG matchedCount = 0;
    pMatched->get_Count(&matchedCount);
    std::wcout << L"[WU] Number of matching driver updates: " << matchedCount << std::endl;

    if (matchedCount == 0) {
        pMatched->Release();
        pSession->Release();
        return L"Not Found (No WU Driver)";
    }

    IUpdateDownloader* pDownloader = nullptr;
    hr = pSession->CreateUpdateDownloader(&pDownloader);
    if (FAILED(hr)) {
        pMatched->Release();
        pSession->Release();
        std::wcout << L"[WU] Failed to create Downloader (0x" << std::hex << hr << L")" << std::endl;
        return L"Failed (WU Downloader)";
    }

    pDownloader->put_Updates(pMatched);
    IDownloadResult* pDlResult = nullptr;
    HRESULT dlHr = pDownloader->Download(&pDlResult);
    if (pDlResult) pDlResult->Release();
    pDownloader->Release();

    if (FAILED(dlHr)) {
        pMatched->Release();
        pSession->Release();
        std::wcout << L"[WU] Download failed (0x" << std::hex << dlHr << L")" << std::endl;
        return L"Failed (WU Download)";
    }

    std::wcout << L"[WU] Download successful. Installing..." << std::endl;

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
                std::wcout << L"[WU] Installation result code: " << resCode << std::endl;
            } else {
                std::wcout << L"[WU] Installation succeeded." << std::endl;
            }
            pInstResult->Release();
        } else {
            finalStatus = L"WU Install Exception (0x" + std::to_wstring(instHr) + L")";
            std::wcout << L"[WU] Install call failed (0x" << std::hex << instHr << L")" << std::endl;
        }
        pInstaller->Release();
    }

    pMatched->Release();
    pSession->Release();
    return finalStatus;
}

// ============================================================
//  ORCHESTRATOR
// ============================================================

std::wstring ProcessDeviceUpdate(const std::vector<std::wstring>& hardwareIds,
                                 const std::wstring& currentAction) {
    if (hardwareIds.empty()) return L"No Hardware ID";

    std::wcout << L"[Update] Processing device with action: " << currentAction << std::endl;

    if (currentAction == L"Fix / Update") {
        std::wstring localResult = TryLocalDriverStoreInstall(hardwareIds);
        if (!localResult.empty()) {
            std::wcout << L"[Update] Local install result: " << localResult << std::endl;
            return localResult;
        }
    }

    std::wcout << L"[Update] Falling back to Windows Update..." << std::endl;
    return TryWindowsUpdateInstall(hardwareIds);
}

// ============================================================
//  THREAD ENTRY POINT
// ============================================================

DWORD WINAPI UpdateDriversThread(LPVOID lpParam) {
    UpdateThreadParams* params = (UpdateThreadParams*)lpParam;
    HWND hWnd = params->hWnd;

    HRESULT comHr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(comHr)) {
        std::wcout << L"[Update] CoInitializeEx failed (0x" << std::hex << comHr << L")" << std::endl;
    }

    PostMessage(hWnd, WM_INSTALL_START, 0, 0);

    int processed = 0;
    int total = (int)params->deviceList.size();

    for (DeviceInfo* device : params->deviceList) {
        std::wcout << L"\n[Update] Device: " << device->deviceName
                   << L" (Action: " << device->action << L")" << std::endl;

        if (device->action == L"Check for Update" || device->action == L"Fix / Update") {
            if (device->hardwareIds.empty()) {
                device->status = L"No Hardware ID";
                device->action = L"Cannot Update";
                std::wcout << L"[Update] No hardware IDs, skipping." << std::endl;
                PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
                processed++;
                continue;
            }

            device->status = L"Searching...";
            device->action = L"Processing...";
            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);

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

            std::wcout << L"[Update] Final status: " << device->status << std::endl;
            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
        } else {
            std::wcout << L"[Update] Device not eligible for update (action = " << device->action << L")" << std::endl;
        }
        processed++;
    }

    if (SUCCEEDED(comHr)) CoUninitialize();

    for (DeviceInfo* device : params->deviceList) delete device;
    delete params;

    PostMessage(hWnd, WM_INSTALL_COMPLETE, 0, 0);
    return 0;
}