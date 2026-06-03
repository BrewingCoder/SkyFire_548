/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Log.h"
#include "WorldSocketAcceptor.h"
#include "WorldSocketMgr.h"
#include <memory>

#if PLATFORM == PLATFORM_WINDOWS
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
#if PLATFORM == PLATFORM_WINDOWS
    bool IsValidSocket(WorldSocketHandle socket)
    {
        return socket != INVALID_SOCKET;
    }

    int LastSocketError()
    {
        return WSAGetLastError();
    }

    void CloseSocketHandle(WorldSocketHandle socket)
    {
        if (IsValidSocket(socket))
            closesocket(socket);
    }

    bool EnsureSocketLibrary()
    {
        static bool initialized = []() -> bool
        {
            WSADATA data;
            return WSAStartup(MAKEWORD(2, 2), &data) == 0;
        }();

        return initialized;
    }

    bool SetNonBlocking(WorldSocketHandle socket)
    {
        u_long mode = 1;
        return ioctlsocket(socket, FIONBIO, &mode) == 0;
    }
#else
    bool IsValidSocket(WorldSocketHandle socket)
    {
        return socket >= 0;
    }

    int LastSocketError()
    {
        return errno;
    }

    void CloseSocketHandle(WorldSocketHandle socket)
    {
        if (IsValidSocket(socket))
            close(socket);
    }

    bool EnsureSocketLibrary()
    {
        return true;
    }

    bool SetNonBlocking(WorldSocketHandle socket)
    {
        int flags = fcntl(socket, F_GETFL, 0);
        return flags >= 0 && fcntl(socket, F_SETFL, flags | O_NONBLOCK) == 0;
    }
#endif

    std::string GetPeerAddress(sockaddr_in const& addr)
    {
        char host[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &addr.sin_addr, host, sizeof(host));
        return host;
    }
}

WorldSocketAcceptor::WorldSocketAcceptor() :
#if PLATFORM == PLATFORM_WINDOWS
    m_ListenSocket(INVALID_SOCKET)
#else
    m_ListenSocket(-1)
#endif
{
}

WorldSocketAcceptor::~WorldSocketAcceptor()
{
    Close();
}

bool WorldSocketAcceptor::Open(uint16 port, const char* address)
{
    if (!EnsureSocketLibrary())
    {
        SF_LOG_ERROR("network", "Failed to initialize socket library");
        return false;
    }

    m_ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!IsValidSocket(m_ListenSocket))
    {
        SF_LOG_ERROR("network", "Failed to create world listener socket, error %d", LastSocketError());
        return false;
    }

    int reuseAddr = 1;
    setsockopt(m_ListenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseAddr), sizeof(reuseAddr));

    sockaddr_in bindAddress = {};
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(port);
    if (inet_pton(AF_INET, address, &bindAddress.sin_addr) != 1)
    {
        SF_LOG_ERROR("network", "Invalid world bind address %s", address);
        Close();
        return false;
    }

    if (bind(m_ListenSocket, reinterpret_cast<sockaddr*>(&bindAddress), sizeof(bindAddress)) != 0)
    {
        SF_LOG_ERROR("network", "Failed to bind world listener to %s:%u, error %d",
            address, port, LastSocketError());
        Close();
        return false;
    }

    if (listen(m_ListenSocket, SOMAXCONN) != 0)
    {
        SF_LOG_ERROR("network", "Failed to listen on world socket, error %d", LastSocketError());
        Close();
        return false;
    }

    if (!SetNonBlocking(m_ListenSocket))
    {
        SF_LOG_ERROR("network", "Failed to set world listener nonblocking, error %d", LastSocketError());
        Close();
        return false;
    }

    return true;
}

void WorldSocketAcceptor::Close()
{
    CloseSocketHandle(m_ListenSocket);
#if PLATFORM == PLATFORM_WINDOWS
    m_ListenSocket = INVALID_SOCKET;
#else
    m_ListenSocket = -1;
#endif
}

void WorldSocketAcceptor::Update()
{
    if (!IsValidSocket(m_ListenSocket))
        return;

    while (true)
    {
        sockaddr_in clientAddress = {};
        socklen_t clientAddressSize = sizeof(clientAddress);
        WorldSocketHandle clientSocket = accept(m_ListenSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);
        if (!IsValidSocket(clientSocket))
            break;

        SetNonBlocking(clientSocket);

        std::unique_ptr<WorldSocket> socket(new WorldSocket(clientSocket, GetPeerAddress(clientAddress)));
        if (sWorldSocketMgr->OnSocketOpen(socket.get()) == -1)
        {
            socket->CloseSocket();
            continue;
        }

        socket.release();
    }
}
