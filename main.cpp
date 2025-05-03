#define _WIN32_WINNT 0x0500
#define WINVER 0x0500
#define UNICODE
#define _UNICODE

#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <commctrl.h>
#include <fstream>
#include <string>
#include <sstream>
#include <iostream>
#include <memory>
#include <thread>
#include <map>

// Для работы с JSON используем header-only библиотеку
#include "json.hpp"

// Для использования в коде
using json = nlohmann::json;
using namespace Gdiplus;

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "user32.lib")

// Идентификаторы для системного трея
#define ID_TRAY_APP_ICON 1001
#define ID_TRAY_EXIT 1002
#define ID_TRAY_OPEN_GITHUB 1003
#define WM_TRAYICON (WM_USER + 1)

// Глобальные переменные
HWND g_hwnd = NULL;
HHOOK g_keyboardHook = NULL;
NOTIFYICONDATA g_notifyIconData = {};
bool g_overlayVisible = false;
HICON g_hIcon = NULL;
std::unique_ptr<Bitmap> g_pBitmap;

// Настройки приложения
struct Settings {
    std::wstring imageName = L"hint.png";
    int transparency = 90;
    int keyCode = VK_F8;
};

Settings g_settings;

// Функция отображения/скрытия оверлея
void ToggleOverlayVisibility(bool visible) {
    g_overlayVisible = visible;
    ShowWindow(g_hwnd, visible ? SW_SHOW : SW_HIDE);
}

// Хук для отслеживания клавиатуры
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKbdStruct = (KBDLLHOOKSTRUCT*)lParam;
        
        if (pKbdStruct->vkCode == g_settings.keyCode) {
            if (wParam == WM_KEYDOWN && !g_overlayVisible) {
                ToggleOverlayVisibility(true);
            } else if (wParam == WM_KEYUP && g_overlayVisible) {
                ToggleOverlayVisibility(false);
            }
        }
    }
    
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

// Функция сохранения настроек
void SaveSettings() {
    try {
        json j;
        
        // Конвертируем wstring в string
        std::wstring wideImageName = g_settings.imageName;
        std::string narrowImageName(wideImageName.begin(), wideImageName.end());
        
        j["image_name"] = narrowImageName;
        j["image_transparency"] = g_settings.transparency;
        j["key_for_show"] = g_settings.keyCode;
        
        std::ofstream o("settings.json");
        o << std::setw(4) << j << std::endl;
    } catch (const std::exception& e) {
        std::wstringstream ss;
        ss << L"Ошибка сохранения настроек: " << e.what();
        MessageBox(NULL, ss.str().c_str(), L"Ошибка", MB_OK | MB_ICONERROR);
    }
}

// Функция загрузки настроек
void LoadSettings() {
    std::ifstream i("settings.json");
    
    if (i.good()) {
        try {
            json j;
            i >> j;
            
            std::string narrowImageName = j["image_name"];
            g_settings.imageName = std::wstring(narrowImageName.begin(), narrowImageName.end());
            
            g_settings.transparency = j["image_transparency"];
            g_settings.keyCode = j["key_for_show"];
        } catch (const std::exception& e) {
            std::wstringstream ss;
            ss << L"Ошибка загрузки настроек: " << e.what() << L"\nБудут использованы настройки по умолчанию.";
            MessageBox(NULL, ss.str().c_str(), L"Ошибка", MB_OK | MB_ICONWARNING);
        }
    } else {
        // Если файл не существует, сохраняем настройки по умолчанию
        SaveSettings();
    }
}

// Процедура обработки сообщений для главного окна
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            
            // Создаем Graphics объект для рисования
            Graphics graphics(hdc);
            
            // Рисуем изображение, если оно загружено
            if (g_pBitmap) {
                RECT clientRect;
                GetClientRect(hWnd, &clientRect);
                
                // Рисуем изображение на весь экран
                graphics.DrawImage(g_pBitmap.get(), 
                                  Rect(0, 0, clientRect.right, clientRect.bottom));
            }
            
            EndPaint(hWnd, &ps);
            return 0;
        }
        
        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                
                HMENU hMenu = CreatePopupMenu();
                if (hMenu) {
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_OPEN_GITHUB, L"Open GitHub");
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
                    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_STRING, ID_TRAY_EXIT, L"Close");
                    
                    // Необходимо, чтобы меню исчезало при клике вне меню
                    SetForegroundWindow(hWnd);
                    
                    TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, NULL);
                    DestroyMenu(hMenu);
                }
            }
            return 0;
        }
        
        case WM_COMMAND: {
            switch (LOWORD(wParam)) {
                case ID_TRAY_EXIT:
                    DestroyWindow(hWnd);
                    return 0;
                    
                case ID_TRAY_OPEN_GITHUB:
                    ShellExecute(NULL, L"open", L"https://github.com/nntdgrss/OverlayImageHint", NULL, NULL, SW_SHOWNORMAL);
                    return 0;
            }
            break;
        }
        
        case WM_DESTROY:
            // Очистка ресурсов
            if (g_keyboardHook) {
                UnhookWindowsHookEx(g_keyboardHook);
                g_keyboardHook = NULL;
            }
            
            // Удаляем иконку из трея
            Shell_NotifyIcon(NIM_DELETE, &g_notifyIconData);
            
            // Освобождаем иконку
            if (g_hIcon) {
                DestroyIcon(g_hIcon);
                g_hIcon = NULL;
            }
            
            // Освобождаем изображение
            g_pBitmap.reset();
            
            // Завершаем GDI+
            GdiplusShutdown(0);
            
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Функция для загрузки изображения
bool LoadImageFromFile(const std::wstring& filePath) {
    g_pBitmap.reset(Bitmap::FromFile(filePath.c_str()));
    
    if (!g_pBitmap || g_pBitmap->GetLastStatus() != Ok) {
        std::wstringstream ss;
        ss << L"Не удалось загрузить изображение из файла: " << filePath;
        MessageBox(NULL, ss.str().c_str(), L"Ошибка", MB_OK | MB_ICONERROR);
        return false;
    }
    
    return true;
}

// Точка входа в программу
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Инициализация GDI+
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    
    // Загружаем настройки
    LoadSettings();
    
    // Загружаем изображение
    if (!LoadImageFromFile(g_settings.imageName)) {
        return 1;
    }
    
    // Регистрируем класс окна
    WNDCLASSEX wcex = {0};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"OverlayImageHintClass";
    
    if (!RegisterClassEx(&wcex)) {
        MessageBox(NULL, L"Не удалось зарегистрировать класс окна", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Получаем размеры экрана
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    
    // Создаем окно
    g_hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
        L"OverlayImageHintClass",
        L"Overlay Image Hint",
        WS_POPUP,
        0, 0, screenWidth, screenHeight,
        NULL, NULL, hInstance, NULL
    );
    
    if (!g_hwnd) {
        MessageBox(NULL, L"Не удалось создать окно", L"Ошибка", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    // Устанавливаем прозрачность окна
    SetLayeredWindowAttributes(g_hwnd, RGB(0, 0, 0), (BYTE)(255 * g_settings.transparency / 100.0), LWA_ALPHA);
    
    // Загружаем иконку
    g_hIcon = NULL;
    if (GetFileAttributes(L"icon.ico") != INVALID_FILE_ATTRIBUTES) {
        g_hIcon = (HICON)LoadImage(NULL, L"icon.ico", IMAGE_ICON, 16, 16, LR_LOADFROMFILE);
    }
    
    if (!g_hIcon) {
        // Используем стандартную иконку, если своя не найдена
        g_hIcon = LoadIcon(NULL, IDI_APPLICATION);
    }
    
    // Создаем иконку в трее
    ZeroMemory(&g_notifyIconData, sizeof(NOTIFYICONDATA));
    g_notifyIconData.cbSize = sizeof(NOTIFYICONDATA);
    g_notifyIconData.hWnd = g_hwnd;
    g_notifyIconData.uID = ID_TRAY_APP_ICON;
    g_notifyIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_notifyIconData.uCallbackMessage = WM_TRAYICON;
    g_notifyIconData.hIcon = g_hIcon;
    wcscpy_s(g_notifyIconData.szTip, L"Оверлей подсказки");
    
    Shell_NotifyIcon(NIM_ADD, &g_notifyIconData);
    
    // Устанавливаем хук на клавиатуру
    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
    
    if (!g_keyboardHook) {
        MessageBox(NULL, L"Не удалось установить хук на клавиатуру", L"Ошибка", MB_OK | MB_ICONERROR);
        DestroyWindow(g_hwnd);
        return 1;
    }
    
    // По умолчанию окно скрыто
    ShowWindow(g_hwnd, SW_HIDE);
    UpdateWindow(g_hwnd);
    
    // Основной цикл сообщений
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    return (int)msg.wParam;
} 