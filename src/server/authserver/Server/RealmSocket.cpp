/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Log.h"
#include "RealmSocket.h"
#include <algorithm>

#if PLATFORM == PLATFORM_WINDOWS
#include <ws2tcpip.h>
#else
#include <cerrno>
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
#endif
}

RealmSocket::Session::Session(void) { }

RealmSocket::Session::~Session(void) { }

RealmSocket::RealmSocket(RealmSocketHandle socket, std::string remoteAddress, uint16 remotePort) :
    _socket(socket), _inputBuffer(), _inputReadPos(0), _session(NULL),
    _remoteAddress(std::move(remoteAddress)), _remotePort(remotePort), _closed(false)
{
    _inputBuffer.reserve(4096);
}

RealmSocket::~RealmSocket(void)
{
    CloseSocket();
    delete _session;
}

void RealmSocket::Start()
{
    if (_session)
        _session->OnAccept();

    std::thread(&RealmSocket::Run, this).detach();
}

void RealmSocket::shutdown()
{
    CloseSocket();
}

const std::string& RealmSocket::getRemoteAddress(void) const
{
    return _remoteAddress;
}

uint16 RealmSocket::getRemotePort(void) const
{
    return _remotePort;
}

size_t RealmSocket::recv_len(void) const
{
    return _inputBuffer.size() - _inputReadPos;
}

bool RealmSocket::recv_soft(char* buf, size_t len)
{
    if (recv_len() < len)
        return false;

    memcpy(buf, _inputBuffer.data() + _inputReadPos, len);
    return true;
}

bool RealmSocket::recv(char* buf, size_t len)
{
    bool ret = recv_soft(buf, len);

    if (ret)
        recv_skip(len);

    return ret;
}

void RealmSocket::recv_skip(size_t len)
{
    _inputReadPos = std::min(_inputReadPos + len, _inputBuffer.size());
}

bool RealmSocket::send(const char* buf, size_t len)
{
    if (buf == NULL || len == 0)
        return true;

    std::lock_guard<std::mutex> guard(_sendLock);

    size_t sent = 0;
    while (sent < len && !_closed)
    {
#ifdef MSG_NOSIGNAL
        ssize_t n = ::send(_socket, buf + sent, len - sent, MSG_NOSIGNAL);
#else
        int n = ::send(_socket, buf + sent, int(len - sent), 0);
#endif

        if (n <= 0)
        {
            SF_LOG_DEBUG("server.authserver", "Socket send failed for %s:%u with error %d",
                _remoteAddress.c_str(), _remotePort, LastSocketError());
            CloseSocket();
            return false;
        }

        sent += size_t(n);
    }

    return sent == len;
}

void RealmSocket::set_session(Session* session)
{
    delete _session;
    _session = session;
}

void RealmSocket::Run()
{
    char buffer[4096];

    while (!_closed)
    {
        int n = ::recv(_socket, buffer, sizeof(buffer), 0);

        if (n <= 0)
            break;

        _inputBuffer.insert(_inputBuffer.end(), buffer, buffer + n);

        if (_session)
        {
            _session->OnRead();
            CompactInputBuffer();
        }
    }

    CloseSocket();

    if (_session)
        _session->OnClose();

    delete this;
}

void RealmSocket::CloseSocket()
{
    bool expected = false;
    if (!_closed.compare_exchange_strong(expected, true))
        return;

    if (IsValidSocket(_socket))
    {
#if PLATFORM == PLATFORM_WINDOWS
        ::shutdown(_socket, SD_BOTH);
#else
        ::shutdown(_socket, SHUT_RDWR);
#endif
        CloseSocketHandle(_socket);
    }
}

void RealmSocket::CompactInputBuffer()
{
    if (_inputReadPos == 0)
        return;

    if (_inputReadPos >= _inputBuffer.size())
        _inputBuffer.clear();
    else
        _inputBuffer.erase(_inputBuffer.begin(), _inputBuffer.begin() + ptrdiff_t(_inputReadPos));

    _inputReadPos = 0;
}
