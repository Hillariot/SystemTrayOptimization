#define _WIN32_WINNT 0x0600 // Определяем минимальную версию Windows для использования API
#include "resource1.h"      // Подключаем заголовочный файл ресурсов (например, иконки)
#include <winsock2.h>       // Библиотека для работы с сетевыми сокетами
#include <ws2tcpip.h>       // Дополнительные функции для работы с сокетами
#include <windows.h>        // Основные функции Windows API
#include <shellapi.h>       // Для работы с Shell (например, запуск процессов)
#include <stdio.h>          // Стандартный ввод/вывод
#include <fstream>          // Для работы с файлами
#include <thread>           // Для многопоточности
#pragma comment(lib, "Ws2_32.lib") // Линкуем библиотеку WinSock

// Константы для работы с иконкой в трее
#define WM_TRAYICON (WM_USER + 1) // Сообщение для обработки событий иконки в трее
#define ID_TRAY_EXIT 1001         // Идентификатор пункта меню "Выход"
#define ID_TRAY_PROFILE 1002      // Идентификатор пункта меню "Профиль"
#define ID_TRAY_CONNECT 1003      // Идентификатор пункта меню "Подключиться"
#define ID_TRAY_SPEED 1004        // Идентификатор пункта меню "Скорость"
#define ID_TRAY_HELP 1005         // Идентификатор пункта меню "Справка"

// Глобальные переменные
PROCESS_INFORMATION vpnProcessInfo = {}; // Информация о процессе OpenVPN
NOTIFYICONDATA trayIconData = {};        // Данные для иконки в трее
HMENU trayMenu = NULL;                   // Контекстное меню для иконки в трее
HWND mainWindowHandle = NULL;            // Основное окно приложения (скрытое)
HICON appIcon = NULL;                    // Иконка приложения
HWND speedPopupWindow = NULL;            // Окно для отображения скорости соединения

/**
 * Функция извлекает ресурс из исполняемого файла и сохраняет его в указанный файл.
 * @param resourceId ID ресурса в исполняемом файле.
 * @param outputPath Путь, куда будет сохранён файл.
 * @return true, если операция успешна, иначе false.
 */
bool ExtractResourceToFile(int resourceId, const std::wstring& outputPath) {
    HMODULE moduleHandle = GetModuleHandle(NULL); // Получаем дескриптор текущего модуля
    HRSRC resourceHandle = FindResource(moduleHandle, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!resourceHandle) return false; // Если ресурс не найден, возвращаем ошибку

    HGLOBAL resourceData = LoadResource(moduleHandle, resourceHandle); // Загружаем ресурс
    DWORD resourceSize = SizeofResource(moduleHandle, resourceHandle); // Получаем размер ресурса
    void* resourcePointer = LockResource(resourceData); // Блокируем ресурс для чтения
    if (!resourcePointer) return false;

    std::ofstream outputFile(outputPath, std::ios::binary); // Открываем файл для записи
    outputFile.write(static_cast<const char*>(resourcePointer), resourceSize); // Записываем данные
    outputFile.close();
    return true;
}

/**
 * Функция подключается к VPN, извлекая необходимые файлы из ресурсов и запуская OpenVPN.
 */
void ConnectToVPN() {
    wchar_t tempDirectory[MAX_PATH];
    GetTempPath(MAX_PATH, tempDirectory); // Получаем путь к временной директории

    // Формируем пути для временных файлов
    std::wstring openvpnExecutablePath = std::wstring(tempDirectory) + L"openvpn.exe";
    std::wstring configFilePath = std::wstring(tempDirectory) + L"OpenVPN_7.ovpn";
    std::wstring cryptoLibraryPath = std::wstring(tempDirectory) + L"libcrypto-3-x64.dll";
    std::wstring pkcsLibraryPath = std::wstring(tempDirectory) + L"libpkcs11-helper-1.dll";
    std::wstring sslLibraryPath = std::wstring(tempDirectory) + L"libssl-3-x64.dll";
    std::wstring wintunDriverPath = std::wstring(tempDirectory) + L"wintun.dll";

    // Извлекаем ресурсы в файлы
    if (!ExtractResourceToFile(IDR_RCDATA4, openvpnExecutablePath) ||
        !ExtractResourceToFile(IDR_RCDATA5, configFilePath) ||
        !ExtractResourceToFile(IDR_RCDATA1, cryptoLibraryPath) ||
        !ExtractResourceToFile(IDR_RCDATA2, pkcsLibraryPath) ||
        !ExtractResourceToFile(IDR_RCDATA3, sslLibraryPath) ||
        !ExtractResourceToFile(IDR_RCDATA6, wintunDriverPath)) {
        MessageBox(NULL, L"Ошибка извлечения файлов OpenVPN", L"Ошибка", MB_ICONERROR);
        return;
    }

    // Настройка параметров для запуска OpenVPN
    SHELLEXECUTEINFO shellExecuteInfo = { sizeof(shellExecuteInfo) };
    shellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
    shellExecuteInfo.hwnd = mainWindowHandle;
    shellExecuteInfo.lpFile = openvpnExecutablePath.c_str();
    shellExecuteInfo.lpVerb = L"runas"; // Запуск с правами администратора
    std::wstring parameters = L"--config \"" + configFilePath + L"\" --log C:\\log.txt";
    shellExecuteInfo.lpParameters = parameters.c_str();
    shellExecuteInfo.lpDirectory = tempDirectory;
    shellExecuteInfo.nShow = SW_HIDE; // Скрываем окно OpenVPN

    // Запуск OpenVPN
    if (!ShellExecuteEx(&shellExecuteInfo)) {
        DWORD errorCode = GetLastError();
        wchar_t errorMessage[256];
        wsprintf(errorMessage, L"Ошибка при запуске OpenVPN. Код ошибки: %lu", errorCode);
        MessageBoxW(NULL, errorMessage, L"Ошибка", MB_ICONERROR);
    } else {
        vpnProcessInfo.hProcess = shellExecuteInfo.hProcess; // Сохраняем хендл процесса
        MessageBoxW(NULL, L"OpenVPN успешно запущен.", L"Успех", MB_ICONINFORMATION);
    }
}

/**
 * Обработчик события "Подключиться".
 */
void OnConnectClick() {
    std::thread vpnThread(ConnectToVPN); // Запускаем подключение в отдельном потоке
    vpnThread.detach();                  // Отделяем поток, чтобы он работал независимо
    Shell_NotifyIcon(NIM_DELETE, &trayIconData); // Удаляем иконку из трея
    PostQuitMessage(0);                  // Завершаем приложение
}

/**
 * Функция отключает VPN, завершая процесс OpenVPN.
 */
void DisconnectVPN() {
    system("taskkill /F /IM openvpn.exe /T"); // Принудительно завершаем процесс OpenVPN
    if (vpnProcessInfo.hProcess) {
        TerminateProcess(vpnProcessInfo.hProcess, 0); // Завершаем процесс через хендл
        CloseHandle(vpnProcessInfo.hProcess);         // Закрываем хендл
        vpnProcessInfo.hProcess = NULL;              // Обнуляем хендл
    }
    system("sc stop openvpnservice"); // Останавливаем службу OpenVPN (если используется)
}

/**
 * Функция показывает всплывающее окно с информацией о скорости соединения.
 */
void ShowSpeedPopup() {
    if (speedPopupWindow) {
        ShowWindow(speedPopupWindow, SW_SHOWNORMAL); // Показываем существующее окно
        SetForegroundWindow(speedPopupWindow);      // Перемещаем окно на передний план
        return;
    }

    // Создаём новое окно для отображения скорости
    speedPopupWindow = CreateWindowEx(
        0,                            // Нет дополнительных стилей
        L"SpeedPopupClass",           // Класс окна
        L"Информация о скорости",     // Заголовок окна
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, // Обычное окно с рамкой и кнопками
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 120, // Размеры и позиция окна
        mainWindowHandle, NULL, NULL, NULL);

    if (!speedPopupWindow) return;

    SetWindowLong(speedPopupWindow, GWL_STYLE, GetWindowLong(speedPopupWindow, GWL_STYLE) & ~WS_SIZEBOX); // Отключаем изменение размера
    ShowWindow(speedPopupWindow, SW_SHOWNORMAL); // Показываем окно
    UpdateWindow(speedPopupWindow);             // Обновляем окно

    // Добавляем иконку в окно
    appIcon = LoadIcon(NULL, IDI_INFORMATION);
    HWND iconControl = CreateWindowEx(0, L"STATIC", NULL,
        WS_CHILD | WS_VISIBLE | SS_ICON, 10, 10, 32, 32, speedPopupWindow, NULL, NULL, NULL);
    SendMessage(iconControl, STM_SETICON, (WPARAM)appIcon, 0);

    // Добавляем текст с информацией о скорости
    CreateWindowEx(0, L"STATIC", L"Скорость: 4.2 Мбит/с",
        WS_CHILD | WS_VISIBLE | SS_LEFT, 50, 18, 200, 20, speedPopupWindow, NULL, NULL, NULL);
}
/**
 * Обработчик сообщений для окна "Информация о скорости".
 * @param hwnd Дескриптор окна.
 * @param msg Сообщение.
 * @param wParam Параметр сообщения (WPARAM).
 * @param lParam Параметр сообщения (LPARAM).
 * @return Результат обработки сообщения.
 */
LRESULT CALLBACK SpeedWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // Если нажата кнопка с ID 1
                ShowWindow(hwnd, SW_HIDE); // Скрываем окно
            }
            break;

        case WM_CLOSE: // Обработка закрытия окна
            ShowWindow(hwnd, SW_HIDE); // Вместо закрытия просто скрываем окно
            return 0;

        case WM_DESTROY: // Обработка уничтожения окна
            PostQuitMessage(0); // Завершаем приложение
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam); // Передаём необработанные сообщения системе
}

/**
 * Основной обработчик сообщений для главного окна приложения.
 * @param hwnd Дескриптор окна.
 * @param msg Сообщение.
 * @param wParam Параметр сообщения (WPARAM).
 * @param lParam Параметр сообщения (LPARAM).
 * @return Результат обработки сообщения.
 */
LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Текст справки
    LPCWSTR helpMessage =
        L"Справка по приложению VPN-клиент\n"
        L"Добро пожаловать в VPN-клиент!\n"
        L"Это приложение позволяет установить защищённое соединение с удалённым VPN-сервером "
        L"для обеспечения безопасности и конфиденциальности в интернете.\n"
        L"Основные функции:\n"
        L"- Профиль — просмотр и настройка пользовательского профиля\n"
        L"- Подключиться — установить VPN-соединение с сервером\n"
        L"- Скорость — показать текущую скорость соединения\n"
        L"- Справка — открыть данное окно помощи\n"
        L"- Выход — закрыть приложение и отключиться от VPN\n"
        L"Часто задаваемые вопросы:\n"
        L"1. Как подключиться к VPN?\n"
        L"   Нажмите правой кнопкой на иконке в трее, выберите «Подключиться».\n"
        L"   Если сервер доступен — соединение установится автоматически.\n"
        L"2. Как узнать текущую скорость соединения?\n"
        L"   Нажмите «Скорость» в контекстном меню — появится всплывающее окно с информацией.\n"
        L"3. У меня возникли проблемы. Что делать?\n"
        L"   Свяжитесь с поддержкой: hillariot2070@gmail.com\n";

    switch (msg) {
        case WM_TRAYICON: // Обработка событий иконки в трее
            if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
                POINT cursorPosition; // Получаем позицию курсора
                GetCursorPos(&cursorPosition);
                SetForegroundWindow(hwnd); // Устанавливаем фокус на окно
                TrackPopupMenu(trayMenu, TPM_RIGHTBUTTON, cursorPosition.x, cursorPosition.y, 0, hwnd, NULL);
                PostMessage(hwnd, WM_NULL, 0, 0); // Исправляем поведение меню
            }
            break;

        case WM_COMMAND: // Обработка команд из меню
            switch (LOWORD(wParam)) {
                case ID_TRAY_PROFILE: // Открытие профиля
                    MessageBox(NULL, L"Открытие профиля", L"Профиль", MB_OK);
                    break;

                case ID_TRAY_CONNECT: // Подключение к VPN
                    MessageBox(NULL, L"Идёт подключение к OpenVPN...", L"Подключение", MB_OK);
                    ConnectToVPN();
                    break;

                case ID_TRAY_SPEED: // Отображение скорости
                    ShowSpeedPopup();
                    break;

                case ID_TRAY_HELP: // Отображение справки
                    MessageBox(NULL, helpMessage, L"Справка по VPN", MB_OK);
                    break;

                case ID_TRAY_EXIT: // Выход из приложения
                    DisconnectVPN();
                    OnConnectClick();
                    break;
            }
            break;

        case WM_TIMER: // Обработка таймера
            if (wParam == 1 && speedPopupWindow) {
                ShowWindow(speedPopupWindow, SW_HIDE); // Скрываем окно скорости
            }
            break;

        case WM_MOUSELEAVE: // Обработка выхода курсора за пределы окна
            if ((HWND)wParam == speedPopupWindow) {
                ShowWindow(speedPopupWindow, SW_HIDE); // Скрываем окно скорости
            }
            break;

        case WM_DESTROY: // Обработка завершения работы приложения
            DisconnectVPN(); // Отключаем VPN
            Shell_NotifyIcon(NIM_DELETE, &trayIconData); // Удаляем иконку из трея
            PostQuitMessage(0); // Завершаем приложение
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam); // Передаём необработанные сообщения системе
}

/**
 * Точка входа в приложение.
 * @param hInstance Дескриптор экземпляра приложения.
 * @param hPrevInstance Дескриптор предыдущего экземпляра (не используется).
 * @param lpCmdLine Командная строка (не используется).
 * @param nCmdShow Флаг отображения окна (не используется).
 * @return Код завершения приложения.
 */
int APIENTRY WinMain(HINSTANCE appInstance, HINSTANCE prevInstance, LPSTR cmdLine, int showCmd) {
    // Регистрация класса окна для всплывающего окна "Информация о скорости"
    WNDCLASS speedWindowClass = {};
    speedWindowClass.lpfnWndProc = SpeedWindowProc; // Указываем обработчик сообщений
    speedWindowClass.hInstance = appInstance;      // Дескриптор экземпляра приложения
    speedWindowClass.lpszClassName = L"SpeedPopupClass"; // Имя класса окна
    RegisterClass(&speedWindowClass);              // Регистрируем класс окна

    // Регистрация класса главного окна приложения
    WNDCLASS mainWindowClass = {};
    mainWindowClass.lpfnWndProc = MainWindowProc;  // Указываем обработчик сообщений
    mainWindowClass.hInstance = appInstance;      // Дескриптор экземпляра приложения
    mainWindowClass.lpszClassName = L"MyTrayApp";  // Имя класса окна
    RegisterClass(&mainWindowClass);              // Регистрируем класс окна

    // Создание скрытого главного окна
    mainWindowHandle = CreateWindowEx(
        0,                                         // Нет дополнительных стилей
        mainWindowClass.lpszClassName,             // Имя класса окна
        L"HiddenWindow",                           // Заголовок окна (скрыто)
        0,                                         // Нет стилей
        0, 0, 0, 0,                                // Размеры и позиция (не важны)
        HWND_MESSAGE,                              // Окно сообщений
        NULL, appInstance, NULL);

    // Создание контекстного меню для иконки в трее
    trayMenu = CreatePopupMenu();
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_PROFILE, L"Профиль");
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_CONNECT, L"Подключиться");
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_SPEED, L"Скорость");
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_HELP, L"Справка");
    AppendMenu(trayMenu, MF_SEPARATOR, 0, NULL);   // Разделитель
    AppendMenu(trayMenu, MF_STRING, ID_TRAY_EXIT, L"Выход");

    // Настройка иконки в трее
    trayIconData.cbSize = sizeof(trayIconData);    // Размер структуры
    trayIconData.hWnd = mainWindowHandle;          // Дескриптор окна
    trayIconData.uID = 1;                          // Идентификатор иконки
    trayIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP; // Флаги
    trayIconData.uCallbackMessage = WM_TRAYICON;   // Сообщение для обработки событий
    trayIconData.hIcon = LoadIcon(appInstance, MAKEINTRESOURCE(IDI_ICON1)); // Загружаем иконку
    wcscpy_s(trayIconData.szTip, L"VPN-клиент на C++"); // Подсказка для иконки

    // Добавляем иконку в трей
    if (!Shell_NotifyIcon(NIM_ADD, &trayIconData)) {
        MessageBox(NULL, L"Ошибка при добавлении иконки в трей", L"Ошибка", MB_ICONERROR);
        return 1; // Завершаем приложение с ошибкой
    }

    // Цикл обработки сообщений
    MSG message;
    while (GetMessage(&message, NULL, 0, 0)) {
        TranslateMessage(&message); // Преобразуем сообщения клавиш
        DispatchMessage(&message);  // Передаём сообщения обработчику
    }

    return 0; // Завершаем приложение
}

/**
 * Переименование переменных:
   - hMenu ? trayMenu: Более понятное название для контекстного меню иконки в трее.
   - hwnd ? mainWindowHandle: Четко указывает, что это дескриптор главного окна.
   - nid ? trayIconData: Указывает, что это данные для иконки в трее.
 * Использованы более осмысленные имена классов и окон (например, SpeedPopupClass вместо SpeedWndProc).
 * Убраны лишние вызовы функций и добавлены проверки.
 */