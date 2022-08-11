#include "Proxifier.h"
#include "SocketExtension.h"
#include "PaperAirplaneInteractive.h"

extern PaperAirplaneInteractive PaperAirplaneInteractive_Current;

static BOOL
PRXFilterAddressEP(PaperAirplaneConfiguration* configuration, ULONG address, INT port)
{
    if (!configuration->Enabled) {
        return FALSE;
    }

    if (port <= 0 || port > 65535) {
        return FALSE;
    }
    else { // 屏蔽保留地址段
        BYTE k1 = ((BYTE*)&address)[0];
        if (k1 == 0 || k1 == 10 || k1 == 127 || k1 >= 224) {
            return FALSE;
        }

        BYTE k2 = ((BYTE*)&address)[1];
        if ((k1 == 100 && (k2 >= 64 && k2 <= 127)) ||
            (k1 == 169 && k2 == 254) ||
            (k1 == 172 && (k2 >= 16 && k2 <= 31)) ||
            (k1 == 192 && k2 == 168) ||
            (k1 == 198 && (k2 >= 18 && k2 <= 19)) ||
            (k1 == 198 && k2 == 100)) {
            return FALSE;
        }

        BYTE k3 = ((BYTE*)&address)[2];
        if ((k1 == 192 && k2 == 0 && (k3 == 0 || k3 == 2)) ||
            (k1 == 192 && k2 == 88 && k3 == 99) ||
            (k1 == 198 && k2 == 51 && k3 == 100) ||
            (k1 == 203 && k2 == 0 && k3 == 113)) {
            return FALSE;
        }
    }

    if (configuration->Index != UINT_MAX) {
        DWORD dwIndex;
        if (GetBestInterface(address, &dwIndex) == NO_ERROR) {
            if (dwIndex != configuration->Index) {
                return FALSE;
            }
        }
    }
    return TRUE;
}

static int
PRXGetLocalEP(SOCKET s, ULONG* bindaddr) {
    if (NULL != bindaddr) {
        *bindaddr = htonl(INADDR_ANY);
    }

    if (s == (SOCKET)INVALID_HANDLE_VALUE) {
        return 0;
    }

    struct sockaddr_in localEP;
    int szEP = sizeof(localEP);
    if (getsockname(s, (struct sockaddr*)&localEP, &szEP) < 0) {
        struct sockaddr_in bindEP;
        memset(&bindEP, 0, sizeof(bindEP));

        bindEP.sin_family = AF_INET;
        bindEP.sin_addr.s_addr = htonl(INADDR_ANY);
        bindEP.sin_port = htons(0);

        if (bind(s, (struct sockaddr*)&bindEP, sizeof(bindEP)) < 0) {
            return 0;
        }
        else if (getsockname(s, (struct sockaddr*)&localEP, &szEP) < 0) {
            return 0;
        }
    }

    if (localEP.sin_family != AF_INET) {
        return 0;
    }

    ULONG localAddr = localEP.sin_addr.s_addr;
    if (localAddr == htonl(INADDR_BROADCAST)) {
        return 0;
    }

    if (NULL != bindaddr) {
        if (localAddr == htonl(INADDR_ANY)) {
            localAddr = htonl(INADDR_LOOPBACK);
        }
        *bindaddr = localAddr;
    }
    return ntohs(localEP.sin_port);
}

int
PRXCreateTunnel(SOCKET s, const struct sockaddr* name, int namelen, ULONG* hostaddr) // 创建隧道
{
    if (NULL == name || name->sa_family != AF_INET) {
        return 0;
    }

    ULONG localIP = htonl(INADDR_ANY);
    int localPort = PRXGetLocalEP(s, &localIP);
    if (NULL != hostaddr) {
        *hostaddr = localIP;
    }

    if (localPort <= 0 || localPort > 65535) {
        return 0;
    }

    std::shared_ptr<PaperAirplaneConfiguration> configuration = PaperAirplaneInteractive_Current.GetConfiguration();
    if (NULL == configuration) {
        return 0;
    }

    struct sockaddr_in* destinationEP = (struct sockaddr_in*)name;
    USHORT destinationPort = ntohs(destinationEP->sin_port);
    ULONG destinationAddr = destinationEP->sin_addr.s_addr;
    if (PRXFilterAddressEP(configuration.get(), destinationAddr, destinationPort) != TRUE) {
        return 0;
    }

    int outport;
    if (PaperAirplaneInteractive_Current.
        Connect(configuration.get(), localIP, localPort, destinationAddr, destinationPort, &outport) != TRUE) {
        return 0;
    }
    return outport;
}