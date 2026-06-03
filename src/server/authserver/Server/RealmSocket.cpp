/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Log.h"
#include "RealmSocket.h"
#include <boost/asio/buffer.hpp>
#include <algorithm>

RealmSocket::Session::Session(void) { }

RealmSocket::Session::~Session(void) { }

RealmSocket::RealmSocket(std::unique_ptr<RealmSocketHandle> socket, std::string remoteAddress, uint16 remotePort) :
    _socket(std::move(socket)), _inputBuffer(), _inputReadPos(0), _session(NULL),
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
    while (sent < len && !_closed && IsOpen())
    {
        boost::system::error_code error;
        size_t n = _socket->write_some(boost::asio::buffer(buf + sent, len - sent), error);

        if (error || n == 0)
        {
            SF_LOG_DEBUG("server.authserver", "Socket send failed for %s:%u with error %d",
                _remoteAddress.c_str(), _remotePort, error.value());
            CloseSocket();
            return false;
        }

        sent += n;
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
        boost::system::error_code error;
        size_t n = _socket->read_some(boost::asio::buffer(buffer), error);

        if (error || n == 0)
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

bool RealmSocket::IsOpen(void) const
{
    return _socket && _socket->is_open();
}

void RealmSocket::CloseSocket()
{
    bool expected = false;
    if (!_closed.compare_exchange_strong(expected, true))
        return;

    if (IsOpen())
    {
        boost::system::error_code ignored;
        _socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        _socket->close(ignored);
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
