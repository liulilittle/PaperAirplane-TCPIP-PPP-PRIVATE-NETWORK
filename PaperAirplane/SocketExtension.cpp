#include "SocketExtension.h"
#include "LayeredServiceProvider.h"

extern LayeredServiceProvider LayeredServiceProvider_Current;

VOID* SocketExtension::GetExtensionFunction(SOCKET s, GUID* clasid)
{
    if (NULL == clasid || s == INVALID_SOCKET)
    {
        return NULL;
    }
    VOID* func = NULL;
    DWORD size = 0;
    //
    WSATHREADID threadid;
    threadid.ThreadHandle = OpenThread(THREAD_ALL_ACCESS, FALSE, GetCurrentThreadId());
    threadid.Reserved = NULL;

    INT error = 0;
    if (LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, SIO_GET_EXTENSION_FUNCTION_POINTER, clasid,
        sizeof(GUID), &func, sizeof(VOID*), &size, NULL, NULL, &threadid, &error) == SOCKET_ERROR)
    {
        CloseHandle(threadid.ThreadHandle);
        return NULL;
    }
    CloseHandle(threadid.ThreadHandle);
    return func;
}

VOID* SocketExtension::GetExtensionFunction(GUID* clasid)
{
    if (NULL == clasid)
    {
        return NULL;
    }
    INT error = 0;
    SOCKET s = LayeredServiceProvider_Current.NextProcTable.lpWSPSocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED, &error);
    if (s == INVALID_SOCKET)
    {
        return NULL;
    }
    VOID* func = GetExtensionFunction(s, clasid);
    LayeredServiceProvider_Current.NextProcTable.lpWSPShutdown(s, SD_BOTH, &error);
    LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, &error);
    return func;
}

VOID* SocketExtension::GetExtensionFunction(SOCKET s, GUID clasid)
{
    return GetExtensionFunction(s, &clasid);
}

VOID* SocketExtension::GetExtensionFunction(GUID clasid)
{
    return GetExtensionFunction(&clasid);
}

bool SocketExtension::SetSocketNonblock(SOCKET s, u_long nonblock)
{
    WSATHREADID id;
    id.ThreadHandle = GetCurrentThread();
    id.Reserved = NULL;
    INT err;
    DWORD pvout, pvlen = sizeof(DWORD);
    return LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, FIONBIO, &nonblock, sizeof(u_long), &pvout,
        sizeof(DWORD), &pvlen, NULL, NULL, &id, &err) == 0;
}

struct sockaddr* SocketExtension::LocalIP(BOOL localhost)
{
    if (localhost)
    {
        char hostname[MAX_PATH];
        if (gethostname(hostname, MAX_PATH) != 0)
        {
            return NULL;
        }
        hostent* hostinfo = gethostbyname(hostname);
        if (hostinfo == NULL || *hostinfo->h_addr_list == NULL)
        {
            return NULL;
        }
        if (hostinfo->h_addrtype == AF_INET)
        {
            struct sockaddr_in* addr = new struct sockaddr_in;
            addr->sin_family = AF_INET;
            addr->sin_port = 0;
            addr->sin_addr.s_addr = *(u_long*)*hostinfo->h_addr_list;
            return (struct sockaddr*)addr;
        }
        else if (hostinfo->h_addrtype != AF_INET6)
        {
            return NULL;
        }
        else
        {
            struct sockaddr_in6* addr = new struct sockaddr_in6;
            memset(addr, 0, sizeof(struct sockaddr_in6));
            addr->sin6_family = AF_INET6;
            addr->sin6_port = 0;
            memcpy(addr->sin6_addr.u.Byte, *hostinfo->h_addr_list, 16);
            return (struct sockaddr*)addr;
        }
    }
    else
    {
        std::vector<ULONG>* localEPs = SocketExtension::LocalIPs();
        if (localEPs == NULL)
        {
            return NULL;
        }
        struct sockaddr_in* addr = NULL;
        if (!localEPs->empty())
        {
            addr = new struct sockaddr_in;
            addr->sin_family = AF_INET;
            addr->sin_port = 0;
            addr->sin_addr.s_addr = (*localEPs)[0];
        }
        delete localEPs;
        return (struct sockaddr*)addr;
    }
}

std::vector<ULONG>* SocketExtension::LocalIPs()
{
    PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
    unsigned long stSize = sizeof(IP_ADAPTER_INFO);
    int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
    if (ERROR_BUFFER_OVERFLOW == nRel)
    {
        delete pIpAdapterInfo;
        pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
        nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
    }
    std::vector<ULONG>* address = NULL;
    if (ERROR_SUCCESS == nRel)
    {
        while (pIpAdapterInfo)
        {
            IP_ADDR_STRING* pIpAddrString = &(pIpAdapterInfo->IpAddressList);
            do
            {
                ULONG addrnum = inet_addr(pIpAddrString->IpAddress.String);
                if (addrnum != 0)
                {
                    if (address == NULL)
                    {
                        address = new std::vector<ULONG>();
                    }
                    address->push_back(addrnum);
                }
                pIpAddrString = pIpAddrString->Next;
            } while (pIpAddrString);
            pIpAdapterInfo = pIpAdapterInfo->Next;
        }
    }
    if (pIpAdapterInfo)
    {
        delete pIpAdapterInfo;
    }
    return address;
}

void SocketExtension::CloseSocket(SOCKET s)
{
    if (s != INVALID_SOCKET)
    {
        INT error;
        LayeredServiceProvider_Current.NextProcTable.lpWSPShutdown(s, SD_BOTH, &error);
        LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, &error);
    }
}

int SocketExtension::GetSocketType(SOCKET s)
{
    if (s != INVALID_SOCKET) {
        int sockType = 0;
        int optlen = 4;
        if (getsockopt(s, SOL_SOCKET, SO_TYPE, (char*)&sockType, &optlen) != SOCKET_ERROR) {
            return sockType;
        }
    }
    return SOCKET_ERROR;
}

int* SocketExtension::IOControl(SOCKET s, int ioControlCode, void* optionInValue, int optionInValueLen, void* optionOutValue, int optionOutValueLen)
{
    WSATHREADID threadid;
    threadid.ThreadHandle = GetCurrentThread();
    threadid.Reserved = NULL;
    int* result = new int(0);
    INT err = 0;
    if (LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, ioControlCode, optionInValue, optionInValueLen,
        optionOutValue, optionOutValueLen, (LPDWORD)&result, NULL, NULL, &threadid, &err) != 0)
    {
        delete result;
        return NULL;
    }
    return result;
}