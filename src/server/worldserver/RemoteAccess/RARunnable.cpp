/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

/** \file
    \ingroup Skyfired
 */

#include "Common.h"
#include "Config.h"
#include "Log.h"
#include "RARunnable.h"
#include "World.h"

#include "RASocket.h"

#include <cstring>

namespace
{
    int LastSocketError()
    {
#if PLATFORM == PLATFORM_WINDOWS
        return WSAGetLastError();
#else
        return errno;
#endif
    }

    void CloseSocket(RASocketHandle socket)
    {
#if PLATFORM == PLATFORM_WINDOWS
        closesocket(socket);
#else
        close(socket);
#endif
    }

    bool IsValidSocket(RASocketHandle socket)
    {
#if PLATFORM == PLATFORM_WINDOWS
        return socket != INVALID_SOCKET;
#else
        return socket >= 0;
#endif
    }

    bool SetReuseAddress(RASocketHandle socket)
    {
        int enabled = 1;
        return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&enabled), sizeof(enabled)) == 0;
    }

    std::string FormatAddress(sockaddr_storage const& address)
    {
        char buffer[INET6_ADDRSTRLEN] = { 0 };

        if (address.ss_family == AF_INET)
        {
            sockaddr_in const* ipv4 = reinterpret_cast<sockaddr_in const*>(&address);
            inet_ntop(AF_INET, &ipv4->sin_addr, buffer, sizeof(buffer));
        }
        else if (address.ss_family == AF_INET6)
        {
            sockaddr_in6 const* ipv6 = reinterpret_cast<sockaddr_in6 const*>(&address);
            inet_ntop(AF_INET6, &ipv6->sin6_addr, buffer, sizeof(buffer));
        }

        return buffer;
    }
}

void RARunnable::Run()
{
    if (!sConfigMgr->GetBoolDefault("Ra.Enable", false))
        return;

    uint16 raPort = uint16(sConfigMgr->GetIntDefault("Ra.Port", 3443));
    std::string stringIp = sConfigMgr->GetStringDefault("Ra.IP", "0.0.0.0");

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    addrinfo* result = NULL;
    std::string port = std::to_string(raPort);
    if (getaddrinfo(stringIp.c_str(), port.c_str(), &hints, &result) != 0)
    {
        SF_LOG_ERROR("server.worldserver", "Skyfire RA can not resolve bind address %s:%d", stringIp.c_str(), raPort);
        return;
    }

#if PLATFORM == PLATFORM_WINDOWS
    RASocketHandle listenSocket = INVALID_SOCKET;
#else
    RASocketHandle listenSocket = -1;
#endif

    for (addrinfo* address = result; address; address = address->ai_next)
    {
        listenSocket = ::socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (!IsValidSocket(listenSocket))
            continue;

        SetReuseAddress(listenSocket);

        if (::bind(listenSocket, address->ai_addr, int(address->ai_addrlen)) == 0 &&
            ::listen(listenSocket, SOMAXCONN) == 0)
            break;

        CloseSocket(listenSocket);
#if PLATFORM == PLATFORM_WINDOWS
        listenSocket = INVALID_SOCKET;
#else
        listenSocket = -1;
#endif
    }

    freeaddrinfo(result);

    if (!IsValidSocket(listenSocket))
    {
        SF_LOG_ERROR("server.worldserver", "Skyfire RA can not bind to port %d on %s, error %d", raPort, stringIp.c_str(), LastSocketError());
        return;
    }

    SF_LOG_INFO("server.worldserver", "Starting Skyfire RA on port %d on %s", raPort, stringIp.c_str());

    while (!World::IsStopped())
    {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(listenSocket, &readSet);

        timeval interval;
        interval.tv_sec = 0;
        interval.tv_usec = 100000;

        int ready = select(int(listenSocket + 1), &readSet, NULL, NULL, &interval);
        if (ready <= 0)
            continue;

        sockaddr_storage remoteAddress;
        socklen_t remoteLength = sizeof(remoteAddress);
        RASocketHandle clientSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&remoteAddress), &remoteLength);
        if (!IsValidSocket(clientSocket))
            continue;

        std::string remote = FormatAddress(remoteAddress);
        SF_LOG_INFO("commands.ra", "Incoming connection from %s", remote.c_str());

        (new RASocket(clientSocket, remote))->start();
    }

    CloseSocket(listenSocket);

    SF_LOG_DEBUG("server.worldserver", "Skyfire RA thread exiting");
}
