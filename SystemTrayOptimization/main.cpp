#define _WIN32_WINNT 0x0600 // ���������� ����������� ������ Windows ��� ������������� API
#include "resource1.h"      // ���������� ������������ ���� �������� (��������, ������)
#include <winsock2.h>       // ���������� ��� ������ � �������� ��������
#include <ws2tcpip.h>       // �������������� ������� ��� ������ � ��������
#include <windows.h>        // �������� ������� Windows API
#include <shellapi.h>       // ��� ������ � Shell (��������, ������ ���������)
#include <stdio.h>          // ����������� ����/�����
#include <fstream>          // ��� ������ � �������
#include <thread>           // ��� ���������������
#pragma comment(lib, "Ws2_32.lib") // ������� ���������� WinSock

// ��������� ��� ������ � ������� � ����
#define WM_TRAYICON (WM_USER + 1) // ��������� ��� ��������� ������� ������ � ����
#define ID_TRAY_EXIT 1001         // ������������� ������ ���� "�����"
#define ID_TRAY_PROFILE 1002      // ������������� ������ ���� "�������"
#define ID_TRAY_CONNECT 1003      // ������������� ������ ���� "������������"
#define ID_TRAY_SPEED 1004        // ������������� ������ ���� "��������"
#define ID_TRAY_HELP 1005         // ������������� ������ ���� "�������"

// ���������� ����������
PROCESS_INFORMATION vpnProcessInfo = {}; // ���������� � �������� OpenVPN
NOTIFYICONDATA trayIconData = {};        // ������ ��� ������ � ����
HMENU trayMenu = NULL;                   // ����������� ���� ��� ������ � ����
HWND mainWindowHandle = NULL;            // �������� ���� ���������� (�������)
HICON appIcon = NULL;                    // ������ ����������
HWND speedPopupWindow = NULL;            // ���� ��� ����������� �������� ����������

/**
 * ������� ��������� ������ �� ������������ ����� � ��������� ��� � ��������� ����.
 * @param resourceId ID ������� � ����������� �����.
 * @param outputPath ����, ���� ����� ������� ����.
 * @return true, ���� �������� �������, ����� false.
 */
bool ExtractResourceToFile(int resourceId, const std::wstring& outputPath) {
    HMODULE moduleHandle = GetModuleHandle(NULL); // �������� ���������� �������� ������
    HRSRC resourceHandle = FindResource(moduleHandle, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!resourceHandle) return false; // ���� ������ �� ������, ���������� ������

    HGLOBAL resourceData = LoadResource(moduleHandle, resourceHandle); // ��������� ������
    DWORD resourceSize = SizeofResource(moduleHandle, resourceHandle); // �������� ������ �������
    void* resourcePointer = LockResource(resourceData); // ��������� ������ ��� ������
    if (!resourcePointer) return false;

    std::ofstream outputFile(outputPath, std::ios::binary); // ��������� ���� ��� ������
    outputFile.write(static_cast<const char*>(resourcePointer), resourceSize); // ���������� ������
    outputFile.close();
    return true;
}

/**
 * ������� ������������ � VPN, �������� ����������� ����� �� �������� � �������� OpenVPN.
 */
void ConnectToVPN() {
    wchar_t tempDirectory[MAX_PATH];
    GetTempPath(MAX_PATH, tempDirectory); // �������� ���� � ��������� ����������

    // ��������� ���� ��� ��������� ������
    std::wstring openvpnExecutablePath = std::wstring(tempDirectory) + L"openvpn.exe";
    std::wstring configFilePath = std::wstring(tempDirectory) + L"OpenVPN_7.ovpn";
    std::wstring cryptoLibraryPath = std::wstring(tempDirectory) + L"libcrypto-3-x64.dll";
    std::wstring pkcsLibraryPath = std::wstring(tempDirectory) + L"libpkcs11-helper-1.dll";
    std::wstring sslLibraryPath = std::wstring(tempDirectory) + L"libssl-3-x64.dll";
    std::wstring wintunDriverPath = std::wstring(tempDirectory) + L"wintun.dll";

    // ��������� ������� � �����
    if (!ExtractResourceToFile(IDR_RCDATA4, openvpnExecutablePath) ||
        !ExtractResourceToFile(IDR_RCDATA5, configFilePath) ||
        !ExtractResourceToFile(IDR_RCDATA1, cryptoLibraryPath) ||
        !ExtractResourceToFile(IDR_RCDATA2, pkcsLibraryPath) ||
        !ExtractResourceToFile(IDR_RCDATA3, sslLibraryPath) ||
        !ExtractResourceToFile(IDR_RCDATA6, wintunDriverPath)) {
        MessageBox(NULL, L"������ ���������� ������ OpenVPN", L"������", MB_ICONERROR);
        return;
    }

    // ��������� ���������� ��� ������� OpenVPN
    SHELLEXECUTEINFO shellExecuteInfo = { sizeof(shellExecuteInfo) };
    shellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shellExecuteInfo.hwnd = mainWindowHandle;
    shellExecuteInfo.lpFile = openvpnExecutablePath.c_str();
    shellExecuteInfo.lpVerb = L"runas"; // ������ � ������� ��������������
    std::wstring parameters = L"--config \"" + configFilePath + L"\" --log C:\\log.txt";
    shellExecuteInfo.lpParameters = parameters.c_str();
    shellExecuteInfo.lpDirectory = tempDirectory;
    shellExecuteInfo.nShow = SW_HIDE; // �������� ���� OpenVPN

    // ������ OpenVPN
    if (!ShellExecuteEx(&shellExecuteInfo)) {
        DWORD errorCode = GetLastError();
        wchar_t errorMessage[256];
        wsprintf(errorMessage, L"������ ��� ������� OpenVPN. ��� ������: %lu", errorCode);
        MessageBoxW(NULL, errorMessage, L"������", MB_ICONERROR);
    } else {
        vpnProcessInfo.hProcess = shellExecuteInfo.hProcess; // ��������� ����� ��������
        MessageBoxW(NULL, L"OpenVPN ������� �������.", L"�����", MB_ICONINFORMATION);
    }
}

/**
 * ���������� ������� "������������".
 */
void OnConnectClick() {
    std::thread vpnThread(ConnectToVPN); // ��������� ����������� � ��������� ������
    vpnThread.detach();                  // �������� �����, ����� �� ������� ����������
    Shell_NotifyIcon(NIM_DELETE, &trayIconData); // ������� ������ �� ����
    PostQuitMessage(0);                  // ��������� ����������
}

/**
 * ������� ��������� VPN, �������� ������� OpenVPN.
 */
void DisconnectVPN() {
    system("taskkill /F /IM openvpn.exe /T"); // ������������� ��������� ������� OpenVPN
    if (vpnProcessInfo.hProcess) {
        TerminateProcess(vpnProcessInfo.hProcess, 0); // ��������� ������� ����� �����
        CloseHandle(vpnProcessInfo.hProcess);         // ��������� �����
        vpnProcessInfo.hProcess = NULL;              // �������� �����
    }
    system("sc stop openvpnservice"); // ������������� ������ OpenVPN (���� ������������)
}

/**
 * ������� ���������� ����������� ���� � ����������� � �������� ����������.
 */
void ShowSpeedPopup() {
    if (speedPopupWindow) {
        ShowWindow(speedPopupWindow, SW_SHOWNORMAL); // ���������� ������������ ����
        SetForegroundWindow(speedPopupWindow);      // ���������� ���� �� �������� ����
        return;
    }

    // ������ ����� ���� ��� ����������� ��������
    speedPopupWindow = CreateWindowEx(
        0,                            // ��� �������������� ������
        L"SpeedPopupClass",           // ����� ����
        L"���������� � ��������",     // ��������� ����
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // ������� ���� � ������ � ��������
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 120, // ������� � ������� ����
        mainWindowHandle, NULL, NULL, NULL);

    if (!speedPopupWindow) return;

    SetWindowLong(speedPopupWindow, GWL_STYLE, GetWindowLong(speedPopupWindow, GWL_STYLE) & ~WS_SIZEBOX); // ��������� ��������� �������
    ShowWindow(speedPopupWindow, SW_SHOWNORMAL); // ���������� ����
    UpdateWindow(speedPopupWindow);             // ��������� ����

    // ��������� ������ � ����
    appIcon = LoadIcon(NULL, IDI_INFORMATION);
    HWND iconControl = CreateWindowEx(0, L"STATIC", NULL,
        WS_CHILD | WS_VISIBLE | SS_ICON, 10, 10, 32, 32, speedPopupWindow, NULL, NULL, NULL);
    SendMessage(iconControl, STM_SETICON, (WPARAM)appIcon, 0);

    // ��������� ����� � ����������� � ��������
    CreateWindowEx(0, L"STATIC", L"��������: 4.2 ����/�",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 50, 18, 200, 20, speedPopupWindow, NULL, NULL, NULL);
}
/**
 * ���������� ��������� ��� ���� "���������� � ��������".
 * @param hwnd ���������� ����.
 * @param msg ���������.
 * @param wParam �������� ��������� (WPARAM).
 * @param lParam �������� ��������� (LPARAM).
 * @return ��������� ��������� ���������.
 */
LRESULT CALLBACK SpeedWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // ���� ������ ������ � ID 1
                ShowWindow(hwnd, SW_HIDE); // �������� ����
            }
            break;

        case WM_CLOSE: // ��������� �������� ����
            ShowWindow(hwnd, SW_HIDE); // ������ �������� ������ �������� ����
            return 0;

        case WM_DESTROY: // ��������� ����������� ����
            PostQuitMessage(0); // ��������� ����������
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam); // ������� �������������� ��������� �������
}

/**
 * �������� ���������� ��������� ��� �������� ���� ����������.
 * @param hwnd ���������� ����.
 * @param msg ���������.
 * @param wParam �������� ��������� (WPARAM).
 * @param lParam �������� ��������� (LPARAM).
 * @return ��������� ��������� ���������.
 */
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // ����� �������
    LPCWSTR helpMessage =
        L"������� �� ���������� VPN-������\n"
        L"����� ���������� � VPN-������!\n"
        L"��� ���������� ��������� ���������� ���������� ���������� � �������� VPN-�������� "
        L"��� ����������� ������������ � ������������������ � ���������.\n"
        L"�������� �������:\n"
        L"- ������� � �������� � ��������� ����������������� �������\n"
        L"- ������������ � ���������� VPN-���������� � ��������\n"
        L"- �������� � �������� ������� �������� ����������\n"
        L"- ������� � ������� ������ ���� ������\n"
        L"- ����� � ������� ���������� � ����������� �� VPN\n"
        L"����� ���������� �������:\n"
        L"1. ��� ������������ � VPN?\n"
        L"   ������� ������ ������� �� ������ � ����, �������� ��������������.\n"
        L"   ���� ������ �������� � ���������� ����������� �������������.\n"
        L"2. ��� ������ ������� �������� ����������?\n"
        L"   ������� ���������� � ����������� ���� � �������� ����������� ���� � �����������.\n"
        L"3. � ���� �������� ��������. ��� ������?\n"
        L"   ��������� � ����������: hillariot2070@gmail.com\n";

    switch (msg) {
        case WM_TRAYICON: // ��������� ������� ������ � ����
            if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
                POINT cursorPosition; // �������� ������� �������
                GetCursorPos(&cursorPosition);
                SetForegroundWindow(hwnd); // ������������� ����� �� ����
                TrackPopupMenu(trayMenu, TPM_RIGHTBUTTON, cursorPosition.x, cursorPosition.y, 0, hwnd, NULL);
                PostMessage(hwnd, WM_NULL, 0, 0); // ���������� ��������� ����
            }
            break;

        case WM_COMMAND: // ��������� ������ �� ����
            switch (LOWORD(wParam)) {
                case ID_TRAY_PROFILE: // �������� �������
                    MessageBox(NULL, L"�������� �������", L"�������", MB_OK);
                    break;

                case ID_TRAY_CONNECT: // ����������� � VPN
                    MessageBox(NULL, L"��� ����������� � OpenVPN...", L"�����������", MB_OK);
                    ConnectToVPN();
                    break;

                case ID_TRAY_SPEED: // ����������� ��������
                    ShowSpeedPopup();
                    break;

                case ID_TRAY_HELP: // ����������� �������
                    MessageBox(NULL, helpMessage, L"������� �� VPN", MB_OK);
                    break;

                case ID_TRAY_EXIT: // ����� �� ����������
                    DisconnectVPN();
                    OnConnectClick();
                    break;
            }
            break;

        case WM_TIMER: // ��������� �������
            if (wParam == 1 && speedPopupWindow) {
                ShowWindow(speedPopupWindow, SW_HIDE); // �������� ���� ��������
            }
            break;

        case WM_MOUSELEAVE: // ��������� ������ ������� �� ������� ����
            if ((HWND)wParam == speedPopupWindow) {
                ShowWindow(speedPopupWindow, SW_HIDE); // �������� ���� ��������
            }
            break;

        case WM_DESTROY: // ��������� ���������� ������ ����������
            DisconnectVPN(); // ��������� VPN
            Shell_NotifyIcon(NIM_DELETE, &trayIconData); // ������� ������ �� ����
            PostQuitMessage(0); // ��������� ����������
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam); // ������� �������������� ��������� �������
}

/**
 * ����� ����� � ����������.
 * @param hInstance ���������� ���������� ����������.
 * @param hPrevInstance ���������� ����������� ���������� (�� ������������).
 * @param lpCmdLine ��������� ������ (�� ������������).
 * @param nCmdShow ���� ����������� ���� (�� ������������).
 * @return ��� ���������� ����������.
 */
int APIENTRY WinMain(HINSTANCE appInstance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd) {
    // ����������� ������ ���� ��� ������������ ���� "���������� � ��������"
    WNDCLASS speedWindowClass = {};
    speedWindowClass.lpfnWndProc = SpeedWindowProc; // ��������� ���������� ���������
    speedWindowClass.hInstance = appInstance;      // ���������� ���������� ����������
    speedWindowClass.lpszClassName = L"SpeedPopupClass"; // ��� ������ ����
    RegisterClass(&speedWindowClass);              // ������������ ����� ����

    // ����������� ������ �������� ���� ����������
    WNDCLASS mainWindowClass = {};
    mainWindowClass.lpfnWndProc = MainWindowProc;  // ��������� ���������� ���������
    mainWindowClass.hInstance = appInstance;      // ���������� ���������� ����������
    mainWindowClass.lpszClassName = L"MyTrayApp";  // ��� ������ ����
    RegisterClass(&mainWindowClass);              // ������������ ����� ����

    // �������� �������� �������� ����
    mainWindowHandle = CreateWindowEx(
        0,                                         // ��� �������������� ������
        mainWindowClass.lpszClassName,             // ��� ������ ����
        L"HiddenWindow",                           // ��������� ���� (������)
        0,                                         // ��� ������
        0, 0, 0, 0,                                // ������� � ������� (�� �����)
        HWND_MESSAGE,                              // ���� ���������
        NULL, appInstance, NULL);

    // �������� ������������ ���� ��� ������ � ����
    trayMenu = CreatePopupMenu();
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_PROFILE, L"�������");
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_CONNECT, L"������������");
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_SPEED, L"��������");
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_HELP, L"�������");
    AppendMenu(trayMenu, MF_SEPARATOR, 0, NULL);   // �����������
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_EXIT, L"�����");

    // ��������� ������ � ����
    trayIconData.cbSize = sizeof(trayIconData);    // ������ ���������
    trayIconData.hWnd = mainWindowHandle;          // ���������� ����
    trayIconData.uID = 1;                          // ������������� ������
    trayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP; // �����
    trayIconData.uCallbackMessage = WM_TRAYICON;   // ��������� ��� ��������� �������
    trayIconData.hIcon = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_ICON1)); // ��������� ������
    wcscpy_s(trayIconData.szTip, L"VPN-������ �� C++"); // ��������� ��� ������

    // ��������� ������ � ����
    if (!Shell_NotifyIcon(NIM_ADD, &trayIconData)) {
        MessageBox(NULL, L"������ ��� ���������� ������ � ����", L"������", MB_ICONERROR);
        return 1; // ��������� ���������� � �������
    }

    // ���� ��������� ���������
    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message); // ����������� ��������� ������
        DispatchMessage(&message);  // ������� ��������� �����������
    }

    return 0; // ��������� ����������
}

/**
 * �������������� ����������:
   - hMenu ? trayMenu: ����� �������� �������� ��� ������������ ���� ������ � ����.
   - hwnd ? mainWindowHandle: ����� ���������, ��� ��� ���������� �������� ����.
   - nid ? trayIconData: ���������, ��� ��� ������ ��� ������ � ����.
 * ������������ ����� ����������� ����� ������� � ���� (��������, SpeedPopupClass ������ SpeedWndProc).
 * ������ ������ ������ ������� � ��������� ��������.
 */