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

#define TXTCOLOR_SELECTED    224 // 96
#define TXTCOLOR_UNSELECTED  15
#define TXTCOLOR_NORMAL      10
#define TXTCOLOR_BOX         3
#define TXTCOLOR_NOTICE      13
#define TXTCOLOR_PS_SEL      143
#define TXTCOLOR_PS_UNSEL    3

#define MAX_STRING_SIZE 2500
#define MAX_STRING_LINES 25

#define PRINTSTATE_DATA    0
#define PRINTSTATE_MARKED  1

#define TOP_OFFSET 6

/******************************* decls *******************************/

struct LR_borders;
struct DataObject;
void clrprintf(const char* strin, ...);
void PrintWString(const std::wstring& text);
int countLinesStr(const std::wstring& text);
void copyData();
void pasteData();
void printData();
void ForceForegroundWindow(HWND hWnd);
void moveCursor(SHORT x, SHORT y);
int countLinesBuffer(int left, int right);
void gotoActiveLine(int left_brd);


/******************************* structs *******************************/


struct DataObject
{
    std::wstring data;
    int cntlines;
    unsigned ID;
};

struct LR_borders
{
    LR_borders(std::deque<DataObject> buffer, int selected, HANDLE hndl)
    {
        if (buffer.size() == 0) {
            left = 0;
            right = 0;
        }

        CONSOLE_SCREEN_BUFFER_INFO csbi;
        GetConsoleScreenBufferInfo(hndl, &csbi);
        int windowWidth  = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        int windowHeight = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

        for (int shift = 1; shift < 50; ++shift) {
            int tmp_l = selected - shift > 0 ? selected - shift : 0;
            int tmp_r = selected + shift < buffer.size() ? selected + shift : buffer.size();
            if (countLinesBuffer(tmp_l, tmp_r) > windowHeight + 25) {
                left = tmp_l;
                right = tmp_r;
                return;
            }
        }
        left = 0;
        right = buffer.size();
        
    }
    int left, right;
};


/******************************** global vars *******************************/


HWND hConsole = GetConsoleWindow(), hTargetWindow, hClipboardListenerWindow = nullptr;
HANDLE hndl = GetStdHandle(STD_OUTPUT_HANDLE);
HHOOK hKeyboardHook = nullptr;
std::deque<DataObject> copy_buffer, marked_buffer;
const std::array<std::deque<DataObject>*, 2> buffers = {&copy_buffer, &marked_buffer};
int selected_pointer = 0, selected_pointer_mrkd = 0, print_state = 0;
unsigned ID_counter = 0;
unsigned long long size_of_all_strings = 0;
std::array<int*, 2> pointers = {&selected_pointer, &selected_pointer_mrkd};
bool is_activated_window = false, is_need_to_paste = false;


/********************************* functions *******************************/

void clrprintf(const char* strin, ...) {
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

int countLinesStr(const std::wstring& text) {
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
    if (pText != nullptr) {
        bool is_not_duplicate = true;
        for (DataObject& obj : copy_buffer) {
            if (obj.data == pText) {
                is_not_duplicate = false;
                break;
            }
        }
        if (is_not_duplicate) {
            copy_buffer.push_front(DataObject{pText, countLinesStr(pText), ++ID_counter});
            size_of_all_strings += wcslen(pText) * sizeof(wchar_t);
        }
    }
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
            GlobalUnlock(hMem);
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
    system("cls");
    clrprintf("@copied: @%d@ texts | marked: @%d@ texts | size: @%d@ bytes (@%d@ KiB)@\n", 
        TXTCOLOR_UNSELECTED, 11, copy_buffer.size(),
        TXTCOLOR_UNSELECTED, 11, marked_buffer.size(),
        TXTCOLOR_UNSELECTED, 11, size_of_all_strings,
        TXTCOLOR_UNSELECTED, 11, size_of_all_strings / 0x400,
        TXTCOLOR_UNSELECTED, 7
    );
    clrprintf("@====[@Clipboard data@]=[@Marked data@]====@\n",
        TXTCOLOR_NORMAL,
        print_state == PRINTSTATE_DATA ? TXTCOLOR_PS_SEL : TXTCOLOR_PS_UNSEL, 
        TXTCOLOR_NORMAL,
        print_state == PRINTSTATE_MARKED ? TXTCOLOR_PS_SEL : TXTCOLOR_PS_UNSEL, 
        TXTCOLOR_NORMAL,
        7
    );

    int i = 0;
    bool is_notice = false;
    LR_borders borders(*buffers[print_state], *pointers[print_state], hndl);

    for (i = borders.left; i < borders.right; ++i) {
        SetConsoleTextAttribute(hndl, TXTCOLOR_UNSELECTED);
        if ((buffers[print_state]->begin() + i)->data.size() > MAX_STRING_SIZE ||
            (buffers[print_state]->begin() + i)->cntlines > MAX_STRING_LINES) 
        {
            clrprintf("@<Too big text>@\n", TXTCOLOR_NOTICE, TXTCOLOR_UNSELECTED);
            is_notice = true;
        }
        if (i == *pointers[print_state])
            SetConsoleTextAttribute(hndl, TXTCOLOR_SELECTED);
        if (is_notice) {
            PrintWString((buffers[print_state]->begin() + i)->data.substr(0, 100));
            clrprintf("@...@", TXTCOLOR_UNSELECTED, 7);
            is_notice = false;
        }
        else PrintWString((buffers[print_state]->begin() + i)->data);
        
        clrprintf("@\n========================================\n@", TXTCOLOR_BOX, 7);
    }
    if (!i) {
        clrprintf("@<The buffer is empty>@\n", TXTCOLOR_NOTICE, 7);
    }
    clrprintf("@============<End of buffer>============\n@", TXTCOLOR_NORMAL, 7);
    gotoActiveLine(borders.left);
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

int countLinesBuffer(int left, int right) {
    SHORT lines = 2;
    for (int i = left; i < right; i++) {
        if ((buffers[print_state]->begin() + i)->data.size() <= MAX_STRING_SIZE &&
            (buffers[print_state]->begin() + i)->cntlines <= MAX_STRING_LINES)
        {
            lines += (SHORT)(buffers[print_state]->begin() + i)->cntlines + 1;
            continue;
        }
        lines += countLinesStr((buffers[print_state]->begin() + i)->data.substr(0, 100)) + 2;
    }
    return lines;
}

void gotoActiveLine(int left_brd) {
    SHORT coordLine = countLinesBuffer(left_brd, *pointers[print_state]);
    moveCursor(0, coordLine > TOP_OFFSET ? coordLine - TOP_OFFSET : 0);
}

bool is_have_ID(std::deque<DataObject>* buffer, unsigned ID) {
    for (DataObject& obj : *buffer) {
        if (obj.ID == ID)
            return true;
    }
    return false;
}


/********************************* Listeners *******************************/


LRESULT CALLBACK ClipboardListener(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message)
    {
    case WM_CLIPBOARDUPDATE:
        copyData();
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

bool ctrlPressed  = false;
bool altPressed   = false;
bool shiftPressed = false;
bool winPressed   = false;

LRESULT CALLBACK KeyboardListener(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* pKey = (KBDLLHOOKSTRUCT*)lParam;

        bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
        bool isKeyUp   = (wParam == WM_KEYUP   || wParam == WM_SYSKEYUP);

        switch (pKey->vkCode) {
            case VK_LCONTROL:  ctrlPressed  = isKeyDown; break;
            case VK_LMENU:     altPressed   = isKeyDown; break; // Alt
            case VK_LSHIFT:    shiftPressed = isKeyDown; break;
        }

        if (isKeyDown) { // key handling
            if (ctrlPressed && altPressed && pKey->vkCode == 'V') {
                hTargetWindow = GetForegroundWindow();
                ShowWindow(hConsole, SW_SHOW);
                ForceForegroundWindow(hConsole);
                printData();
                is_activated_window = true;
            }
            if (is_activated_window) { // key handling while the window is active
                switch (pKey->vkCode)
                {
                case VK_UP:
                    if (*pointers[print_state] - 1 >= 0) {
                        --*pointers[print_state];
                        printData();
                    }
                    break;

                case VK_DOWN:
                    if (*pointers[print_state] + 1 < buffers[print_state]->size()) {
                        ++*pointers[print_state];
                        printData();
                    }
                    break;

                case VK_LEFT:
                    if (print_state - 1 >= 0) {
                        --print_state;
                        printData(); 
                    }
                    break;

                case VK_RIGHT:
                    if (print_state + 1 < 2) {
                        ++print_state;
                        printData(); 
                    }
                    break;

                case 'M':
                    if (print_state == PRINTSTATE_DATA && !is_have_ID(&marked_buffer, copy_buffer[selected_pointer].ID)) {
                        marked_buffer.push_front(copy_buffer[selected_pointer]);
                        printData();
                    }
                    break;

                case VK_RETURN:
                    PostMessage(hClipboardListenerWindow, WM_USER + 1, 0, 0);
                    is_need_to_paste = true;
                    break;

                case VK_DELETE: {
                    unsigned ID_for_del = (buffers[print_state]->begin() + *pointers[print_state])->ID;
                    for (std::deque<DataObject>* buf : buffers) {
                        for (int i = 0; i < buf->size(); i++) {
                            if ((*buf)[i].ID == ID_for_del) 
                                buf->erase(buf->begin() + i);
                        }
                        
                    }
                    printData();
                    break; 
                }
                case VK_ESCAPE:
                    ShowWindow(hConsole, SW_HIDE);
                    is_activated_window = false;  
                    break;
                }
            }
        }
    }
    return CallNextHookEx(hKeyboardHook, nCode, wParam, lParam);

}

/******************************** main ********************************/

int main() { 
    ShowWindow(hConsole, SW_HIDE);

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = ClipboardListener;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = L"ClipboardListenerWindowClass";
    RegisterClassEx(&wc);
    hClipboardListenerWindow = CreateWindowEx(0, wc.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
    AddClipboardFormatListener(hClipboardListenerWindow);

    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardListener, GetModuleHandle(nullptr), 0);

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) { // main loop
        if (is_need_to_paste) { //if Enter pressed
            ShowWindow(hConsole, SW_HIDE);
            is_activated_window = false;
            pasteData();
            std::iter_swap(buffers[print_state]->begin(), buffers[print_state]->begin() + *pointers[print_state]);
            is_need_to_paste = false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    RemoveClipboardFormatListener(hClipboardListenerWindow);
    UnhookWindowsHookEx(hKeyboardHook);
    
    return 0;
}