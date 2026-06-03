/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "AuthSocket.h"
#include "Log.h"
#include "RealmAcceptor.h"
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

RealmAcceptor::RealmAcceptor() :
    _ioContext(),
    _acceptor(_ioContext)
{
}

RealmAcceptor::~RealmAcceptor()
{
    Close();
}

bool RealmAcceptor::Open(uint16 port, std::string const& bindIp)
{
    boost::system::error_code error;
    boost::asio::ip::address bindAddress = boost::asio::ip::make_address(bindIp, error);
    if (error)
    {
        SF_LOG_ERROR("server.authserver", "Invalid auth bind address %s, error %d", bindIp.c_str(), error.value());
        return false;
    }

    boost::asio::ip::tcp::endpoint endpoint(bindAddress, port);

    _acceptor.open(endpoint.protocol(), error);
    if (error)
    {
        SF_LOG_ERROR("server.authserver", "Failed to create auth listener socket, error %d", error.value());
        return false;
    }

    _acceptor.set_option(boost::asio::socket_base::reuse_address(true), error);
    if (error)
    {
        SF_LOG_ERROR("server.authserver", "Failed to set auth listener reuse address, error %d", error.value());
        Close();
        return false;
    }

    _acceptor.bind(endpoint, error);
    if (error)
    {
        SF_LOG_ERROR("server.authserver", "Failed to bind auth listener to %s:%u, error %d",
            bindIp.c_str(), port, error.value());
        Close();
        return false;
    }

    _acceptor.listen(boost::asio::socket_base::max_listen_connections, error);
    if (error)
    {
        SF_LOG_ERROR("server.authserver", "Failed to listen on auth socket, error %d", error.value());
        Close();
        return false;
    }

    _acceptor.non_blocking(true, error);
    if (error)
    {
        SF_LOG_ERROR("server.authserver", "Failed to set auth listener nonblocking, error %d", error.value());
        Close();
        return false;
    }

    return true;
}

void RealmAcceptor::Close()
{
    boost::system::error_code ignored;
    _acceptor.close(ignored);
}

void RealmAcceptor::Update()
{
    if (!_acceptor.is_open())
        return;

    while (true)
    {
        boost::system::error_code error;
        std::unique_ptr<RealmSocketHandle> clientSocket(new RealmSocketHandle(_ioContext));
        _acceptor.accept(*clientSocket, error);

        if (error)
        {
            if (!IsWouldBlock(error))
                SF_LOG_ERROR("server.authserver", "Failed to accept auth socket, error %d", error.value());

            break;
        }

        boost::asio::ip::tcp::endpoint remoteEndpoint = clientSocket->remote_endpoint(error);
        std::string remoteAddress = error ? std::string("<unknown>") : remoteEndpoint.address().to_string();
        uint16 remotePort = error ? 0 : remoteEndpoint.port();

        std::unique_ptr<RealmSocket> socket(new RealmSocket(std::move(clientSocket), remoteAddress, remotePort));
        socket->set_session(new AuthSocket(*socket));
        socket->Start();
        socket.release();
    }
}
