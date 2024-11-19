#include <Windows.h>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <tlhelp32.h>
#include <shellapi.h>
#include <shlobj.h>
#include <direct.h>
#include "resource.h"
#include "resource2.h"

std::string GetAppDataPath() {
    char appDataPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appDataPath))) {
        std::string customPath = std::string(appDataPath) + "\\JabimoCS2Launcher";

        if (_mkdir(customPath.c_str()) != 0 && errno != EEXIST) {
            OutputDebugStringA("Failed to create directory for config file.\n");
        }

        return customPath + "\\config.txt";
    }
    return "config.txt";
}

const std::string configFile = GetAppDataPath();

struct Resolution {
    int width;
    int height;
};

std::vector<Resolution> resolutions_4_3 = { {640, 480}, {720, 576}, {800, 600}, {1024, 768}, {1152, 864}, {1280, 960}, {1280, 1024}, {1440, 1080}, {1600, 1200} };
std::vector<Resolution> resolutions_16_9 = { {1176, 664}, {1280, 720}, {1360, 768}, {1366, 768}, {1600, 900}, {1920, 1080}, {2560, 1440}, {3840, 2160} };
std::vector<Resolution> resolutions_16_10 = { {720, 480}, {1280, 768}, {1280, 800}, {1440, 900}, {1600, 1024}, {1680, 1050}, {1920, 1200} };

Resolution nativeResolution;

bool GetNativeResolution() {
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(devMode);

    if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode)) {
        nativeResolution = { static_cast<int>(devMode.dmPelsWidth), static_cast<int>(devMode.dmPelsHeight) };
        return true;
    }
    return false;
}

std::vector<Resolution> FilterResolutions(const std::vector<Resolution>& resolutions) {
    std::vector<Resolution> filtered;
    for (const auto& res : resolutions) {
        if (res.width <= nativeResolution.width && res.height <= nativeResolution.height) {
            filtered.push_back(res);
        }
    }
    return filtered;
}

bool SetResolution(int width, int height, int colorDepth) {
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(devMode);
    devMode.dmPelsWidth = width;
    devMode.dmPelsHeight = height;
    devMode.dmBitsPerPel = colorDepth;
    devMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;

    return ChangeDisplaySettings(&devMode, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL;
}

bool RestoreNativeResolution() {
    DEVMODE devMode = {};
    devMode.dmSize = sizeof(DEVMODE);

    if (!EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &devMode)) {
        return false;
    }

    return SetResolution(devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel);
}

bool IsProcessRunning(const std::wstring& processName) {
    HANDLE hProcessSnap;
    PROCESSENTRY32 pe32;
    hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hProcessSnap == INVALID_HANDLE_VALUE) return false;

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return false;
    }

    do {
        if (processName == pe32.szExeFile) {
            CloseHandle(hProcessSnap);
            return true;
        }
    } while (Process32Next(hProcessSnap, &pe32));

    CloseHandle(hProcessSnap);
    return false;
}

Resolution LoadConfig() {
    std::ifstream file(configFile);
    if (file.is_open()) {
        Resolution res;
        file >> res.width >> res.height;
        file.close();
        return res;
    }
    return { 0, 0 };
}

void SaveConfig(const Resolution& res) {
    std::ofstream file(configFile);
    if (file.is_open()) {
        file << res.width << " " << res.height;
        file.close();
    }
}

void UpdateResolutions(HWND hwndCombo, const std::vector<Resolution>& resolutions) {
    SendMessage(hwndCombo, CB_RESETCONTENT, 0, 0);

    int index = 0;
    for (const auto& res : resolutions) {
        std::wstring item = std::to_wstring(res.width) + L"x" + std::to_wstring(res.height);
        SendMessage(hwndCombo, CB_INSERTSTRING, index++, (LPARAM)item.c_str());
    }

    SendMessage(hwndCombo, CB_SETCURSEL, 0, 0);
}

LRESULT CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    static HWND hwndAspectRatio, hwndResolution, hwndCheckbox;

    switch (msg) {
    case WM_INITDIALOG: {
        hwndAspectRatio = GetDlgItem(hwnd, IDC_COMBO1);
        hwndResolution = GetDlgItem(hwnd, IDC_COMBO2);
        hwndCheckbox = GetDlgItem(hwnd, IDC_CHECK1);

        SetWindowText(hwnd, L"Jabimo's CS2 Launcher");

        HICON hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_APP_ICON));
        if (hIcon) {
            SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
        }

        RECT rect = { 0 };
        GetWindowRect(hwnd, &rect);

        int dialogWidth = rect.right - rect.left;
        int dialogHeight = rect.bottom - rect.top;

        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        int screenHeight = GetSystemMetrics(SM_CYSCREEN);

        int posX = (screenWidth - dialogWidth) / 2;
        int posY = (screenHeight - dialogHeight) / 2 - 100;

        SetWindowPos(hwnd, NULL, posX, posY, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

        SendMessage(hwndAspectRatio, CB_INSERTSTRING, 0, (LPARAM)L"4:3");
        SendMessage(hwndAspectRatio, CB_INSERTSTRING, 1, (LPARAM)L"16:9");
        SendMessage(hwndAspectRatio, CB_INSERTSTRING, 2, (LPARAM)L"16:10");
        SendMessage(hwndAspectRatio, CB_SETCURSEL, 0, 0);

        UpdateResolutions(hwndResolution, FilterResolutions(resolutions_4_3));
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_COMBO1:
            if (HIWORD(wParam) == CBN_SELCHANGE) {
                int index = static_cast<int>(SendMessage(hwndAspectRatio, CB_GETCURSEL, 0, 0));
                const std::vector<Resolution>* selectedResolutions = nullptr;

                if (index == 0) {
                    selectedResolutions = &resolutions_4_3;
                }
                else if (index == 1) {
                    selectedResolutions = &resolutions_16_9;
                }
                else if (index == 2) {
                    selectedResolutions = &resolutions_16_10;
                }

                if (selectedResolutions) {
                    UpdateResolutions(hwndResolution, FilterResolutions(*selectedResolutions));
                }
            }
            break;

        case IDOK: {
            int aspectIndex = static_cast<int>(SendMessage(hwndAspectRatio, CB_GETCURSEL, 0, 0));
            int resIndex = static_cast<int>(SendMessage(hwndResolution, CB_GETCURSEL, 0, 0));

            const std::vector<Resolution>* selectedResolutions = nullptr;

            if (aspectIndex == 0) {
                selectedResolutions = &resolutions_4_3;
            }
            else if (aspectIndex == 1) {
                selectedResolutions = &resolutions_16_9;
            }
            else if (aspectIndex == 2) {
                selectedResolutions = &resolutions_16_10;
            }

            if (selectedResolutions && resIndex >= 0 && resIndex < static_cast<int>(selectedResolutions->size())) {
                Resolution selectedRes = (*selectedResolutions)[resIndex];
                bool saveConfig = SendMessage(hwndCheckbox, BM_GETCHECK, 0, 0) == BST_CHECKED;

                if (saveConfig) {
                    SaveConfig(selectedRes);
                }

                SetResolution(selectedRes.width, selectedRes.height, 32);
                EndDialog(hwnd, IDOK);
            }
            break;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            break;
        }
        return TRUE;

    case WM_CLOSE:
        EndDialog(hwnd, IDCANCEL);
        return TRUE;
    }

    return FALSE;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd) {
    if (!GetNativeResolution()) {
        MessageBox(NULL, L"Failed to get monitor resolution.", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    Resolution config = LoadConfig();

    int dialogResult;
    if (config.width == 0 || config.height == 0) {
        dialogResult = DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc);
    }
    else {
        SetResolution(config.width, config.height, 32);
        dialogResult = IDOK;
    }

    if (dialogResult != IDOK) {
        return 0;
    }

    ShellExecute(0, L"open", L"steam://rungameid/730", 0, 0, SW_SHOWNORMAL);

    while (!IsProcessRunning(L"cs2.exe")) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    while (IsProcessRunning(L"cs2.exe")) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    if (!RestoreNativeResolution()) {
        return 1;
    }

    return 0;
}