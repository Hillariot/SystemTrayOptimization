#define _WIN32_WINNT 0x0600
#include "resource1.h"  // Подключаем ресурсный заголовок
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <fstream>
#include <thread>
#pragma comment(lib, "Ws2_32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_PROFILE 1002
#define ID_TRAY_CONNECT 1003
#define ID_TRAY_SPEED 1004
#define ID_TRAY_HELP 1005


PROCESS_INFORMATION vpnProcessInfo = {};  // Хендл процесса OpenVPN


NOTIFYICONDATA nid = {};
HMENU hMenu;
HWND hwnd;
HICON hIcon;

HWND hSpeedWindow = NULL;

bool WriteResourceToFile(int resourceId, const std::wstring& outputPath) {
    HMODULE hModule = GetModuleHandle(NULL);
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) return false;

    HGLOBAL hResData = LoadResource(hModule, hRes);
    DWORD resSize = SizeofResource(hModule, hRes);
    void* pResData = LockResource(hResData);
    if (!pResData) return false;

    std::ofstream outFile(outputPath, std::ios::binary);
    outFile.write(static_cast<const char*>(pResData), resSize);
    outFile.close();
    return true;
}

void ConnectToVPN()
{
    wchar_t tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);

    WCHAR tempPath1[MAX_PATH];
    DWORD pathLen = GetTempPathW(MAX_PATH, tempPath1);

    std::wstring exePath = std::wstring(tempPath1) + L"openvpn.exe";
    std::wstring configPath = std::wstring(tempPath1) + L"OpenVPN_7.ovpn";
    std::wstring lib1Path = std::wstring(tempPath1) + L"libcrypto-3-x64.dll";
    std::wstring lib2Path = std::wstring(tempPath1) + L"libpkcs11-helper-1.dll";
    std::wstring lib3Path = std::wstring(tempPath1) + L"libssl-3-x64.dll";
    std::wstring wintun = std::wstring(tempPath1) + L"wintun.dll";


    if (!WriteResourceToFile(IDR_RCDATA4, exePath) ||
        !WriteResourceToFile(IDR_RCDATA5, configPath) ||
        !WriteResourceToFile(IDR_RCDATA1, lib1Path) ||
        !WriteResourceToFile(IDR_RCDATA2, lib2Path) ||
        !WriteResourceToFile(IDR_RCDATA3, lib3Path)||
        !WriteResourceToFile(IDR_RCDATA6, wintun)) {

        MessageBox(NULL, L"Ошибка извлечения файлов OpenVPN", L"Ошибка", MB_ICONERROR);
        return;
    }

    SHELLEXECUTEINFO sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = hwnd;
    sei.lpFile = exePath.c_str();
    sei.lpVerb = L"runas";
    std::wstring params = L"--config \"" + configPath + L"\" --log C:\\log.txt";
    sei.lpParameters = params.c_str();
    sei.lpDirectory = tempPath;
    sei.nShow = SW_HIDE;

    if (!ShellExecuteEx(&sei)) {
        DWORD err = GetLastError();
        wchar_t buffer[256];
        wsprintf(buffer, L"Ошибка при запуске OpenVPN. Код ошибки: %lu", err);
        MessageBoxW(NULL, buffer, L"Ошибка", MB_ICONERROR);
    }
    else {
        vpnProcessInfo.hProcess = sei.hProcess;
        MessageBoxW(NULL, L"OpenVPN успешно запущен.", L"Успех", MB_ICONINFORMATION);
    }
}


void OnConnectClick()
{
    std::thread vpnThread(ConnectToVPN);
    vpnThread.detach();  // Отделяем поток, чтобы он работал независимо

    Shell_NotifyIcon(NIM_DELETE, &nid);
    PostQuitMessage(0);
}


void DisconnectVPN()
{
    // Используем команду taskkill для завершения OpenVPN
    system("taskkill /F /IM openvpn.exe /T");

    // Пример, если процесс работает как сервис
    // system("sc stop openvpnservice");

    // Если OpenVPN был запущен как отдельный процесс, его хендл можно использовать
    if (vpnProcessInfo.hProcess) {
        TerminateProcess(vpnProcessInfo.hProcess, 0);  // Завершаем процесс
        CloseHandle(vpnProcessInfo.hProcess);  // Закрываем хендл
        vpnProcessInfo.hProcess = NULL;  // Обнуляем хендл
    }
    system("sc stop openvpnservice");
}


void ShowSpeedPopup() {
    if (hSpeedWindow) {
        ShowWindow(hSpeedWindow, SW_SHOWNORMAL);
        SetForegroundWindow(hSpeedWindow);
        return;
    }

    // Создаём обычное окно с заголовком
    hSpeedWindow = CreateWindowEx(
        0,                            // Убираем WS_EX_TOOLWINDOW, добавим нормальное поведение
        L"SpeedPopupClass",          // Класс окна
        L"Информация о скорости",    // Заголовок окна
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,         // Обычное окно с рамкой, заголовком и кнопками
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 120,
        hwnd, NULL, NULL, NULL
    );

    if (!hSpeedWindow) return;

    SetWindowLong(hSpeedWindow, GWL_STYLE, GetWindowLong(hSpeedWindow, GWL_STYLE) & ~WS_SIZEBOX);


    ShowWindow(hSpeedWindow, SW_SHOWNORMAL);
    UpdateWindow(hSpeedWindow);

    // Иконка
    hIcon = LoadIcon(NULL, IDI_INFORMATION);
    HWND hIconCtrl = CreateWindowEx(0, L"STATIC", NULL,
        WS_CHILD | WS_VISIBLE | SS_ICON,
        10, 10, 32, 32, hSpeedWindow, NULL, NULL, NULL);
    SendMessage(hIconCtrl, STM_SETICON, (WPARAM)hIcon, 0);

    // Текст с информацией
    CreateWindowEx(0, L"STATIC", L"Скорость: 4.2 Мбит/с",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        50, 18, 200, 20, hSpeedWindow, NULL, NULL, NULL);
}

LRESULT CALLBACK SpeedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {  // Проверяем, нажата ли кнопка с ID 1
            ShowWindow(hwnd, SW_HIDE);  // Скрываем окно
        }
        break;
    case WM_CLOSE:  // Обработка закрытия окна
        ShowWindow(hwnd, SW_HIDE);  // Просто скрыть окно вместо закрытия
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}



LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LPCWSTR helpMessage =
        L"Справка по приложению VPN-клиент\n\n"
        L"Добро пожаловать в VPN-клиент!\n"
        L"  Это приложение позволяет установить защищённое соединение с удалённым VPN-сервером для обеспечения безопасности и конфиденциальности в интернете.\n\n"
        L"Основные функции:\n"
        L"- Профиль — просмотр и настройка пользовательского профиля\n"
        L"- Подключиться — установить VPN-соединение с сервером\n"
        L"- Скорость — показать текущую скорость соединения\n"
        L"- Справка — открыть данное окно помощи\n"
        L"- Выход — закрыть приложение и отключиться от VPN\n\n"
        L"Часто задаваемые вопросы:\n"
        L"1. Как подключиться к VPN?\n"
        L"   Нажмите правой кнопкой на иконке в трее, выберите «Подключиться».\n"
        L"   Если сервер доступен — соединение установится автоматически.\n\n"
        L"2. Как узнать текущую скорость соединения?\n"
        L"   Нажмите «Скорость» в контекстном меню — появится всплывающее окно с информацией.\n\n"
        L"3. У меня возникли проблемы. Что делать?\n"
        L"   Свяжитесь с поддержкой: hillariot2070@gmail.com\n";
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd); // Нужно для правильного поведения меню
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
            PostMessage(hwnd, WM_NULL, 0, 0);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_TRAY_PROFILE:
            MessageBox(NULL, L"Открытие профиля", L"Профиль", MB_OK);
            break;
        case ID_TRAY_CONNECT:
            MessageBox(NULL, L"Идёт подключение в OpenVPN...", L"Подключение", MB_OK);
            ConnectToVPN();
            break;
        case ID_TRAY_SPEED:
            ShowSpeedPopup();
            break;
        case ID_TRAY_HELP:
            MessageBox(NULL, helpMessage, L"Справка по VPN", MB_OK);
            break;
        case ID_TRAY_EXIT:
            DisconnectVPN();
            OnConnectClick();
            break;
        }
        break;

    case WM_TIMER:
        if (wParam == 1 && hSpeedWindow) {
            ShowWindow(hSpeedWindow, SW_HIDE);
        }
        break;
    case WM_MOUSELEAVE:
        if ((HWND)wParam == hSpeedWindow) {
            ShowWindow(hSpeedWindow, SW_HIDE);
        }
        break;
    case WM_DESTROY:
        DisconnectVPN();
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)

{
    WNDCLASS speedClass = {};
    speedClass.lpfnWndProc = SpeedWndProc;
    speedClass.hInstance = hInstance;
    speedClass.lpszClassName = L"SpeedPopupClass";
    RegisterClass(&speedClass);

    // Регистрируем класс окна
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyTrayApp";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, wc.lpszClassName, L"HiddenWindow", 0,
        0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    // Контекстное меню
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_PROFILE, L"Профиль");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_CONNECT, L"Подключиться");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_SPEED, L"Скорость");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_HELP, L"Справка");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

    // Иконка в трее
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); // Загружаем иконку из ресурсов
    wcscpy_s(nid.szTip, L"VPN-клиент на C++");

    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        MessageBox(NULL, L"Ошибка при добавлении иконки в трей", L"Ошибка", MB_ICONERROR);
        return 1;
    }

    // Цикл сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
