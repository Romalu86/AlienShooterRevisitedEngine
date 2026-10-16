
#include "rail.h"
#include "railway.h"
#include "railway.h"
#include "vid.h"
#include "basetype.h"
#include <new>

RAIL::RAIL(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent)
    : TERRAIN(vid, position, direction, parent), m_dotA(0), m_dotB(0)
{
    (void)Action(ACT_CHANGE_DIRECTION, static_cast<int>(direction.value), 0, 0);
}

RAIL::~RAIL()
{
    if (m_dotA)
        m_dotA->Release();
    if (m_dotB)
        m_dotB->Release();
}

void RAIL::UnBreak(R_DOT* dot)
{
    if (dot != m_dotA && dot != m_dotB)
        return;
    VID* const railVid = m_vid;
    if (!railVid->childVid[15])
        return;
    const int unbrokenAnimation = railVid->nChildVid[15];
    if (unbrokenAnimation >= railVid->nVid)
        return;
    (void)Action(ACT_CHANGE_VID, unbrokenAnimation, 0, 0);
    const unsigned int armyIndex = (m_runtimeFlags>>SPRITE::ArmyShift)&7u;
    ChangeHp(m_vid->maxHpByPlayer[armyIndex]);
}
namespace
{
const float railFirstX[12] = {0.0f, 1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f};
const float railFirstY[12] = {-1.0f, 0.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f};
const float railSecondX[12] = {0.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 1.0f, 1.0f};
const float railSecondY[12] = {1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
unsigned int railHeightOffsetsInitialized = 0;
float railFirstZ[4] = {};
float railSecondZ[4] = {};
}

int RAIL::Action(int command, int directionArgument, int argument2, int argument3)
{
    unsigned int initializedOffsets = railHeightOffsetsInitialized;
    VID* railVid = m_vid;
    if ((initializedOffsets&1u) == 0u)
    {
        initializedOffsets |= 1u;
        railHeightOffsetsInitialized = initializedOffsets;
        railFirstZ[1] = railVid->sizeXYZ.z;
        railFirstZ[2] = 0.0f;
        railFirstZ[3] = 0.0f;
    }
    if ((initializedOffsets&2u) == 0u)
    {
        initializedOffsets |= 2u;
        railHeightOffsetsInitialized = initializedOffsets;
        railSecondZ[2] = railVid->sizeXYZ.z;
        railSecondZ[3] = 0.0f;
    }
    const float ownerBaseZ = railVid->topZ;
    if (command != ACT_CHANGE_DIRECTION && command != ACT_DAMAGE)
        return TERRAIN::Action(command, directionArgument, argument2, argument3);
    if (command == ACT_DAMAGE)
    {
        (void)SPRITE::Action(ACT_DAMAGE, directionArgument, argument2, argument3);
        if (m_currentAnimation<15)
            return 0;
        railVid = m_vid;
        VID* const linkedVid = railVid->childVid[15];
        if (!linkedVid || linkedVid->spriteClass != 22)
            return 0;
        if (railVid->nChildVid[15]>railVid->nVid)
        {
            m_dotA->m_broken = 1u;
            m_dotB->m_broken = 1u;
        }
        (void)Action(ACT_CHANGE_VID, railVid->nChildVid[15], 0, 0);
        const unsigned int armyIndex = (m_runtimeFlags>>SPRITE::ArmyShift)&7u;
        ChangeHp(m_vid->maxHpByPlayer[armyIndex]);
        return 0;
    }
    ChangeDirection(ANGLE(static_cast<unsigned char>(static_cast<unsigned int>(static_cast<unsigned char>(directionArgument)))));
    R_DOT* dotA = m_dotA;
    if (dotA)
        dotA->Release();
    R_DOT* dotB = m_dotB;
    if (dotB)
        dotB->Release();
    const unsigned int railDirection = static_cast<unsigned int>(m_vid->RealDirection(m_direction.value));
    const unsigned int elevationIndex = railDirection/12u;
    const unsigned int directionIndex = railDirection%12u;
    R_MAP* const railMap = RailMap.Address();
    float railX = m_xyz.x, railY = m_xyz.y;
    railVid = m_vid;
    const volatile float endpointAZ = static_cast<float>((static_cast<double>(m_xyz.z)+railFirstZ[elevationIndex])+ownerBaseZ);
    const volatile float endpointAY = static_cast<float>(static_cast<double>(railVid->sizeXYZ.y)*railFirstY[directionIndex]*0.25f+railY);
    const volatile float endpointAX = static_cast<float>(static_cast<double>(railVid->sizeXYZ.x)*railFirstX[directionIndex]*0.25f+railX);
    m_dotA = railMap->CreateDot(endpointAX, endpointAY, endpointAZ);
    railX = m_xyz.x;
    railY = m_xyz.y;
    railVid = m_vid;
    const volatile float endpointBZ = static_cast<float>((static_cast<double>(m_xyz.z)+railSecondZ[elevationIndex])+ownerBaseZ);
    const volatile float endpointBY = static_cast<float>(static_cast<double>(railVid->sizeXYZ.y)*railSecondY[directionIndex]*0.25f+railY);
    const volatile float endpointBX = static_cast<float>(static_cast<double>(railVid->sizeXYZ.x)*railSecondX[directionIndex]*0.25f+railX);
    m_dotB = railMap->CreateDot(endpointBX, endpointBY, endpointBZ);
    railVid = m_vid;
    const volatile float linkAY = static_cast<float>(static_cast<double>(railVid->sizeXYZ.y)*railFirstY[directionIndex]*0.75f+m_xyz.y);
    const volatile float linkAX = static_cast<float>(static_cast<double>(railVid->sizeXYZ.x)*railFirstX[directionIndex]*0.75f+m_xyz.x);
    dotA = m_dotA;
    const volatile float linkAZValue = static_cast<float>((static_cast<double>(m_xyz.z)+railFirstZ[elevationIndex])+ownerBaseZ);
    const int linkAZ = TruncateFloatViaCrtToInt32(linkAZValue);
    const int aLinkY = TruncateFloatViaCrtToInt32(linkAY);
    const int aLinkX = TruncateFloatViaCrtToInt32(linkAX);
    R_DOT* const linkA = railMap->GetDot(aLinkX, aLinkY, linkAZ);
    dotA->Link(linkA);
    railVid = m_vid;
    const volatile float linkBY = static_cast<float>(static_cast<double>(railVid->sizeXYZ.y)*railSecondY[directionIndex]*0.75f+m_xyz.y);
    const volatile float linkBX = static_cast<float>(static_cast<double>(railVid->sizeXYZ.x)*railSecondX[directionIndex]*0.75f+m_xyz.x);
    dotB = m_dotB;
    const volatile float linkBZValue = static_cast<float>((static_cast<double>(m_xyz.z)+railSecondZ[elevationIndex])+ownerBaseZ);
    const int linkBZ = TruncateFloatViaCrtToInt32(linkBZValue);
    const int bLinkY = TruncateFloatViaCrtToInt32(linkBY);
    const int bLinkX = TruncateFloatViaCrtToInt32(linkBX);
    R_DOT* const linkB = railMap->GetDot(bLinkX, bLinkY, linkBZ);
    dotB->Link(linkB);
    m_dotB->Link(m_dotA);
    railVid = m_vid;
    if (railVid->nChildVid[15]<railVid->nVid)
    {
        m_dotA->m_broken = 1u;
        m_dotB->m_broken = 1u;
    }
    return 0;
}

