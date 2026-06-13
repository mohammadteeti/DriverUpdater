#include "driver_updater.h"
#include "resource.h"
#include <windows.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

// Helper: Escape WQL special characters
std::wstring EscapeForWQL(const std::wstring& input) {
    std::wstring escaped;
    escaped.reserve(input.length() * 2);
    for (wchar_t c : input) {
        if (c == L'\\') escaped += L"\\\\";
        else if (c == L'\'') escaped += L"''";
        else escaped += c;
    }
    return escaped;
}

// Helper: Execute PowerShell script silently
bool ExecutePowerShellScript(const std::wstring& scriptContent, std::vector<std::wstring>& outputLines) {
    wchar_t tempPath[MAX_PATH];
    GetTempPathW(MAX_PATH, tempPath);
    std::wstring scriptPath = std::wstring(tempPath) + L"tt_driver_update.ps1";
    
    std::wofstream outFile(scriptPath.c_str());
    if (!outFile.is_open()) return false;
    outFile << scriptContent;
    outFile.close();

    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hReadPipe, hWritePipe;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) return false;
    if (!SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0)) return false;

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.hStdError = hWritePipe;
    si.hStdOutput = hWritePipe;
    si.dwFlags |= STARTF_USESTDHANDLES;
    ZeroMemory(&pi, sizeof(pi));

    // Target native 64-bit PowerShell and force it into Single-Threaded Apartment (-Sta) mode
    std::wstring cmd = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe -Sta -NoProfile -ExecutionPolicy Bypass -File \"" + scriptPath + L"\"";
    std::vector<wchar_t> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(L'\0');

    // --- PAUSE 32-BIT FILE SYSTEM REDIRECTION ---
    PVOID oldWow64Value = NULL;
    BOOL isWow64 = FALSE;
    IsWow64Process(GetCurrentProcess(), &isWow64);
    
    if (isWow64) {
        Wow64DisableWow64FsRedirection(&oldWow64Value);
    }

    // Launch the process while the 64-bit view is temporarily unlocked
    BOOL success = CreateProcessW(NULL, cmdBuf.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    
    // --- RESTORE 32-BIT REDIRECTION IMMEDIATELY ---
    if (isWow64) {
        Wow64RevertWow64FsRedirection(oldWow64Value);
    }

    CloseHandle(hWritePipe);

    if (success) {
        DWORD bytesRead;
        char buffer[4096];
        std::string rawOutput;
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            rawOutput += buffer;
        }
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        int wlen = MultiByteToWideChar(CP_UTF8, 0, rawOutput.c_str(), -1, NULL, 0);
        if (wlen > 0) {
            std::vector<wchar_t> wBuf(wlen);
            MultiByteToWideChar(CP_UTF8, 0, rawOutput.c_str(), -1, wBuf.data(), wlen);
            std::wstring wOutput(wBuf.data());
            std::wstringstream ss(wOutput);
            std::wstring line;
            while (std::getline(ss, line)) {
                if (!line.empty() && line.back() == L'\r') line.pop_back();
                outputLines.push_back(line);
            }
        }
    }
    CloseHandle(hReadPipe);
    DeleteFileW(scriptPath.c_str());
    return success;
}

// Generates the safe, architecture-robust PowerShell script
std::wstring GetPowerShellScript(const std::wstring& hardwareId) {
    std::wstring escapedHwId = EscapeForWQL(hardwareId);
    std::wstringstream ss;
    
    ss << L"[Console]::OutputEncoding = [System.Text.Encoding]::UTF8\n";
    ss << L"$ErrorActionPreference = 'Stop'\n";
    ss << L"Write-Output 'STATUS:Searching MS Catalog...'\n";
    ss << L"try {\n";
    
    // --- CLEAN NATIVE 64-BIT INVOCATION ---
    ss << L"    $Session = New-Object -ComObject Microsoft.Update.Session\n";
    // --------------------------------------

    ss << L"    $Searcher = $Session.CreateUpdateSearcher()\n";
    ss << L"    $Query = \"IsInstalled=0 and Type='Driver'\"\n";
    ss << L"    $SearchResult = $Searcher.Search($Query)\n";
    
    ss << L"    $TargetHwId = '" << escapedHwId << L"'\n";
    ss << L"    $MatchedUpdates = New-Object -ComObject Microsoft.Update.UpdateCollection\n";
    
    ss << L"    foreach ($Update in $SearchResult.Updates) {\n";
    ss << L"        if ($Update.Identity -and $Update.Compatibility -and ($Update.Compatibility -contains $TargetHwId)) {\n";
    ss << L"            $MatchedUpdates.Add($Update) | Out-Null\n";
    ss << L"        }\n";
    ss << L"    }\n"; // <-- FIXED: Both brackets are now safely inside the text generator block
    
    ss << L"    if ($MatchedUpdates.Count -eq 0) {\n";
    ss << L"        Write-Output 'STATUS:Up to Date (No MS Driver)'\n";
    ss << L"        exit 0\n";
    ss << L"    }\n";
    
    ss << L"    Write-Output 'STATUS:Downloading...'\n";
    ss << L"    $Downloader = $Session.CreateUpdateDownloader()\n";
    ss << L"    $Downloader.Updates = $MatchedUpdates\n";
    ss << L"    $Downloader.Download() | Out-Null\n";
    
    ss << L"    Write-Output 'STATUS:Installing...'\n";
    ss << L"    $Installer = $Session.CreateUpdateInstaller()\n";
    ss << L"    $Installer.Updates = $MatchedUpdates\n";
    ss << L"    $InstallResult = $Installer.Install()\n";
    
    ss << L"    if ($InstallResult.ResultCode -eq 2 -or $InstallResult.ResultCode -eq 3) {\n";
    ss << L"        Write-Output 'STATUS:Updated Successfully'\n";
    ss << L"    } else {\n";
    ss << L"        Write-Output \"STATUS:Install Failed (Code: $($InstallResult.ResultCode))\"\n";
    ss << L"    }\n";
    ss << L"} catch {\n";
    ss << L"    $CleanedMessage = $_.Exception.Message -replace '\\r|\\n', ' '\n";
    ss << L"    Write-Output \"STATUS:Failed ($CleanedMessage)\"\n";
    ss << L"}\n";
    
    return ss.str();
}

DWORD WINAPI UpdateDriversThread(LPVOID lpParam) {
    UpdateThreadParams* params = (UpdateThreadParams*)lpParam;
    HWND hWnd = params->hWnd;
    PostMessage(hWnd, WM_INSTALL_START, 0, 0);

    int total = params->deviceList.size();
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

            std::wstring script = GetPowerShellScript(device->hardwareId);
            std::vector<std::wstring> outputLines;
            
            if (ExecutePowerShellScript(script, outputLines)) {
                for (const std::wstring& line : outputLines) {
                    if (line.find(L"STATUS:") == 0) {
                        std::wstring status = line.substr(7);
                        device->status = status;
                        
                        if (status == L"Updated Successfully") device->action = L"Done";
                        else if (status == L"Up to Date (No MS Driver)") device->action = L"Up to Date";
                        else if (status.find(L"Failed") != std::wstring::npos || status.find(L"Error") != std::wstring::npos) device->action = L"Error";
                        else device->action = L"Processing...";
                        
                        PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
                    }
                }
            } else {
                device->status = L"Execution Failed";
                device->action = L"PowerShell Err";
                PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
            }
        }
        processed++;
        PostMessage(hWnd, WM_INSTALL_UPDATE, (WPARAM)processed, (LPARAM)device);
    }

    for (DeviceInfo* device : params->deviceList) delete device;
    delete params;
    PostMessage(hWnd, WM_INSTALL_COMPLETE, 0, 0);
    return 0;
}