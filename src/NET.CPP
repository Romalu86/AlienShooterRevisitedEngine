
#include "myerror.h"
#include "engine_globals.h"
#include "mylib.h"
#include "net.h"
#include "basetype.h"
#include "map.h"
#include <new>
#include <exception>
#include <cstring>
#include "zm/Utils/zStaticStorage.h"

namespace {
enum NetPacketType : unsigned int
{
    NET_PACKET_BINARY = 0u,
    NET_PACKET_GAME_START = 134u,
    NET_PACKET_GAME_END = 135u,
    NET_PACKET_CHAT = 136u,
    NET_PACKET_STRING = 137u
};

enum NetDirectPlayMessageId : unsigned int
{
    NET_MSG_INDICATE_CONNECT = 0xFFFF000Eu,
    NET_MSG_ENUM_HOSTS_QUERY = 0xFFFF000Au,
    NET_MSG_ENUM_HOSTS_RESPONSE = 0xFFFF000Bu,
    NET_MSG_DESTROY_PLAYER = 0xFFFF0009u,
    NET_MSG_CREATE_PLAYER = 0xFFFF0007u,
    NET_MSG_RECEIVE = 0xFFFF0011u,
    NET_MSG_HOST_MIGRATE = 0xFFFF000Du,
    NET_MSG_PEER_INFO = 0xFFFF0010u,
    NET_MSG_TERMINATE_SESSION = 0xFFFF0016u
};
}

static void CopyDirectPlayQueueBytes(void* destination,const void* source,unsigned int size);


NET* Net = 0;
unsigned int networkTransmissionId = 0;
IDirectPlay8Peer* g_pDP = 0;
IDirectPlay8LobbiedApplication* g_pLobbyApp = 0;
IDirectPlay8Address* g_pDeviceAddress = 0;
IDirectPlay8Address* g_pHostAddress = 0;
DPNHANDLE g_hLobbyHandle = 0;
DPNID g_dpnidLocalPlayer = 0;
DPNID g_dpnidHostPlayer = 0;
DPNHANDLE hAsyncCancelEnumHosts = 0;
LONG g_lNumberOfActivePlayers = 0;
int g_NumberOfHost = 0;
CRITICAL_SECTION g_csHostList;


namespace {

struct HOST_RESPONSE
{
    unsigned int checkListCrc;
    char playerNames[4092];
};
HOST_RESPONSE hostResponse;
}

struct HOST_NODE
{
    HOST_NODE();
    ~HOST_NODE() noexcept(false);

    DPN_APPLICATION_DESC applicationDesc;
    IDirectPlay8Address* address;
    STRING sessionName;
    unsigned int latencyMs;
    STRING playerNames;
    unsigned int checkListCrc;
};

StaticObjectStorage<HOST_NODE[16]> g_HostList;

HOST_NODE::HOST_NODE()
{
    address = 0;
}

HOST_NODE::~HOST_NODE() noexcept(false)
{
    if (address)
    {
        (void)address->Release();
        address = 0;
    }
}

void __cdecl NetHostListConstruct()
{
    HOST_NODE* const nodes = g_HostList.Get();
    unsigned int constructed = 0;
    try
    {
        for (; constructed < 16; ++constructed)
            ::new (static_cast<void*>(nodes + constructed)) HOST_NODE;
    }
    catch (...)
    {
        try
        {
            while (constructed != 0)
                nodes[--constructed].~HOST_NODE();
        }
        catch (...)
        {
            std::terminate();
        }
        throw;
    }
}

void __cdecl NetHostListCleanup()
{
    HOST_NODE* const nodes = g_HostList.Get();
    unsigned int remaining = 16;
    try
    {
        while (remaining != 0)
            nodes[--remaining].~HOST_NODE();
    }
    catch (...)
    {
        try
        {
            while (remaining != 0)
                nodes[--remaining].~HOST_NODE();
        }
        catch (...)
        {
            std::terminate();
        }
        throw;
    }
}

unsigned int __cdecl NetHostListRegisterCleanup()
{
    return RegisterCrtAtExit(&NetHostListCleanup);
}

unsigned int __cdecl NetHostListStaticInit()
{
    NetHostListConstruct();
    return NetHostListRegisterCleanup();
}


NET::NET(HINSTANCE__* hInstance, STRING title)
{
    (void)hInstance;
    (void)title;

    unsigned int initialFlags = m_flags;
    initialFlags &= 0xFFFFFFFEu;
    initialFlags &= 0xFFFFFF00u;
    m_receiveWrite = 0u;
    m_receiveRead = 0u;
    m_noPlayers = 0u;
    m_localPlayer = 0u;
    m_flags = initialFlags;
    static_cast<void>(::Net = this);
    m_chatEnd = 0u;

    for (unsigned int playerIndex = 0;playerIndex<8u;++playerIndex)
    {
        m_players[playerIndex].m_dpnid = 0u;
        m_players[playerIndex].m_chatColor = 0u;
        m_players[playerIndex].m_0C = 0u;
        m_players[playerIndex].m_pendingDestroy = 0u;
        m_players[playerIndex].m_roundTripLatencyMs = 0u;
    }


}

NET::NET_PLAYER::NET_PLAYER()
{
}

NET::NET_PLAYER::~NET_PLAYER()
{
}

NET::~NET() noexcept(false)
{
}

void NET::GameEnd()
{
    if ((m_flags&1u) != 0u)(void)SendGuaranteed(0, 0u, NET_PACKET_GAME_END, -1);
}

void NET::NetworkOff()
{
    if ((m_flags&1u) != 0u)
    {
        if ((m_flags&8u) != 0u)(void)GameDisconnect();
        m_flags &= ~0x20u;
    }
}







const STRING WideCharToString(const unsigned short* wideText)
{
    if (wideText == 0)
        return STRING();

    char convertedText[4096];
    WideCharToMultiByte(0, 0, wideText, -1, convertedText, 4096, 0, 0);
    return STRING(convertedText);
}


long NET::GameDisconnect()
{
    if ((m_flags&1u) == 0u)return E_FAIL;
    CancelHostSearch();
    ClearHostList();
    m_noPlayers = 0u;
    m_localPlayer = 0u;
    m_flags &= ~0x1Au;
    IDirectPlay8Peer* peer = g_pDP;
    if (peer)
    {
        (void)peer->Close(0u);
        peer = g_pDP;
    }
    const HRESULT initializeResult = peer->Initialize(0, &DirectPlayMessageHandler, 0u);
    if ((int)initializeResult >= 0)return 0u;
    STRING message("DirectPlay in GameDisconnect");
    STRING category("NET");
    (void)::Error->Error(category, E_INIT, message, initializeResult);
    return initializeResult;
}






DPN_PLAYER_INFO* GetPeerInfo(DPNID peerId)
{


    IDirectPlay8Peer* peer = g_pDP;
    if (!peer)return 0;
    DWORD bufferBytes = 0u;
    HRESULT queryResult = peer->GetPeerInfo(peerId, 0, &bufferBytes, 0u);
    if (((int)queryResult<0 && queryResult != 0x80158100u) || bufferBytes == 0u)
    {
        STRING subject("Failed GetPeerInfo:DirectPlayMessageHandler");
        STRING category("NET");
        ::Error->Error(category, E_GET, subject, queryResult);
        return 0;
    }
    DPN_PLAYER_INFO* const playerInfo = static_cast<DPN_PLAYER_INFO*>(CrtNew(bufferBytes));
    std::memset(playerInfo, 0, bufferBytes);
    playerInfo->dwSize = sizeof(DPN_PLAYER_INFO);
    peer = g_pDP;
    queryResult = peer->GetPeerInfo(peerId, playerInfo, &bufferBytes, 0u);
    if (static_cast<int>(queryResult) >= 0)
        return playerInfo;
    {
        STRING subject("Failed GetPeerInfo2:DirectPlayMessageHandler");
        STRING category("NET");
        ::Error->Error(category, E_GET, subject, queryResult);
    }
    CrtFreePointer(playerInfo);
    return 0;
}





static void DirectPlayQueueDiagnostic(const char* messageText, int errorCode);


long __stdcall DirectPlayMessageHandler(void* userContext, unsigned long messageId, void* messageBody)
{
    (void)userContext;
    NET* network = ::Net;
    switch (messageId)
    {
    case NET_MSG_INDICATE_CONNECT:
    {

        if ((network->m_flags&0x10u) != 0u)
        {
            ::Error->Log("DPN_MSGID_INDICATE_CONNECT - DPNERR_NOTALLOWED");
            return static_cast<long>(0x80158520u);
        }

        const unsigned int currentTime = ::RealCurrentTime;
        int playerIndex = 3;
        for (;playerIndex<8;++playerIndex)
        {
            const NET::NET_PLAYER& player = network->m_players[playerIndex];
            if (player.m_dpnid == 0u && currentTime-player.m_0C>5000u)
                break;
        }
        const long connectResult = playerIndex<8?0:static_cast<long>(0x80158520u);
        ::Error->Log("DPN_MSGID_INDICATE_CONNECT - player reserved %i", playerIndex);
        return connectResult;
    }
    case NET_MSG_ENUM_HOSTS_QUERY:
    {

        (void)EnterCriticalSection(&g_csHostList);
        ::Error->Log("DPN_MSGID_ENUM_HOSTS_QUERY %u", ::CurrentTime);

        network = ::Net;
        if ((network->m_flags&0x10u) != 0u)
        {
            (void)LeaveCriticalSection(&g_csHostList);
            return static_cast<long>(0x80158520u);
        }

        hostResponse.checkListCrc = network->m_checkListCrc;
        char* const responseText = hostResponse.playerNames;
        unsigned int textLength = 0u;
        responseText[0] = '\0';
        for (unsigned int playerIndex = 0;playerIndex<8u;++playerIndex)
        {
            const NET::NET_PLAYER& player = network->m_players[playerIndex];
            if (player.m_dpnid == 0u)continue;
            const char* playerName = player.m_name.value;
            while (*playerName)responseText[textLength++] = *playerName++;
            responseText[textLength++] = '\n';
            responseText[textLength++] = '\r';
            responseText[textLength] = '\0';
        }
        unsigned int responseTextBytes = textLength+1u;
        if (responseTextBytes>4090u)responseTextBytes = 4090u;
        static_cast<DPNMSG_ENUM_HOSTS_QUERY*>(messageBody)->dwResponseDataSize = responseTextBytes+4u;
        static_cast<DPNMSG_ENUM_HOSTS_QUERY*>(messageBody)->pvResponseData = &hostResponse;
        (void)LeaveCriticalSection(&g_csHostList);
        return 0;
    }
    case NET_MSG_ENUM_HOSTS_RESPONSE:
    {

        int hostCount = static_cast<int>(g_NumberOfHost);
        if (hostCount >= 16)
        {
            STRING message;
            STRING category;
            message = "Too many host session";
            category = "NET";
            (void)::Error->Error(category, E_ERROR, message, static_cast<unsigned int>(hostCount));
            return 0;
        }

        const DPN_APPLICATION_DESC* const applicationDesc = static_cast<DPNMSG_ENUM_HOSTS_RESPONSE*>(messageBody)->pApplicationDescription;
        (void)EnterCriticalSection(&g_csHostList);
        ::Error->Log("DPN_MSGID_ENUM_HOSTS_RESPONSE %u", ::CurrentTime);

        hostCount = static_cast<int>(g_NumberOfHost);
        const unsigned char* const instanceGuidBytes =
            reinterpret_cast<const unsigned char*>(&applicationDesc->guidInstance);
        for (int itemIndex = 0;itemIndex<hostCount;++itemIndex)
        {
            const unsigned char* const existingGuidBytes = reinterpret_cast<const unsigned char*>(&g_HostList.Get()[itemIndex].applicationDesc.guidInstance);
            bool sameInstance = true;
            for (unsigned int guidByteIndex = 0;guidByteIndex<16u;++guidByteIndex)if (instanceGuidBytes[guidByteIndex] != existingGuidBytes[guidByteIndex])
            {
                sameInstance = false;
                break;
            }
            if (sameInstance)
            {
                (void)LeaveCriticalSection(&g_csHostList);
                return 0;
            }
        }

        HOST_NODE& addressNode = g_HostList.Get()[static_cast<unsigned int>(hostCount)];
        IDirectPlay8Address* const responseAddress = static_cast<DPNMSG_ENUM_HOSTS_RESPONSE*>(messageBody)->pAddressSender;
        const HRESULT duplicateHr = responseAddress->Duplicate(&addressNode.address);
        if (duplicateHr<0)
        {
            (void)LeaveCriticalSection(&g_csHostList);
            return 0;
        }

        const unsigned char* const applicationDescBytes = reinterpret_cast<const unsigned char*>(applicationDesc);
        HOST_NODE& hostNode = g_HostList.Get()[g_NumberOfHost];
        unsigned char* const copiedDescBytes = reinterpret_cast<unsigned char*>(&hostNode.applicationDesc);
        for (unsigned int itemIndex = 0;itemIndex<sizeof(DPN_APPLICATION_DESC);++itemIndex)copiedDescBytes[itemIndex] = 0u;
        for (unsigned int itemIndex = 0;itemIndex<sizeof(DPN_APPLICATION_DESC);++itemIndex)copiedDescBytes[itemIndex] = applicationDescBytes[itemIndex];
        hostNode.applicationDesc.pwszSessionName = 0;
        hostNode.applicationDesc.pwszPassword = 0;
        hostNode.applicationDesc.pvReservedData = 0;
        hostNode.applicationDesc.dwReservedDataSize = 0u;
        hostNode.applicationDesc.pvApplicationReservedData = 0;
        hostNode.applicationDesc.dwApplicationReservedDataSize = 0u;

        STRING sessionName = WideCharToString(applicationDesc->pwszSessionName);
        g_HostList.Get()[g_NumberOfHost].sessionName = sessionName;

        unsigned int hostIndex = g_NumberOfHost;
        g_HostList.Get()[hostIndex].latencyMs = static_cast<DPNMSG_ENUM_HOSTS_RESPONSE*>(messageBody)->dwRoundTripLatencyMS;
        if (static_cast<DPNMSG_ENUM_HOSTS_RESPONSE*>(messageBody)->dwResponseDataSize != 0u)
        {
            void* responseData = static_cast<DPNMSG_ENUM_HOSTS_RESPONSE*>(messageBody)->pvResponseData;
            g_HostList.Get()[hostIndex].playerNames = static_cast<const char*>(responseData)+4u;
            hostIndex = g_NumberOfHost;
            g_HostList.Get()[hostIndex].checkListCrc = ReadU32(responseData, 0u);
        }
        (void)(g_NumberOfHost = hostIndex+1u);
        (void)LeaveCriticalSection(&g_csHostList);

        return 0;
    }
    case NET_MSG_DESTROY_PLAYER:
    {

        const unsigned int playerId = static_cast<DPNMSG_DESTROY_PLAYER*>(messageBody)->dpnidPlayer;
        unsigned int playerIndex = 0u;
        for (;playerIndex<8u;++playerIndex)
            if (network->m_players[playerIndex].m_dpnid == playerId)break;

        if (playerIndex<8u)
        {
            NET::NET_PLAYER& playerRecord = network->m_players[playerIndex];
            if (g_dpnidLocalPlayer == playerRecord.m_dpnid)
            {
                playerRecord.m_dpnid = 0u;
                network = ::Net;
            }
            if ((network->m_flags&0x10u) != 0u)
            {
                network->m_players[playerIndex].m_pendingDestroy = 1u;
            }
            else
            {
                network->m_players[playerIndex].m_dpnid = 0u;
                network = ::Net;
                if ((network->m_flags&0x40u) == 0u)
                {
                    for (unsigned int shiftIndex = playerIndex;shiftIndex<7u;++shiftIndex)
                    {
                        network->m_players[shiftIndex].m_dpnid = network->m_players[shiftIndex+1u].m_dpnid;
                        network = ::Net;
                        network->m_players[shiftIndex].m_name = network->m_players[shiftIndex+1u].m_name;
                        network = ::Net;
                        network->m_players[shiftIndex].m_chatColor = network->m_players[shiftIndex+1u].m_chatColor;
                        network = ::Net;

                        unsigned int chatIndex = (network->m_chatEnd+1u)&0xFFu;
                        while (chatIndex != network->m_chatEnd)
                        {
                            const unsigned int chatPlayerIndex = network->m_chatPlayer[chatIndex];
                            if (chatPlayerIndex == shiftIndex)
                            {
                                network->m_chatPlayer[chatIndex] = 7u;
                                network = ::Net;
                            }
                            else if (chatPlayerIndex == shiftIndex+1u)
                            {
                                network->m_chatPlayer[chatIndex] = shiftIndex;
                                network = ::Net;
                            }
                            chatIndex = (chatIndex+1u)&0xFFu;
                        }
                        if (network->m_localPlayer == shiftIndex+1u)
                        {
                            network->m_localPlayer = shiftIndex;
                            network = ::Net;
                        }
                    }
                    network->m_players[7].m_dpnid = 0u;
                }
            }
        }

        ::Error->Log("DPN_MSGID_DESTROY_PLAYER %u %u %u", static_cast<DPNMSG_DESTROY_PLAYER*>(messageBody)->dpnidPlayer, playerIndex, ::CurrentTime);

        switch (static_cast<DPNMSG_DESTROY_PLAYER*>(messageBody)->dwReason)
        {
        case 2u:
            DirectPlayQueueDiagnostic("Connection lost:DirectPlayMessageHandler", 0);
            break;
        case 3u:
            DirectPlayQueueDiagnostic("Session terminated:DirectPlayMessageHandler", 0);
            break;
        case 4u:
            DirectPlayQueueDiagnostic("Host destroyed local player:DirectPlayMessageHandler", 0);
            break;
        default:
            break;
        }

        if (static_cast<DPNMSG_DESTROY_PLAYER*>(messageBody)->dpnidPlayer == g_dpnidLocalPlayer)g_dpnidLocalPlayer = 0u;
        if (static_cast<DPNMSG_DESTROY_PLAYER*>(messageBody)->dpnidPlayer == g_dpnidHostPlayer)
        {
            g_dpnidHostPlayer = 0u;
            network = ::Net;
            network->GameEnd();
        }
        if (g_lNumberOfActivePlayers != 0u)
            (void)InterlockedDecrement(&g_lNumberOfActivePlayers);

        return 0;
    }
    case NET_MSG_CREATE_PLAYER:
    {

        const unsigned int playerId = static_cast<DPNMSG_CREATE_PLAYER*>(messageBody)->dpnidPlayer;
        DPN_PLAYER_INFO* const playerInfo = GetPeerInfo(playerId);
        if (!playerInfo)return 0;
        network = ::Net;

        unsigned int playerIndex = 8u;

        if ((playerInfo->dwPlayerFlags&0x04u) != 0u)
        {

            g_dpnidHostPlayer = static_cast<DPNMSG_CREATE_PLAYER*>(messageBody)->dpnidPlayer;
            playerIndex = 0u;
        }
        else
        {

            for (unsigned int vacantIndex = 3u;vacantIndex<8u;++vacantIndex)
            {
                if (network->m_players[vacantIndex].m_dpnid != 0u || vacantIndex >= 7u)continue;
                for (unsigned int shiftIndex = vacantIndex;shiftIndex<7u;++shiftIndex)
                {
                    network->m_players[shiftIndex].m_dpnid = network->m_players[shiftIndex+1u].m_dpnid;
                    network = ::Net;
                    network->m_players[shiftIndex].m_name = network->m_players[shiftIndex+1u].m_name;
                    network = ::Net;
                    network->m_players[shiftIndex].m_chatColor = network->m_players[shiftIndex+1u].m_chatColor;
                    network = ::Net;
                    {
        unsigned int chatIndex = (network->m_chatEnd+1u)&0xFFu;
        while (chatIndex != network->m_chatEnd)
        {
            const unsigned int chatPlayerIndex = network->m_chatPlayer[chatIndex];
            if (chatPlayerIndex == shiftIndex)
            {
                network->m_chatPlayer[chatIndex] = 7u;
                network = ::Net;
            }
            else if (chatPlayerIndex == shiftIndex+1u)
            {
                network->m_chatPlayer[chatIndex] = shiftIndex;
                network = ::Net;
            }
            chatIndex = (chatIndex+1u)&0xFFu;
        }

    }
                    if (network->m_localPlayer == shiftIndex+1u)
                    {
                        network->m_localPlayer = shiftIndex;
                        network = ::Net;
                    }
                }
                network->m_players[7].m_dpnid = 0u;
                network = ::Net;
            }


            playerIndex = 3u;
            while (playerIndex<8u)
            {
                const unsigned int existingPlayerId = network->m_players[playerIndex].m_dpnid;
                if (existingPlayerId == 0u || static_cast<DPNMSG_CREATE_PLAYER*>(messageBody)->dpnidPlayer<existingPlayerId)break;
                ++playerIndex;
            }
            if (playerIndex <= 6u && network->m_players[playerIndex].m_dpnid != 0u)
            {
                for (int shiftIndex = 6;shiftIndex >= static_cast<int>(playerIndex);--shiftIndex)
                {
                    const unsigned int shiftedPlayerIndex = static_cast<unsigned int>(shiftIndex);
                    network->m_players[shiftedPlayerIndex+1u].m_dpnid = network->m_players[shiftedPlayerIndex].m_dpnid;
                    network = ::Net;
                    network->m_players[shiftedPlayerIndex+1u].m_name = network->m_players[shiftedPlayerIndex].m_name;
                    network = ::Net;
                    network->m_players[shiftedPlayerIndex+1u].m_chatColor = network->m_players[shiftedPlayerIndex].m_chatColor;
                    network = ::Net;
                    {
        unsigned int chatIndex = (network->m_chatEnd+1u)&0xFFu;
        while (chatIndex != network->m_chatEnd)
        {
            if (network->m_chatPlayer[chatIndex] == shiftedPlayerIndex)
            {
                network->m_chatPlayer[chatIndex] = shiftedPlayerIndex+1u;
                network = ::Net;
            }
            chatIndex = (chatIndex+1u)&0xFFu;
        }

    }
                    if (network->m_localPlayer == shiftedPlayerIndex)
                    {
                        network->m_localPlayer = shiftedPlayerIndex+1u;
                        network = ::Net;
                    }
                }
            }

            if (playerIndex >= 8u)
                DirectPlayQueueDiagnostic(
                    "n_player = %i",
                    static_cast<int>(playerIndex));
        }

        if (playerIndex<8u)
        {
            if ((playerInfo->dwPlayerFlags&0x02u) != 0u)
            {
                g_dpnidLocalPlayer = static_cast<DPNMSG_CREATE_PLAYER*>(messageBody)->dpnidPlayer;
                network->m_localPlayer = playerIndex;
                network = ::Net;
            }
            network->m_players[playerIndex].m_dpnid = static_cast<DPNMSG_CREATE_PLAYER*>(messageBody)->dpnidPlayer;
            {
                STRING playerName = WideCharToString(playerInfo->pwszName);
                network = ::Net;
                network->m_players[playerIndex].m_name = playerName;
            }
            network = ::Net;
            network->m_players[playerIndex].m_chatColor = 0u;
            (void)InterlockedIncrement(&g_lNumberOfActivePlayers);
        }

        STRING playerDescription;
        playerDescription = (playerInfo->dwPlayerFlags&0x04u) != 0u ? " host " : StringGlobals::EmptyText();
        if ((playerInfo->dwPlayerFlags&0x02u) != 0u)
            playerDescription += " local ";

        STRING logFormat = "DPN_MSGID_CREATE_PLAYER %u %u"+playerDescription;
        ::Error->Log(logFormat, static_cast<DPNMSG_CREATE_PLAYER*>(messageBody)->dpnidPlayer, playerIndex);
        CrtFreePointer(playerInfo);

        return 0;
    }
    case NET_MSG_RECEIVE:
    {

        (void)EnterCriticalSection(&g_csHostList);
        network = ::Net;

        const unsigned int senderId = static_cast<DPNMSG_RECEIVE*>(messageBody)->dpnidSender;
        unsigned char* packetData = reinterpret_cast<unsigned char*>(static_cast<DPNMSG_RECEIVE*>(messageBody)->pReceiveData);
        const unsigned int packetBytes = static_cast<DPNMSG_RECEIVE*>(messageBody)->dwReceiveDataSize;
        const unsigned int packetType = packetData[0];
        if (packetType == NET_PACKET_BINARY || packetType == NET_PACKET_STRING)
        {
            network->AddReceivedData(packetData+1u, packetBytes-1u, senderId, packetType);
        }
        else if (packetType == NET_PACKET_GAME_START)
        {
            ::Error->Log("DPN_MSGID_RECEIVE game start %s %u", reinterpret_cast<const char*>(packetData+1u), ::CurrentTime);
            Map->PauseOff();
            packetData = reinterpret_cast<unsigned char*>(static_cast<DPNMSG_RECEIVE*>(messageBody)->pReceiveData);
            {
                STRING networkMapName(reinterpret_cast<const char*>(packetData+1u));
                Map->SetNetworkMapName(networkMapName);
            }
            network = ::Net;
            network->m_receiveWrite = 0u;
            network->m_receiveRead = 0u;
            network = ::Net;
            network->m_flags |= 0x10u;
            network = ::Net;
            network->m_flags |= 0x40u;
        }
        else if (packetType == NET_PACKET_GAME_END)
        {
            ::Error->Log("DPN_MSGID_RECEIVE game end %u", ::CurrentTime);
            network = ::Net;
            network->m_flags &= ~0x10u;
        }
        else if (packetType == NET_PACKET_CHAT)
        {
            unsigned int playerIndex = 0u;
            for (;playerIndex<8u;++playerIndex)
                if (network->m_players[playerIndex].m_dpnid == senderId)break;
            if (playerIndex<8u)
            {
                network->m_chatPlayer[network->m_chatEnd] = playerIndex;
                network = ::Net;
            }
            packetData = static_cast<const volatile DPNMSG_RECEIVE*>(messageBody)->pReceiveData;
            network->m_chatText[network->m_chatEnd] = reinterpret_cast<const char*>(packetData+1u);
            network = ::Net;
            network->m_chatEnd = (network->m_chatEnd+1u)&0xFFu;
        }
        (void)LeaveCriticalSection(&g_csHostList);

        return 0;
    }
    case NET_MSG_HOST_MIGRATE:
        (void)EnterCriticalSection(&g_csHostList);
        ::Error->Log("DPN_MSGID_HOST_MIGRATE %u", ::CurrentTime);
        g_dpnidHostPlayer = static_cast<DPNMSG_HOST_MIGRATE*>(messageBody)->dpnidNewHost;
        (void)LeaveCriticalSection(&g_csHostList);
        return 0;
    case NET_MSG_PEER_INFO:
    {

        const unsigned int peerId = static_cast<DPNMSG_PEER_INFO*>(messageBody)->dpnidPeer;
        DPN_PLAYER_INFO* playerInfo = GetPeerInfo(peerId);
        if (!playerInfo)return 0;
        network = ::Net;

        unsigned int playerIndex = 0u;
        for (;playerIndex<8u;++playerIndex)
            if (network->m_players[playerIndex].m_dpnid == static_cast<DPNMSG_PEER_INFO*>(messageBody)->dpnidPeer)break;

        if (playerIndex<8u)
        {
            STRING playerName = WideCharToString(playerInfo->pwszName);
            network = ::Net;
            network->m_players[playerIndex].m_name = playerName;
        }
        else
        {
            DirectPlayQueueDiagnostic(
                "DPN_MSGID_PEER_INFO:DirectPlayMessageHandler() invalid id player", static_cast<int>(static_cast<DPNMSG_PEER_INFO*>(messageBody)->dpnidPeer));
        }
        CrtFreePointer(playerInfo);

        return 0;
    }
    case NET_MSG_TERMINATE_SESSION:
        ClearHostList();
        (void)EnterCriticalSection(&g_csHostList);
        ::Error->Log("DPN_MSGID_TERMINATE_SESSION %u", ::CurrentTime);
        g_lNumberOfActivePlayers = 0u;
        g_dpnidLocalPlayer = 0u;
        g_dpnidHostPlayer = 0u;
        (void)LeaveCriticalSection(&g_csHostList);
        return 0;
    case 0xFFFF0008u:
    case 0xFFFF000Cu:
    case 0xFFFF000Fu:
    case 0xFFFF0012u:
    case 0xFFFF0013u:
    case 0xFFFF0014u:
    case 0xFFFF0015u:
    default:
        return 0;
    }
}

#if !defined(_MSC_VER)
#  if !defined(__stdcall)
#    define __stdcall
#  endif
#endif

void NET::CancelHostSearch()
{
    const unsigned long asyncHandle = hAsyncCancelEnumHosts;
    if (asyncHandle == 0)
        return;

    IDirectPlay8Peer* const peer = g_pDP;
    if (peer == 0)
        return;

    (void)peer->CancelAsyncOperation(asyncHandle, 2u);

    hAsyncCancelEnumHosts = 0u;
}

#if !defined(_MSC_VER)
#  if !defined(__cdecl)
#    define __cdecl
#  endif
#  if !defined(__stdcall)
#    define __stdcall
#  endif
#endif


long __stdcall LobbyAppMessageHandler(
    void*                ,
    unsigned long messageId,
    void* messageBody)
{

    if (messageId != 0x00008002u)
        return 0;

    MYERROR* log =
        ::Error;
    log->Log("DPL_MSGID_CONNECT");

    IDirectPlay8Peer* const peer = g_pDP;
    const DPNHANDLE connectHandle = static_cast<DPL_MESSAGE_CONNECT*>(messageBody)->hConnectId;
    (void)peer->RegisterLobby(connectHandle,
        g_pLobbyApp, 1u);
    return 0;
}

#if !defined(_MSC_VER) && !defined(__stdcall)
#define __stdcall
#endif


static void EnterDirectPlayRuntimeCriticalSection()
{
    EnterCriticalSection(&g_csHostList);
}
static void LeaveDirectPlayRuntimeCriticalSection()
{
    LeaveCriticalSection(&g_csHostList);
}


void ClearHostList()
{
    EnterDirectPlayRuntimeCriticalSection();
    int hostCount = g_NumberOfHost;
    for (int hostIndex = 0; hostIndex < hostCount; ++hostIndex)
    {
        HOST_NODE& hostNode = g_HostList.Get()[hostIndex];
        if (hostNode.address)
        {
            (void)hostNode.address->Release();
            hostNode.address = 0;
        }

        hostCount = g_NumberOfHost;
    }
    g_NumberOfHost = 0;
    LeaveDirectPlayRuntimeCriticalSection();
}


#if !defined(_MSC_VER) && !defined(__stdcall)
#define __stdcall
#endif

static void CopyDirectPlayPayload(void* destination, const void* source, unsigned int byteCount)
{
    std::memmove(destination, source, byteCount);
}

static void DirectPlayNoPlayersMessage()
{
    MessageBoxA(0, "No one is around to wave at! :(",
       "SimplePeer", 0u);
}


static long DirectPlaySendPacket(NET* network, const void* payload, unsigned int payloadSize,
                          unsigned char packetType, int playerIndex)
{
    unsigned char packet[4096];
    packet[0] = packetType;
    CopyDirectPlayPayload(packet + 1, payload, payloadSize);

    DPN_BUFFER_DESC packetDesc;
    packetDesc.dwBufferSize = payloadSize+1u;
    packetDesc.pBufferData = packet;
    DPNHANDLE asyncHandle;
    unsigned long destination = 0;
    if (playerIndex != -1)destination = network->m_players[playerIndex].m_dpnid;
    IDirectPlay8Peer* const peer = g_pDP;
    (void)peer->SendTo(destination, &packetDesc, 1u, 0u, 0, &asyncHandle, 8u);
    return 0;
}


long NET::Send(
    const void* payload, unsigned int payloadSize,
    unsigned char packetType, int playerIndex)
{
    if ((m_flags&1u) == 0u)return E_FAIL;
    if ((m_flags&8u) == 0u)return 0;
    if (m_noPlayers == 0u)
    {
        DirectPlayNoPlayersMessage();
        return 0;
    }
    return DirectPlaySendPacket(this, payload, payloadSize, packetType, playerIndex);
}


long NET::SendGuaranteed(
    const void* payload, unsigned int payloadSize,
    unsigned char packetType, int                    )
{
    if ((m_flags&1u) == 0u)return E_FAIL;
    if ((m_flags&8u) == 0u)return 0;
    if (m_noPlayers == 0u)
    {
        DirectPlayNoPlayersMessage();
        return 0;
    }
    return DirectPlaySendPacket(this, payload, payloadSize, packetType, -1);
}

#if !defined(_MSC_VER) && !defined(__stdcall)
#define __stdcall
#endif


static void CopyDirectPlayQueueBytes(void* destination, const void* source, unsigned int byteCount)
{
    std::memmove(destination, source, byteCount);
}
static void EnterDirectPlayQueueCriticalSection()
{
    EnterCriticalSection(&g_csHostList);
}
static void LeaveDirectPlayQueueCriticalSection()
{
    LeaveCriticalSection(&g_csHostList);
}
static void DirectPlayQueueDiagnostic(const char* messageText, int errorCode)
{
    STRING message;
    STRING categoryText;
    message = messageText;
    categoryText = "NET";

    MYERROR* log =
        ::Error;
    log->Error(categoryText, E_ERROR, message, errorCode);

}


void NET::AddReceivedData(
    void* payload, unsigned int payloadBytes,
    unsigned long senderPlayerId, unsigned char packetType)
{
    NET* const network = ::Net;
    const unsigned int senderId = static_cast<unsigned int>(senderPlayerId);
    if ((network->m_flags&1u) == 0u || payloadBytes>1024u)return;
    while ((static_cast<const volatile unsigned int&>(network->m_flags)&0x80u) != 0u)
    {
    }
    network->m_flags |= 0x80u;

    CopyDirectPlayQueueBytes(network->m_receiveData[network->m_receiveWrite], payload, payloadBytes);
    network->m_receiveSize[network->m_receiveWrite] = payloadBytes;
    network->m_receiveType[network->m_receiveWrite] = packetType;
    unsigned int senderIndex = 0u;
    for (;senderIndex<8u;++senderIndex)if (network->m_players[senderIndex].m_dpnid == senderId)break;
    if (senderIndex >= 8u)
    {
        DirectPlayQueueDiagnostic("unknown sender", static_cast<int>(senderId));
        network->m_flags &= 0xFFFFFF7Fu;
        return;
    }
    network->m_receiveSender[network->m_receiveWrite] = senderIndex;
    unsigned int writeIndex = network->m_receiveWrite+1u;
    if (writeIndex >= 64u)writeIndex = 0u;
    network->m_receiveWrite = writeIndex;
    if (writeIndex == network->m_receiveRead)
    {
        ++network->m_receiveRead;
        if (network->m_receiveRead >= 64u)network->m_receiveRead = 0u;
        DirectPlayQueueDiagnostic("Received messages stack is full", static_cast<int>(writeIndex));
    }
    network->m_flags &= 0xFFFFFF7Fu;
}


int NET::Receive(
    void* output, unsigned int expectedSize, int senderIndex)
{
    if ((m_flags&1u) == 0u)return 1;
    while ((static_cast<const volatile unsigned int&>(m_flags)&0x80u) != 0u)
    {
    }
    unsigned int matchedIndex = m_receiveRead;
    if (matchedIndex == static_cast<const volatile unsigned int&>(m_receiveWrite))return 1;
    for (;;)
    {
        if ((senderIndex == -1 || senderIndex == static_cast<int>(m_receiveSender[matchedIndex])) && m_receiveType[matchedIndex] == 0u)break;
        ++matchedIndex;
        if (matchedIndex >= 64u)matchedIndex = 0u;
        if (matchedIndex == static_cast<const volatile unsigned int&>(m_receiveWrite))return 1;
    }
    if (expectedSize != m_receiveSize[matchedIndex])
    {
        DirectPlayQueueDiagnostic("Incorrect order message", 0);
        return 1;
    }
    EnterDirectPlayQueueCriticalSection();
    CopyDirectPlayQueueBytes(output, m_receiveData[matchedIndex], expectedSize);
    while (matchedIndex != m_receiveRead)
    {
        const unsigned int previousIndex = matchedIndex == 0u?63u:matchedIndex-1u;
        CopyDirectPlayQueueBytes(m_receiveData[matchedIndex], m_receiveData[previousIndex], m_receiveSize[previousIndex]);
        m_receiveSize[matchedIndex] = m_receiveSize[previousIndex];
        m_receiveSender[matchedIndex] = m_receiveSender[previousIndex];
        m_receiveType[matchedIndex] = m_receiveType[previousIndex];
        matchedIndex = previousIndex;
    }
    ++m_receiveRead;
    if (m_receiveRead >= 64u)m_receiveRead = 0u;
    LeaveDirectPlayQueueCriticalSection();
    return 0;
}






