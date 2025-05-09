// ** Подключение заголовков, определение макросов и начальные глобальные переменные
// Устанавливаем минимальную версию Windows (Windows Vista)
#define _WIN32_WINNT 0x0600

// Подключаем ресурсный заголовок (например, для иконки и строк)
#include "resource1.h"

// Стандартные библиотеки Windows
#include <windows.h>
#include <shellapi.h>     // Для работы с системным треем (Shell_NotifyIcon)
#include <iphlpapi.h>     // Для получения информации о сетевых интерфейсах
#include <fstream>        // Для работы с файлами
#include <string>
#include <thread>         // Для многопоточности (запуск OpenVPN в отдельном потоке)
#include <chrono>         // Для работы с таймером скорости соединения
#include <sstream>        // Для форматирования текста (в т.ч. JSON-данных)
#include <locale>         // Для конвертации между wstring и string
#include <codecvt>        // То же — для конвертации строк
#include <vector>         // Для хранения допустимых доменов электронной почты
#include <curl/curl.h>    // Для выполнения HTTP-запросов к серверу аутентификации

// Связываем необходимые библиотеки Windows
#pragma comment(lib, "Ws2_32.lib")     // Работа с сокетами
#pragma comment(lib, "iphlpapi.lib")   // Получение сетевой информации

// Определяем пользовательские сообщения и ID элементов UI
#define WM_TRAYICON (WM_USER + 1)      // Сообщение для клика по иконке в трее
#define ID_TRAY_EXIT 1001              // Кнопка выхода в меню трея
#define ID_TRAY_PROFILE 1002           // Кнопка "Профиль"
#define ID_TRAY_CONNECT 1003           // Кнопка "Подключиться"
#define ID_TRAY_SPEED 1004             // Кнопка "Скорость"
#define ID_TRAY_HELP 1005              // Кнопка "Справка"
#define ID_REPEAT_PASSWORD_EDIT 3003   // Поле повторного ввода пароля при регистрации

// Флаг авторизации и режим формы (вход / регистрация)
bool ID_HAVE_LOGIN = false;
bool isRegisterMode = false;

// Дескрипторы оконных элементов
HWND hRepeatPasswordEdit = NULL;       // Поле ввода повторного пароля
HWND hSpeedLabel = NULL;               // Метка для отображения скорости
HWND hBtnLogin, hBtnRegistration, hLabelLogin, hLabelPassword;

// Информация о запущенном процессе OpenVPN
PROCESS_INFORMATION vpnProcessInfo = {};

// Основное окно логина
HWND hLoginWnd = NULL;
HWND hUsernameEdit = NULL;             // Поле ввода логина
HWND hPasswordEdit = NULL;             // Поле ввода пароля

// Иконка трея и другие элементы UI
NOTIFYICONDATA nid = {};
HMENU hMenu;
HWND hwnd;
HICON hIcon;
HWND hSpeedWindow = NULL;


// ** Перебор и удаление дочерних окон
// Callback-функция для перечисления и удаления всех дочерних окон
BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) {
    DestroyWindow(hwndChild); // Уничтожаем дочернее окно
    return TRUE;
}

// Удаляет все дочерние элементы управления из указанного родительского окна
void DestroyAllControls(HWND hwndParent) {
    EnumChildWindows(hwndParent, EnumChildProc, 0);
}


// ** Обработка ответа от сервера (через libcurl)
// Callback-функция для записи данных ответа от сервера
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

// ** Отправка POST-запроса для входа
// Отправляет запрос на сервер для проверки логина и пароля
bool sendLoginRequest(const std::wstring& email, const std::wstring& password) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // URL эндпоинта аутентификации
        curl_easy_setopt(curl, CURLOPT_URL, "http://185.184.122.74:5000/auth_auth");

        // Преобразуем данные в UTF-8
        std::string jsonData = "{\"email\": \"" +
            std::string(email.begin(), email.end()) + "\", \"password\": \"" +
            std::string(password.begin(), password.end()) + "\"}";

        // Устанавливаем заголовки
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Отправляем JSON-тело
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());

        // Устанавливаем callback для чтения ответа
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // Выполняем запрос
        res = curl_easy_perform(curl);

        // Проверяем результат
        if (res != CURLE_OK) {
            MessageBoxW(NULL, L"Ошибка подключения к серверу", L"Ошибка", MB_ICONERROR);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();

        // Проверяем успешность аутентификации
        return response.find("\"success\":true") != std::string::npos;
    }

    curl_global_cleanup();
    return false;
}


// ** Функция SwitchMode — переключение между формами "Вход" и "Регистрация"
void SwitchMode(HWND hwnd, bool registerMode) {
    isRegisterMode = registerMode; // Обновляем флаг режима: true - регистрация, false - вход

    // Показываем или скрываем поле повторного ввода пароля
    ShowWindow(hRepeatPasswordEdit, registerMode ? SW_SHOW : SW_HIDE);

    // Получаем дескрипторы кнопок по их ID
    HWND hRegisterBtn = GetDlgItem(hwnd, ID_BTN_REGISTER);
    HWND hLoginBtn = GetDlgItem(hwnd, ID_BTN_LOGIN);

    // Меняем текст на кнопках в зависимости от режима
    SetWindowTextW(hRegisterBtn, registerMode ? L"Авторизоваться" : L"Регистрация");
    SetWindowTextW(hLoginBtn, registerMode ? L"Регистрация" : L"Войти");

    // Удаляем все текущие элементы управления из окна
    DestroyAllControls(hwnd);

    if (registerMode && !hRepeatPasswordEdit) {
        // Режим регистрации: создаём поля для логина, пароля и повторного пароля

        SetWindowText(hwnd, L"Регистрация"); // Меняем заголовок окна

        // Создаём поле ввода повторного пароля
        hRepeatPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            180, 100, 160, 20, hwnd, (HMENU)ID_REPEAT_PASSWORD_EDIT, NULL, NULL);

        // Метка для поля повторного пароля
        CreateWindowW(L"STATIC", L"Повторите пароль:", WS_VISIBLE | WS_CHILD,
            20, 100, 135, 20, hwnd, NULL, NULL, NULL);

        // Получаем размер клиентской области окна
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // Метки для полей ввода
        hLabelLogin = CreateWindowW(L"STATIC", L"Логин:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hLabelPassword = CreateWindowW(L"STATIC", L"Пароль:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);

        // Поля ввода логина и пароля
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            180, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            180, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        // Расширяем размер окна под новые элементы
        int desiredClientWidth = (clientRect.right - clientRect.left) + 60;
        int desiredClientHeight = (clientRect.bottom - clientRect.top) + 30;

        RECT windowRect = { 0, 0, desiredClientWidth, desiredClientHeight };
        AdjustWindowRect(&windowRect, GetWindowLong(hwnd, GWL_STYLE), FALSE); // Корректируем размеры с учётом стиля окна

        int finalWidth = windowRect.right - windowRect.left;
        int finalHeight = windowRect.bottom - windowRect.top;

        // Изменяем размер окна
        SetWindowPos(hwnd, NULL, 0, 0, finalWidth, finalHeight, SWP_NOMOVE | SWP_NOZORDER);

        // Прячем и снова показываем окно, чтобы обновить отображение
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_SHOW);

        // Кнопки действий
        hBtnLogin = CreateWindowW(L"BUTTON", L"Авторизация", WS_VISIBLE | WS_CHILD,
            60, 130, 100, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration = CreateWindowW(L"BUTTON", L"Регистрация", WS_VISIBLE | WS_CHILD,
            200, 130, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
    }
    else {
        // Режим входа: только логин и пароль

        SetWindowText(hwnd, L"Логин"); // Меняем заголовок окна

        // Получаем размер клиентской области
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // Метки
        hLabelLogin = CreateWindowW(L"STATIC", L"Логин:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hLabelPassword = CreateWindowW(L"STATIC", L"Пароль:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);

        // Поля ввода
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            110, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            110, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        // Корректируем размеры окна
        int desiredClientWidth = (clientRect.right - clientRect.left);
        int desiredClientHeight = (clientRect.bottom - clientRect.top);

        RECT windowRect = { 0, 0, desiredClientWidth, desiredClientHeight };
        AdjustWindowRect(&windowRect, GetWindowLong(hwnd, GWL_STYLE), FALSE);

        int finalWidth = windowRect.right - windowRect.left;
        int finalHeight = windowRect.bottom - windowRect.top;

        // Уменьшаем размер окна
        SetWindowPos(hwnd, NULL, 0, 0, finalWidth - 60, finalHeight - 30, SWP_NOMOVE | SWP_NOZORDER);

        // Прячем и снова показываем окно
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_SHOW);

        // Кнопки действий
        hBtnLogin = CreateWindowW(L"BUTTON", L"Войти", WS_VISIBLE | WS_CHILD,
            60, 100, 80, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration = CreateWindowW(L"BUTTON", L"Регистрация", WS_VISIBLE | WS_CHILD,
            160, 100, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);

        hRepeatPasswordEdit = 0; // Сбрасываем указатель
    }
}


// ** Функция преобразования строк
// Преобразует широкую строку (std::wstring) в UTF-8 строку (std::string)
std::string wstring_to_string(const std::wstring& wstr) {
    // Используем стандартный конвертер из wchar_t в UTF-8
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(wstr); // Конвертируем и возвращаем как обычную строку
}
// Прим: Эта функция нужна, чтобы передавать данные на сервер через libcurl, который работает с std::string (а не с std::wstring).


// ** Отправка запроса на регистрацию
// Отправляет POST-запрос для регистрации пользователя
bool sendRegistrationRequest(const std::string& email, const std::string& password) {
    CURL* curl;           // Дескриптор libcurl
    CURLcode res;         // Результат выполнения запроса
    std::string response; // Ответ от сервера

    // Инициализируем библиотеку libcurl
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (!curl) {
        // Если инициализация не удалась
        curl_global_cleanup();
        return false;
    }

    // Формируем JSON-тело запроса
    std::stringstream json;
    json << "{ \"email\": \"" << email << "\", \"password\": \"" << password << "\" }";
    std::string jsonStr = json.str();

    // Устанавливаем заголовки
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // Настраиваем параметры запроса
    curl_easy_setopt(curl, CURLOPT_URL, "http://185.184.122.74:5001/auth_reg"); // URL регистрации
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);                     // Заголовки
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());              // Тело запроса
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);             // Callback для ответа
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);                    // Буфер для данных

    // Выполняем запрос
    res = curl_easy_perform(curl);

    // Очищаем ресурсы
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    // Проверяем результат
    if (res != CURLE_OK) {
        return false;
    }

    // Проверяем успешность операции по ключу "success": true в ответе
    return response.find("\"success\":true") != std::string::npos;
}
// Были убраны лишние вызовы return -1 без контекста.


// ** Оконная процедура формы логина/регистрации
LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // Создаём статические метки "Логин" и "Пароль"
            hLabelLogin = CreateWindowW(L"STATIC", L"Логин:", WS_VISIBLE | WS_CHILD,
                20, 20, 80, 20, hwnd, NULL, NULL, NULL);

            // Поле ввода логина
            hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                110, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);

            // Метка для поля пароля
            hLabelPassword = CreateWindowW(L"STATIC", L"Пароль:", WS_VISIBLE | WS_CHILD,
                20, 60, 80, 20, hwnd, NULL, NULL, NULL);

            // Поле ввода пароля (с маской)
            hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
                110, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

            // Кнопка "Войти"
            hBtnLogin = CreateWindowW(L"BUTTON", L"Войти", WS_VISIBLE | WS_CHILD,
                60, 100, 80, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);

            // Кнопка "Регистрация"
            hBtnRegistration = CreateWindowW(L"BUTTON", L"Регистрация", WS_VISIBLE | WS_CHILD,
                160, 100, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_REGISTER) {
                // Переключаемся в режим регистрации при нажатии на кнопку "Регистрация"
                SwitchMode(hwnd, true);
                isRegisterMode = true;
                return 0;
            }

            if (LOWORD(wParam) == ID_BTN_LOGIN && isRegisterMode) {
                // Если в режиме регистрации нажата кнопка "Авторизация", переключаемся обратно
                SwitchMode(hwnd, false);
                isRegisterMode = false;
                return 0;
            }

            if (LOWORD(wParam) == ID_BTN_LOGIN && !isRegisterMode) {
                // Получаем текст из полей ввода логина и пароля
                wchar_t username[1000], password[1000];
                GetWindowTextW(hUsernameEdit, username, 1000);
                GetWindowTextW(hPasswordEdit, password, 1000);

                std::wstring userStr(username);     // Логин как wstring
                std::string passwordStr = wstring_to_string(std::wstring(password)); // Пароль как string

                // Проверяем, что оба поля заполнены
                if (userStr.empty() || passwordStr.empty()) {
                    MessageBoxW(hwnd, L"Заполните все поля.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                // Проверяем формат email
                if (userStr.find(L'@') == std::wstring::npos || userStr.find(L'.') == std::wstring::npos) {
                    MessageBoxW(hwnd, L"Введите корректный адрес электронной почты.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                // Разделяем email на домен
                size_t atPos = userStr.find(L'@');
                std::wstring domain = userStr.substr(atPos + 1);

                // Список разрешённых доменов
                std::vector<std::wstring> allowedDomains = {
                    L"gmail.com", L"yandex.ru", L"mail.ru", L"outlook.com",
                    L"yahoo.com", L"rambler.ru", L"protonmail.com", L"aol.com"
                };

                bool validDomain = false;
                for (const auto& d : allowedDomains) {
                    if (domain == d) {
                        validDomain = true;
                        break;
                    }
                }

                if (!validDomain) {
                    // Если домен не в списке разрешённых
                    MessageBoxW(hwnd, L"Допустимы только почты популярных сервисов.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                // Проверка сложности пароля
                int validPassword[4] = { 0, 0, 0, 0 };
                for (const auto& symbol : passwordStr) {
                    int symbol_int = int(symbol);
                    if (symbol_int > 64 && symbol_int < 91) {
                        validPassword[0] += 1; // Заглавные буквы
                    } else if (symbol_int > 96 && symbol_int < 123) {
                        validPassword[1] += 1; // Строчные буквы
                    } else if (symbol_int < 128 && isdigit(symbol)) {
                        validPassword[2] += 1; // Цифры
                    }
                    validPassword[3] += 1; // Общее количество символов
                }

                if (validPassword[3] < 8 ||
                    validPassword[2] == 0 ||
                    validPassword[1] == 0 ||
                    validPassword[0] == 0) {
                    // Пароль не соответствует требованиям
                    MessageBoxW(hwnd, L"Пароль должен состоять минимум из 8 символов, иметь 1 заглавную и строчную латинскую букву и цифру.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                // Отправляем запрос на сервер для авторизации
                if (sendLoginRequest(userStr, passwordStr)) {
                    MessageBoxW(hwnd, L"Авторизация успешна!", L"Авторизация", MB_OK);
                    ID_HAVE_LOGIN = true;
                } else {
                    MessageBoxW(hwnd, L"Указанный логин или пароль не существуют.", L"Ошибка", MB_ICONERROR);
                }

                ShowWindow(hwnd, SW_HIDE); // Скрываем окно после действия
            }

            if (LOWORD(wParam) == ID_BTN_LOGIN && isRegisterMode) {
                // Режим регистрации — обработка данных
                wchar_t username[1000], password[1000], repeatPassword[1000];
                GetWindowTextW(hUsernameEdit, username, 1000);
                GetWindowTextW(hPasswordEdit, password, 1000);
                GetWindowTextW(hRepeatPasswordEdit, repeatPassword, 1000);

                std::wstring userStr(username);
                std::string passwordStr = wstring_to_string(std::wstring(password));
                std::string repeatPasswordStr = wstring_to_string(std::wstring(repeatPassword));

                if (userStr.empty() || passwordStr.empty() || repeatPasswordStr.empty()) {
                    MessageBoxW(hwnd, L"Заполните все поля.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                if (passwordStr != repeatPasswordStr) {
                    MessageBoxW(hwnd, L"Пароли не совпадают.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                if (userStr.find(L'@') == std::wstring::npos || userStr.find(L'.') == std::wstring::npos) {
                    MessageBoxW(hwnd, L"Введите корректный адрес электронной почты.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                size_t atPos = userStr.find(L'@');
                std::wstring domain = userStr.substr(atPos + 1);

                std::vector<std::wstring> allowedDomains = {
                    L"gmail.com", L"yandex.ru", L"mail.ru", L"outlook.com",
                    L"yahoo.com", L"rambler.ru", L"protonmail.com", L"aol.com"
                };

                bool validDomain = false;
                for (const auto& d : allowedDomains) {
                    if (domain == d) {
                        validDomain = true;
                        break;
                    }
                }

                if (!validDomain) {
                    MessageBoxW(hwnd, L"Допустимы только почты популярных сервисов.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                int validPassword[4] = { 0, 0, 0, 0 };
                for (const auto& symbol : passwordStr) {
                    int symbol_int = int(symbol);
                    if (symbol_int > 64 && symbol_int < 91) {
                        validPassword[0] += 1;
                    } else if (symbol_int > 96 && symbol_int < 123) {
                        validPassword[1] += 1;
                    } else if (symbol_int < 128 && isdigit(symbol)) {
                        validPassword[2] += 1;
                    }
                    validPassword[3] += 1;
                }

                if (validPassword[3] < 8 ||
                    validPassword[2] == 0 ||
                    validPassword[1] == 0 ||
                    validPassword[0] == 0) {
                    MessageBoxW(hwnd, L"Пароль должен состоять минимум из 8 символов, иметь 1 заглавную и строчную латинскую букву и цифру.", L"Ошибка", MB_ICONERROR);
                    return -1;
                }

                if (sendRegistrationRequest(wstring_to_string(userStr), passwordStr)) {
                    MessageBoxW(hwnd, L"Регистрация успешна!", L"Регистрация", MB_OK);
                    ID_HAVE_LOGIN = true;
                } else {
                    MessageBoxW(hwnd, L"Ошибка регистрации.", L"Ошибка", MB_ICONERROR);
                }

                ShowWindow(hwnd, SW_HIDE);
            }
            break;

        case WM_CLOSE:
            // При закрытии окна просто скрываем его, не завершая программу
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT); // Устанавливаем прозрачность фона для статических элементов
            return (INT_PTR)GetStockObject(HOLLOW_BRUSH); // Используем пустую кисть
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// ** Извлечение ресурса из исполняемого файла в файл
bool WriteResourceToFile(int resourceId, const std::wstring& outputPath) {
    HMODULE hModule = GetModuleHandle(NULL); // Получаем дескриптор текущего модуля (.exe)

    // Находим ресурс по ID и типу RT_RCDATA (бинарные данные)
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) return false; // Если ресурс не найден — выходим с ошибкой

    HGLOBAL hResData = LoadResource(hModule, hRes); // Загружаем ресурс в память
    DWORD resSize = SizeofResource(hModule, hRes); // Получаем размер ресурса
    void* pResData = LockResource(hResData); // Блокируем память и получаем указатель на данные

    if (!pResData) return false; // Если не удалось получить доступ к данным — ошибка

    // !! Измённый код в блоке с данного момента:
    // Открываем или создаём файл для записи
    HANDLE hFile = CreateFileW(outputPath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) return false; // Ошибка создания файла

    DWORD bytesWritten;
    // Записываем данные из ресурса в файл
    BOOL success = WriteFile(hFile, pResData, resSize, &bytesWritten, NULL);

    // Закрываем дескриптор файла
    CloseHandle(hFile);

    // Возвращаем результат операции
    return success && (bytesWritten == resSize);
}

// !! Была полностью изменена структура ConnectToVPN
// ** Запуск OpenVPN
void ConnectToVPN() {
    try {
        // Получаем временную папку для распаковки файлов OpenVPN
        wchar_t tempPath1[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath1); // Получаем путь к временной директории

        // Пути к необходимым файлам OpenVPN
        std::wstring exePath = std::wstring(tempPath1) + L"openvpn.exe";
        std::wstring configPath = std::wstring(tempPath1) + L"config.ovpn";
        std::wstring lib1Path = std::wstring(tempPath1) + L"libeay32.dll";
        std::wstring lib2Path = std::wstring(tempPath1) + L"libpkcs11-helper-1.dll";
        std::wstring lib3Path = std::wstring(tempPath1) + L"libssl-3-x64.dll";
        std::wstring wintun = std::wstring(tempPath1) + L"wintun.dll";

        // Извлекаем все ресурсы (файлы OpenVPN) во временную папку
        if (!WriteResourceToFile(IDR_RCDATA4, exePath) ||
            !WriteResourceToFile(IDR_RCDATA5, configPath) ||
            !WriteResourceToFile(IDR_RCDATA1, lib1Path) ||
            !WriteResourceToFile(IDR_RCDATA2, lib2Path) ||
            !WriteResourceToFile(IDR_RCDATA3, lib3Path) ||
            !WriteResourceToFile(IDR_RCDATA6, wintun)) {
            MessageBox(NULL, L"Ошибка извлечения файлов OpenVPN", L"Ошибка", MB_ICONERROR);
            return;
        }

        // Подготавливаем командную строку для запуска OpenVPN
        std::wstring cmdLine = L"\"" + exePath + L"\" --config \"" + configPath + L"\"";

        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(SHELLEXECUTEINFOW);
        sei.lpVerb = L"runas";              // Запуск от имени администратора
        sei.lpFile = L"cmd.exe";            // Используем командную строку
        sei.lpParameters = L"/c " + cmdLine; // Передаём команду запуска OpenVPN
        sei.nShow = SW_HIDE;                // Скрываем окно консоли

        // Запускаем OpenVPN
        if (!ShellExecuteExW(&sei)) {
            DWORD err = GetLastError(); // Получаем код ошибки
            wchar_t buffer[256];
            wsprintf(buffer, L"Ошибка при запуске OpenVPN. Код ошибки: %lu", err);
            MessageBoxW(NULL, buffer, L"Ошибка", MB_ICONERROR);
        } else {
            vpnProcessInfo.hProcess = sei.hProcess; // Сохраняем дескриптор процесса
            MessageBoxW(NULL, L"OpenVPN успешно запущен.", L"Успех", MB_ICONINFORMATION);
        }

        // Планируем удаление временных файлов после перезагрузки
        MoveFileExW(exePath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(configPath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib1Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib2Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib3Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(wintun.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    } catch (...) {
        MessageBoxW(NULL, L"Неизвестная ошибка при подключении к VPN.", L"Ошибка", MB_ICONERROR);
    }
}


// ** Обработка клика на "Подключиться"
// Обработчик нажатия на пункт меню "Подключиться"
void OnConnectClick() {
    // Запускаем подключение к VPN в отдельном потоке
    std::thread vpnThread(ConnectToVPN);
    vpnThread.detach(); // Отделяем поток, чтобы он работал независимо

    Shell_NotifyIcon(NIM_DELETE, &nid); // Удаляем иконку из трея
    PostQuitMessage(0); // Завершаем приложение
}


// ** Завершение работы OpenVPN
// Принудительно завершает процесс OpenVPN
void DisconnectVPN() {
    system("taskkill /F /IM openvpn.exe /T"); // Убиваем процесс и его дерево

    // Если есть сохранённый дескриптор процесса — завершаем его
    if (vpnProcessInfo.hProcess) {
        TerminateProcess(vpnProcessInfo.hProcess, 0); // Завершаем процесс
        CloseHandle(vpnProcessInfo.hProcess);         // Освобождаем хендл
        vpnProcessInfo.hProcess = NULL;               // Обнуляем указатель
    }

    // Если процесс был запущен как служба — останавливаем её
    system("sc stop openvpnservice");
}


// ** Получение сетевой статистики
std::wstring GetTotalNetworkSpeed() {
    // Статические переменные сохраняют значения между вызовами функции
    static ULONGLONG lastInOctetsBytes = 0, lastOutOctetsBytes = 0;
    static auto lastTime = std::chrono::steady_clock::now();

    MIB_IFROW interfaceRow;
    ZeroMemory(&interfaceRow, sizeof(MIB_IFROW));
    interfaceRow.dwType = IF_TYPE_SOFTWARE_LOOPBACK; // Можно изменить под нужный тип интерфейса

    DWORD dwRetVal = GetIfEntry(&interfaceRow);
    if (dwRetVal != NO_ERROR) return L"Ошибка получения данных сети.";

    auto now = std::chrono::steady_clock::now();
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTime).count();

    ULONGLONG currentInOctetsBytes = interfaceRow.dwInOctets;
    ULONGLONG currentOutOctetsBytes = interfaceRow.dwOutOctets;

    // Проверяем на аномальные скачки в данных
    if ((currentInOctetsBytes < lastInOctetsBytes) || (currentOutOctetsBytes < lastOutOctetsBytes)) {
        // Если данные уменьшились, возможно, произошёл сброс счётчика
        lastInOctetsBytes = currentInOctetsBytes;
        lastOutOctetsBytes = currentOutOctetsBytes;
        lastTime = now;
        return L"Подождите...";
    }

    ULONGLONG inDiff = currentInOctetsBytes - lastInOctetsBytes;
    ULONGLONG outDiff = currentOutOctetsBytes - lastOutOctetsBytes;

    double bytesPerSecondInOctets = inDiff / elapsedSeconds;
    double bytesPerSecondOutOctets = outDiff / elapsedSeconds;

    // Сохраняем текущие значения для следующего вызова
    lastInOctetsBytes = currentInOctetsBytes;
    lastOutOctetsBytes = currentOutOctetsBytes;
    lastTime = now;

    // Переводим биты в байты
    bytesPerSecondInOctets /= 8.0;
    bytesPerSecondOutOctets /= 8.0;

    // Формируем выводимую строку
    std::wstringstream ss;
    ss.precision(2); // Два знака после запятой
    ss << std::fixed;

    ss << L"Входящий трафик: ";
    if (bytesPerSecondInOctets >= 1024 * 1024) {
        double mb = bytesPerSecondInOctets / (1024.0 * 1024.0);
        ss << mb << L" МБ/с";
    } else if (bytesPerSecondInOctets >= 1024) {
        double kb = bytesPerSecondInOctets / 1024.0;
        ss << kb << L" КБ/с";
    } else {
        ss << bytesPerSecondInOctets << L" Б/с";
    }

    ss << L"\nИсходящий трафик: ";
    if (bytesPerSecondOutOctets >= 1024 * 1024) {
        double mb = bytesPerSecondOutOctets / (1024.0 * 1024.0);
        ss << mb << L" МБ/с";
    } else if (bytesPerSecondOutOctets >= 1024) {
        double kb = bytesPerSecondOutOctets / 1024.0;
        ss << kb << L" КБ/с";
    } else {
        ss << bytesPerSecondOutOctets << L" Б/с";
    }

    if (lastInOctetsBytes != 0 || lastOutOctetsBytes != 0)
        return ss.str(); // Возвращаем результат
    else {
        // Если первые данные ещё не собраны
        lastInOctetsBytes = currentInOctetsBytes;
        lastOutOctetsBytes = currentOutOctetsBytes;
        lastTime = now;
        return L"Подождите...";
    }
}


// ** Обновление метки с информацией о скорости на лейбле
void UpdateSpeedLabel(HWND hwnd) {
    std::wstring speed = GetTotalNetworkSpeed(); // Получаем текущую скорость
    SetWindowText(hSpeedLabel, speed.c_str());   // Обновляем текст лейбла
}


// !! Была полностью изменена структура SpeedWndProc
// ** Оконная процедура окна скорости
LRESULT CALLBACK SpeedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // Нажата кнопка с ID 1
                ShowWindow(hwnd, SW_HIDE); // Скрываем окно
            }
            break;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE); // Не закрываем, просто скрываем
            return 0;

        case WM_DESTROY:
            break;

        case WM_TIMER:
            UpdateSpeedLabel(hwnd); // Обновляем каждую секунду по таймеру
            break;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT); // Прозрачный фон
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW); // Цвет фона по умолчанию
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// ** Функция для создания всплывающего окна скорости
void ShowSpeedPopup() {
    if (hSpeedWindow) {
        ShowWindow(hSpeedWindow, SW_SHOW);      // Показываем уже созданное окно
        SetForegroundWindow(hSpeedWindow);      // Делаем активным
        return;
    }

    HINSTANCE hInst = GetModuleHandle(NULL);
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));

    WNDCLASS speedClass = {};
    speedClass.lpfnWndProc = SpeedWndProc;
    speedClass.hInstance = hInst;
    speedClass.lpszClassName = L"SpeedPopupClass";
    speedClass.hIcon = hIcon;

    RegisterClass(&speedClass);

    hSpeedWindow = CreateWindowExW(
        WS_EX_TOPMOST,
        L"SpeedPopupClass",
        L"Скорость соединения",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 340, 100,
        NULL, NULL, hInst, NULL);

    hSpeedLabel = CreateWindowEx(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 20, 300, 30,
        hSpeedWindow, NULL, GetModuleHandle(NULL), NULL);

    SetTimer(hSpeedWindow, 1, 1000, NULL); // Таймер обновления раз в секунду
}


// !! Оставшаяся часть кода была оставлена без изменений
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
            if (!hLoginWnd) {
                HINSTANCE hInst = GetModuleHandle(NULL);
                HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
                WNDCLASS wc = {};
                wc.lpfnWndProc = LoginWndProc;
                wc.hInstance = GetModuleHandle(NULL);
                wc.lpszClassName = L"LoginWindowClass";
                wc.hIcon = hIcon;
                RegisterClass(&wc);

                hLoginWnd = CreateWindowExW(0, L"LoginWindowClass", L"Логин",
                    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                    CW_USEDEFAULT, CW_USEDEFAULT, 320, 180,
                    NULL, NULL, GetModuleHandle(NULL), NULL);
            }
            ShowWindow(hLoginWnd, SW_SHOWNORMAL);
            SetForegroundWindow(hLoginWnd);
            break;

        case ID_TRAY_CONNECT:
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
