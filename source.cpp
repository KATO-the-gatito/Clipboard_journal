#define UNICODE
#define _UNICODE

#include <Windows.h>
#include <shellapi.h>
#include <iostream>
#include <vector>
#include <deque>
#include <format>
#include <string>
#include <algorithm>
#include <stdarg.h>
#include <array>
#include <utility>

#define TXTCOLOR_SELECTED    224 // 96
#define TXTCOLOR_UNSELECTED  15
#define TXTCOLOR_NORMAL      10
#define TXTCOLOR_BOX         3
#define TXTCOLOR_NOTICE      13
#define TXTCOLOR_PS_SEL      143
#define TXTCOLOR_PS_UNSEL    15

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
std::string convert_size(size_t len);
void ClearBuffer(HANDLE hBuffer);
void ForceForegroundWindow(HWND hWnd);
void moveCursor(SHORT x, SHORT y);
int countLinesBuffer(int left, int right);
void gotoActiveLine(int left_brd);


/******************************* structs *******************************/


struct DataObject
{
    std::wstring data; // the copied text
    int cntlines; // the number of all lines of text
    unsigned ID; // the unique ID of the text
};

struct BufferObject
{
    std::deque<DataObject> buffer; // the buffer containing the copied texts
    size_t size; // the total weight of all texts in the buffer
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
HANDLE hActiveConsoleBuffer = GetStdHandle(STD_OUTPUT_HANDLE), hHiddenConsoleBuffer;
HHOOK hKeyboardHook = nullptr;
BufferObject copy_buffer, marked_buffer;
int selected_pointer = 0, selected_pointer_mrkd = 0, print_state = 0;
unsigned ID_counter = 0;
bool is_activated_window = false, is_need_to_paste = false;
const std::array<BufferObject*, 2> buffers = {&copy_buffer, &marked_buffer};
const std::array<int*, 2> pointers = {&selected_pointer, &selected_pointer_mrkd};


/********************************* functions *******************************/


// formatted and colored printing
void clrprintf(const char* strin, ...) { 
    size_t len = strlen(strin);
    char str[1024], tmp_str[1024];
    char* chr = str;
    strcpy(str, strin);
    int cntr_of_cmds = 0;
    va_list colors;
    va_start(colors, strin);

    for (int i = 0; i < len; ++i) {
        if (str[i] == '@') {
            str[i] = '\0';
            vsprintf(tmp_str, chr, colors);
            WriteConsoleA(hHiddenConsoleBuffer, tmp_str, strlen(tmp_str), nullptr, nullptr);
            for (int j = 0; j < cntr_of_cmds; j++)
                va_arg(colors, int);
            chr = str + i + 1;
            cntr_of_cmds = 0;
            SetConsoleTextAttribute(hHiddenConsoleBuffer, va_arg(colors, int));
        }
        if (str[i] == '%' && str[i + 1] != '%' && str[i - 1] != '%') ++cntr_of_cmds;
    }
    vsprintf(tmp_str, chr, colors);
    WriteConsoleA(hHiddenConsoleBuffer, tmp_str, strlen(tmp_str), nullptr, nullptr);
    va_end(colors);
}

void PrintWString(const std::wstring& text) {
    DWORD written;
    WriteConsoleW(hHiddenConsoleBuffer, text.c_str(), text.length(), &written, nullptr);
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
        for (int i = 0; i < copy_buffer.buffer.size(); i++) {
            if (copy_buffer.buffer[i].data == pText) {
                std::rotate(copy_buffer.buffer.begin(), copy_buffer.buffer.begin() + i, copy_buffer.buffer.end());
                goto exit;
            }
        }
        copy_buffer.buffer.push_front(DataObject{pText, countLinesStr(pText), ++ID_counter});
        copy_buffer.size += wcslen(pText) * sizeof(wchar_t);
    }
exit:
    GlobalUnlock(hMem);
    CloseClipboard();
}

void pasteData() {
    std::wstring text = (buffers[print_state]->buffer.begin() + *pointers[print_state])->data;

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
    ClearBuffer(hHiddenConsoleBuffer);
    clrprintf("@====[@Clipboard data@]=[@Marked data@]====\n",
        TXTCOLOR_NORMAL,
        print_state == PRINTSTATE_DATA ? TXTCOLOR_PS_SEL : TXTCOLOR_PS_UNSEL, 
        TXTCOLOR_NORMAL,
        print_state == PRINTSTATE_MARKED ? TXTCOLOR_PS_SEL : TXTCOLOR_PS_UNSEL, 
        TXTCOLOR_NORMAL
    );
    clrprintf("@elements: @%d@ | size: @%s\n", 
        7, 11, buffers[print_state]->buffer.size(),
        7, 11, convert_size(buffers[print_state]->size).c_str()
    );
    clrprintf("@=======================================\n", TXTCOLOR_NORMAL);

    int i = 0;
    bool is_notice = false;
    LR_borders borders(buffers[print_state]->buffer, *pointers[print_state], hHiddenConsoleBuffer);

    for (i = borders.left; i < borders.right; ++i) {
        SetConsoleTextAttribute(hHiddenConsoleBuffer, TXTCOLOR_UNSELECTED);
        if ((buffers[print_state]->buffer.begin() + i)->data.size() > MAX_STRING_SIZE ||
            (buffers[print_state]->buffer.begin() + i)->cntlines > MAX_STRING_LINES) 
        {
            clrprintf("@<Too big text>@\n", TXTCOLOR_NOTICE, TXTCOLOR_UNSELECTED);
            is_notice = true;
        }
        if (i == *pointers[print_state])
            SetConsoleTextAttribute(hHiddenConsoleBuffer, TXTCOLOR_SELECTED);
        if (is_notice) {
            PrintWString((buffers[print_state]->buffer.begin() + i)->data.substr(0, 100));
            clrprintf("@...", TXTCOLOR_UNSELECTED);
            is_notice = false;
        }
        else PrintWString((buffers[print_state]->buffer.begin() + i)->data);
        
        clrprintf("@\n=======================================\n", TXTCOLOR_BOX);
    }
    if (!i) clrprintf("@<The buffer is empty>\n", TXTCOLOR_NOTICE);

    clrprintf("@============<End of buffer>============\n", TXTCOLOR_NORMAL);
    gotoActiveLine(borders.left);

    SetConsoleActiveScreenBuffer(hHiddenConsoleBuffer);
    std::swap(hActiveConsoleBuffer, hHiddenConsoleBuffer);
}

std::string convert_size(size_t len) {
    if (!(len >> 10)) 
        return std::to_string(len) + " bytes";
    if (!(len >> 20))
        return std::to_string(len >> 10) + " KiB";
    if (!(len >> 30))
        return std::to_string(len >> 20) + " MiB";
    return std::to_string(len >> 30) + " GiB";
}

void ClearBuffer(HANDLE hBuffer) {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hBuffer, &csbi);

    DWORD totalCells = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written;

    FillConsoleOutputCharacter(hBuffer, ' ', totalCells, {0, 0}, &written);
    FillConsoleOutputAttribute(hBuffer, csbi.wAttributes, totalCells, {0, 0}, &written);

    SetConsoleCursorPosition(hBuffer, {0, 0});
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
    SetConsoleCursorPosition(hHiddenConsoleBuffer, coord);
}

int countLinesBuffer(int left, int right) {
    SHORT lines = 3;
    for (int i = left; i < right; i++) {
        if ((buffers[print_state]->buffer.begin() + i)->data.size() <= MAX_STRING_SIZE &&
            (buffers[print_state]->buffer.begin() + i)->cntlines <= MAX_STRING_LINES)
        {
            lines += (SHORT)(buffers[print_state]->buffer.begin() + i)->cntlines + 1;
            continue;
        }
        lines += countLinesStr((buffers[print_state]->buffer.begin() + i)->data.substr(0, 100)) + 2;
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


// a function that is activated if the contents of the clipboard have changed
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

// a function that is activated by pressing any key.
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
                    if (*pointers[print_state] + 1 < buffers[print_state]->buffer.size()) {
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
                    if (print_state == PRINTSTATE_DATA && !is_have_ID(&marked_buffer.buffer, copy_buffer.buffer[selected_pointer].ID)) {
                        marked_buffer.size += copy_buffer.buffer[selected_pointer].data.length() * sizeof(wchar_t);
                        marked_buffer.buffer.push_front(copy_buffer.buffer[selected_pointer]);
                        printData();
                    }
                    break;

                case VK_RETURN:
                    PostMessage(hClipboardListenerWindow, WM_USER + 1, 0, 0);
                    is_need_to_paste = true;
                    break;

                case VK_DELETE: {
                    unsigned ID_for_del = (buffers[print_state]->buffer.begin() + *pointers[print_state])->ID;
                    for (BufferObject* bfr : buffers) {
                        std::deque<DataObject>* buf = &bfr->buffer;
                        for (int i = 0; i < buf->size(); i++) {
                            if ((*buf)[i].ID == ID_for_del) {
                                bfr->size -= (size_t)((buf->begin() + i)->data.length() * sizeof(wchar_t));
                                buf->erase(buf->begin() + i);
                            }
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

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hActiveConsoleBuffer, &csbi);

    hHiddenConsoleBuffer = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        CONSOLE_TEXTMODE_BUFFER, nullptr);
    SetConsoleScreenBufferSize(hHiddenConsoleBuffer, csbi.dwSize);

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
            std::rotate(buffers[print_state]->buffer.begin(), buffers[print_state]->buffer.begin() + *pointers[print_state], buffers[print_state]->buffer.end());
            is_need_to_paste = false;
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    RemoveClipboardFormatListener(hClipboardListenerWindow);
    UnhookWindowsHookEx(hKeyboardHook);
    
    return 0;
}