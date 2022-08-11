#include "Environment.h"

BOOL WINAPI DllMain(HINSTANCE hmodule,
    DWORD reason,
    LPVOID lpreserved)
{
    switch (reason)
    {
    case DLL_PROCESS_ATTACH:
        break;
    case DLL_PROCESS_DETACH:
        break;
    };
    return TRUE;
}

int main(int argc, char* argv)
{
    return 0;
}