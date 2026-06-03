/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "Log.h"
#include "WorldSocketAcceptor.h"
#include "WorldSocketMgr.h"
#include <boost/asio/error.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/system/error_code.hpp>
#include <memory>

namespace
{
    bool IsWouldBlock(boost::system::error_code const& error)
    {
        return error == boost::asio::error::would_block || error == boost::asio::error::try_again;
    }
}

WorldSocketAcceptor::WorldSocketAcceptor() :
    m_IoContext(),
    m_Acceptor(m_IoContext)
{
}

WorldSocketAcceptor::~WorldSocketAcceptor()
{
    Close();
}

bool WorldSocketAcceptor::Open(uint16 port, const char* address)
{
    boost::system::error_code error;
    boost::asio::ip::address bindAddress = boost::asio::ip::make_address(address, error);
    if (error)
    {
        SF_LOG_ERROR("network", "Invalid world bind address %s, error %d", address, error.value());
        return false;
    }

    boost::asio::ip::tcp::endpoint endpoint(bindAddress, port);

    m_Acceptor.open(endpoint.protocol(), error);
    if (error)
    {
        SF_LOG_ERROR("network", "Failed to create world listener socket, error %d", error.value());
        return false;
    }

    m_Acceptor.set_option(boost::asio::socket_base::reuse_address(true), error);
    if (error)
    {
        SF_LOG_ERROR("network", "Failed to set world listener reuse address, error %d", error.value());
        Close();
        return false;
    }

    m_Acceptor.bind(endpoint, error);
    if (error)
    {
        SF_LOG_ERROR("network", "Failed to bind world listener to %s:%u, error %d",
            address, port, error.value());
        Close();
        return false;
    }

    m_Acceptor.listen(boost::asio::socket_base::max_listen_connections, error);
    if (error)
    {
        SF_LOG_ERROR("network", "Failed to listen on world socket, error %d", error.value());
        Close();
        return false;
    }

    m_Acceptor.non_blocking(true, error);
    if (error)
    {
        SF_LOG_ERROR("network", "Failed to set world listener nonblocking, error %d", error.value());
        Close();
        return false;
    }

    return true;
}

void WorldSocketAcceptor::Close()
{
    boost::system::error_code ignored;
    m_Acceptor.close(ignored);
}

void WorldSocketAcceptor::Update()
{
    if (!m_Acceptor.is_open())
        return;

    while (true)
    {
        boost::system::error_code error;
        std::unique_ptr<WorldSocketHandle> clientSocket(new WorldSocketHandle(m_IoContext));
        m_Acceptor.accept(*clientSocket, error);

        if (error)
        {
            if (!IsWouldBlock(error))
                SF_LOG_ERROR("network", "Failed to accept world socket, error %d", error.value());

            break;
        }

        boost::asio::ip::tcp::endpoint remoteEndpoint = clientSocket->remote_endpoint(error);
        std::string remoteAddress = error ? std::string("<unknown>") : remoteEndpoint.address().to_string();

        clientSocket->non_blocking(true, error);
        if (error)
        {
            SF_LOG_ERROR("network", "Failed to set world client nonblocking, error %d", error.value());
            continue;
        }

        std::unique_ptr<WorldSocket> socket(new WorldSocket(std::move(clientSocket), remoteAddress));
        if (sWorldSocketMgr->OnSocketOpen(socket.get()) == -1)
        {
            socket->CloseSocket();
            continue;
        }

        socket.release();
    }
}
