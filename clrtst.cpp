#include <iostream>
#include <Windows.h>

int main() {
    HANDLE pen = GetStdHandle(STD_OUTPUT_HANDLE);

    for (WORD i = 0; i < 256; i++) {
        SetConsoleTextAttribute(pen, i);
        std::cout << "meow! " << i << '\n';
    }
    system("pause");
    return 0;
    
}