/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

/** \addtogroup u2w User to World Communication
 *  @{
 *  \file WorldSocketMgr.h
 *  \author Derex <derex101@gmail.com>
 */

#ifndef SF_WORLDSOCKETMGR_H
#define SF_WORLDSOCKETMGR_H

#include "Common.h"
#include "Platform/Singleton.h"

class WorldSocket;
class ReactorRunnable;

/// Manages all sockets connected to peers and network threads
class WorldSocketMgr
{
public:
    friend class WorldSocket;
    friend class WorldSocketAcceptor;
    friend class Skyfire::Singleton<WorldSocketMgr, Skyfire::Mutex>;

    /// Start network, listen at address:port .
    int StartNetwork(uint16 port, const char* address);

    /// Stops all network threads, It will wait for all running threads .
    void StopNetwork();

    /// Wait untill all network threads have "joined" .
    void Wait();

private:
    int OnSocketOpen(WorldSocket* sock);

    int StartReactiveIO(uint16 port, const char* address);

private:
    WorldSocketMgr();
    virtual ~WorldSocketMgr();

    ReactorRunnable* m_NetThreads;
    size_t m_NetThreadsCount;

    int m_SockOutKBuff;
    int m_SockOutUBuff;
    bool m_UseNoDelay;

    class WorldSocketAcceptor* m_Acceptor;
};

#define sWorldSocketMgr Skyfire::Singleton<WorldSocketMgr, Skyfire::Mutex>::instance()

#endif
/// @}
