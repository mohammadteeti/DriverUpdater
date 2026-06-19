#include "driver_updater.h"
#include "resource.h"
#include <windows.h>
#include <wuapi.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// Helper to convert to lowercase for reliable string comparison
std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] >= L'A' && result[i] <= L'Z') {
            result[i] = result[i] + 32;
        }
    }
    return result;
}

// FIX #1: The original code had the find() direction reversed.
// It was checking: hardwareId.find(categoryName)  -- almost never true
// because "PCI\VEN_8086&DEV_1E3A&..." does not *contain* a short word like "net".
// Correct check: categoryName/title should contain a fragment of the hardware ID,
// OR we check if the hardware ID contains the vendor/device token from the category.
// Best real-world approach: check if the update title or category name contains
// a known token from our hardware ID (vendor ID, device ID, etc.).
bool IsUpdateCompatible(IUpdate* pUpdate, const std::wstring& targetHwId) {
    if (!pUpdate) return false;

    std::wstring lowerTarget = ToLower(targetHwId);

    // Extract meaningful tokens from the hardware ID for matching.
    // e.g. from "PCI\VEN_8086&DEV_1E3A&SUBSYS_..." we want "ven_8086", "dev_1e3a"
    // We split on '\\', '&', '_' and keep tokens of reasonable length.
    std::vector<std::wstring> hwTokens;
    {
        std::wstring token;
        for (wchar_t c : lowerTarget) {
            if (c == L'\\' || c == L'&') {
                if (token.length() >= 4) hwTokens.push_back(token);
                token.clear();
            } else {
                token += c;
            }
        }
        if (token.length() >= 4) hwTokens.push_back(token);
    }

    // Helper lambda: returns true if any of our hardware ID tokens appear in 'haystack'
    auto anyTokenIn = [&](const std::wstring& haystack) -> bool {
        for (const std::wstring& tok : hwTokens) {
            if (haystack.find(tok) != std::wstring::npos) return true;
        }
        return false;
    };

    // Strategy 1: Check the Categories collection
    ICategoryCollection* pCategories = nullptr;
    HRESULT hr = pUpdate->get_Categories(&pCategories);
    if (SUCCEEDED(hr) && pCategories) {
        LONG count = 0;
        pCategories->get_Count(&count);

        for (LONG i = 0; i < count; ++i) {
            ICategory* pCategory = nullptr;
            if (SUCCEEDED(pCategories->get_Item(i, &pCategory)) && pCategory) {
                BSTR bstrCatName = nullptr;
                if (SUCCEEDED(pCategory->get_Name(&bstrCatName)) && bstrCatName) {
                    std::wstring catName = ToLower(bstrCatName);
                    SysFreeString(bstrCatName);

                    // FIX #1: check if catName contains our hw tokens (not the other way around)
                    if (anyTokenIn(catName)) {
                        pCategory->Release();
                        pCategories->Release();
                        return true;
                    }
                }
                pCategory->Release();
            }
        }
        pCategories->Release();
    }

    // Strategy 2: Fallback - check the human-readable update title
    BSTR bstrTitle = nullptr;
    hr = pUpdate->get_Title(&bstrTitle);
    if (SUCCEEDED(hr) && bstrTitle) {
        std::wstring updateTitle = ToLower(bstrTitle);
        SysFreeString(bstrTitle);

        // FIX #1: check if title contains our hw tokens
        if (anyTokenIn(updateTitle)) {
            return true;
        }
    }

    return false;
}

// FIX #2: Accept the full list of hardware IDs and try each one.
// Windows Update may only index a device under a less-specific ID
// (e.g. "PCI\VEN_8086&DEV_1E3A" rather than the fully qualified subsystem string).
std::wstring ProcessDeviceUpdateNative(const std::vector<std::wstring>& hardwareIds) {
    if (hardwareIds.empty()) return L"No Hardware ID";

    HRESULT hr;

    // 1. Initialize the Update Session
    IUpdateSession* pSession = nullptr;
    hr = CoCreateInstance(__uuidof(UpdateSession), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pSession));
    if (FAILED(hr)) {
        return L"Failed (Session Init Err: 0x" + std::to_wstring(hr) + L")";
    }

    // 2. Create Update Searcher
    IUpdateSearcher* pSearcher = nullptr;
    hr = pSession->CreateUpdateSearcher(&pSearcher);
    if (FAILED(hr)) {
        pSession->Release();
        return L"Failed (Searcher Init Err)";
    }

    // Query for uninstalled driver updates
    BSTR queryStr = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    ISearchResult* pSearchResult = nullptr;
    hr = pSearcher->Search(queryStr, &pSearchResult);
    SysFreeString(queryStr);

    if (FAILED(hr)) {
        pSearcher->Release();
        pSession->Release();
        return L"Failed (Catalog Search Drop)";
    }

    // 3. Extract the update list
    IUpdateCollection* pAllUpdates = nullptr;
    pSearchResult->get_Updates(&pAllUpdates);

    LONG totalUpdates = 0;
    pAllUpdates->get_Count(&totalUpdates);

    // Build a collection of updates that match ANY of our hardware IDs
    IUpdateCollection* pMatchedUpdates = nullptr;
    CoCreateInstance(__uuidof(UpdateCollection), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pMatchedUpdates));

    for (LONG i = 0; i < totalUpdates; ++i) {
        IUpdate* pUpdate = nullptr;
        if (SUCCEEDED(pAllUpdates->get_Item(i, &pUpdate)) && pUpdate) {
            bool matched = false;

            // FIX #2: Try every hardware ID in the list, not just the first one
            for (const std::wstring& hwId : hardwareIds) {
                if (IsUpdateCompatible(pUpdate, hwId)) {
                    matched = true;
                    break;
                }
            }

            if (matched) {
                LONG indexAdded = 0;
                pMatchedUpdates->Add(pUpdate, &indexAdded);
            }
            pUpdate->Release();
        }
    }

    LONG matchedCount = 0;
    pMatchedUpdates->get_Count(&matchedCount);

    if (matchedCount == 0) {
        pMatchedUpdates->Release();
        pAllUpdates->Release();
        pSearchResult->Release();
        pSearcher->Release();
        pSession->Release();
        return L"Up to Date (No MS Driver)";
    }

    // 4. Download matched updates
    // FIX #5: Capture the HRESULT from Download() itself, not from CreateUpdateDownloader()
    IUpdateDownloader* pDownloader = nullptr;
    hr = pSession->CreateUpdateDownloader(&pDownloader);
    if (FAILED(hr)) {
        pMatchedUpdates->Release();
        pAllUpdates->Release();
        pSearchResult->Release();
        pSearcher->Release();
        pSession->Release();
        return L"Failed (Downloader Init Err)";
    }

    pDownloader->put_Updates(pMatchedUpdates);
    IDownloadResult* pDownloadResult = nullptr;
    HRESULT dlHr = pDownloader->Download(&pDownloadResult); // FIX #5: separate HRESULT
    pDownloader->Release();

    if (pDownloadResult) pDownloadResult->Release();

    if (FAILED(dlHr)) { // FIX #5: check the right HRESULT
        pMatchedUpdates->Release();
        pAllUpdates->Release();
        pSearchResult->Release();
        pSearcher->Release();
        pSession->Release();
        return L"Failed (Download Err: 0x" + std::to_wstring(dlHr) + L")";
    }

    // 5. Install the downloaded drivers
    std::wstring finalStatus = L"Updated Successfully";
    IUpdateInstaller* pInstaller = nullptr;
    hr = pSession->CreateUpdateInstaller(&pInstaller);
    if (SUCCEEDED(hr)) {
        pInstaller->put_Updates(pMatchedUpdates);
        IInstallationResult* pInstallResult = nullptr;
        HRESULT instHr = pInstaller->Install(&pInstallResult);

        if (SUCCEEDED(instHr) && pInstallResult) {
            OperationResultCode resCode;
            pInstallResult->get_ResultCode(&resCode);

            if (resCode != orcSucceeded && resCode != orcSucceededWithErrors) {
                std::wstringstream wss;
                wss << L"Install Failed (Code: " << resCode << L")";
                finalStatus = wss.str();
            }
            pInstallResult->Release();
        } else {
            finalStatus = L"Install Exception (0x" + std::to_wstring(instHr) + L")";
        }
        pInstaller->Release();
    } else {
        finalStatus = L"Failed (Installer Init Err)";
    }

    // Cleanup
    pMatchedUpdates->Release();
    pAllUpdates->Release();
    pSearchResult->Release();
    pSearcher->Release();
    pSession->Release();

    return finalStatus;
}

// Thread Entry Point
DWORD WINAPI UpdateDriversThread(LPVOID lpParam) {
    UpdateThreadParams* params = (UpdateThreadParams*)lpParam;
    HWND hWnd = params->hWnd;

    // FIX #3: Windows Update Agent requires COINIT_MULTITHREADED.
    // Using COINIT_APARTMENTTHREADED can cause RPC_E_CHANGED_MODE or silent failures.
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

            device->status = L"Searching MS Catalog...";
            device->action = L"Processing...";
            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);

            // FIX #2: Pass the full hardware ID list
            std::wstring executionResult = ProcessDeviceUpdateNative(device->hardwareIds);

            device->status = executionResult;
            if (executionResult == L"Updated Successfully") {
                device->action = L"Done";
            } else if (executionResult == L"Up to Date (No MS Driver)") {
                device->action = L"Up to Date";
            } else {
                device->action = L"Error";
            }

            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
        }
        processed++;
    }

    if (SUCCEEDED(comHr)) {
        CoUninitialize();
    }

    for (DeviceInfo* device : params->deviceList) delete device;
    delete params;

    PostMessage(hWnd, WM_INSTALL_COMPLETE, 0, 0);
    return 0;
}
