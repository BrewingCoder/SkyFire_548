/*
* This file is part of Project SkyFire https://www.projectskyfire.org.
* See LICENSE.md file for Copyright information
*/

#include "AccountMgr.h"
#include "BigNumber.h"
#include "ByteBuffer.h"
#include "Common.h"
#include "CryptoHash.h"
#include "CryptoRandom.h"
#include "DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "PacketLog.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "WorldSocket.h"
#include "WorldSocketMgr.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <new>
#include <thread>

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

struct ServerPktHeader
{
    ServerPktHeader(uint32 size, uint32 cmd, AuthCrypt* _authCrypt) : size(size)
    {
        if (_authCrypt->IsInitialized())
        {
            uint32 data = (size << 13) | (cmd & MAX_OPCODE);
            memcpy(&header[0], &data, 4);
            _authCrypt->EncryptSend((uint8*)&header[0], getHeaderLength());
        }
        else
        {
            // Dynamic header size is not needed anymore, we are using not encrypted part for only the first few packets
            memcpy(&header[0], &size, 2);
            memcpy(&header[2], &cmd, 2);
        }
    }

    uint8 getHeaderLength()
    {
        return 4;
    }

    const uint32 size;
    uint8 header[4];
};

struct AuthClientPktHeader
{
    uint16 size;
    uint32 cmd;
};

struct WorldClientPktHeader
{
    uint16 size;
    uint16 cmd;
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

WorldSocket::WorldSocket(std::unique_ptr<WorldSocketHandle> socket, std::string remoteAddress) :
m_LastPingTime(), m_HasLastPingTime(false), m_OverSpeedPings(0), m_Address(std::move(remoteAddress)),
m_Session(0), m_RecvWPct(0), m_RecvPctRead(0), m_Header(sizeof(AuthClientPktHeader)),
m_HeaderRead(0), m_WorldHeader(sizeof(WorldClientPktHeader)), m_WorldHeaderRead(0),
m_OutBuffer(), m_OutBufferReadPos(0), m_OutQueue(), m_OutBufferSize(65536),
m_Socket(std::move(socket)), m_LastSocketError(), m_ReferenceCount(0), m_Closed(false)
{
    SkyFire::Crypto::GetRandomBytes(m_Seed);
}

WorldSocket::~WorldSocket(void)
{
    delete m_RecvWPct;
    CloseSocket();
}

bool WorldSocket::IsClosed(void) const
{
    return m_Closed;
}

void WorldSocket::CloseSocket(void)
{
    {
        GuardType Guard(m_OutBufferLock);

        if (m_Closed.exchange(true))
            return;
    }

    if (m_Socket && m_Socket->is_open())
    {
        boost::system::error_code ignored;
        m_Socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
        m_Socket->close(ignored);
    }

    {
        GuardType Guard(m_SessionLock);

        m_Session = NULL;
    }
}

const std::string& WorldSocket::GetRemoteAddress(void) const
{
    return m_Address;
}

bool WorldSocket::HasPendingOutput(void) const
{
    GuardType Guard(m_OutBufferLock);
    return m_OutBuffer.size() != m_OutBufferReadPos || !m_OutQueue.empty();
}

bool WorldSocket::IsValidSocket(void) const
{
    return m_Socket && m_Socket->is_open();
}

bool WorldSocket::IsWouldBlock(boost::system::error_code const& error) const
{
    return error == boost::asio::error::would_block || error == boost::asio::error::try_again;
}

int WorldSocket::SendBuffer(char const* data, size_t length, size_t& sent)
{
    sent = 0;
    m_LastSocketError.clear();

    if (!IsValidSocket())
        return -1;

    boost::system::error_code error;
    sent = m_Socket->write_some(boost::asio::buffer(data, length), error);
    m_LastSocketError = error;

    if (!error && sent > 0)
    {
        return 1;
    }

    if (!error)
        return 0;

    return -1;
}

int WorldSocket::SendPacket(WorldPacket const& pct)
{
    GuardType Guard(m_OutBufferLock);

    if (m_Closed)
        return -1;

    // Dump outgoing packet
    if (sPacketLog->CanLogPacket())
        sPacketLog->LogPacket(pct, SERVER_TO_CLIENT);

    WorldPacket const* pkt = &pct;

    // Empty buffer used in case packet should be compressed
    // Disable compression for now :)
   /* WorldPacket buff;
    if (m_Session && pkt->size() > 0x400)
    {
        buff.Compress(m_Session->GetCompressionStream(), pkt);
        pkt = &buff;
    }*/

    uint16 opcodeNumber = serverOpcodeTable[pkt->GetOpcode()]->OpcodeNumber;

    if (m_Session)
        SF_LOG_TRACE("network.opcode", "S->C: %s %s", m_Session->GetPlayerInfo().c_str(), GetOpcodeNameForLogging(pkt->GetOpcode(), true).c_str());

    sScriptMgr->OnPacketSend(this, *pkt);

    ServerPktHeader header(!m_Crypt.IsInitialized() ? pkt->size() + 2 : pct.size(), opcodeNumber, &m_Crypt);

    size_t packetSize = pkt->size() + header.getHeaderLength();
    std::vector<char> serialized;
    serialized.reserve(packetSize);
    serialized.insert(serialized.end(), reinterpret_cast<char*>(header.header),
        reinterpret_cast<char*>(header.header) + header.getHeaderLength());
    if (!pkt->empty())
        serialized.insert(serialized.end(), reinterpret_cast<char const*>(pkt->contents()),
            reinterpret_cast<char const*>(pkt->contents()) + pkt->size());

    if (m_OutQueue.empty() && m_OutBuffer.size() - m_OutBufferReadPos + serialized.size() <= m_OutBufferSize)
    {
        if (m_OutBufferReadPos == m_OutBuffer.size())
        {
            m_OutBuffer.clear();
            m_OutBufferReadPos = 0;
        }

        m_OutBuffer.insert(m_OutBuffer.end(), serialized.begin(), serialized.end());
    }
    else
    {
        m_OutQueue.push_back(std::move(serialized));
    }

    return 0;
}

long WorldSocket::AddReference(void)
{
    return ++m_ReferenceCount;
}

long WorldSocket::RemoveReference(void)
{
    long result = --m_ReferenceCount;
    if (result == 0)
        delete this;

    return result;
}

int WorldSocket::Initialize(void)
{
    // not an opcode. this packet sends raw string WORLD OF WARCRAFT CONNECTION - SERVER TO CLIENT"
    // because of our implementation, bytes "WO" become the opcode
    WorldPacket packet(MSG_VERIFY_CONNECTIVITY);
    packet << std::string("RLD OF WARCRAFT CONNECTION - SERVER TO CLIENT");

    if (SendPacket(packet) == -1)
        return -1;

    return 0;
}

int WorldSocket::Read(void)
{
    if (m_Closed)
        return -1;

    errno = 0;
    m_LastSocketError.clear();

    switch (handle_input_missing_data())
    {
        case -1:
        {
            if (IsWouldBlock(m_LastSocketError) || errno == EWOULDBLOCK || errno == EAGAIN)
            {
                return Update();                           // interesting line, isn't it ?
            }

            SF_LOG_DEBUG("network", "WorldSocket::Read: Peer error closing connection errno = %d", m_LastSocketError.value());
            return -1;
        }
        case 0:
        {
            SF_LOG_DEBUG("network", "WorldSocket::Read: Peer has closed connection");
            return -1;
        }
        case 1:
            return 1;
        default:
            return Update();                               // another interesting line ;)
    }

    return -1;
}

int WorldSocket::handle_output(void)
{
    GuardType Guard(m_OutBufferLock);

    if (m_Closed)
        return -1;

    size_t send_len = m_OutBuffer.size() - m_OutBufferReadPos;

    if (send_len == 0)
        return handle_output_queue(Guard);

    size_t sent = 0;
    int sendResult = SendBuffer(m_OutBuffer.data() + m_OutBufferReadPos, send_len, sent);

    if (sendResult == 0)
        return -1;
    else if (sendResult == -1)
    {
        if (IsWouldBlock(m_LastSocketError))
            return 0;

        return -1;
    }
    else if (sent < send_len)
    {
        m_OutBufferReadPos += sent;
        return 0;
    }
    else
    {
        m_OutBuffer.clear();
        m_OutBufferReadPos = 0;
        return handle_output_queue(Guard);
    }
}

int WorldSocket::handle_output_queue(GuardType& g)
{
    if (m_OutQueue.empty())
        return 0;

    std::vector<char>& packet = m_OutQueue.front();
    size_t send_len = packet.size();

    size_t sent = 0;
    int sendResult = SendBuffer(packet.data(), send_len, sent);

    if (sendResult == 0)
        return -1;
    else if (sendResult == -1)
    {
        if (IsWouldBlock(m_LastSocketError))
            return 0;

        return -1;
    }
    else if (sent < send_len)
    {
        m_OutBuffer.assign(packet.begin() + ptrdiff_t(sent), packet.end());
        m_OutBufferReadPos = 0;
        m_OutQueue.pop_front();
        return 0;
    }
    else
    {
        m_OutQueue.pop_front();
        return m_OutQueue.empty() ? 0 : 1;
    }
}

int WorldSocket::Update(void)
{
    if (m_Closed)
        return -1;

    {
        GuardType Guard(m_OutBufferLock);
        if (m_OutBuffer.size() == m_OutBufferReadPos && m_OutQueue.empty())
            return 0;
    }

    int ret;
    do
        ret = handle_output();
    while (ret > 0);

    return ret;
}

int WorldSocket::handle_input_header(void)
{
    ASSERT(m_RecvWPct == NULL);


    if (m_Crypt.IsInitialized())
    {
        ASSERT(m_WorldHeaderRead == sizeof(WorldClientPktHeader));
        uint8* uintHeader = m_WorldHeader.data();
        m_Crypt.DecryptRecv(uintHeader, sizeof(WorldClientPktHeader));
        WorldClientPktHeader& header = *(WorldClientPktHeader*)uintHeader;

        uint32 value = *(uint32*)uintHeader;
        header.cmd = value & 0x1FFF;
        header.size = ((value & ~(uint32)0x1FFF) >> 13);

        if (header.size > 10236)
        {
            Player* _player = m_Session ? m_Session->GetPlayer() : NULL;
            SF_LOG_ERROR("network", "WorldSocket::handle_input_header(): client (account: %u, char [GUID: %u, name: %s]) sent malformed packet (size: %d, cmd: %d)",
                m_Session ? m_Session->GetAccountId() : 0,
                _player ? _player->GetGUIDLow() : 0,
                _player ? _player->GetName().c_str() : "<none>",
                header.size, header.cmd);

            errno = EINVAL;
            return -1;
        }

        uint16 opcodeNumber = PacketFilter::DropHighBytes(header.cmd);
        m_RecvWPct = new (std::nothrow) WorldPacket(clientOpcodeTable.GetOpcodeByNumber(opcodeNumber), header.size);
        if (!m_RecvWPct)
            return -1;
        m_RecvWPct->SetReceivedOpcode(opcodeNumber);

        if (header.size > 0)
        {
            m_RecvWPct->resize(header.size);
            m_RecvPctRead = 0;
        }
        else
            ASSERT(m_RecvPctRead == 0);
    }
    else
    {
        ASSERT(m_HeaderRead == sizeof(AuthClientPktHeader));
        uint8* uintHeader = m_Header.data();
        AuthClientPktHeader& header = *((AuthClientPktHeader*)uintHeader);

        if ((header.size < 4) || (header.size > 10240))
        {
            Player* _player = m_Session ? m_Session->GetPlayer() : NULL;
            SF_LOG_ERROR("network", "WorldSocket::handle_input_header(): client (account: %u, char [GUID: %u, name: %s]) sent malformed packet (size: %d, cmd: %d)",
                m_Session ? m_Session->GetAccountId() : 0,
                _player ? _player->GetGUIDLow() : 0,
                _player ? _player->GetName().c_str() : "<none>",
                header.size, header.cmd);

            errno = EINVAL;
            return -1;
        }

        header.size -= 4;

        uint16 opcodeNumber = PacketFilter::DropHighBytes(header.cmd);
        m_RecvWPct = new (std::nothrow) WorldPacket(clientOpcodeTable.GetOpcodeByNumber(opcodeNumber), header.size);
        if (!m_RecvWPct)
            return -1;
        m_RecvWPct->SetReceivedOpcode(opcodeNumber);

        if (header.size > 0)
        {
            m_RecvWPct->resize(header.size);
            m_RecvPctRead = 0;
        }
        else
            ASSERT(m_RecvPctRead == 0);
    }

    return 0;
}
int WorldSocket::handle_input_payload(void)
{
    // set errno properly here on error !!!
    // now have a header and payload

    if (m_Crypt.IsInitialized())
    {
        ASSERT(m_RecvWPct == NULL || m_RecvPctRead == m_RecvWPct->size());
        ASSERT(m_WorldHeaderRead == sizeof(WorldClientPktHeader));
        ASSERT(m_RecvWPct != NULL);

        const int ret = ProcessIncoming(m_RecvWPct);

        m_RecvPctRead = 0;
        m_RecvWPct = NULL;

        m_WorldHeaderRead = 0;

        if (ret == -1)
            errno = EINVAL;

        return ret;
    }
    else
    {
        ASSERT(m_RecvWPct == NULL || m_RecvPctRead == m_RecvWPct->size());
        ASSERT(m_HeaderRead == sizeof(AuthClientPktHeader));
        ASSERT(m_RecvWPct != NULL);

        const int ret = ProcessIncoming(m_RecvWPct);

        m_RecvPctRead = 0;
        m_RecvWPct = NULL;

        m_HeaderRead = 0;

        if (ret == -1)
            errno = EINVAL;

        return ret;
    }
}

int WorldSocket::handle_input_missing_data(void)
{
    char buf[4096];

    if (!IsValidSocket())
        return -1;

    boost::system::error_code error;
    size_t n = m_Socket->read_some(boost::asio::buffer(buf), error);
    m_LastSocketError = error;

    if (error)
    {
        if (IsWouldBlock(error))
            return -1;

        if (error == boost::asio::error::eof)
            return 0;

        return -1;
    }

    if (n == 0)
        return 0;

    m_LastSocketError.clear();
    return handle_input_missing_data(buf, n);
}

int WorldSocket::handle_input_missing_data(char const* data, size_t length)
{
    size_t readPos = 0;

    while (readPos < length)
    {
        if (m_Crypt.IsInitialized())
        {
            if (m_WorldHeaderRead < m_WorldHeader.size())
            {
                //need to receive the header
                const size_t needed = m_WorldHeader.size() - m_WorldHeaderRead;
                const size_t available = length - readPos;
                const size_t to_header = std::min(available, needed);
                memcpy(m_WorldHeader.data() + m_WorldHeaderRead, data + readPos, to_header);
                m_WorldHeaderRead += to_header;
                readPos += to_header;

                if (m_WorldHeaderRead < m_WorldHeader.size())
                {
                    // Couldn't receive the whole header this time.
                    ASSERT(readPos == length);
                    errno = EWOULDBLOCK;
                    return -1;
                }

                // We just received nice new header
                if (handle_input_header() == -1)
                {
                    ASSERT((errno != EWOULDBLOCK) && (errno != EAGAIN));
                    return -1;
                }
            }
        }
        else
        {
            if (m_HeaderRead < m_Header.size())
            {
                //need to receive the header
                const size_t needed = m_Header.size() - m_HeaderRead;
                const size_t available = length - readPos;
                const size_t to_header = std::min(available, needed);
                memcpy(m_Header.data() + m_HeaderRead, data + readPos, to_header);
                m_HeaderRead += to_header;
                readPos += to_header;

                if (m_HeaderRead < m_Header.size())
                {
                    // Couldn't receive the whole header this time.
                    ASSERT(readPos == length);
                    errno = EWOULDBLOCK;
                    return -1;
                }

                // We just received nice new header
                if (handle_input_header() == -1)
                {
                    ASSERT((errno != EWOULDBLOCK) && (errno != EAGAIN));
                    return -1;
                }
            }
        }

        // Its possible on some error situations that this happens
        // for example on closing when epoll receives more chunked data and stuff
        // hope this is not hack, as proper m_RecvWPct is asserted around
        if (!m_RecvWPct)
        {
            SF_LOG_ERROR("network", "Forcing close on input m_RecvWPct = NULL");
            errno = EINVAL;
            return -1;
        }

        // We have full read header, now check the data payload
        if (m_RecvWPct && m_RecvPctRead < m_RecvWPct->size())
        {
            //need more data in the payload
            const size_t needed = m_RecvWPct->size() - m_RecvPctRead;
            const size_t available = length - readPos;
            const size_t to_data = std::min(available, needed);
            memcpy(reinterpret_cast<char*>(m_RecvWPct->contents()) + m_RecvPctRead, data + readPos, to_data);
            m_RecvPctRead += to_data;
            readPos += to_data;

            if (m_RecvPctRead < m_RecvWPct->size())
            {
                // Couldn't receive the whole data this time.
                ASSERT(readPos == length);
                errno = EWOULDBLOCK;
                return -1;
            }
        }

        //just received fresh new payload
        if (handle_input_payload() == -1)
        {
            ASSERT((errno != EWOULDBLOCK) && (errno != EAGAIN));
            return -1;
        }
    }

    return length == 4096 ? 1 : 2;
}

int WorldSocket::ProcessIncoming(WorldPacket* new_pct)
{
    ASSERT(new_pct);

    // manage memory ;)
    std::unique_ptr<WorldPacket> aptr(new_pct);

    Opcodes opcode = new_pct->GetOpcode();

    if (m_Closed)
        return -1;

    // Dump received packet.
    if (sPacketLog->CanLogPacket())
        sPacketLog->LogPacket(*new_pct, CLIENT_TO_SERVER);

    std::string opcodeName = GetOpcodeNameForLogging(opcode, false);
    if (m_Session)
        SF_LOG_TRACE("network.opcode", "C->S: %s %s", m_Session->GetPlayerInfo().c_str(), opcodeName.c_str());

    try
    {
        switch (opcode)
        {
            case CMSG_PING:
                return HandlePing(*new_pct);
            case CMSG_AUTH_SESSION:
                if (m_Session)
                {
                    SF_LOG_ERROR("network", "WorldSocket::ProcessIncoming: received duplicate CMSG_AUTH_SESSION from %s", m_Session->GetPlayerInfo().c_str());
                    return -1;
                }

                sScriptMgr->OnPacketReceive(this, WorldPacket(*new_pct));
                return HandleAuthSession(*new_pct);
            case CMSG_KEEP_ALIVE:
                sScriptMgr->OnPacketReceive(this, WorldPacket(*new_pct));
                return 0;
            case CMSG_LOG_DISCONNECT:
                new_pct->rfinish(); // contains uint32 disconnectReason;
                sScriptMgr->OnPacketReceive(this, WorldPacket(*new_pct));
                return 0;
                // not an opcode, client sends string "WORLD OF WARCRAFT CONNECTION - CLIENT TO SERVER" without opcode
                // first 4 bytes become the opcode (2 dropped)
            case MSG_VERIFY_CONNECTIVITY:
            {
                sScriptMgr->OnPacketReceive(this, WorldPacket(*new_pct));
                std::string str;
                *new_pct >> str;
                if (str != "D OF WARCRAFT CONNECTION - CLIENT TO SERVER")
                    return -1;
                return HandleSendAuthSession();
            }
            /*case CMSG_ENABLE_NAGLE:
            {
                SF_LOG_DEBUG("network", "%s", opcodeName.c_str());
                sScriptMgr->OnPacketReceive(this, WorldPacket(*new_pct));
                return m_Session ? m_Session->HandleEnableNagleAlgorithm() : -1;
            }*/
            default:
            {
                GuardType Guard(m_SessionLock);
                if (!m_Session)
                {
                    SF_LOG_ERROR("network.opcode", "ProcessIncoming: Client not authed opcode = %u", uint32(opcode));
                    return -1;
                }

                // prevent invalid memory access/crash with custom opcodes
                if (opcode >= NUM_OPCODES)
                    return 0;

                OpcodeHandler const* handler = clientOpcodeTable[opcode];
                if (!handler || handler->Status == STATUS_UNHANDLED)
                {
                    SF_LOG_ERROR("network.opcode", "No defined handler for opcode %s sent by %s", GetOpcodeNameForLogging(new_pct->GetOpcode(), false, new_pct->GetReceivedOpcode()).c_str(), m_Session->GetPlayerInfo().c_str());
                    return 0;
                }

                // Our Idle timer will reset on any non PING opcodes.
                // Catches people idling on the login screen and any lingering ingame connections.
                m_Session->ResetTimeOutTime();

                // OK, give the packet to WorldSession
                aptr.release();
                // WARNING here we call it with locks held.
                // Its possible to cause deadlock if QueuePacket calls back
                m_Session->QueuePacket(new_pct);
                return 0;
            }
        }
    }
    catch (ByteBufferException&)
    {
        SF_LOG_ERROR("network", "WorldSocket::ProcessIncoming ByteBufferException occured while parsing an instant handled packet %s from client %s, accountid=%i. Disconnected client.",
            opcodeName.c_str(), GetRemoteAddress().c_str(), m_Session ? int32(m_Session->GetAccountId()) : -1);
        new_pct->hexlike();
        return -1;
    }

    return 0;
}

int WorldSocket::HandleSendAuthSession()
{
    WorldPacket packet(SMSG_AUTH_CHALLENGE, 37);
    packet << uint16(0);

    packet.append(SkyFire::Crypto::GetRandomBytes<32>()); // new encryption seeds

    packet << uint8(1);
    packet.append(m_Seed);

    return SendPacket(packet);
}

int WorldSocket::HandleAuthSession(WorldPacket& recvPacket)
{
    uint8 security;
    uint16 clientBuild;
    uint32 id;
    uint32 addonSize;
    LocaleConstant locale;
    std::string account;
    WorldPacket addonsData;
    std::array<uint8, 4> clientSeed;
    SkyFire::Crypto::SHA1::Digest digest;
    uint32 VirtualRealmID;

    recvPacket.read_skip<uint32>();
    recvPacket.read_skip<uint32>();
    recvPacket >> digest[18];
    recvPacket >> digest[14];
    recvPacket >> digest[3];
    recvPacket >> digest[4];
    recvPacket >> digest[0];
    recvPacket >> VirtualRealmID;
    recvPacket >> digest[11];
    recvPacket.read(clientSeed);
    recvPacket >> digest[19];
    recvPacket.read_skip<uint8>();
    recvPacket.read_skip<uint8>();
    recvPacket >> digest[2];
    recvPacket >> digest[9];
    recvPacket >> digest[12];
    recvPacket.read_skip<uint64>();
    recvPacket.read_skip<uint32>();
    recvPacket >> digest[16];
    recvPacket >> digest[5];
    recvPacket >> digest[6];
    recvPacket >> digest[8];
    recvPacket >> clientBuild;
    recvPacket >> digest[17];
    recvPacket >> digest[7];
    recvPacket >> digest[13];
    recvPacket >> digest[15];
    recvPacket >> digest[1];
    recvPacket >> digest[10];
    recvPacket >> addonSize;

    addonsData.resize(addonSize);
    recvPacket.read((uint8*)addonsData.contents(), addonSize);

    recvPacket.ReadBit();
    uint32 accountNameLength = recvPacket.ReadBits(11);

    account = recvPacket.ReadString(accountNameLength);

    if (sWorld->IsClosed())
    {
        SendAuthResponseError(ResponseCodes::AUTH_REJECT);
        SF_LOG_ERROR("network", "WorldSocket::HandleAuthSession: World closed, denying client (%s).", GetRemoteAddress().c_str());
        return -1;
    }

    // Get the account information from the realmd database
    //         0           1        2       3          4         5       6          7   8
    // SELECT id, sessionkey, last_ip, locked, expansion, mutetime, locale, recruiter, os FROM account WHERE username = ?
    PreparedStatement* stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_INFO_BY_NAME);

    stmt->setString(0, account);

    PreparedQueryResult result = LoginDatabase.Query(stmt);

    // Stop if the account is not found
    if (!result)
    {
        SendAuthResponseError(ResponseCodes::AUTH_UNKNOWN_ACCOUNT);
        SF_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Sent Auth Response (unknown account).");
        return -1;
    }

    Field* fields = result->Fetch();

    uint8 expansion = fields[4].GetUInt8();
    uint32 world_expansion = sWorld->getIntConfig(WorldIntConfigs::CONFIG_EXPANSION);
    if (expansion > world_expansion)
        expansion = world_expansion;

    ///- Re-check ip locking (same check as in realmd).
    if (fields[3].GetUInt8() == 1) // if ip is locked
    {
        if (strcmp(fields[2].GetCString(), GetRemoteAddress().c_str()))
        {
            SendAuthResponseError(ResponseCodes::AUTH_FAILED);
            SF_LOG_DEBUG("network", "WorldSocket::HandleAuthSession: Sent Auth Response (Account IP differs).");
            return -1;
        }
    }

    id = fields[0].GetUInt32();

    SessionKey sessionKey = fields[1].GetBinary<SESSION_KEY_LENGTH>();

    int64 mutetime = fields[5].GetInt64();
    //! Negative mutetime indicates amount of seconds to be muted effective on next login - which is now.
    if (mutetime < 0)
    {
        mutetime = time(NULL) + llabs(mutetime);

        stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_MUTE_TIME_LOGIN);

        stmt->setInt64(0, mutetime);
        stmt->setUInt32(1, id);

        LoginDatabase.Execute(stmt);
    }

    locale = LocaleConstant(fields[6].GetUInt8());
    if (locale >= TOTAL_LOCALES)
        locale = LOCALE_enUS;

    uint32 recruiter = fields[7].GetUInt32();
    std::string os = fields[8].GetString();
    bool hasBoost = fields[9].GetBool();

    // Must be done before WorldSession is created
    if (sWorld->GetBoolConfig(WorldBoolConfigs::CONFIG_WARDEN_ENABLED) && os != "Win" && os != "OSX")
    {
        SendAuthResponseError(ResponseCodes::AUTH_REJECT);
        SF_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Client %s attempted to log in using invalid client OS (%s).", GetRemoteAddress().c_str(), os.c_str());
        return -1;
    }

    // Checks gmlevel per Realm
    stmt = LoginDatabase.GetPreparedStatement(LOGIN_GET_GMLEVEL_BY_REALMID);

    stmt->setUInt32(0, id);
    stmt->setInt32(1, int32(VirtualRealmID));

    result = LoginDatabase.Query(stmt);

    if (!result)
        security = 0;
    else
    {
        fields = result->Fetch();
        security = fields[0].GetUInt8();
    }

    // Re-check account ban (same check as in realmd)
    stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_BANS);

    stmt->setUInt32(0, id);
    stmt->setString(1, GetRemoteAddress());

    PreparedQueryResult banresult = LoginDatabase.Query(stmt);

    if (banresult) // if account banned
    {
        SendAuthResponseError(ResponseCodes::AUTH_BANNED);
        SF_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Sent Auth Response (Account banned).");
        return -1;
    }

    // Check locked state for server
    AccountTypes allowedAccountType = sWorld->GetPlayerSecurityLimit();
    SF_LOG_DEBUG("network", "Allowed Level: %u Player Level %u", uint8(allowedAccountType), security);
    if (allowedAccountType > AccountTypes::SEC_PLAYER && AccountTypes(security) < allowedAccountType)
    {
        SendAuthResponseError(ResponseCodes::AUTH_UNAVAILABLE);
        SF_LOG_INFO("network", "WorldSocket::HandleAuthSession: User tries to login but his security level is not enough");
        return -1;
    }

    // Check that Key and account name are the same on client and server
    uint8 t[4] = { 0x00, 0x00, 0x00, 0x00 };

    SkyFire::Crypto::SHA1 sha;
    sha.UpdateData(account);
    sha.UpdateData(t);
    sha.UpdateData(clientSeed);
    sha.UpdateData(m_Seed);
    sha.UpdateData(sessionKey);
    sha.Finalize();

    std::string address = GetRemoteAddress();

    if (sha.GetDigest() != digest)
    {
        SendAuthResponseError(ResponseCodes::AUTH_FAILED);
        SF_LOG_ERROR("network", "WorldSocket::HandleAuthSession: Authentication failed for account: %u ('%s') address: %s", id, account.c_str(), address.c_str());
        return -1;
    }

    SF_LOG_DEBUG("network", "WorldSocket::HandleAuthSession: Client '%s' authenticated successfully from %s.",
        account.c_str(),
        address.c_str());

    // Check if this user is by any chance a recruiter
    stmt = LoginDatabase.GetPreparedStatement(LOGIN_SEL_ACCOUNT_RECRUITER);

    stmt->setUInt32(0, id);

    result = LoginDatabase.Query(stmt);

    bool isRecruiter = false;
    if (result)
        isRecruiter = true;

    // Update the last_ip in the database

    stmt = LoginDatabase.GetPreparedStatement(LOGIN_UPD_LAST_IP);

    stmt->setString(0, address);
    stmt->setString(1, account);

    LoginDatabase.Execute(stmt);

    // NOTE ATM the socket is single-threaded, have this in mind ...
    m_Session = new (std::nothrow) WorldSession(id, this, AccountTypes(security), expansion, mutetime, locale, recruiter, isRecruiter, hasBoost);
    if (!m_Session)
        return -1;

    m_Crypt.Init(sessionKey);

    m_Session->LoadGlobalAccountData();
    m_Session->LoadTutorialsData();
    m_Session->ReadAddonsInfo(addonsData);
    m_Session->LoadPermissions();

    m_Session->SetVirtualRealmID(VirtualRealmID);

    // Initialize Warden system only if it is enabled by config
    if (sWorld->GetBoolConfig(WorldBoolConfigs::CONFIG_WARDEN_ENABLED))
        m_Session->InitWarden(sessionKey, os);

    // Sleep this Network thread for
    uint32 sleepTime = sWorld->getIntConfig(WorldIntConfigs::CONFIG_SESSION_ADD_DELAY);
    std::this_thread::sleep_for(std::chrono::microseconds(sleepTime));

    sWorld->AddSession(m_Session);
    return 0;
}

int WorldSocket::HandlePing(WorldPacket& recvPacket)
{
    uint32 ping;
    uint32 latency;

    // Get the ping packet content
    recvPacket >> latency;
    recvPacket >> ping;

    if (!m_HasLastPingTime)
    {
        m_LastPingTime = std::chrono::steady_clock::now(); // for 1st ping
        m_HasLastPingTime = true;
    }
    else
    {
        std::chrono::steady_clock::time_point cur_time = std::chrono::steady_clock::now();
        auto diff_time = cur_time - m_LastPingTime;
        m_LastPingTime = cur_time;

        if (diff_time < std::chrono::seconds(27))
        {
            ++m_OverSpeedPings;

            uint32 max_count = sWorld->getIntConfig(WorldIntConfigs::CONFIG_MAX_OVERSPEED_PINGS);

            if (max_count && m_OverSpeedPings > max_count)
            {
                GuardType Guard(m_SessionLock);

                if (m_Session && !m_Session->HasPermission(rbac::RBAC_PERM_SKIP_CHECK_OVERSPEED_PING))
                {
                    SF_LOG_ERROR("network", "WorldSocket::HandlePing: %s kicked for over-speed pings (address: %s)",
                        m_Session->GetPlayerInfo().c_str(), GetRemoteAddress().c_str());

                    return -1;
                }
            }
        }
        else
            m_OverSpeedPings = 0;
    }

    // critical section
    {
        GuardType Guard(m_SessionLock);

        if (m_Session)
        {
            m_Session->SetLatency(latency);
            m_Session->ResetClientTimeDelay();
        }
        else
        {
            SF_LOG_ERROR("network", "WorldSocket::HandlePing: peer sent CMSG_PING, "
                "but is not authenticated or got recently kicked, "
                " address = %s",
                GetRemoteAddress().c_str());
            return -1;
        }
    }

    WorldPacket packet(SMSG_PONG, 4);
    packet << ping;
    return SendPacket(packet);
}

void WorldSocket::SendAuthResponseError(ResponseCodes code)
{
    WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
    packet.WriteBit(0); // has account info
    packet.WriteBit(0); // has queue info
    packet << uint8(code);
    SendPacket(packet);
}
