#include "Environment.h"
#include "Debugger.h"
#include "Proxifier.h"
#include "SocketExtension.h"
#include "LayeredServiceProvider.h"
#include "LayeredServiceProvider.h"

extern LayeredServiceProvider								LayeredServiceProvider_Current;
static LPFN_CONNECTEX										PFN_ConnectEx = NULL;
class ConnectionTable {
    typedef std::hash_map<UINT64, struct sockaddr_in>		AddressTable;

public:
    inline ConnectionTable() {
        memset(&this->_cs, 0, sizeof(CRITICAL_SECTION));
        InitializeCriticalSection(&this->_cs);
    }
    inline ~ConnectionTable() {
        DeleteCriticalSection(&this->_cs);
    }

public:
    class Scope {
    public:
        inline Scope(ConnectionTable& connectionTable)
            : _connectionTable(connectionTable) {
            EnterCriticalSection(&this->_connectionTable._cs);
        }
        inline ~Scope() {
            LeaveCriticalSection(&this->_connectionTable._cs);
        }

    private:
        ConnectionTable& _connectionTable;
    };
    inline BOOL												FindAddress(SOCKET s, struct sockaddr* name, LPINT namelen) {
        Scope scope(ConnectionTable_Current);
        {
            AddressTable::iterator tail = this->_addressTable.find(s);
            AddressTable::iterator endl = this->_addressTable.end();
            if (tail == endl) {
                return FALSE;
            }
            if (NULL != namelen) {
                *namelen = sizeof(struct sockaddr_in);
            }
            if (NULL != name) {
                *(struct sockaddr_in*)name = tail->second;
            }
            return TRUE;
        }
    }
    inline BOOL												RemoveAddress(SOCKET s) {
        Scope scope(ConnectionTable_Current);
        {
            AddressTable::iterator tail = this->_addressTable.find(s);
            AddressTable::iterator endl = this->_addressTable.end();
            if (tail == endl) {
                return FALSE;
            }
            this->_addressTable.erase(tail);
            return TRUE;
        }
    }
    inline BOOL												AddAddress(SOCKET s, const struct sockaddr* name) {
        if (NULL == name || name->sa_family != AF_INET) {
            return FALSE;
        }
        Scope scope(ConnectionTable_Current);
        {
            this->_addressTable[s] = *(struct sockaddr_in*)name;
            return TRUE;
        }
    }

private:
    AddressTable											_addressTable;
    CRITICAL_SECTION										_cs;
}															ConnectionTable_Current;

int WSPAPI WSPConnect(SOCKET s, const struct sockaddr* name, int namelen, LPWSABUF lpCallerData, LPWSABUF lpCalleeData, LPQOS lpSQOS,
    LPQOS lpGQOS, LPINT lpErrno)
{
    ULONG host;
    int port = PRXCreateTunnel(s, name, namelen, &host);
    if (port != 0) {
        struct sockaddr_in server;
        ZeroMemory(&server, sizeof(struct sockaddr_in));

        server.sin_family = AF_INET;
        server.sin_port = port; // PORT
        server.sin_addr.s_addr = host;

        ConnectionTable_Current.AddAddress(s, name);
        return LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in),
            lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
    }
    return LayeredServiceProvider_Current.NextProcTable.lpWSPConnect(s, name, namelen, lpCallerData, lpCalleeData, lpSQOS, lpGQOS, lpErrno);
}

BOOL PASCAL WSPConnectEx(SOCKET s, const sockaddr* name, int namelen, PVOID lpSendBuffer, DWORD dwSendDataLength, LPDWORD lpdwBytesSent,
    LPOVERLAPPED lpOverlapped)
{
    if (NULL == PFN_ConnectEx) {
        GUID metid = WSAID_CONNECTEX;
        PFN_ConnectEx = (LPFN_CONNECTEX)SocketExtension::GetExtensionFunction(s, metid);
        if (NULL == PFN_ConnectEx) {
            return FALSE;
        }
    }
    ULONG host;
    int port = PRXCreateTunnel(s, name, namelen, &host);
    if (port != 0) {
        struct sockaddr_in server;
        ZeroMemory(&server, sizeof(struct sockaddr_in));

        server.sin_family = AF_INET;
        server.sin_port = port; // PORT
        server.sin_addr.s_addr = host;

        return PFN_ConnectEx(s, (struct sockaddr*)&server, sizeof(struct sockaddr_in), lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
    }
    return PFN_ConnectEx(s, name, namelen, lpSendBuffer, dwSendDataLength, lpdwBytesSent, lpOverlapped);
}

int WSPAPI WSPIoctl(SOCKET s, DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer, LPVOID lpvOutBuffer, DWORD cbOutBuffer,
    LPDWORD lpcbBytesReturned, LPWSAOVERLAPPED lpOverlapped, LPWSAOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine, LPWSATHREADID lpThreadId, LPINT lpErrno)
{
    if (dwIoControlCode == SIO_GET_EXTENSION_FUNCTION_POINTER) {
        GUID connectex = WSAID_CONNECTEX;
        if (memcmp(lpvInBuffer, &connectex, sizeof(GUID)) == 0) {
            *((LPFN_CONNECTEX*)lpvOutBuffer) = &WSPConnectEx;
            *lpcbBytesReturned = sizeof(HANDLE);
            *lpErrno = NO_ERROR;
            return NO_ERROR;
        }
    }
    return LayeredServiceProvider_Current.NextProcTable.lpWSPIoctl(s, dwIoControlCode, lpvInBuffer, cbInBuffer, lpvOutBuffer, cbOutBuffer
        , lpcbBytesReturned, lpOverlapped, lpCompletionRoutine, lpThreadId, lpErrno);
}

int WSPAPI WSPGetPeerName(SOCKET s, struct sockaddr* name, LPINT namelen, LPINT lpErrno)
{
    if (ConnectionTable_Current.FindAddress(s, name, namelen)) {
        if (NULL != lpErrno) {
            *lpErrno = ERROR_SUCCESS;
        }
        return ERROR_SUCCESS;
    }
    return LayeredServiceProvider_Current.NextProcTable.lpWSPGetPeerName(s, name, namelen, lpErrno);
}

int WSPAPI WSPCloseSocket(SOCKET s, LPINT lpErrno)
{
    if (ConnectionTable_Current.RemoveAddress(s)) {
        if (NULL != lpErrno) {
            *lpErrno = ERROR_SUCCESS;
        }
        return ERROR_SUCCESS;
    }
    return LayeredServiceProvider_Current.NextProcTable.lpWSPCloseSocket(s, lpErrno);
}

void WSPAPI StartProviderCompleted(WSPPROC_TABLE* sender, WSPPROC_TABLE* e)
{
    if (NULL != e) {
        e->lpWSPIoctl = &WSPIoctl;
        e->lpWSPConnect = &WSPConnect;
        e->lpWSPGetPeerName = &WSPGetPeerName;
        e->lpWSPCloseSocket = &WSPCloseSocket;
    }
}

int WSPAPI WSPStartup(WORD wversionrequested, LPWSPDATA lpwspdata, LPWSAPROTOCOL_INFOW lpProtoInfo, WSPUPCALLTABLE upcalltable, LPWSPPROC_TABLE lpproctable)
{
    TCHAR processname[MAX_PATH];
    GetModuleFileName(NULL, processname, MAX_PATH);
    Debugger::Write(L"[PaperAirplane]%s Loading WSPStartup ...", processname);

    LayeredServiceProvider_Current.StartProviderCompleted = &StartProviderCompleted;
    return LayeredServiceProvider_Current.Start(wversionrequested, lpwspdata, lpProtoInfo, upcalltable, lpproctable);
}