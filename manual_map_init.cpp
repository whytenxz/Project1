#include <Windows.h>

BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved);

extern "C" __declspec(dllexport) DWORD WINAPI ManualMapInit(HMODULE base)
{
    if (!DllMain(base, DLL_PROCESS_ATTACH, nullptr))
        return 8;

    return 0;
}
