#define _WIN32_WINNT 0x0600
#include "resource1.h"  // ���������� ��������� ���������
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <iphlpapi.h>
#include <string>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <sstream>
#include <locale>
#include <codecvt>
#include <vector>
#pragma comment(lib, "Ws2_32.lib")

#pragma comment(lib, "iphlpapi.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_PROFILE 1002
#define ID_TRAY_CONNECT 1003
#define ID_TRAY_SPEED 1004
#define ID_TRAY_HELP 1005


#define ID_REPEAT_PASSWORD_EDIT 3003

bool ID_HAVE_LOGIN = false;
bool isRegisterMode = false;
HWND hRepeatPasswordEdit = NULL;
HWND hSpeedLabel = NULL;
HWND hBtnLogin, hBtnRegistration,hLabelLogin,hLabelPassword;


PROCESS_INFORMATION vpnProcessInfo = {};  // ����� �������� OpenVPN

HWND hLoginWnd = NULL;
HWND hUsernameEdit = NULL;
HWND hPasswordEdit = NULL;


NOTIFYICONDATA nid = {};
HMENU hMenu;
HWND hwnd;
HICON hIcon;

HWND hSpeedWindow = NULL;



BOOL CALLBACK EnumChildProc(HWND hwndChild, LPARAM lParam) {
    DestroyWindow(hwndChild); // ������� �������� ����
    return TRUE;
}

void DestroyAllControls(HWND hwndParent) {
    EnumChildWindows(hwndParent, EnumChildProc, 0);
}

// ������� ��� ��������� ������ �� �������
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* out) {
    size_t totalSize = size * nmemb;
    out->append((char*)contents, totalSize);
    return totalSize;
}

// ������� ��� �������� POST-�������
bool sendLoginRequest(const std::wstring& email, const std::wstring& password) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        // ������������� URL
        curl_easy_setopt(curl, CURLOPT_URL, "http://185.184.122.74:5000/auth_auth");

        // ������ ��� �������� (� ������� JSON)
        std::string jsonData = "{\"email\": \"" + std::string(email.begin(), email.end()) + "\", \"password\": \"" + std::string(password.begin(), password.end()) + "\"}";

        // ������������� ���������
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        // ������������� ����� POST-������� � �������
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());

        // ������������� ������� ��� ��������� ������
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        // ���������� ������
        res = curl_easy_perform(curl);

        // �������� �� ������
        if (res != CURLE_OK) {
            return false;
        }

        // ����� �� �������
        return response.find("\"success\":true") != std::string::npos;
    }

    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return false; // ������ �����������
}

void SwitchMode(HWND hwnd, bool registerMode) {
    isRegisterMode = registerMode;

    ShowWindow(hRepeatPasswordEdit, registerMode ? SW_SHOW : SW_HIDE);

    HWND hRegisterBtn = GetDlgItem(hwnd, ID_BTN_REGISTER);
    HWND hLoginBtn = GetDlgItem(hwnd, ID_BTN_LOGIN);
    SetWindowTextW(hRegisterBtn, registerMode ? L"��������������" : L"�����������");
    SetWindowTextW(hLoginBtn, registerMode ? L"�����������" : L"�����");
    DestroyAllControls(hwnd);

    if (registerMode && !hRepeatPasswordEdit) {
        SetWindowText(hwnd, L"�����������");
        hRepeatPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            180, 100, 160, 20, hwnd, (HMENU)ID_REPEAT_PASSWORD_EDIT, NULL, NULL);
        CreateWindowW(L"STATIC", L"��������� ������:", WS_VISIBLE | WS_CHILD,
            20, 100, 135, 20, hwnd, NULL, NULL, NULL);
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        hLabelLogin = CreateWindowW(L"STATIC", L"�����:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hLabelPassword = CreateWindowW(L"STATIC", L"������:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            180, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            180, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        int desiredClientWidth = (clientRect.right - clientRect.left) + 60;
        int desiredClientHeight = (clientRect.bottom - clientRect.top) + 30;

        RECT windowRect = { 0, 0, desiredClientWidth, desiredClientHeight };
        AdjustWindowRect(&windowRect, GetWindowLong(hwnd, GWL_STYLE), FALSE);

        int finalWidth = windowRect.right - windowRect.left;
        int finalHeight = windowRect.bottom - windowRect.top;

        SetWindowPos(hwnd, NULL, 0, 0, finalWidth, finalHeight, SWP_NOMOVE | SWP_NOZORDER);
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_SHOW);
        hBtnLogin=CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            60, 130, 100, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration=CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            200, 130, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
    }
    else
    {
        SetWindowText(hwnd, L"�����");
        RECT clientRect;
        GetClientRect(hwnd, &clientRect);
        hLabelLogin = CreateWindowW(L"STATIC", L"�����:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hLabelPassword = CreateWindowW(L"STATIC", L"������:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            110, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            110, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        int desiredClientWidth = (clientRect.right - clientRect.left);
        int desiredClientHeight = (clientRect.bottom - clientRect.top);

        RECT windowRect = { 0, 0, desiredClientWidth, desiredClientHeight };
        AdjustWindowRect(&windowRect, GetWindowLong(hwnd, GWL_STYLE), FALSE);

        int finalWidth = windowRect.right - windowRect.left;
        int finalHeight = windowRect.bottom - windowRect.top;

        SetWindowPos(hwnd, NULL, 0, 0, finalWidth-60, finalHeight-30, SWP_NOMOVE | SWP_NOZORDER);
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_SHOW);
        hBtnLogin = CreateWindowW(L"BUTTON", L"�����", WS_VISIBLE | WS_CHILD,
            60, 100, 80, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration = CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            160, 100, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
        hRepeatPasswordEdit = 0;
    }
}
std::string wstring_to_string(const std::wstring& wstr) {
    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv;
    return conv.to_bytes(wstr);
}

bool sendRegistrationRequest(const std::string& email, const std::string& password) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl_global_init(CURL_GLOBAL_ALL);  // ���������� �������������
    curl = curl_easy_init();
    if (!curl) {
        curl_global_cleanup();
        return false;
    }

    std::stringstream json;
    json << "{ \"email\": \"" << email << "\", \"password\": \"" << password << "\" }";
    std::string jsonStr = json.str();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, "http://185.184.122.74:5001/auth_reg");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonStr.c_str());

    // ������� ������ ������ �������
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
            auto* data = static_cast<std::string*>(userdata);
            if (data && ptr) {
                data->append(ptr, size * nmemb);
            }
            return size * nmemb;
        });

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    res = curl_easy_perform(curl);

    // �������
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    curl_global_cleanup();

    if (res != CURLE_OK) {
        return false;
    }

    // �������� ����������
    return response.find("\"success\":true") != std::string::npos;
}
LRESULT CALLBACK LoginWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        hLabelLogin=CreateWindowW(L"STATIC", L"�����:", WS_VISIBLE | WS_CHILD,
            20, 20, 80, 20, hwnd, NULL, NULL, NULL);
        hUsernameEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL,
            110, 20, 160, 20, hwnd, (HMENU)ID_USERNAME_EDIT, NULL, NULL);


        hLabelPassword=CreateWindowW(L"STATIC", L"������:", WS_VISIBLE | WS_CHILD,
            20, 60, 80, 20, hwnd, NULL, NULL, NULL);
        hPasswordEdit = CreateWindowW(L"EDIT", L"", WS_VISIBLE | WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD,
            110, 60, 160, 20, hwnd, (HMENU)ID_PASSWORD_EDIT, NULL, NULL);

        SendMessage(hUsernameEdit, EM_LIMITTEXT, 0, 0);
        SendMessage(hPasswordEdit, EM_LIMITTEXT, 0, 0);


        hBtnLogin= CreateWindowW(L"BUTTON", L"�����", WS_VISIBLE | WS_CHILD,
            60, 100, 80, 25, hwnd, (HMENU)ID_BTN_LOGIN, NULL, NULL);
        hBtnRegistration = CreateWindowW(L"BUTTON", L"�����������", WS_VISIBLE | WS_CHILD,
            160, 100, 100, 25, hwnd, (HMENU)ID_BTN_REGISTER, NULL, NULL);
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_BTN_REGISTER) {
            if (!isRegisterMode) {
                SwitchMode(hwnd, true);
                return 0;
            }
            else {
                wchar_t username[1000], password[1000];
                GetWindowTextW(hUsernameEdit, username, 1000);
                GetWindowTextW(hPasswordEdit, password, 1000);

                std::wstring userStr(username);  // ����������� username � std::wstring
                std::wstring passwordStr(password);

                std::vector<std::wstring> allowedDomains = { L"@gmail.com", L"@yahoo.com", L"@mail.ru", L"@yandex.ru", L"@outlook.com" };
                if (userStr.empty() || passwordStr.empty())
                {
                    MessageBoxW(hwnd, L"�������, ����������, ����� � ������.", L"������", MB_ICONERROR);
                    return -1;
                }
                bool validDomain = false;
                for (const auto& domain : allowedDomains) {
                    if (userStr.size() > domain.size() &&
                        userStr.compare(userStr.size() - domain.size(), domain.size(), domain) == 0) {
                        validDomain = true;
                        break;
                    }
                }
                if (!validDomain) {
                    MessageBoxW(hwnd, L"��������� ������ ����� ���������� ��������.", L"������", MB_ICONERROR);
                    return -1;
                }
                int validPassword[4] = { 0,0,0,0 };
                for (const auto& symbol : passwordStr) 
                {
                    if (symbol > 64 && symbol < 91)
                    { // big letter
                        validPassword[0] += 1;
                    }
                    else if (symbol > 96 && symbol < 123) // small letter
                    {
                        validPassword[1] += 1;
                    }
                    else if (isdigit(symbol)) // digit
                    {
                        validPassword[2] += 1;
                    }
                    validPassword[3] += 1;
                }
                if (validPassword[3] < 8 || validPassword[2] == 0 || validPassword[1] == 0 || validPassword[0] == 0) {
                    MessageBoxW(hwnd, L"������ ������ �������� ������� �� 8 ��������, ����� 1 ��������� � �������� ��������� ����� � �����.", L"������", MB_ICONERROR);
                    return -1;
                }

                wchar_t repeatPassword[1000];
                GetWindowTextW(hRepeatPasswordEdit, repeatPassword, 1000);
                if (wcscmp(password, repeatPassword) != 0) {
                    MessageBoxW(hwnd, L"������ �� ���������.", L"������", MB_ICONERROR);
                    return -1;
                }

                if (sendRegistrationRequest(wstring_to_string(userStr), wstring_to_string(passwordStr))) {
                    MessageBoxW(hwnd, L"����������� �������!", L"�����������", MB_OK);
                    ID_HAVE_LOGIN = true;
                }
                else {
                    MessageBoxW(hwnd, L"������ �����������.", L"������", MB_ICONERROR);
                }

                ShowWindow(hwnd, SW_HIDE);  // �������� ���� ����� ��������
            }
        }
        else if (LOWORD(wParam) == ID_BTN_LOGIN && isRegisterMode) {
            SwitchMode(hwnd, true);
            isRegisterMode = false;
            return 0;
        }
        else if (LOWORD(wParam) == ID_BTN_LOGIN && !isRegisterMode) {
            wchar_t username[1000], password[1000];
            GetWindowTextW(hUsernameEdit, username, 1000);
            GetWindowTextW(hPasswordEdit, password, 1000);

            std::wstring userStr(username);  // ����������� username � std::wstring
            std::wstring passwordStr(password);

            std::vector<std::wstring> allowedDomains = { L"@gmail.com", L"@yahoo.com", L"@mail.ru", L"@yandex.ru", L"@outlook.com" };
            if (userStr.empty() || passwordStr.empty())
            {
                MessageBoxW(hwnd, L"�������, ����������, ����� � ������.", L"������", MB_ICONERROR);
                return -1;
            }
            bool validDomain = false;
            for (const auto& domain : allowedDomains) {
                if (userStr.size() > domain.size() &&
                    userStr.compare(userStr.size() - domain.size(), domain.size(), domain) == 0) {
                    validDomain = true;
                    break;
                }
            }
            if (!validDomain) {
                MessageBoxW(hwnd, L"��������� ������ ����� ���������� ��������.", L"������", MB_ICONERROR);
                return -1;
            }
            int validPassword[4] = { 0,0,0,0 };
            for (const auto& symbol : passwordStr) {
                int symbol_int = int(symbol);
                if (symbol_int > 64 && symbol_int < 91)
                { // big letter
                    validPassword[0] += 1;
                }
                else if (symbol_int > 96 && symbol_int < 123) // small letter
                {
                    validPassword[1] += 1;
                }
                else if (symbol_int <128&&isdigit(symbol)) // digit
                {
                    validPassword[2] += 1;
                }
                validPassword[3] += 1;
            }
            if (validPassword[3] < 8 || validPassword[2] == 0 || validPassword[1] == 0 || validPassword[0] == 0) {
                MessageBoxW(hwnd, L"������ ������ �������� ������� �� 8 ��������, ����� 1 ��������� � �������� ��������� ����� � �����.", L"������", MB_ICONERROR);
                return -1;
            }
            if (sendLoginRequest(userStr, passwordStr)) {
                MessageBoxW(hwnd, L"����������� �������!", L"�����������", MB_OK);
                ID_HAVE_LOGIN = true;
            }
            else {
                MessageBoxW(hwnd, L"��������� ����� ��� ������ �� ����������.", L"������", MB_ICONERROR);
            }

            ShowWindow(hwnd, SW_HIDE);  // �������� ���� ����� ��������

        }
        break;
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);  // ������, �� ����������
        return 0;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT);

        // ����� ������������ �����, ��������������� ����� ���� ����
        return (INT_PTR)GetStockObject(HOLLOW_BRUSH); // ��� ������ ���������� �����
    }

    }


    return DefWindowProc(hwnd, msg, wParam, lParam);
}


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
    if (ID_HAVE_LOGIN)
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
            !WriteResourceToFile(IDR_RCDATA3, lib3Path) ||
            !WriteResourceToFile(IDR_RCDATA6, wintun)) {

            MessageBox(NULL, L"������ ���������� ������ OpenVPN", L"������", MB_ICONERROR);
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

        try
        {
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
                wsprintf(buffer, L"������ ��� ������� OpenVPN. ��� ������: %lu", err);
                MessageBoxW(NULL, buffer, L"������", MB_ICONERROR);
            }
            else {
                vpnProcessInfo.hProcess = sei.hProcess;
                MessageBoxW(NULL, L"OpenVPN ������� �������.", L"�����", MB_ICONINFORMATION);
            }
        }
        catch (...)
        {
        }
        MoveFileExW(exePath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(configPath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib1Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib2Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(lib3Path.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
        MoveFileExW(wintun.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    else
    {
        MessageBoxW(NULL, L"� ������, ������������� ��� �����������������!", L"������", MB_ICONERROR);
    }
    
}


void OnConnectClick()
{
    std::thread vpnThread(ConnectToVPN);
    vpnThread.detach();  // �������� �����, ����� �� ������� ����������

    Shell_NotifyIcon(NIM_DELETE, &nid);
    PostQuitMessage(0);
}


void DisconnectVPN()
{
    // ���������� ������� taskkill ��� ���������� OpenVPN
    system("taskkill /F /IM openvpn.exe /T");

    // ������, ���� ������� �������� ��� ������
    // system("sc stop openvpnservice");

    // ���� OpenVPN ��� ������� ��� ��������� �������, ��� ����� ����� ������������
    if (vpnProcessInfo.hProcess) {
        TerminateProcess(vpnProcessInfo.hProcess, 0);  // ��������� �������
        CloseHandle(vpnProcessInfo.hProcess);  // ��������� �����
        vpnProcessInfo.hProcess = NULL;  // �������� �����
    }
    system("sc stop openvpnservice");
}

std::wstring GetTotalNetworkSpeed() {
    static ULONGLONG lastInOctetsBytes = 0, lastOutOctetsBytes = 0;
    static auto lastTime = std::chrono::steady_clock::now();

    ULONGLONG currentInOctetsBytes = 0, currentOutOctetsBytes=0;

    // �������� ������� �����������
    DWORD size = 0;
    if (GetIfTable(nullptr, &size, TRUE) != ERROR_INSUFFICIENT_BUFFER) {
        return L"������: ����� �� �������";
    }

    std::vector<BYTE> buffer(size);
    MIB_IFTABLE* pTable = reinterpret_cast<MIB_IFTABLE*>(buffer.data());

    if (GetIfTable(pTable, &size, TRUE) != NO_ERROR) {
        return L"������: �� ������� �������� ������ �����������";
    }


    for (DWORD i = 0; i < pTable->dwNumEntries; ++i) {
        const MIB_IFROW& row = pTable->table[i];
        if (row.dwOperStatus == IF_OPER_STATUS_OPERATIONAL) {
            currentInOctetsBytes += static_cast<ULONGLONG>(row.dwInOctets);
            currentOutOctetsBytes += static_cast<ULONGLONG>(row.dwOutOctets);
        }
    }

    auto now = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(now - lastTime).count();

    double bytesPerSecondInOctets = 0.0, bytesPerSecondOutOctets=0.0;
    if (seconds > 0.1) { // ��������� ������ ���� ������ ���������� �������
        bytesPerSecondInOctets = static_cast<double>(currentInOctetsBytes - lastInOctetsBytes) / seconds;
        bytesPerSecondOutOctets = static_cast<double>(currentOutOctetsBytes - lastOutOctetsBytes) / seconds;
        lastInOctetsBytes = currentInOctetsBytes;
        lastOutOctetsBytes = currentOutOctetsBytes;
        lastTime = now;
    }
    else {
        // ���������� ������ ��� �����, ����� �������� "������" � ��������
        bytesPerSecondInOctets = 0.0;
    }
    bytesPerSecondInOctets /= 8.0;
    bytesPerSecondOutOctets /= 8.0;
    std::wstringstream ss;
    ss.precision(2);
    ss << std::fixed;

    ss << L"�������� ������: ";

    if (bytesPerSecondInOctets >= 1024 * 1024) {
        double mb = bytesPerSecondInOctets / (1024.0 * 1024.0);
        ss << mb << L" ��/�";
    }
    else if (bytesPerSecondInOctets >= 1024) {
        double kb = bytesPerSecondInOctets / 1024.0 ;
        ss << kb << L" ��/�";
    }
    else {
        ss << bytesPerSecondInOctets << L" �/�";
    }
    ss << L"\n��������� ������: ";

    if (bytesPerSecondOutOctets >= 1024 * 1024) {
        double mb = bytesPerSecondOutOctets / (1024.0 * 1024.0);
        ss << mb << L" ��/�";
    }
    else if (bytesPerSecondOutOctets >= 1024) {
        double kb = bytesPerSecondOutOctets / 1024.0;
        ss << kb << L" ��/�";
    }
    else {
        ss << bytesPerSecondOutOctets << L" �/�";
    }

    if (lastInOctetsBytes != 0 || lastOutOctetsBytes)
        return ss.str();
    else
    {
        lastInOctetsBytes = currentInOctetsBytes;
        lastOutOctetsBytes = currentOutOctetsBytes;
        lastTime = now;
        return L"���������...";
    }


}


// ������� ��� ���������� �������� �� ������
void UpdateSpeedLabel(HWND hwnd) {

    std::wstring speed = GetTotalNetworkSpeed();  // �������� ������� ��������
    SetWindowText(hSpeedLabel, speed.c_str());  // ��������� ����� ������
}

// ���������� ��������� ��� ���� ��������
LRESULT CALLBACK SpeedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wParam) == 1) {  // ���������, ������ �� ������ � ID 1
            ShowWindow(hwnd, SW_HIDE);  // �������� ����
        }
        break;
    case WM_CLOSE:  // ��������� �������� ����
        ShowWindow(hwnd, SW_HIDE);  // ������ ������ ���� ������ ��������
        return 0;
    case WM_DESTROY:
        break;
    case WM_TIMER:  // ���������� �������
        UpdateSpeedLabel(hwnd);  // ��������� �������� ������ �������
        break;
    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        SetBkMode(hdcStatic, TRANSPARENT); // ��� OPAQUE ��� �������������
        return (INT_PTR)GetSysColorBrush(COLOR_WINDOW); // ��� �� ����� ����������
    }

    }


    return DefWindowProc(hwnd, msg, wParam, lParam);
}


// ������� ��� �������� ���� � ����������� � ��������
void ShowSpeedPopup() {
    // ���� ���� ��� �������, �� ��������� �����
    if (hSpeedWindow) {
        ShowWindow(hSpeedWindow, SW_SHOW);         // ��������, ���� ���� ������
        SetForegroundWindow(hSpeedWindow);         // ������������
        return;
    }

    // ������������ ����� ���� ��� ��������
    HINSTANCE hInst = GetModuleHandle(NULL);
    HICON hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_ICON1));
    WNDCLASS speedClass = {};
    speedClass.lpfnWndProc = SpeedWndProc;
    speedClass.hInstance = hInst;
    speedClass.lpszClassName = L"SpeedPopupClass";
    speedClass.hIcon = hIcon; // ������������� ������ � ��������� WNDCLASS
    RegisterClass(&speedClass);

    // ������� ���� ��� ����������� ��������

    hSpeedWindow = CreateWindowExW(
        WS_EX_TOPMOST, L"SpeedPopupClass", L"�������� ����������",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 340, 100,
        NULL, NULL, hInst, NULL);
    

    // ������� ����� ��� ����������� ��������
    hSpeedLabel = CreateWindowEx(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        20,20,300, 30, hSpeedWindow, NULL, GetModuleHandle(NULL), NULL);

    // ������ ��� ���������� �������� ������ �������
    SetTimer(hSpeedWindow, 1, 1000, NULL);  // ������ � ���������� 1 �������
}

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
