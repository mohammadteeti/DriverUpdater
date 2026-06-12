#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "resource.h"
#include "driver_scanner.h"
#include "driver_updater.h"
#include <iostream>
#include <fcntl.h>
#include <io.h>

// Helper function to replace ListView_SetItemData
void ListView_SetItemData(HWND hList, int index, LPARAM data) {
    LVITEMW lvi = {0};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = index;
    lvi.lParam = data;
    ListView_SetItem(hList, &lvi);
}

// Helper function to replace ListView_GetItemData
LPARAM ListView_GetItemData(HWND hList, int index) {
    LVITEMW lvi = {0};
    lvi.mask = LVIF_PARAM;
    lvi.iItem = index;
    if (ListView_GetItem(hList, &lvi)) {
        return lvi.lParam;
    }
    return 0;
}

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "newdev.lib")
#pragma comment(lib, "winhttp.lib")

HINSTANCE g_hInst;
HWND g_hListView;
HWND g_hProgress;
HWND g_hStatus;
std::vector<DeviceInfo*> g_deviceDatabase; // Main thread owns this after WM_SCAN_UPDATE

void AddColumn(HWND hList, int col, const wchar_t* name, int width) {
    LVCOLUMNW lvc = {0};
    lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    lvc.pszText = (LPWSTR)name;
    lvc.cx = width;
    lvc.iSubItem = col;
    ListView_InsertColumn(hList, col, &lvc);
}

void AddDeviceToListView(DeviceInfo* pInfo) {
    LVITEMW lvi = {0};
    lvi.mask = LVIF_TEXT;
    lvi.iItem = ListView_GetItemCount(g_hListView);
    
    // Column 0: Device Name
    lvi.iSubItem = 0;
    lvi.pszText = (LPWSTR)pInfo->deviceName.c_str();
    int index = ListView_InsertItem(g_hListView, &lvi);

    // Subsequent columns
    ListView_SetItemText(g_hListView, index, 1, (LPWSTR)pInfo->className.c_str());
    ListView_SetItemText(g_hListView, index, 2, (LPWSTR)pInfo->hardwareId.c_str());
    ListView_SetItemText(g_hListView, index, 3, (LPWSTR)pInfo->driverVersion.c_str());
    ListView_SetItemText(g_hListView, index, 4, (LPWSTR)pInfo->status.c_str());
    ListView_SetItemText(g_hListView, index, 5, (LPWSTR)pInfo->action.c_str());

    // Store pointer in item data for later retrieval during update
    ListView_SetItemData(g_hListView, index, (LPARAM)pInfo);
    g_deviceDatabase.push_back(pInfo);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            // Initialize Common Controls
            INITCOMMONCONTROLSEX icex = {sizeof(INITCOMMONCONTROLSEX), ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS};
            InitCommonControlsEx(&icex);

            // Controls
            CreateWindowW(L"BUTTON", L"Scan Hardware", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 
                          10, 10, 120, 30, hWnd, (HMENU)ID_BTN_SCAN, g_hInst, NULL);
            
            CreateWindowW(L"BUTTON", L"Update All", WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON, 
                          140, 10, 120, 30, hWnd, (HMENU)ID_BTN_UPDATE_ALL, g_hInst, NULL);

            g_hProgress = CreateWindowW(PROGRESS_CLASSW, NULL, WS_VISIBLE | WS_CHILD, 
                                        270, 15, 300, 20, hWnd, (HMENU)ID_PROGRESS, g_hInst, NULL);
            SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

            g_hStatus = CreateWindowW(L"STATIC", L"Ready.", WS_VISIBLE | WS_CHILD | SS_LEFT, 
                                      10, 50, 500, 20, hWnd, (HMENU)ID_STATIC_STATUS, g_hInst, NULL);

            g_hListView = CreateWindowW(WC_LISTVIEWW, NULL, 
                WS_VISIBLE | WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 
                10, 80, 760, 400, hWnd, (HMENU)ID_LIST_DEVICES, g_hInst, NULL);
            
            ListView_SetExtendedListViewStyle(g_hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
            AddColumn(g_hListView, 0, L"Device Name", 200);
            AddColumn(g_hListView, 1, L"Class", 100);
            AddColumn(g_hListView, 2, L"Hardware ID", 150);
            AddColumn(g_hListView, 3, L"Driver Version", 100);
            AddColumn(g_hListView, 4, L"Status", 100);
            AddColumn(g_hListView, 5, L"Action", 100);
            break;
        }

        case WM_COMMAND: {
            if (LOWORD(wParam) == ID_BTN_SCAN) {
                EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), FALSE);
                SetWindowTextW(g_hStatus, L"Scanning hardware...");
                SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
                CreateThread(NULL, 0, ScanHardwareThread, hWnd, 0, NULL);
            }
            else if (LOWORD(wParam) == ID_BTN_UPDATE_ALL) {
                SetWindowTextW(g_hStatus, L"Preparing updates...");
                SendMessage(g_hProgress, PBM_SETPOS, 0, 0);
                
                UpdateThreadParams* params = new UpdateThreadParams();
                params->hWnd = hWnd;
                for (DeviceInfo* dev : g_deviceDatabase) {
                    DeviceInfo* clone = new DeviceInfo(*dev);
                    params->deviceList.push_back(clone);
                }
                
                CreateThread(NULL, 0, UpdateDriversThread, params, 0, NULL);
            }
            break;
        }

        case WM_SCAN_START: {
            SendMessage(g_hProgress, PBM_SETMARQUEE, TRUE, 0);
            break;
        }
        case WM_SCAN_UPDATE: {
            DeviceInfo* pInfo = (DeviceInfo*)lParam;
            AddDeviceToListView(pInfo);
            break;
        }
        case WM_SCAN_COMPLETE: {
            SendMessage(g_hProgress, PBM_SETMARQUEE, FALSE, 0);
            SendMessage(g_hProgress, PBM_SETPOS, 100, 0);
            SetWindowTextW(g_hStatus, L"Scan complete.");
            EnableWindow(GetDlgItem(hWnd, ID_BTN_SCAN), TRUE);
            break;
        }
        case WM_INSTALL_START: {
            SendMessage(g_hProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
            break;
        }
        case WM_INSTALL_UPDATE: {
            int processed = (int)wParam;
            DeviceInfo* updatedInfo = (DeviceInfo*)lParam;
            
            int count = ListView_GetItemCount(g_hListView);
            for (int i = 0; i < count; i++) {
                DeviceInfo* existing = (DeviceInfo*)ListView_GetItemData(g_hListView, i);
                if (existing->hardwareId == updatedInfo->hardwareId) {
                    ListView_SetItemText(g_hListView, i, 4, (LPWSTR)updatedInfo->status.c_str());
                    ListView_SetItemText(g_hListView, i, 5, (LPWSTR)updatedInfo->action.c_str());
                    
                    existing->status = updatedInfo->status;
                    existing->action = updatedInfo->action;
                    existing->downloadUrl = updatedInfo->downloadUrl;

                    // --- FIXED LINE: Uses pointer operator and wide console output ---
                    std::wcout << L"Device Updated: " << existing->deviceName 
                               << L" -> Status: " << existing->status << std::endl;
                    
                    break;
                }
            }
            
            int percent = (count > 0) ? ((processed * 100) / count) : 0;
            SendMessage(g_hProgress, PBM_SETPOS, percent, 0);
            break;
        }
        case WM_INSTALL_COMPLETE: {
            SetWindowTextW(g_hStatus, L"All updates processed.");
            MessageBoxW(hWnd, L"Driver update process finished.", L"Success", MB_OK | MB_ICONINFORMATION);
            break;
        }

        case WM_DESTROY: {
            for (DeviceInfo* dev : g_deviceDatabase) {
                delete dev;
            }
            g_deviceDatabase.clear();
            
            // Free allocated console before exiting
            FreeConsole();
            
            PostQuitMessage(0);
            break;
        }
        default:
            return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    g_hInst = hInstance;

    // --- ALLOCATE COMMAND LINE CONSOLE WINDOW ---
    if (AllocConsole()) {
        FILE* fp;
        // Redirect standard output streams to the new console window
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        
        // Change console translation mode to handle Wide strings / Unicode correctly
        _setmode(_fileno(stdout), _O_U16TEXT);
    }

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = L"DriverDetectorClass";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);

    RegisterClassExW(&wc);

    HWND hWnd = CreateWindowExW(0, L"DriverDetectorClass", L"Native Device Driver Manager", 
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 550, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}