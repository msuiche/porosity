#ifdef _WIN32
uint16_t GetConsoleTextAttribute(uint32_t hConsole);
#else
#define C_RED   "[0;31m"
#define C_RESET "[0m"
#endif

void Red(const char *Format, ...);
