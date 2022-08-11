#ifndef SOCKETEXTENSION_H
#define SOCKETEXTENSION_H

#include "Environment.h"

class SocketExtension
{
public:
	static VOID*					GetExtensionFunction(SOCKET s, GUID* clasid);
	static VOID*					GetExtensionFunction(GUID* clasid);
	static VOID*					GetExtensionFunction(SOCKET s, GUID clasid);
	static VOID*					GetExtensionFunction(GUID clasid);
	static bool						SetSocketNonblock(SOCKET s, u_long nonblock);

public:
	static struct sockaddr*			LocalIP(BOOL localhost);
	static std::vector<ULONG>*		LocalIPs();
	static void						CloseSocket(SOCKET s);
	static int						GetSocketType(SOCKET s);
	static int*						IOControl(SOCKET s, int ioControlCode, void* optionInValue, int optionInValueLen, void* optionOutValue, int optionOutValueLen);
};
#endif
