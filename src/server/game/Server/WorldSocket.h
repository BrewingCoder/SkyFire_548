/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

/** \addtogroup u2w User to World Communication
 * @{
 * \file WorldSocket.h
 * \author Derex <derex101@gmail.com>
 */

#ifndef SF_WORLDSOCKET_H
#define SF_WORLDSOCKET_H

#include "AuthCrypt.h"
#include "Common.h"
#include "Platform/Threading.h"
#include "SharedDefines.h"
#include <boost/asio/ip/tcp.hpp>
#include <boost/system/error_code.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <vector>

typedef boost::asio::ip::tcp::socket WorldSocketHandle;

class WorldPacket;
class WorldSession;

/// Handler that can communicate over stream sockets.
class WorldSocket
{
public:
    WorldSocket(std::unique_ptr<WorldSocketHandle> socket, std::string remoteAddress);
    ~WorldSocket(void);

    friend class WorldSocketMgr;
    friend class ReactorRunnable;

    /// Mutex type used for various synchronizations.
    typedef Skyfire::Mutex LockType;
    typedef std::unique_lock<LockType> GuardType;

    /// Check if socket is closed.
    bool IsClosed(void) const;

    /// Close the socket.
    void CloseSocket(void);

    /// Get address of connected peer.
    const std::string& GetRemoteAddress(void) const;

    /// Send A packet on the socket, this function is reentrant.
    /// @param pct packet to send
    /// @return -1 of failure
    int SendPacket(const WorldPacket& pct);

    /// Add reference to this object.
    long AddReference(void);

    /// Remove reference to this object.
    long RemoveReference(void);

    /// Called after socket accept and manager setup.
    int Initialize(void);

    /// Called when the socket can read.
    int Read(void);

    /// Called by WorldSocketMgr/ReactorRunnable.
    int Update(void);

    /// Returns true when outgoing data is waiting to be flushed.
    bool HasPendingOutput(void) const;

private:
    /// Helper functions for processing incoming data.
    int handle_input_header(void);
    int handle_input_payload(void);
    int handle_input_missing_data(void);
    int handle_input_missing_data(char const* data, size_t length);

    /// Drain outgoing buffers.
    int handle_output(void);
    int handle_output_queue(GuardType& g);

    /// process one incoming packet.
    /// @param new_pct received packet, note that you need to delete it.
    int ProcessIncoming(WorldPacket* new_pct);

    /// Called by ProcessIncoming() on CMSG_AUTH_SESSION.
    int HandleAuthSession(WorldPacket& recvPacket);

    /// Called by ProcessIncoming() on CMSG_PING.
    int HandlePing(WorldPacket& recvPacket);

    /// Called by MSG_VERIFY_CONNECTIVITY_RESPONSE
    int HandleSendAuthSession();

private:
    void SendAuthResponseError(ResponseCodes code);
    bool IsValidSocket(void) const;
    bool IsWouldBlock(boost::system::error_code const& error) const;
    int SendBuffer(char const* data, size_t length, size_t& sent);

    /// Time in which the last ping was received
    std::chrono::steady_clock::time_point m_LastPingTime;
    bool m_HasLastPingTime;

    /// Keep track of over-speed pings, to prevent ping flood.
    uint32 m_OverSpeedPings;

    /// Address of the remote peer
    std::string m_Address;

    /// Class used for managing encryption of the headers
    AuthCrypt m_Crypt;

    /// Mutex lock to protect m_Session
    LockType m_SessionLock;

    /// Session to which received packets are routed
    WorldSession* m_Session;

    /// here are stored the fragments of the received data
    WorldPacket* m_RecvWPct;

    size_t m_RecvPctRead;
    std::vector<uint8> m_Header;
    size_t m_HeaderRead;
    std::vector<uint8> m_WorldHeader;
    size_t m_WorldHeaderRead;

    /// Mutex for protecting output related data.
    mutable LockType m_OutBufferLock;

    /// Buffer used for writing output.
    std::vector<char> m_OutBuffer;
    size_t m_OutBufferReadPos;

    /// Queue used when the main output buffer is full.
    std::deque<std::vector<char>> m_OutQueue;

    /// Size of the m_OutBuffer.
    size_t m_OutBufferSize;

    std::array<uint8, 4> m_Seed;

    std::unique_ptr<WorldSocketHandle> m_Socket;
    boost::system::error_code m_LastSocketError;
    std::atomic<long> m_ReferenceCount;
    std::atomic<bool> m_Closed;

    WorldSocket(WorldSocket const& right) = delete;
    WorldSocket& operator=(WorldSocket const& right) = delete;
};

#endif  /* _WORLDSOCKET_H */

/// @}
