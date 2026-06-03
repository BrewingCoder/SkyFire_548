/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "AuthSocket.h"
#include "Log.h"
#include "RealmAcceptor.h"
#include <memory>

#if PLATFORM == PLATFORM_WINDOWS
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
#if PLATFORM == PLATFORM_WINDOWS
    bool IsValidSocket(RealmSocketHandle socket)
    {
        return socket != INVALID_SOCKET;
    }

    int LastSocketError()
    {
        return WSAGetLastError();
    }

    void CloseSocketHandle(RealmSocketHandle socket)
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

    bool SetNonBlocking(RealmSocketHandle socket)
    {
        u_long mode = 1;
        return ioctlsocket(socket, FIONBIO, &mode) == 0;
    }
#else
    bool IsValidSocket(RealmSocketHandle socket)
    {
        return socket >= 0;
    }

    int LastSocketError()
    {
        return errno;
    }

    void CloseSocketHandle(RealmSocketHandle socket)
    {
        if (IsValidSocket(socket))
            close(socket);
    }

    bool EnsureSocketLibrary()
    {
        return true;
    }

    bool SetNonBlocking(RealmSocketHandle socket)
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

RealmAcceptor::RealmAcceptor() :
#if PLATFORM == PLATFORM_WINDOWS
    _listenSocket(INVALID_SOCKET)
#else
    _listenSocket(-1)
#endif
{
}

RealmAcceptor::~RealmAcceptor()
{
    Close();
}

bool RealmAcceptor::Open(uint16 port, std::string const& bindIp)
{
    if (!EnsureSocketLibrary())
    {
        SF_LOG_ERROR("server.authserver", "Failed to initialize socket library");
        return false;
    }

    _listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!IsValidSocket(_listenSocket))
    {
        SF_LOG_ERROR("server.authserver", "Failed to create auth listener socket, error %d", LastSocketError());
        return false;
    }

    int reuseAddr = 1;
    setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&reuseAddr), sizeof(reuseAddr));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, bindIp.c_str(), &address.sin_addr) != 1)
    {
        SF_LOG_ERROR("server.authserver", "Invalid auth bind address %s", bindIp.c_str());
        Close();
        return false;
    }

    if (bind(_listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0)
    {
        SF_LOG_ERROR("server.authserver", "Failed to bind auth listener to %s:%u, error %d",
            bindIp.c_str(), port, LastSocketError());
        Close();
        return false;
    }

    if (listen(_listenSocket, SOMAXCONN) != 0)
    {
        SF_LOG_ERROR("server.authserver", "Failed to listen on auth socket, error %d", LastSocketError());
        Close();
        return false;
    }

    if (!SetNonBlocking(_listenSocket))
    {
        SF_LOG_ERROR("server.authserver", "Failed to set auth listener nonblocking, error %d", LastSocketError());
        Close();
        return false;
    }

    return true;
}

void RealmAcceptor::Close()
{
    CloseSocketHandle(_listenSocket);
#if PLATFORM == PLATFORM_WINDOWS
    _listenSocket = INVALID_SOCKET;
#else
    _listenSocket = -1;
#endif
}

void RealmAcceptor::Update()
{
    if (!IsValidSocket(_listenSocket))
        return;

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(_listenSocket, &readSet);

    timeval timeout = {};
    int ready = select(int(_listenSocket + 1), &readSet, NULL, NULL, &timeout);
    if (ready <= 0)
        return;

    while (true)
    {
        sockaddr_in clientAddress = {};
        socklen_t clientAddressSize = sizeof(clientAddress);
        RealmSocketHandle clientSocket = accept(_listenSocket, reinterpret_cast<sockaddr*>(&clientAddress), &clientAddressSize);

        if (!IsValidSocket(clientSocket))
            break;

        std::unique_ptr<RealmSocket> socket(new RealmSocket(
            clientSocket, GetPeerAddress(clientAddress), ntohs(clientAddress.sin_port)));
        socket->set_session(new AuthSocket(*socket));
        socket->Start();
        socket.release();
    }
}
