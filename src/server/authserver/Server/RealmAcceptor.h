/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_REALMACCEPTOR_H
#define SF_REALMACCEPTOR_H

#include "Common.h"
#include "RealmSocket.h"
#include <string>

class RealmAcceptor
{
public:
    RealmAcceptor();
    ~RealmAcceptor();

    bool Open(uint16 port, std::string const& bindIp);
    void Close();
    void Update();

private:
    RealmSocketHandle _listenSocket;
};

#endif
