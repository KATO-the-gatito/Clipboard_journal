#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
//#include <ShlObj_core.h>

#define HOTKEY_ID_ACTIVATEWINDOW 1
#define HOTKEY_ID_COPY 2

#define TXTCOLOR_SELECTED 224 // 96
#define TXTCOLOR_UNSELECTED 15
#define TXTCOLOR_NORMAL 10

struct DataObject
{
    std::wstring data;
    int cntlines;
};

HWND hConsole = GetConsoleWindow();
HANDLE hndl = GetStdHandle(STD_OUTPUT_HANDLE);
std::vector<DataObject> copy_buffer;
HWND hTargetWindow;
int selected_pointer = 0;
bool is_activated_window = false;
bool is_hotkey_was_pressed = false;

void PrintWString(const std::wstring& text) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD written;
    WriteConsoleW(hOut, text.c_str(), text.length(), &written, nullptr);
}

int countLines(const std::wstring& text) {
    int cnt = 1;
    for (wchar_t ch : text) {
        if (ch == L'\n')
            cnt++;
    }
    return cnt;
}

void copyData() {
    OpenClipboard(nullptr);
    HGLOBAL hMem = GetClipboardData(CF_UNICODETEXT);
    wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hMem));
    if (pText != nullptr) copy_buffer.push_back(DataObject{pText, countLines(pText)});
    GlobalUnlock(hMem);
    CloseClipboard();
}

void pasteData() {
    std::wstring text = copy_buffer[selected_pointer].data;

    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();

    SetForegroundWindow(hTargetWindow);
    size_t size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (hMem) {
        wchar_t* pMem = static_cast<wchar_t*>(GlobalLock(hMem));
        if (pMem) {
            wmemcpy(pMem, text.c_str(), text.length() + 1);
            GlobalUnlock(pMem);
        }
        SetClipboardData(CF_UNICODETEXT, hMem);
    }
    CloseClipboard();

    Sleep(50);

    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = 'V';
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = 'V';
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

void printData() {
    SetConsoleTextAttribute(hndl, TXTCOLOR_NORMAL);
    std::cout << "-----------< Clipboard data >-----------\n";
    for (int i = 0; i < copy_buffer.size(); i++) {
        SetConsoleTextAttribute(hndl, TXTCOLOR_NORMAL);
        printf("<%d> ", i + 1);
        SetConsoleTextAttribute(hndl, TXTCOLOR_UNSELECTED);
        if (i == selected_pointer)
            SetConsoleTextAttribute(hndl, TXTCOLOR_SELECTED);
        PrintWString(copy_buffer[i].data);
        std::cout << '\n';
        SetConsoleTextAttribute(hndl, TXTCOLOR_UNSELECTED);
    }
    SetConsoleTextAttribute(hndl, TXTCOLOR_NORMAL);
    std::cout << "----------------------------------------\n";
    
}

bool isKeyPressed() {
    for (int vk = 0x08; vk <= 0xFF; ++vk) {  
        if (GetAsyncKeyState(vk) & 0x8000) {
            return true;
        }
    }
    return false;
}

void ForceForegroundWindow(HWND hWnd) {
    INPUT input = { INPUT_KEYBOARD };
    input.ki.wVk = VK_MENU; 
    SendInput(1, &input, sizeof(INPUT));

    SetForegroundWindow(hWnd);

    input.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

void moveCursor(SHORT x, SHORT y) {
    COORD coord;
    coord.X = x;
    coord.Y = y;
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

void gotoActiveLine() {
    SHORT coordLine = 0;
    for (int i = 0; i < selected_pointer; i++) {
        coordLine += (SHORT)copy_buffer[i].cntlines;
    }
    
    moveCursor(0, coordLine);
}

////////////////////////////////////////////////

int main() { 
    //system("mode con cols=80 lines=30");

    ShowWindow(hConsole, SW_HIDE); // <--- show

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = DefWindowProc;
    wc.hInstance     = GetModuleHandle(nullptr);
    wc.lpszClassName = "HiddenHotkeyWindow";
    RegisterClassA(&wc);

    HWND hWnd = CreateWindowExA(0, "HiddenHotkeyWindow", "", 0,
                                   0, 0, 0, 0, HWND_MESSAGE,
                                   nullptr, nullptr, nullptr);

    while (1){   // main loop
        bool isCtrlPressed = GetAsyncKeyState(VK_CONTROL) & 0x8000;
        bool isShiftPressed = GetAsyncKeyState(VK_SHIFT) & 0x8000;
        bool isAltPressed = GetAsyncKeyState(VK_MENU) & 0x8000;
        bool isWinPressed = GetAsyncKeyState(VK_LWIN) || GetAsyncKeyState(VK_RWIN);
        bool isVPressed = GetAsyncKeyState('V') & 0x8000;
        bool isCPressed = GetAsyncKeyState('C') & 0x8000;
        bool isEscPressed = GetAsyncKeyState(VK_ESCAPE) & 0x8000;
        bool isEnterPressed = GetAsyncKeyState(VK_RETURN) & 0x8000;
        bool isDelPressed = GetAsyncKeyState(VK_DELETE) & 0x8000;

        if (isCtrlPressed && isAltPressed && isVPressed && !is_hotkey_was_pressed) {
            hTargetWindow = GetForegroundWindow();
            ShowWindow(hConsole, SW_SHOW); // <--- show
            ForceForegroundWindow(hConsole);
            system("cls");
            printData();
            is_activated_window = true;
            is_hotkey_was_pressed = true;
        }
        else if (isCtrlPressed && isCPressed && !is_hotkey_was_pressed) {
            copyData();
            is_hotkey_was_pressed = true;
        }
        else if (isEscPressed && !is_hotkey_was_pressed) {
            ShowWindow(hConsole, SW_HIDE); // <--- show
            is_activated_window = false;            is_hotkey_was_pressed = true;
        }
        else if (isCtrlPressed && isCPressed && !is_hotkey_was_pressed) {
            copyData();
            is_hotkey_was_pressed = true;
        }
        else if (isEscPressed && !is_hotkey_was_pressed) {
            ShowWindow(hConsole, SW_HIDE); // <--- show
            is_hotkey_was_pressed = true;
        }

        if (is_activated_window) {
            bool isDownPressed = GetAsyncKeyState(VK_DOWN) & 0x8000;
            bool isUpPressed = GetAsyncKeyState(VK_UP) & 0x8000;

            if (isUpPressed && !is_hotkey_was_pressed) {
                if (selected_pointer - 1 >= 0) {
                    selected_pointer--;
                    system("cls");
                    printData();
                }
                gotoActiveLine();
                is_hotkey_was_pressed = true;
            }
            else if (isDownPressed && !is_hotkey_was_pressed) {
                if (selected_pointer + 1 < copy_buffer.size()) {
                    selected_pointer++;
                    system("cls");
                    printData();
                }
                gotoActiveLine();
                is_hotkey_was_pressed = true;
            }
            else if (isEnterPressed && !is_hotkey_was_pressed) {
                ShowWindow(hConsole, SW_HIDE); // <--- show
                is_activated_window = false;
                pasteData();
                std::iter_swap(copy_buffer.begin(), copy_buffer.begin() + selected_pointer);
                is_hotkey_was_pressed = true;
            }
            else if (isDelPressed && !is_hotkey_was_pressed) {
                copy_buffer.erase(copy_buffer.begin() + selected_pointer);
                system("cls");
                printData();
                is_hotkey_was_pressed = true;
            }
            
        }
        
        if (!isKeyPressed() && is_hotkey_was_pressed) {
            is_hotkey_was_pressed = false;
        }

        Sleep(50);

    }
    
    return 0;
}