/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "NetworkAddress.h"

#if PLATFORM == PLATFORM_WINDOWS
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

namespace Skyfire::Net
{
    Address::Address() : _host("0.0.0.0"), _port(0) { }

    Address::Address(std::string const& host, uint16 port) : _host(host), _port(port) { }

    bool Address::IsLoopback() const
    {
        uint32 const ip = ntohl(ToIPv4NetworkOrder());
        return (ip & 0xFF000000) == 0x7F000000;
    }

    uint32 Address::ToIPv4NetworkOrder() const
    {
        uint32 address = inet_addr(_host.c_str());
        return address == INADDR_NONE ? 0 : address;
    }
}
