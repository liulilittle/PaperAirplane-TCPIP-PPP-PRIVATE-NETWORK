#include "lsp.h"

BOOL IsWow64System()
{
    SYSTEM_INFO stInfo = { 0 };
    GetNativeSystemInfo(&stInfo);
    if (stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_IA64
        || stInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
    {
        return TRUE;
    }
    return FALSE;
}

void install()
{
    WCHAR path[MAX_PATH] = L"%SystemRoot%\\system32\\PaperAirplane.dll";
    if (!InstallLayeredServiceProvider(path))
    {
        printf("0\n");
    }
    else
    {
        printf("1\n");
    }
}

void uninstall()
{
    if (!UninstallLayeredServiceProvider())
    {
        printf("0\n");
    }
    else
    {
        printf("1\n");
    }
}

void usage(char* progname)
{
    printf("usage: %s install | uninstall\n", progname);
    system("pause");
    ExitProcess(-1);
}

int main(int argc, char** argv)
{
    WSADATA        wsd;
    char* ptr;
    if (argc != 2)
    {
        usage(argv[0]);
        return -1;
    }
    if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
    {
        printf("WSAStartup() failed: %d\n", GetLastError());
        return -1;
    }
    ptr = argv[1];
    while (*ptr)
    {
        *ptr++ = tolower(*ptr);
    }
    if (!strncmp(argv[1], "install", 8))
    {
        install();
    }
    else if (!strncmp(argv[1], "uninstall", 10))
    {
        uninstall();
    }
    else
    {
        usage(argv[0]);
    }
    WSACleanup();
    return 0;
}