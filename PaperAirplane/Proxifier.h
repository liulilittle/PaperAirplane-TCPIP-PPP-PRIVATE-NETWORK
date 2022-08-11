#ifndef PROXIFIER_H
#define PROXIFIER_H

#include "Environment.h"

int PRXCreateTunnel(SOCKET s, const struct sockaddr* name, int namelen, ULONG* hostaddr);
#endif