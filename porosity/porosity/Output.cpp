#include "Porosity.h"

#ifdef _WIN32
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
Red(const char *Format, ...)
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

#else
#include <stdarg.h>

void
Red(const char *Format, ...)
{
    va_list va;

    va_start(va, Format);
    vprintf(C_RED, va);
    vprintf(Format, va);
    vprintf(C_RESET, va);
    va_end(va);
}

#endif
