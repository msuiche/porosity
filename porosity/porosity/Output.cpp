#include "Porosity.h"

#include <windows.h>

uint16_t
GetConsoleTextAttribute(
    uint32_t hConsole
)
{
    HANDLE Handle = (HANDLE)hConsole;
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(Handle, &csbi);
    return(csbi.wAttributes);
}

void
Red(char *Format, ...)
{
    HANDLE Handle;
    USHORT Color;
    va_list va;

    Handle = GetStdHandle(STD_OUTPUT_HANDLE);

    Color = GetConsoleTextAttribute((uint32_t)Handle);

    SetConsoleTextAttribute(Handle, FOREGROUND_RED | FOREGROUND_INTENSITY);
    va_start(va, Format);
    vprintf(Format, va);
    va_end(va);

    SetConsoleTextAttribute(Handle, Color);
}