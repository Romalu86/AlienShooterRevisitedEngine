#include "mylib.h"
#include "engine_globals.h"

#include "cannon.h"
#include "vid.h"
#include "constant.h"
#include "basetype.h"
#include <cmath>
#include <new>

int SPRITE::IsXYCross(const VID* otherVid, const VECTOR& otherCoor, ANGLE otherDirection) const
{
    const float extentX = otherVid->halfSizeXY.x+m_vid->halfSizeXY.x;
    const float extentY = otherVid->halfSizeXY.y+m_vid->halfSizeXY.y;
    const float dx = std::fabs(m_xyz.x-otherCoor.x);
    const float dy = std::fabs(m_xyz.y-otherCoor.y);

    const bool xInside = dx<extentX;
    const bool yInside = dy<extentY;
    if (!xInside || !yInside) return 0;
    return IsRotateXYCross(otherVid, otherCoor, otherDirection);
}

const VECTOR& SPRITE::Coor() const
{
    return m_xyz;
}

int VECTOR2::operator<(const VECTOR2& rhs) const
{
    return x<rhs.x && y<rhs.y ? 1 : 0;
}

#include "map.h"


CANNON::CANNON(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent)
    : SPRITE(vid, position, direction, parent), m_cannonMotionFlags(1)
{
    const float lo = m_vid->maxZSpeed;
    const float hi = m_vid->randomZSpeed;
    if (lo != hi)
    {
        const float range = hi-lo;
        m_zSpeed = ((m_vid->runtimeFlags&0x00001000u) != 0u ? ::Random(range) : ::Random2(range))+lo;
    }
    else if (parent && parent->Animation() >= 15 &&
        m_vid->topZ < 0.0f)
    {
        m_zSpeed = lo;
        m_speed = MaxSpeed()+reinterpret_cast<const SPRITE_LAYOUT_ACCESS*>(parent)->m_speed;
    }
    else m_zSpeed = lo;
    StartMove();
}


CANNON::~CANNON()
{
}


int CANNON::Action(int command, int argument1, int argument2, int argument3)
{
    if (command != ACT_NEXT_COMMAND) return SPRITE::Action(command, argument1, argument2, argument3);
    if (m_currentAnimation == 8)
    {
        ChangeAnimation(15);
        return 0;
    }
    if (m_currentAnimation<15)
    {
        if (m_xyz.z < -100.0f)
        {
            ChangeAnimation(16);
            return 0;
        }
        if (m_speed != 0.0f)
        {
            ChangeAnimation(2);
            return 0;
        }
        if (m_currentAnimation >= 7 && m_currentAnimation != 10) ChangeAnimation(0);
    }
    return 0;
}


void CANNON::DeletePointerToSprite(SPRITE* sprite)
{
    if (sprite && m_goalSprite == sprite)
    {
        const unsigned int goalStorageAddress = PtrToU32(CrtNew(104u));
        SPRITE* replacementGoal = 0;
        if (goalStorageAddress) replacementGoal = new (U32ToPtr(goalStorageAddress)) SPRITE(sprite->Coor(), ANGLE(0));
        SetGoal(replacementGoal);
    }
    SPRITE::DeletePointerToSprite(sprite);
}

static unsigned char CannonDesiredDirection(SPRITE* sprite, void* target, unsigned char add)
{
    unsigned char direction = sprite->DirectionTo(static_cast<SPRITE*>(target)).value;
    return static_cast<unsigned char>(direction+add);
}

static unsigned char CannonRotate(SPRITE* sprite, unsigned char desired, unsigned int elapsed)
{
    return sprite->RotateTact(ANGLE(desired), static_cast<int>(elapsed)).value;
}

void CANNON::MoveTact()
{
    if (m_vid->movementTactEnabled == 0) return;

    VECTOR next;
    MoveTactCalcCoor(&next);

    if ((m_runtimeFlags&0x00001000u) != 0u && m_currentAnimation<15)
        ChangeAnimation(15);

    const float groundHere = Map->GetGroundZ(VECTOR2(m_xyz.x, m_xyz.y));
    const float groundNext = Map->GetGroundZ(VECTOR2(next.x, next.y));
    const float oldZ = m_xyz.z;
    const float topNext = groundNext+m_vid->topZ;

    if (next.z <= groundNext && m_xyz.z >= groundHere)
    {
        next.z = m_xyz.z;
        if (m_currentAnimation >= 15)
        {
            Stop();
            m_zSpeed = 0.0f;
            if (groundNext>groundHere)
                (void) CreateChildFor(11, 0);
            next.x = m_xyz.x;
            next.y = m_xyz.y;
        }
        else if ((m_vid->property&0x10000000u) == 0u)
        {
            Stop();
            m_zSpeed = 0.0f;
            (void) CreateChildFor(groundNext>groundHere?11:12, 0);
            ChangeAnimation(15);
            next.x = m_xyz.x;
            next.y = m_xyz.y;
        }
        else
        {
            if (groundNext>groundHere)
            {
                ChangeAnimation(11);
                ChangeDirection(ANGLE(static_cast<unsigned char>(m_direction.value+128u)));
                next.x = m_xyz.x;
                next.y = m_xyz.y;
            }
            else
            {
                if (m_zSpeed < -0.022f)
                {
                    ChangeAnimation(12);
                    m_zSpeed *= -0.5f;
                    next.x = m_xyz.x;
                    next.y = m_xyz.y;
                }
                else if (m_zSpeed > 0.005f)
                {
                    m_zSpeed *= 0.5f;
                    m_speed *= 0.5f;
                    ChangeDirection(ANGLE(static_cast<unsigned char>(m_direction.value+128u)));
                    next.x = m_xyz.x;
                    next.y = m_xyz.y;
                }
                else
                {
                    m_zSpeed = 0.0f;
                    ChangeAnimation(15);
                    next.x = m_xyz.x;
                    next.y = m_xyz.y;
                }
            }
        }
    }
    else if (m_xyz.z != next.z &&
        (m_vid->property&0x6u) == 0u &&
        m_vid->topZ != 0.0f)
    {
        const float currentZ = m_xyz.z;
        if (topNext > currentZ)
        {
            if (!(topNext > next.z))
            {
                next.z = topNext;
                m_zSpeed = 0.0f;
            }
        }
        else if (currentZ > topNext)
        {
            if (!(topNext <= next.z))
            {
                next.z = topNext;
                m_zSpeed = 0.0f;
            }
        }
        else
        {
            if ((m_vid->property&0x08000000u) == 0u)
                m_zSpeed = 0.0f;
        }
    }

    if (m_xyz.x != next.x || m_xyz.y != next.y)
    {
        bool checkDestination = true;
        if (m_currentAnimation < 15)
        {
            const float moveDistance = VECTOR2(next.x-m_xyz.x, next.y-m_xyz.y).NearLength();
            if (moveDistance > 30.0f)
            {
                if (moveDistance > 50.0f)
                {
                    const VECTOR firstQuarter = (next-m_xyz)*0.25f+m_xyz;
                    if (CanPlaceWithCrush(firstQuarter))
                    {
                        ChangeAnimation(15);
                        checkDestination = false;
                    }
                    else
                    {
                        const VECTOR middle = (next+m_xyz)*0.5f;
                        if (CanPlaceWithCrush(middle))
                        {
                            ChangeCoor((next-m_xyz)*0.25f+m_xyz);
                            ChangeAnimation(15);
                            checkDestination = false;
                        }
                        else
                        {
                            const VECTOR thirdQuarter = (next-m_xyz)*0.75f+m_xyz;
                            if (CanPlaceWithCrush(thirdQuarter))
                            {
                                ChangeCoor((next+m_xyz)*0.5f);
                                ChangeAnimation(15);
                                checkDestination = false;
                            }
                            else
                                ChangeCoor((next-m_xyz)*0.75f+m_xyz);
                        }
                    }
                }
                else
                {
                    const VECTOR middle = (next+m_xyz)*0.5f;
                    if (CanPlaceWithCrush(middle))
                    {
                        ChangeAnimation(15);
                        checkDestination = false;
                    }
                    else
                        ChangeCoor((next+m_xyz)*0.5f);
                }
            }
        }

        if (checkDestination && CanPlaceWithCrush(next) != 0)
        {
            if (m_currentAnimation<15) ChangeAnimation(15);
            next.z = m_xyz.z;
        }
        else if (checkDestination)
        {
            const VECTOR2& mapSize = Map->SizeXY();
            const bool normalMap = next.x >= 0.0f && next.y >= 0.0f &&
                next.x < mapSize.x && next.y < mapSize.y;

            ChangeCoor(next);
            if (!normalMap && m_currentAnimation<15 && (m_vid->property&0x6u) == 0u)
            {
                const VECTOR2& mapSize = Map->SizeXY();
                const float maxX = mapSize.x+200.0f;
                const float maxY = mapSize.y+200.0f;
                if (!(m_xyz.x > -200.0f && m_xyz.y > -200.0f &&
                    m_xyz.x <= maxX && m_xyz.y <= maxY))
                    ChangeAnimation(15);
            }
        }
    }

    if (m_xyz.z != next.z)
        ChangeCoor(VECTOR(m_xyz.x, m_xyz.y, next.z));

    SPRITE* const target = m_goalSprite;
    if (!target)
    {
        if (!(m_vid->topZ < 0.0f && m_zSpeed < 0.0f))
            return;
        const unsigned int elapsed = ::CurrentTime-::PrevCurrentTime;
        (void) CannonRotate(this, static_cast<unsigned char>(m_direction.value+32u), elapsed);
        return;
    }

    const VECTOR& targetCoor = target->Coor();
    VECTOR2 delta(targetCoor.x-m_xyz.x, targetCoor.y-m_xyz.y);
    const float distance = delta.NearLength();

    if ((m_vid->property&0x08000000u) != 0u)
    {
        if ((m_cannonMotionFlags&1u) != 0u)
        {
            const double terrainTop = static_cast<double>(Map->GetGroundZ(
                VECTOR2(m_xyz.x, m_xyz.y)))+m_vid->topZ;
            if ((terrainTop>m_xyz.z || targetCoor.z>m_xyz.z) && m_zSpeed>0.0f)
            {
                const unsigned int elapsed = ::CurrentTime-::PrevCurrentTime;
                CONSTANT* const config = ::Const;
                m_zSpeed = static_cast<float>(static_cast<double>(m_zSpeed)-
                    static_cast<double>(elapsed)*config->Gravitation);
            }
            else
                m_cannonMotionFlags &= ~1u;
        }
        else
        {
            const unsigned char halfTurn = m_speed < 0.0f ? 128u : 0u;
            const unsigned char desired = CannonDesiredDirection(this, target, halfTurn);
            const unsigned int elapsed = ::CurrentTime-::PrevCurrentTime;
            const unsigned char remaining = CannonRotate(this, desired, elapsed);
            if (distance>10.0f && distance<30.0f && remaining>70u)
            {
                m_cannonMotionFlags |= 1u;
                StartMove();
            }
            else if (targetCoor.z < m_xyz.z &&
                distance != 0.0f)
            {
                float zSpeed = static_cast<float>((static_cast<double>(targetCoor.z)-
                    m_xyz.z)/distance/10.0);
                const float lower = -m_vid->maxZSpeed;
                if (zSpeed < lower) zSpeed = lower;
                m_zSpeed = zSpeed;
            }
            else
                m_zSpeed = 0.0f;
        }
    }
    else if (distance>100.0f)
    {
        const unsigned char oldDirection = m_direction.value;
        const unsigned char halfTurn = m_speed < 0.0f ? 128u : 0u;
        const unsigned char desired = CannonDesiredDirection(this, target, halfTurn);
        const unsigned int elapsed = ::CurrentTime-::PrevCurrentTime;
        (void) CannonRotate(this, desired, elapsed);
        if (AbsDiffU8(oldDirection, m_direction.value)>100u)
        {
            Stop();
            m_zSpeed = 0.0f;
            ChangeAnimation(15);
            return;
        }
        return;
    }

    bool testOverlap = ((m_runtimeFlags&SPRITE::FlagReachedX) != 0u && (m_runtimeFlags&SPRITE::FlagReachedY) != 0u);
    if (!testOverlap)
        testOverlap = IsXYCross(target->Vid(), targetCoor, target->Direction()) != 0;

    if (testOverlap)
    {
        const VID* const targetOwner = target->Vid();
        const float selfZ = m_xyz.z;
        const float targetZ = targetCoor.z;
        const double selfTop = static_cast<double>(selfZ)+m_vid->sizeXYZ.z;
        const float targetTop = targetZ+targetOwner->sizeXYZ.z;
        if (targetZ <= selfTop && selfZ <= targetTop)
        {
            Stop();
            m_zSpeed = 0.0f;
            ChangeAnimation(15);
            return;
        }

        if (oldZ<selfZ)
        {
            if (targetZ >= oldZ && targetZ <= selfZ)
            {
                Stop();
                m_zSpeed = 0.0f;
                ChangeAnimation(15);
                return;
            }
        }
        else if (targetZ >= selfZ && targetZ <= oldZ)
        {
            Stop();
            m_zSpeed = 0.0f;
            ChangeAnimation(15);
            return;
        }
    }

    if ((m_vid->property&0x08000000u) == 0u) return;

    if (std::fabs(targetCoor.x-m_xyz.x) < 10.0f &&
        std::fabs(targetCoor.y-m_xyz.y) < 10.0f &&
        std::fabs(targetCoor.z-m_xyz.z) < 20.0f)
    {
        Stop();
        m_zSpeed = 0.0f;
        ChangeAnimation(15);
    }
}

extern "C" void*  CannonVectorDeletingDestructorAlias(void* self, unsigned int flags)
{
    CANNON* object = static_cast<CANNON*>(self);
    object->CANNON::~CANNON();
    if (flags&1u) CrtFreePointer(self);
    return self;
}

