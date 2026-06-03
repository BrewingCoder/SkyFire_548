/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#ifndef SF_NETWORKADDRESS_H
#define SF_NETWORKADDRESS_H

#include "Define.h"
#include <string>

namespace Skyfire::Net
{
    class Address
    {
    public:
        Address();
        Address(std::string const& host, uint16 port = 0);

        std::string const& GetHost() const { return _host; }
        uint16 GetPort() const { return _port; }
        void SetPort(uint16 port) { _port = port; }

        bool IsLoopback() const;
        uint32 ToIPv4NetworkOrder() const;

    private:
        std::string _host;
        uint16 _port;
    };
}

#endif
