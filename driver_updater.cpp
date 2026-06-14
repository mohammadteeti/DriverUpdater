#include "driver_updater.h"
#include "resource.h"
#include <windows.h>
#include <wuapi.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <sstream>
#include <iostream>

// Helper to convert lowercase or mixed strings for reliable searching
std::wstring ToLower(const std::wstring& str) {
    std::wstring result = str;
    for (size_t i = 0; i < result.length(); ++i) {
        if (result[i] >= L'A' && result[i] <= L'Z') {
            result[i] = result[i] + 32;
        }
    }
    return result;
}

// Checks if a specific target hardware ID exists inside an update object's metadata or categories
bool IsUpdateCompatible(IUpdate* pUpdate, const std::wstring& targetHwId) {
    if (!pUpdate) return false;

    std::wstring lowerTarget = ToLower(targetHwId);

    // 1. Primary Strategy: Check the Categories Collection for Hardware ID matches
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

                    // If the hardware ID matches a category signature, we have a match
                    if (!catName.empty() && lowerTarget.find(catName) != std::wstring::npos) {
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

    // 2. Fallback Strategy: Check the human-readable update title string
    BSTR bstrTitle = nullptr;
    hr = pUpdate->get_Title(&bstrTitle);
    if (SUCCEEDED(hr) && bstrTitle) {
        std::wstring updateTitle = ToLower(bstrTitle);
        SysFreeString(bstrTitle);

        if (!updateTitle.empty() && lowerTarget.find(updateTitle) != std::wstring::npos) {
            return true;
        }
    }

    return false;
}

// Orchestrates the native Windows Update Agent lifecycle interface
std::wstring ProcessDeviceUpdateNative(const std::wstring& hardwareId) {
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

    // Query flags looking for uninstalled driver updates matching the current environment topology
    BSTR queryStr = SysAllocString(L"IsInstalled=0 and Type='Driver'");
    ISearchResult* pSearchResult = nullptr;
    
    hr = pSearcher->Search(queryStr, &pSearchResult);
    SysFreeString(queryStr);

    if (FAILED(hr)) {
        pSearcher->Release();
        pSession->Release();
        return L"Failed (Catalog Search Drop)";
    }

    // 3. Extract the updates group
    IUpdateCollection* pAllUpdates = nullptr;
    pSearchResult->get_Updates(&pAllUpdates);
    
    LONG totalUpdates = 0;
    pAllUpdates->get_Count(&totalUpdates);

    // Instantiate a new native COM compilation container to accumulate hardware matches
    IUpdateCollection* pMatchedUpdates = nullptr;
    CoCreateInstance(__uuidof(UpdateCollection), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pMatchedUpdates));

    for (LONG i = 0; i < totalUpdates; ++i) {
        IUpdate* pUpdate = nullptr;
        if (SUCCEEDED(pAllUpdates->get_Item(i, &pUpdate)) && pUpdate) {
            if (IsUpdateCompatible(pUpdate, hardwareId)) {
                LONG indexAdded = 0; // Satisfies the dual argument requirement of IUpdateCollection::Add
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

    // 4. Download matched updates packages
    IUpdateDownloader* pDownloader = nullptr;
    hr = pSession->CreateUpdateDownloader(&pDownloader);
    if (SUCCEEDED(hr)) {
        pDownloader->put_Updates(pMatchedUpdates);
        IDownloadResult* pDownloadResult = nullptr;
        hr = pDownloader->Download(&pDownloadResult);
        
        if (SUCCEEDED(hr)) {
            pDownloadResult->Release();
        }
        pDownloader->Release();
    }

    if (FAILED(hr)) {
        pMatchedUpdates->Release();
        pAllUpdates->Release();
        pSearchResult->Release();
        pSearcher->Release();
        pSession->Release();
        return L"Failed (Download Timeout)";
    }

    // 5. Install the staging drivers payload
    std::wstring finalStatus = L"Updated Successfully";
    IUpdateInstaller* pInstaller = nullptr;
    hr = pSession->CreateUpdateInstaller(&pInstaller);
    if (SUCCEEDED(hr)) {
        pInstaller->put_Updates(pMatchedUpdates);
        IInstallationResult* pInstallResult = nullptr;
        hr = pInstaller->Install(&pInstallResult);
        
        if (SUCCEEDED(hr)) {
            OperationResultCode resCode; 
            pInstallResult->get_ResultCode(&resCode);
            
            if (resCode != orcSucceeded && resCode != orcSucceededWithErrors) {
                std::wstringstream wss;
                wss << L"Install Failed (Code: " << resCode << L")";
                finalStatus = wss.str(); 
            }
            pInstallResult->Release();
        } else {
            finalStatus = L"Install Exception (0x" + std::to_wstring(hr) + L")";
        }
        pInstaller->Release();
    }

    // Comprehensive Interface Destruction Chain
    pMatchedUpdates->Release();
    pAllUpdates->Release();
    pSearchResult->Release();
    pSearcher->Release();
    pSession->Release();

    return finalStatus;
}

// Thread Entry Point orchestration engine
DWORD WINAPI UpdateDriversThread(LPVOID lpParam) {
    UpdateThreadParams* params = (UpdateThreadParams*)lpParam;
    HWND hWnd = params->hWnd;
    
    HRESULT comHr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    
    PostMessage(hWnd, WM_INSTALL_START, 0, 0);

    int processed = 0;

    for (DeviceInfo* device : params->deviceList) {
        if (device->action == L"Check for Update" || device->action == L"Fix / Update") {
            if (device->hardwareId.empty()) {
                device->status = L"No Hardware ID";
                device->action = L"Cannot Update";
                PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
                processed++;
                continue; 
            }

            device->status = L"Searching MS Catalog...";
            device->action = L"Processing...";
            PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);

            std::wstring executionResult = ProcessDeviceUpdateNative(device->hardwareId);
            
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