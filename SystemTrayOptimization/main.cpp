// ** ����������� ����������, ����������� �������� � ��������� ���������� ����������
// ������������� ����������� ������ Windows (Windows Vista)
#define _WIN32_WINNT 0x0600

// ���������� ��������� ��������� (��������, ��� ������ � �����)
#include "resource1.h"

// ����������� ���������� Windows
#include <windows.h>
#include <shellapi.h>     // ��� ������ � ��������� ����� (Shell_NotifyIcon)
#include <iphlpapi.h>     // ��� ��������� ���������� � ������� �����������
#include <fstream>        // ��� ������ � �������
#include <string>
#include <thread>         // ��� ��������������� (������ OpenVPN � ��������� ������)
#include <chrono>         // ��� ������ � �������� �������� ����������
#include <sstream>        // ��� �������������� ������ (� �.�. JSON-������)
#include <locale>         // ��� ����������� ����� wstring � string
#include <codecvt>        // �� �� � ��� ����������� �����
#include <vector>         // ��� �������� ���������� ������� ����������� �����
#include <curl/curl.h>    // ��� ���������� HTTP-�������� � ������� ��������������

// ��������� ����������� ���������� Windows
#pragma comment(lib, "Ws2_32.lib")     // ������ � ��������
#pragma comment(lib, "iphlpapi.lib")   // ��������� ������� ����������

// ���������� ���������������� ��������� � ID ��������� UI
#define WM_TRAYICON (WM_USER + 1)      // ��������� ��� ����� �� ������ � ����
#define ID_TRAY_EXIT 1001              // ������ ������ � ���� ����
#define ID_TRAY_PROFILE 1002           // ������ "�������"
#define ID_TRAY_CONNECT 1003           // ������ "������������"
#define ID_TRAY_SPEED 1004             // ������ "��������"
#define ID_TRAY_HELP 1005              // ������ "�������"
#define ID_REPEAT_PASSWORD_EDIT 3003   // ���� ���������� ����� ������ ��� �����������

// ���� ����������� � ����� ����� (���� / �����������)
bool ID_HAVE_LOGIN = false;
bool isRegisterMode = false;

// ����������� ������� ���������
HWND hRepeatPasswordEdit = NULL;       // ���� ����� ���������� ������
HWND hSpeedLabel = NULL;               // ����� ��� ����������� ��������
HWND hBtnLogin, hBtnRegistration, hLabelLogin, hLabelPassword;

// ���������� � ���������� �������� OpenVPN
PROCESS_INFORMATION vpnProcessInfo = {};

// �������� ���� ������
HWND hLoginWnd = NULL;
HWND hUsernameEdit = NULL;             // ���� ����� ������
HWND hPasswordEdit = NULL;             // ���� ����� ������

// ������ ���� � ������ �������� UI
NOTIFYICONDATA nid = {};
HMENU hMenu;
HWND hwnd;
HICON hIcon;
HWND hSpeedWindow = NULL;


// ** ������� � �������� �������� ����
// Callback-������� ��� ������������ � �������� ���� �������� ����
BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) {
    DestroyWindow(hwndChild); // ���������� �������� ����
    return TRUE;
}

// ������� ��� �������� �������� ���������� �� ���������� ������������� ����
void DestroyAllControls(HWND hwndParent) {
    EnumChildWindows(hwndParent, EnumChildProc, 0);
}


// ** ��������� ������ �� ������� (����� libcurl)
// Callback-������� ��� ������ ������ ������ �� �������
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

// ** �������� POST-������� ��� �����
// ���������� ������ �� ������ ��� �������� ������ � ������
bool sendLoginRequest(const std::wstring& email, const std::wstring& password) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // URL ��������� ��������������
        curl_easy_setopt(curl, CURLOPT_URL, "http://185.184.122.74:5000/auth_auth");

        // ����������� ������ � UTF-8
        std::string jsonData = "{\"email\": \"" +
            std::string(email.begin(), email.end()) + "\", \"password\": \"" +
            std::string(password.begin(), password.end()) + "\"}";

        // ������������� ���������
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // ���������� JSON-����
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());

        // ������������� callback ��� ������ ������
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // ��������� ������
        res = curl_easy_perform(curl);

        // ��������� ���������
        if (res != CURLE_OK) {
            MessageBoxW(NULL, L"������ ����������� � �������", L"������", MB_ICONERROR);
            curl_easy_cleanup(curl);
            curl_global_cleanup();
            return false;
        }

        curl_easy_cleanup(curl);
        curl_global_cleanup();

        // ��������� ���������� ��������������
        return response.find("\"success\":true") != std::string::npos;
    }

    curl_global_cleanup();
    return false;
}


// ** ������� SwitchMode � ������������ ����� ������� "����" � "�����������"
void SwitchMode(HWND hwnd, bool registerMode) {
    isRegisterMode = registerMode; // ��������� ���� ������: true - �����������, false - ����

    // ���������� ��� �������� ���� ���������� ����� ������
    ShowWindow(hRepeatPasswordEdit, registerMode ? SW_SHOW : SW_HIDE);

    // �������� ����������� ������ �� �� ID
    HWND hRegisterBtn = GetDlgItem(hwnd, ID_BTN_REGISTER);
    HWND hLoginBtn = GetDlgItem(hwnd, ID_BTN_LOGIN);

    // ������ ����� �� ������� � ����������� �� ������
    SetWindowTextW(hRegisterBtn, registerMode ? L"��������������" : L"�����������");
    SetWindowTextW(hLoginBtn, registerMode ? L"�����������" : L"�����");

    // ������� ��� ������� �������� ���������� �� ����
    DestroyAllControls(hwnd);

    if (registerMode && !hRepeatPasswordEdit) {
        // ����� �����������: ������ ���� ��� ������, ������ � ���������� ������

        SetWindowText(hwnd, L"�����������"); // ������ ��������� ����

        // ������ ���� ����� ���������� ������
        hRepeatPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            180, 100, 160, 20, hwnd, (HMENU)ID_REPEAT_PASSWORD_EDIT, NULL, NULL);

        // ����� ��� ���� ���������� ������
        CreateWindowW(L"STATIC", L"��������� ������:", WS_VISIBLE | WS_CHILD,
            20, 100, 135, 20, hwnd, NULL, NULL, NULL);

        // �������� ������ ���������� ������� ����
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // ����� ��� ����� �����
        hLabelLogin = CreateWindowW(L"STATIC", L"�����:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hLabelPassword = CreateWindowW(L"STATIC", L"������:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);

        // ���� ����� ������ � ������
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            180, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            180, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        // ��������� ������ ���� ��� ����� ��������
        int desiredClientWidth = (clientRect.right - clientRect.left) + 60;
        int desiredClientHeight = (clientRect.bottom - clientRect.top) + 30;

        RECT windowRect = { 0, 0, desiredClientWidth, desiredClientHeight };
        AdjustWindowRect(&windowRect, GetWindowLong(hwnd, GWL_STYLE), FALSE); // ������������ ������� � ������ ����� ����

        int finalWidth = windowRect.right - windowRect.left;
        int finalHeight = windowRect.bottom - windowRect.top;

        // �������� ������ ����
        SetWindowPos(hwnd, NULL, 0, 0, finalWidth, finalHeight, SWP_NOMOVE | SWP_NOZORDER);

        // ������ � ����� ���������� ����, ����� �������� �����������
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_SHOW);

        // ������ ��������
        hBtnLogin = CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            60, 130, 100, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration = CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            200, 130, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
    }
    else {
        // ����� �����: ������ ����� � ������

        SetWindowText(hwnd, L"�����"); // ������ ��������� ����

        // �������� ������ ���������� �������
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);

        // �����
        hLabelLogin = CreateWindowW(L"STATIC", L"�����:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hLabelPassword = CreateWindowW(L"STATIC", L"������:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);

        // ���� �����
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            110, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            110, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        // ������������ ������� ����
        int desiredClientWidth = (clientRect.right - clientRect.left);
        int desiredClientHeight = (clientRect.bottom - clientRect.top);

        RECT windowRect = { 0, 0, desiredClientWidth, desiredClientHeight };
        AdjustWindowRect(&windowRect, GetWindowLong(hwnd, GWL_STYLE), FALSE);

        int finalWidth = windowRect.right - windowRect.left;
        int finalHeight = windowRect.bottom - windowRect.top;

        // ��������� ������ ����
        SetWindowPos(hwnd, NULL, 0, 0, finalWidth - 60, finalHeight - 30, SWP_NOMOVE | SWP_NOZORDER);

        // ������ � ����� ���������� ����
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_SHOW);

        // ������ ��������
        hBtnLogin = CreateWindowW(L"BUTTON", L"�����", WS_VISIBLE | WS_CHILD,
            60, 100, 80, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration = CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            160, 100, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);

        hRepeatPasswordEdit = 0; // ���������� ���������
    }
}


// ** ������� �������������� �����
// ����������� ������� ������ (std::wstring) � UTF-8 ������ (std::string)
std::string wstring_to_string(const std::wstring& wstr) {
    // ���������� ����������� ��������� �� wchar_t � UTF-8
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(wstr); // ������������ � ���������� ��� ������� ������
}
// ����: ��� ������� �����, ����� ���������� ������ �� ������ ����� libcurl, ������� �������� � std::string (� �� � std::wstring).


// ** �������� ������� �� �����������
// ���������� POST-������ ��� ����������� ������������
bool sendRegistrationRequest(const std::string& email, const std::string& password) {
    CURL* curl;           // ���������� libcurl
    CURLcode res;         // ��������� ���������� �������
    std::string response; // ����� �� �������

    // �������������� ���������� libcurl
    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    if (!curl) {
        // ���� ������������� �� �������
        curl_global_cleanup();
        return false;
    }

    // ��������� JSON-���� �������
    std::stringstream json;
    json << "{ \"email\": \"" << email << "\", \"password\": \"" << password << "\" }";
    std::string jsonStr = json.str();

    // ������������� ���������
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // ����������� ��������� �������
    curl_easy_setopt(curl, CURLOPT_URL, "http://185.184.122.74:5001/auth_reg"); // URL �����������
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);                     // ���������
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());              // ���� �������
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);             // Callback ��� ������
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);                    // ����� ��� ������

    // ��������� ������
    res = curl_easy_perform(curl);

    // ������� �������
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    // ��������� ���������
    if (res != CURLE_OK) {
        return false;
    }

    // ��������� ���������� �������� �� ����� "success": true � ������
    return response.find("\"success\":true") != std::string::npos;
}
// ���� ������ ������ ������ return -1 ��� ���������.


// ** ������� ��������� ����� ������/�����������
LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE:
            // ������ ����������� ����� "�����" � "������"
            hLabelLogin = CreateWindowW(L"STATIC", L"�����:", WS_VISIBLE | WS_CHILD,
                20, 20, 80, 20, hwnd, NULL, NULL, NULL);

            // ���� ����� ������
            hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
                110, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);

            // ����� ��� ���� ������
            hLabelPassword = CreateWindowW(L"STATIC", L"������:", WS_VISIBLE | WS_CHILD,
                20, 60, 80, 20, hwnd, NULL, NULL, NULL);

            // ���� ����� ������ (� ������)
            hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
                110, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

            // ������ "�����"
            hBtnLogin = CreateWindowW(L"BUTTON", L"�����", WS_VISIBLE | WS_CHILD,
                60, 100, 80, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);

            // ������ "�����������"
            hBtnRegistration = CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
                160, 100, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
            break;

        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_REGISTER) {
                // ������������� � ����� ����������� ��� ������� �� ������ "�����������"
                SwitchMode(hwnd, true);
                isRegisterMode = true;
                return 0;
            }

            if (LOWORD(wParam) == ID_BTN_LOGIN && isRegisterMode) {
                // ���� � ������ ����������� ������ ������ "�����������", ������������� �������
                SwitchMode(hwnd, false);
                isRegisterMode = false;
                return 0;
            }

            if (LOWORD(wParam) == ID_BTN_LOGIN && !isRegisterMode) {
                // �������� ����� �� ����� ����� ������ � ������
                wchar_t username[1000], password[1000];
                GetWindowTextW(hUsernameEdit, username, 1000);
                GetWindowTextW(hPasswordEdit, password, 1000);

                std::wstring userStr(username);     // ����� ��� wstring
                std::string passwordStr = wstring_to_string(std::wstring(password)); // ������ ��� string

                // ���������, ��� ��� ���� ���������
                if (userStr.empty() || passwordStr.empty()) {
                    MessageBoxW(hwnd, L"��������� ��� ����.", L"������", MB_ICONERROR);
                    return -1;
                }

                // ��������� ������ email
                if (userStr.find(L'@') == std::wstring::npos || userStr.find(L'.') == std::wstring::npos) {
                    MessageBoxW(hwnd, L"������� ���������� ����� ����������� �����.", L"������", MB_ICONERROR);
                    return -1;
                }

                // ��������� email �� �����
                size_t atPos = userStr.find(L'@');
                std::wstring domain = userStr.substr(atPos + 1);

                // ������ ����������� �������
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
                    // ���� ����� �� � ������ �����������
                    MessageBoxW(hwnd, L"��������� ������ ����� ���������� ��������.", L"������", MB_ICONERROR);
                    return -1;
                }

                // �������� ��������� ������
                int validPassword[4] = { 0, 0, 0, 0 };
                for (const auto& symbol : passwordStr) {
                    int symbol_int = int(symbol);
                    if (symbol_int > 64 && symbol_int < 91) {
                        validPassword[0] += 1; // ��������� �����
                    } else if (symbol_int > 96 && symbol_int < 123) {
                        validPassword[1] += 1; // �������� �����
                    } else if (symbol_int < 128 && isdigit(symbol)) {
                        validPassword[2] += 1; // �����
                    }
                    validPassword[3] += 1; // ����� ���������� ��������
                }

                if (validPassword[3] < 8 ||
                    validPassword[2] == 0 ||
                    validPassword[1] == 0 ||
                    validPassword[0] == 0) {
                    // ������ �� ������������� �����������
                    MessageBoxW(hwnd, L"������ ������ �������� ������� �� 8 ��������, ����� 1 ��������� � �������� ��������� ����� � �����.", L"������", MB_ICONERROR);
                    return -1;
                }

                // ���������� ������ �� ������ ��� �����������
                if (sendLoginRequest(userStr, passwordStr)) {
                    MessageBoxW(hwnd, L"����������� �������!", L"�����������", MB_OK);
                    ID_HAVE_LOGIN = true;
                } else {
                    MessageBoxW(hwnd, L"��������� ����� ��� ������ �� ����������.", L"������", MB_ICONERROR);
                }

                ShowWindow(hwnd, SW_HIDE); // �������� ���� ����� ��������
            }

            if (LOWORD(wParam) == ID_BTN_LOGIN && isRegisterMode) {
                // ����� ����������� � ��������� ������
                wchar_t username[1000], password[1000], repeatPassword[1000];
                GetWindowTextW(hUsernameEdit, username, 1000);
                GetWindowTextW(hPasswordEdit, password, 1000);
                GetWindowTextW(hRepeatPasswordEdit, repeatPassword, 1000);

                std::wstring userStr(username);
                std::string passwordStr = wstring_to_string(std::wstring(password));
                std::string repeatPasswordStr = wstring_to_string(std::wstring(repeatPassword));

                if (userStr.empty() || passwordStr.empty() || repeatPasswordStr.empty()) {
                    MessageBoxW(hwnd, L"��������� ��� ����.", L"������", MB_ICONERROR);
                    return -1;
                }

                if (passwordStr != repeatPasswordStr) {
                    MessageBoxW(hwnd, L"������ �� ���������.", L"������", MB_ICONERROR);
                    return -1;
                }

                if (userStr.find(L'@') == std::wstring::npos || userStr.find(L'.') == std::wstring::npos) {
                    MessageBoxW(hwnd, L"������� ���������� ����� ����������� �����.", L"������", MB_ICONERROR);
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
                    MessageBoxW(hwnd, L"��������� ������ ����� ���������� ��������.", L"������", MB_ICONERROR);
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
                    MessageBoxW(hwnd, L"������ ������ �������� ������� �� 8 ��������, ����� 1 ��������� � �������� ��������� ����� � �����.", L"������", MB_ICONERROR);
                    return -1;
                }

                if (sendRegistrationRequest(wstring_to_string(userStr), passwordStr)) {
                    MessageBoxW(hwnd, L"����������� �������!", L"�����������", MB_OK);
                    ID_HAVE_LOGIN = true;
                } else {
                    MessageBoxW(hwnd, L"������ �����������.", L"������", MB_ICONERROR);
                }

                ShowWindow(hwnd, SW_HIDE);
            }
            break;

        case WM_CLOSE:
            // ��� �������� ���� ������ �������� ���, �� �������� ���������
            ShowWindow(hwnd, SW_HIDE);
            return 0;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT); // ������������� ������������ ���� ��� ����������� ���������
            return (INT_PTR)GetStockObject(HOLLOW_BRUSH); // ���������� ������ �����
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// ** ���������� ������� �� ������������ ����� � ����
bool WriteResourceToFile(int resourceId, const std::wstring& outputPath) {
    HMODULE hModule = GetModuleHandle(NULL); // �������� ���������� �������� ������ (.exe)

    // ������� ������ �� ID � ���� RT_RCDATA (�������� ������)
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(resourceId), RT_RCDATA);
    if (!hRes) return false; // ���� ������ �� ������ � ������� � �������

    HGLOBAL hResData = LoadResource(hModule, hRes); // ��������� ������ � ������
    DWORD resSize = SizeofResource(hModule, hRes); // �������� ������ �������
    void* pResData = LockResource(hResData); // ��������� ������ � �������� ��������� �� ������

    if (!pResData) return false; // ���� �� ������� �������� ������ � ������ � ������

    // !! ������� ��� � ����� � ������� �������:
    // ��������� ��� ������ ���� ��� ������
    HANDLE hFile = CreateFileW(outputPath.c_str(), GENERIC_WRITE, 0, NULL,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile == INVALID_HANDLE_VALUE) return false; // ������ �������� �����

    DWORD bytesWritten;
    // ���������� ������ �� ������� � ����
    BOOL success = WriteFile(hFile, pResData, resSize, &bytesWritten, NULL);

    // ��������� ���������� �����
    CloseHandle(hFile);

    // ���������� ��������� ��������
    return success && (bytesWritten == resSize);
}

// !! ���� ��������� �������� ��������� ConnectToVPN
// ** ������ OpenVPN
void ConnectToVPN() {
    try {
        // �������� ��������� ����� ��� ���������� ������ OpenVPN
        wchar_t tempPath1[MAX_PATH];
        GetTempPathW(MAX_PATH, tempPath1); // �������� ���� � ��������� ����������

        // ���� � ����������� ������ OpenVPN
        std::wstring exePath = std::wstring(tempPath1) + L"openvpn.exe";
        std::wstring configPath = std::wstring(tempPath1) + L"config.ovpn";
        std::wstring lib1Path = std::wstring(tempPath1) + L"libeay32.dll";
        std::wstring lib2Path = std::wstring(tempPath1) + L"libpkcs11-helper-1.dll";
        std::wstring lib3Path = std::wstring(tempPath1) + L"libssl-3-x64.dll";
        std::wstring wintun = std::wstring(tempPath1) + L"wintun.dll";

        // ��������� ��� ������� (����� OpenVPN) �� ��������� �����
        if (!WriteResourceToFile(IDR_RCDATA4, exePath) ||
            !WriteResourceToFile(IDR_RCDATA5, configPath) ||
            !WriteResourceToFile(IDR_RCDATA1, lib1Path) ||
            !WriteResourceToFile(IDR_RCDATA2, lib2Path) ||
            !WriteResourceToFile(IDR_RCDATA3, lib3Path) ||
            !WriteResourceToFile(IDR_RCDATA6, wintun)) {
            MessageBox(NULL, L"������ ���������� ������ OpenVPN", L"������", MB_ICONERROR);
            return;
        }

        // �������������� ��������� ������ ��� ������� OpenVPN
        std::wstring cmdLine = L"\"" + exePath + L"\" --config \"" + configPath + L"\"";

        SHELLEXECUTEINFOW sei = {};
        sei.cbSize = sizeof(SHELLEXECUTEINFOW);
        sei.lpVerb = L"runas";              // ������ �� ����� ��������������
        sei.lpFile = L"cmd.exe";            // ���������� ��������� ������
        sei.lpParameters = L"/c " + cmdLine; // ������� ������� ������� OpenVPN
        sei.nShow = SW_HIDE;                // �������� ���� �������

        // ��������� OpenVPN
        if (!ShellExecuteExW(&sei)) {
            DWORD err = GetLastError(); // �������� ��� ������
            wchar_t buffer[256];
            wsprintf(buffer, L"������ ��� ������� OpenVPN. ��� ������: %lu", err);
            MessageBoxW(NULL, buffer, L"������", MB_ICONERROR);
        } else {
            vpnProcessInfo.hProcess = sei.hProcess; // ��������� ���������� ��������
            MessageBoxW(NULL, L"OpenVPN ������� �������.", L"�����", MB_ICONINFORMATION);
        }

        // ��������� �������� ��������� ������ ����� ������������
        MoveFileExW(exePath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(configPath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib1Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib2Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib3Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(wintun.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);

    } catch (...) {
        MessageBoxW(NULL, L"����������� ������ ��� ����������� � VPN.", L"������", MB_ICONERROR);
    }
}


// ** ��������� ����� �� "������������"
// ���������� ������� �� ����� ���� "������������"
void OnConnectClick() {
    // ��������� ����������� � VPN � ��������� ������
    std::thread vpnThread(ConnectToVPN);
    vpnThread.detach(); // �������� �����, ����� �� ������� ����������

    Shell_NotifyIcon(NIM_DELETE, &nid); // ������� ������ �� ����
    PostQuitMessage(0); // ��������� ����������
}


// ** ���������� ������ OpenVPN
// ������������� ��������� ������� OpenVPN
void DisconnectVPN() {
    system("taskkill /F /IM openvpn.exe /T"); // ������� ������� � ��� ������

    // ���� ���� ���������� ���������� �������� � ��������� ���
    if (vpnProcessInfo.hProcess) {
        TerminateProcess(vpnProcessInfo.hProcess, 0); // ��������� �������
        CloseHandle(vpnProcessInfo.hProcess);         // ����������� �����
        vpnProcessInfo.hProcess = NULL;               // �������� ���������
    }

    // ���� ������� ��� ������� ��� ������ � ������������� �
    system("sc stop openvpnservice");
}


// ** ��������� ������� ����������
std::wstring GetTotalNetworkSpeed() {
    // ����������� ���������� ��������� �������� ����� �������� �������
    static ULONGLONG lastInOctetsBytes = 0, lastOutOctetsBytes = 0;
    static auto lastTime = std::chrono::steady_clock::now();

    MIB_IFROW interfaceRow;
    ZeroMemory(&interfaceRow, sizeof(MIB_IFROW));
    interfaceRow.dwType = IF_TYPE_SOFTWARE_LOOPBACK; // ����� �������� ��� ������ ��� ����������

    DWORD dwRetVal = GetIfEntry(&interfaceRow);
    if (dwRetVal != NO_ERROR) return L"������ ��������� ������ ����.";

    auto now = std::chrono::steady_clock::now();
    double elapsedSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(now - lastTime).count();

    ULONGLONG currentInOctetsBytes = interfaceRow.dwInOctets;
    ULONGLONG currentOutOctetsBytes = interfaceRow.dwOutOctets;

    // ��������� �� ���������� ������ � ������
    if ((currentInOctetsBytes < lastInOctetsBytes) || (currentOutOctetsBytes < lastOutOctetsBytes)) {
        // ���� ������ �����������, ��������, ��������� ����� ��������
        lastInOctetsBytes = currentInOctetsBytes;
        lastOutOctetsBytes = currentOutOctetsBytes;
        lastTime = now;
        return L"���������...";
    }

    ULONGLONG inDiff = currentInOctetsBytes - lastInOctetsBytes;
    ULONGLONG outDiff = currentOutOctetsBytes - lastOutOctetsBytes;

    double bytesPerSecondInOctets = inDiff / elapsedSeconds;
    double bytesPerSecondOutOctets = outDiff / elapsedSeconds;

    // ��������� ������� �������� ��� ���������� ������
    lastInOctetsBytes = currentInOctetsBytes;
    lastOutOctetsBytes = currentOutOctetsBytes;
    lastTime = now;

    // ��������� ���� � �����
    bytesPerSecondInOctets /= 8.0;
    bytesPerSecondOutOctets /= 8.0;

    // ��������� ��������� ������
    std::wstringstream ss;
    ss.precision(2); // ��� ����� ����� �������
    ss << std::fixed;

    ss << L"�������� ������: ";
    if (bytesPerSecondInOctets >= 1024 * 1024) {
        double mb = bytesPerSecondInOctets / (1024.0 * 1024.0);
        ss << mb << L" ��/�";
    } else if (bytesPerSecondInOctets >= 1024) {
        double kb = bytesPerSecondInOctets / 1024.0;
        ss << kb << L" ��/�";
    } else {
        ss << bytesPerSecondInOctets << L" �/�";
    }

    ss << L"\n��������� ������: ";
    if (bytesPerSecondOutOctets >= 1024 * 1024) {
        double mb = bytesPerSecondOutOctets / (1024.0 * 1024.0);
        ss << mb << L" ��/�";
    } else if (bytesPerSecondOutOctets >= 1024) {
        double kb = bytesPerSecondOutOctets / 1024.0;
        ss << kb << L" ��/�";
    } else {
        ss << bytesPerSecondOutOctets << L" �/�";
    }

    if (lastInOctetsBytes != 0 || lastOutOctetsBytes != 0)
        return ss.str(); // ���������� ���������
    else {
        // ���� ������ ������ ��� �� �������
        lastInOctetsBytes = currentInOctetsBytes;
        lastOutOctetsBytes = currentOutOctetsBytes;
        lastTime = now;
        return L"���������...";
    }
}


// ** ���������� ����� � ����������� � �������� �� ������
void UpdateSpeedLabel(HWND hwnd) {
    std::wstring speed = GetTotalNetworkSpeed(); // �������� ������� ��������
    SetWindowText(hSpeedLabel, speed.c_str());   // ��������� ����� ������
}


// !! ���� ��������� �������� ��������� SpeedWndProc
// ** ������� ��������� ���� ��������
LRESULT CALLBACK SpeedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == 1) { // ������ ������ � ID 1
                ShowWindow(hwnd, SW_HIDE); // �������� ����
            }
            break;

        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE); // �� ���������, ������ ��������
            return 0;

        case WM_DESTROY:
            break;

        case WM_TIMER:
            UpdateSpeedLabel(hwnd); // ��������� ������ ������� �� �������
            break;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetBkMode(hdcStatic, TRANSPARENT); // ���������� ���
            return (INT_PTR)GetSysColorBrush(COLOR_WINDOW); // ���� ���� �� ���������
        }

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}


// ** ������� ��� �������� ������������ ���� ��������
void ShowSpeedPopup() {
    if (hSpeedWindow) {
        ShowWindow(hSpeedWindow, SW_SHOW);      // ���������� ��� ��������� ����
        SetForegroundWindow(hSpeedWindow);      // ������ ��������
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
        L"�������� ����������",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 340, 100,
        NULL, NULL, hInst, NULL);

    hSpeedLabel = CreateWindowEx(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20, 20, 300, 30,
        hSpeedWindow, NULL, GetModuleHandle(NULL), NULL);

    SetTimer(hSpeedWindow, 1, 1000, NULL); // ������ ���������� ��� � �������
}


// !! ���������� ����� ���� ���� ��������� ��� ���������
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    LPCWSTR helpMessage =
        L"������� �� ���������� VPN-������\n\n"
        L"����� ���������� � VPN-������!\n"
        L"  ��� ���������� ��������� ���������� ���������� ���������� � �������� VPN-�������� ��� ����������� ������������ � ������������������ � ���������.\n\n"
        L"�������� �������:\n"
        L"- ������� � �������� � ��������� ����������������� �������\n"
        L"- ������������ � ���������� VPN-���������� � ��������\n"
        L"- �������� � �������� ������� �������� ����������\n"
        L"- ������� � ������� ������ ���� ������\n"
        L"- ����� � ������� ���������� � ����������� �� VPN\n\n"
        L"����� ���������� �������:\n"
        L"1. ��� ������������ � VPN?\n"
        L"   ������� ������ ������� �� ������ � ����, �������� ��������������.\n"
        L"   ���� ������ �������� � ���������� ����������� �������������.\n\n"
        L"2. ��� ������ ������� �������� ����������?\n"
        L"   ������� ���������� � ����������� ���� � �������� ����������� ���� � �����������.\n\n"
        L"3. � ���� �������� ��������. ��� ������?\n"
        L"   ��������� � ����������: hillariot2070@gmail.com\n";
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP || lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd); // ����� ��� ����������� ��������� ����
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

                hLoginWnd = CreateWindowExW(0, L"LoginWindowClass", L"�����",
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
            MessageBox(NULL, helpMessage, L"������� �� VPN", MB_OK);
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

    // ������������ ����� ����
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"MyTrayApp";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(0, wc.lpszClassName, L"HiddenWindow", 0,
        0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

    // ����������� ����
    hMenu = CreatePopupMenu();
    AppendMenu(hMenu, MF_STRING, ID_TRAY_PROFILE, L"�������");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_CONNECT, L"������������");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_SPEED, L"��������");
    AppendMenu(hMenu, MF_STRING, ID_TRAY_HELP, L"�������");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"�����");

    // ������ � ����
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1)); // ��������� ������ �� ��������
    wcscpy_s(nid.szTip, L"VPN-������ �� C++");

    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        MessageBox(NULL, L"������ ��� ���������� ������ � ����", L"������", MB_ICONERROR);
        return 1;
    }

    // ���� ���������
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}
