#ifndef PAPERAIRPLANEINTERACTIVE_H
#define PAPERAIRPLANEINTERACTIVE_H

#include "SocketExtension.h"
#include "MMap.h"

#include <mutex>
#include <atomic>
#include <string>
#include <memory>

#ifndef PAPERAIRPLANE_CONFIGURATION_MAP
#define PAPERAIRPLANE_CONFIGURATION_MAP "PAPERAIRPLANE_CONFIGURATION_MAP"
#endif

#ifndef PAPERAIRPLANE_CONFIGURATION_EVENT
#define PAPERAIRPLANE_CONFIGURATION_EVENT "PAPERAIRPLANE_CONFIGURATION_EVENT"
#endif

#define PAPERAIRPLANE_PROXECONNECT_MESSAGE 0x1311
#define PAPERAIRPLANE_ADDRESSTYPE_INET 1

#pragma pack(push, 1)
typedef struct
{
    BYTE									                Enabled;    // 启用代理
    USHORT									                Tcp;		// 代理端口
    DWORD									                Index;      // 重定向网卡
    DWORD									                ProcessId;	// 主控进程
} PaperAirplaneConfiguration;
#pragma pack(pop)

class PaperAirplaneInteractive
{
public:
    std::shared_ptr<PaperAirplaneConfiguration>             GetConfiguration();
    BOOL									                Connect(PaperAirplaneConfiguration* pstConf, ULONG srcAddr, USHORT srcPort, ULONG dstAddr, USHORT dstPort, int* nOutport);

private:
    char*									                mmap(HANDLE& hMap, HANDLE& hEvent, int* nErr);
    void									                munmap(HANDLE hMap, HANDLE hEvent, void* pLow);
    static BOOL								                IsProcessActivityById(DWORD pId);
    static UINT64                                           GetTickCount64();
};

extern PaperAirplaneInteractive				                PaperAirplaneInteractive_Current;
#endif