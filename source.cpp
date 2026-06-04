#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <algorithm>
#include <stdarg.h>
#include <array>
//#include <ShlObj_core.h>

#define HOTKEY_ID_ACTIVATEWINDOW 1
#define HOTKEY_ID_COPY 2

#define TXTCOLOR_SELECTED 224 // 96
#define TXTCOLOR_UNSELECTED 15
#define TXTCOLOR_NORMAL 10
#define TXTCOLOR_BOX 3
#define TXTCOLOR_NOTICE 13
#define TXTCOLOR_PS_SEL 143
#define TXTCOLOR_PS_UNSEL 3

#define MAX_STRING_SIZE 2500

#define PRINTSTATE_DATA 0
#define PRINTSTATE_MARKED 1

struct DataObject
{
    std::wstring data;
    int cntlines;
    unsigned ID;
};

HWND hConsole = GetConsoleWindow(), hTargetWindow;
HANDLE hndl = GetStdHandle(STD_OUTPUT_HANDLE);
std::deque<DataObject> copy_buffer, marked_buffer;
const std::array<std::deque<DataObject>*, 2> buffers = {&copy_buffer, &marked_buffer};
int selected_pointer = 0, selected_pointer_mrkd = 0, print_state = 0;
unsigned ID_counter = 0;
std::array<int*, 2> pointers = {&selected_pointer, &selected_pointer_mrkd};
bool is_activated_window = false, is_hotkey_was_pressed = false;

void printfcl(const char* strin, ...) {
    size_t len = strlen(strin);
    char str[100];
    char* chr = str;
    strcpy(str, strin);
    int cntr_of_cmds = 0;
    va_list colors;
    va_start(colors, strin);

    for (int i = 0; i < len; ++i) {
        if (str[i] == '@') {
            str[i] = '\0';
            vprintf(chr, colors);
            for (int j = 0; j < cntr_of_cmds; j++)
                { va_arg(colors, int); }
            chr = str + i + 1;
            cntr_of_cmds = 0;
            SetConsoleTextAttribute(hndl, va_arg(colors, int));
        }
        if (str[i] == '%' && str[i + 1] != '%' && str[i - 1] != '%') ++cntr_of_cmds;
    }
    printf(chr);
    va_end(colors);
}

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
    if (pText != nullptr) copy_buffer.push_front(DataObject{pText, countLines(pText), ++ID_counter});
    GlobalUnlock(hMem);
    CloseClipboard();
}

void pasteData() {
    std::wstring text = (buffers[print_state]->begin() + *pointers[print_state])->data;

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
    printfcl("@====[@Clipboard data@]=[@Marked data@]====@\n",
        TXTCOLOR_NORMAL,
        print_state == PRINTSTATE_DATA ? TXTCOLOR_PS_SEL : TXTCOLOR_PS_UNSEL, 
        TXTCOLOR_NORMAL,
        print_state == PRINTSTATE_MARKED ? TXTCOLOR_PS_SEL : TXTCOLOR_PS_UNSEL, 
        TXTCOLOR_NORMAL,
        7);
    int i = 0;
    bool is_notice = false;

    for (; i < buffers[print_state]->size(); i++) {
        SetConsoleTextAttribute(hndl, TXTCOLOR_UNSELECTED);
        if ((buffers[print_state]->begin() + i)->data.size() > 2'500) {
            printfcl("@<A text with more than %d chars>@\n", TXTCOLOR_NOTICE, MAX_STRING_SIZE, TXTCOLOR_UNSELECTED);
            is_notice = true;
        }
        if (i == *pointers[print_state])
            SetConsoleTextAttribute(hndl, TXTCOLOR_SELECTED);
        if (is_notice) {
            PrintWString((buffers[print_state]->begin() + i)->data.substr(0, 100));
            printfcl("@...@", TXTCOLOR_UNSELECTED, 7);
            is_notice = false;
        }
        else PrintWString((buffers[print_state]->begin() + i)->data);
        
        printfcl("@\n========================================\n@", TXTCOLOR_BOX, 7);
    }
    if (!i) {
        printfcl("@<The buffer is empty>@\n", TXTCOLOR_NOTICE, 7);
    }
    printfcl("@----------------------------------------\n@", TXTCOLOR_NORMAL, 7);
    
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
    for (int i = 0; i < *pointers[print_state]; i++) {
        if ((buffers[print_state]->begin() + i)->data.size() <= MAX_STRING_SIZE)
            coordLine += (SHORT)(buffers[print_state]->begin() + i)->cntlines + 1;
        else coordLine += countLines((buffers[print_state]->begin() + i)->data.substr(0, 100)) + 2;
    }
    
    moveCursor(0, coordLine);
}

bool is_have_ID(std::deque<DataObject>* buffer, unsigned ID) {
    for (DataObject& obj : *buffer) {
        if (obj.ID == ID)
            return true;
    }
    return false;
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
        bool isMPressed = GetAsyncKeyState('M') & 0x8000;
        bool isEscPressed = GetAsyncKeyState(VK_ESCAPE) & 0x8000;
        bool isEnterPressed = GetAsyncKeyState(VK_RETURN) & 0x8000;
        bool isDelPressed = GetAsyncKeyState(VK_DELETE) & 0x8000;
        bool isDownPressed = GetAsyncKeyState(VK_DOWN) & 0x8000;
        bool isUpPressed = GetAsyncKeyState(VK_UP) & 0x8000;
        bool isLeftPressed = GetAsyncKeyState(VK_LEFT) & 0x8000;
        bool isRightPressed = GetAsyncKeyState(VK_RIGHT) & 0x8000;


        // key handling

        if (isCtrlPressed && isAltPressed && isVPressed && !is_hotkey_was_pressed) {
            hTargetWindow = GetForegroundWindow();
            ShowWindow(hConsole, SW_SHOW);
            ForceForegroundWindow(hConsole);
            system("cls");
            printData();
            gotoActiveLine();
            is_activated_window = true;
            is_hotkey_was_pressed = true;
        }
        else if (isCtrlPressed && isCPressed && !is_hotkey_was_pressed) {
            copyData();
            is_hotkey_was_pressed = true;
        }

        if (is_activated_window) { // key handling while the window is active
            if (isUpPressed && !is_hotkey_was_pressed) {
                if (*pointers[print_state] - 1 >= 0) {
                    --*pointers[print_state];
                    system("cls");
                    printData();
                    gotoActiveLine();
                }
                is_hotkey_was_pressed = true;
            }
            else if (isDownPressed && !is_hotkey_was_pressed) {
                if (*pointers[print_state] + 1 < buffers[print_state]->size()) {
                    ++*pointers[print_state];
                    system("cls");
                    printData();
                    gotoActiveLine();
                }
                is_hotkey_was_pressed = true;
            }
            else if (isLeftPressed && !is_hotkey_was_pressed) {
                if (print_state - 1 >= 0) {
                    --print_state;
                    system("cls");
                    printData();
                    gotoActiveLine();   
                }
                is_hotkey_was_pressed = true;
            }
            else if (isRightPressed && !is_hotkey_was_pressed) {
                if (print_state + 1 < 2) {
                    ++print_state;
                    system("cls");
                    printData();
                    gotoActiveLine();   
                }
                is_hotkey_was_pressed = true;
            }
            else if (isMPressed && !is_hotkey_was_pressed) {
                if (print_state == PRINTSTATE_DATA && !is_have_ID(&marked_buffer, copy_buffer[selected_pointer].ID))
                    marked_buffer.push_front(copy_buffer[selected_pointer]);
                is_hotkey_was_pressed = true;
            }
            else if (isEnterPressed && !is_hotkey_was_pressed) {
                ShowWindow(hConsole, SW_HIDE); // <--- show
                is_activated_window = false;
                pasteData();
                std::iter_swap(buffers[print_state]->begin(), buffers[print_state]->begin() + *pointers[print_state]);
                is_hotkey_was_pressed = true;
            }
            else if (isDelPressed && !is_hotkey_was_pressed) {
                //buffers[print_state]->erase(buffers[print_state]->begin() + *pointers[print_state]);
                unsigned ID_for_del = (buffers[print_state]->begin() + *pointers[print_state])->ID;
                for (std::deque<DataObject>* buf : buffers) {
                    for (int i = 0; i < buf->size(); i++) {
                        if ((*buf)[i].ID == ID_for_del) 
                            buf->erase(buf->begin() + i);
                    }
                    
                }
                                
                system("cls");
                printData();
                gotoActiveLine();
                is_hotkey_was_pressed = true;
            }
            else if (isEscPressed && !is_hotkey_was_pressed) {
                ShowWindow(hConsole, SW_HIDE); // <--- show
                is_activated_window = false;        
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