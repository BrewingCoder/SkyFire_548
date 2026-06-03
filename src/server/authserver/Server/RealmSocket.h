/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_REALMSOCKET_H
#define SF_REALMSOCKET_H

#include "Common.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

typedef boost::asio::ip::tcp::socket RealmSocketHandle;

class RealmSocket
{
public:
    class Session
    {
    public:
        Session(void);
        virtual ~Session(void);

        virtual void OnRead(void) = 0;
        virtual void OnAccept(void) = 0;
        virtual void OnClose(void) = 0;
    };

    RealmSocket(std::unique_ptr<RealmSocketHandle> socket, std::string remoteAddress, uint16 remotePort);
    ~RealmSocket(void);

    void Start();
    void shutdown();

    size_t recv_len(void) const;
    bool recv_soft(char* buf, size_t len);
    bool recv(char* buf, size_t len);
    void recv_skip(size_t len);

    bool send(const char* buf, size_t len);

    const std::string& getRemoteAddress(void) const;
    uint16 getRemotePort(void) const;

    void set_session(Session* session);

private:
    void Run();
    void CloseSocket();
    void CompactInputBuffer();

    bool IsOpen(void) const;

    std::unique_ptr<RealmSocketHandle> _socket;
    std::vector<char> _inputBuffer;
    size_t _inputReadPos;
    Session* _session;
    std::string _remoteAddress;
    uint16 _remotePort;
    std::mutex _sendLock;
    std::atomic<bool> _closed;
};

#endif
