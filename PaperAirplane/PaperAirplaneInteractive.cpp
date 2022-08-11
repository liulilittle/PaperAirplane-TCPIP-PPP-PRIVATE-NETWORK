#include "PaperAirplaneInteractive.h"
#include "Debugger.h"
#include "SocketExtension.h"
#include <Windows.h>
#include <TlHelp32.h>

#pragma warning(disable: 4312)
#pragma warning(disable: 4800)
#pragma comment(lib, "WinMM.lib")

PaperAirplaneInteractive PaperAirplaneInteractive_Current;

UINT64 PaperAirplaneInteractive::GetTickCount64() {
    static LARGE_INTEGER ticksPerSecond; // (unsigned long long)GetTickCount64();
    LARGE_INTEGER ticks;
    if (!ticksPerSecond.QuadPart) {
        QueryPerformanceFrequency(&ticksPerSecond);
    }

    QueryPerformanceCounter(&ticks);
    unsigned long long seconds = ticks.QuadPart / ticksPerSecond.QuadPart;
    unsigned long long milliseconds = 1000 * (ticks.QuadPart - (ticksPerSecond.QuadPart * seconds)) / ticksPerSecond.QuadPart;
    unsigned long long nowtick = seconds * 1000 + milliseconds;
    return (unsigned long long)nowtick;
}

std::shared_ptr<PaperAirplaneConfiguration> PaperAirplaneInteractive::GetConfiguration() {
    static std::shared_ptr<PaperAirplaneConfiguration> config;
    static uint64_t last = 0;
    static std::mutex lockobj;

    std::lock_guard<std::mutex> scope(lockobj);
    uint64_t now = GetTickCount64();
    if (last > now || (now - last) >= 1000) {
        config = std::make_shared<PaperAirplaneConfiguration>();
        if (NULL != config) {
            HANDLE hEvent = NULL;
            HANDLE hMap = NULL;

            int nErr = 0;
            PaperAirplaneConfiguration* pstConf = (PaperAirplaneConfiguration*)this->mmap(hMap, hEvent, &nErr);
            if (nErr != WAIT_FAILED) {
                *config = *pstConf;
                if (config->Enabled) {
                    config->Enabled = GetCurrentProcessId() != config->ProcessId;
                }
            }
            this->munmap(hMap, hEvent, pstConf);
            if (!config->Enabled) {
                config = NULL;
            }
        }
    }
    last = now;
    return config;
}

char* PaperAirplaneInteractive::mmap(HANDLE& hMap, HANDLE& hEvent, int* nErr)
{
    void* pLow = NULL;
    if (NULL == hMap) {
        hMap = OpenFileMapping(FILE_MAP_READ, FALSE, _T(PAPERAIRPLANE_CONFIGURATION_MAP));
    }

    if (NULL == hEvent) {
        hEvent = OpenEvent(SYNCHRONIZE, FALSE, _T(PAPERAIRPLANE_CONFIGURATION_EVENT));
    }

    if (NULL != hMap) {
        pLow = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    }

    int err = WAIT_FAILED;
    do {
        if (NULL == hEvent || NULL == pLow) {
            break;
        }

        int rc = WaitForSingleObject(hEvent, 0);
        if (rc == WAIT_TIMEOUT) {
            err = WAIT_TIMEOUT;
            break;
        }
        else if (rc != WAIT_OBJECT_0) {
            break;
        }
        else {
            err = WAIT_OBJECT_0;
        }
    } while (0, 0);
    if (err != WAIT_OBJECT_0) {
        munmap(hMap, hEvent, pLow);
    }

    if (NULL != nErr) {
        *nErr = err;
    }
    return (char*)pLow;
}

void PaperAirplaneInteractive::munmap(HANDLE hMap, HANDLE hEvent, void* pLow)
{
    if (NULL != pLow) {
        UnmapViewOfFile(pLow);
    }

    if (NULL != hMap) {
        CloseHandle(hMap);
    }

    if (NULL != hEvent) {
        CloseHandle(hEvent);
    }
}

BOOL PaperAirplaneInteractive::Connect(PaperAirplaneConfiguration* pstConf, ULONG sourceAddress, USHORT sourcePort, ULONG destinationAddress, USHORT destinationPort, int* outport)
{
    if (NULL == outport) {
        return FALSE;
    }
    else {
        *outport = 0;
    }

    USHORT usLocalPort = pstConf->Tcp;
    if (0 == usLocalPort) {
        return FALSE;
    }

#pragma pack(push, 1)
    struct
    {
        BYTE	address_type;
        ULONG	address_v4;
        USHORT	address_port;
        ULONG	source_addr_v4;
        USHORT	source_port;
    } u_req_inet4;
#pragma pack(pop)

    u_req_inet4.address_type = PAPERAIRPLANE_ADDRESSTYPE_INET;
    u_req_inet4.address_port = destinationPort;
    u_req_inet4.address_v4 = destinationAddress;
    u_req_inet4.source_port = sourcePort;
    u_req_inet4.source_addr_v4 = sourceAddress;

    int fd = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return FALSE;
    }

    int nTimeout = 200;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (char*)&nTimeout, sizeof(nTimeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (char*)&nTimeout, sizeof(nTimeout));

    struct sockaddr_in destinationEP;
    ZeroMemory(&destinationEP, sizeof(destinationEP));
    {
        destinationEP.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        destinationEP.sin_family = AF_INET;
        destinationEP.sin_port = ntohs(usLocalPort);
    }

    for (int iTimes = 0; iTimes < 5; iTimes++) {
        int iErr = sendto(fd, (char*)&u_req_inet4, sizeof(u_req_inet4), 0,
            (struct sockaddr*)&destinationEP, sizeof(destinationEP));
        if (iErr < 0) {
            continue;
        }

        int szEP = sizeof(destinationEP);
        int iPort = 0;
        int iSize = recvfrom(fd, (char*)&iPort, sizeof(iPort), 0, (struct sockaddr*)&destinationEP, &szEP);
        if (iSize < 0) {
            int err = WSAGetLastError();
            if (err == ERROR_SUCCESS) {
                iSize = sizeof(iPort);
            }
        }

        if (iSize < sizeof(iSize)) {
            continue;
        }
        else {
            closesocket(fd);
        }

        *outport = iPort;
        return iPort != 0 ? TRUE : FALSE;
    }

    closesocket(fd);
    return FALSE;
}

BOOL PaperAirplaneInteractive::IsProcessActivityById(DWORD pId)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (INVALID_HANDLE_VALUE == hSnapshot) {
        return FALSE;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);
    for (BOOL bNext = Process32First(hSnapshot, &pe); bNext; bNext = Process32Next(hSnapshot, &pe)) {
        if (pe.th32ProcessID == pId) {
            CloseHandle(hSnapshot);
            return TRUE;
        }
    }

    CloseHandle(hSnapshot);
    return FALSE;
}