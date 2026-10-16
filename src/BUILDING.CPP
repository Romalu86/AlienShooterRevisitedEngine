#include "mylib.h"
#include "engine_globals.h"
#include "basetype.h"
#include "building.h"
#include "map.h"
#include "vid.h"
#include "sprite_collector.h"
#include <new>

BUILDING::BUILDING(VID* vid, const VECTOR& position, ANGLE direction, SPRITE* parent)
    : UNIT(vid, position, direction, parent), m_buildVid(0), m_buildRetryCount(0),
      m_buildStateC0(0), m_buildStateC4(0)
{
}

BUILDING::~BUILDING()
{
}
void BUILDING::MoveTact()
{
}

static VECTOR BuildingScaleRaw(const SPRITE* sprite)
{
    EX_SPRITE_DATA* const extraData = sprite->ExData();
    return extraData ? extraData->scale : VECTOR(1.0f, 1.0f, 1.0f);
}

static GAMMA BuildingGammaRaw(const SPRITE* sprite)
{
    GAMMA result;
    EX_SPRITE_DATA* const extraData = sprite->ExData();
    result.modulate = extraData ? extraData->gammaModulate : 0u;
    result.add = extraData ? extraData->gammaAdd : 0u;
    return result;
}

int BUILDING::Action(int action, int argument1, int argument2, int argument3)
{
    switch (action)
    {
    case ACT_BACKUP_COMMAND:
            if ((m_runtimeFlags&0x7Cu) == 0x40u && m_buildVid)
        {
            AddActionImmediate(0, ACT_BUILD_UNIT, m_buildVid->nVid, 0, 0);
            SetCommand(0, static_cast<SPRITE*>(0));
            return 0;
        }
        return UNIT::Action(action, argument1, argument2, argument3);

    case ACT_BUILD_UNIT:
        {
            int vidIndex = argument1;
            if (vidIndex == 0)
                vidIndex = Action(ACT_GET_ITEM_TYPE, 8, 0, 0);
            if (vidIndex>0 && (m_runtimeFlags&0x7Cu) != 0u)
                return 0;

            VID* buildVid = 0;
            if (vidIndex >= 0 && vidIndex<Map->VidCount())
                buildVid = Map->StoredVid(vidIndex);
            if (!buildVid)
            {
                Error(E_CREATE, STRING("in ACT_BUILD_UNIT"), static_cast<unsigned long>(vidIndex));
                return 0;
            }

            m_buildVid = buildVid;
            m_buildX = argument2;
            m_buildY = argument3;
            if (m_buildX<0)
            {
                const int radius = BitsToSigned32(0u-static_cast<unsigned int>(m_buildX));
                m_buildX = static_cast<double>(buildVid->halfSizeXY.x)<static_cast<double>(radius)
                    ? TruncateDoubleViaCrtToInt32(static_cast<double>(m_buildX)+buildVid->halfSizeXY.x):0;
            }
            if (m_buildY<0)
            {
                const int radius = BitsToSigned32(0u-static_cast<unsigned int>(m_buildY));
                m_buildY = static_cast<double>(buildVid->halfSizeXY.y)<static_cast<double>(radius)
                    ? TruncateDoubleViaCrtToInt32(static_cast<double>(m_buildY)+buildVid->halfSizeXY.y):0;
            }
            m_stateTimer = buildVid->weapon->BuildTime;
            SetCommand(0x10, static_cast<SPRITE*>(0));
            if (m_currentAnimation<15 && m_vid->noAnimCadr[1] != 0)
                ChangeAnimation(1);

            if (Map->IsNetGameStart() && ((m_vid->runtimeFlags^m_buildVid->runtimeFlags)&0x1000u) != 0u)
                Error(E_CREATE, STRING("(synchro)in ACT_BUILD_UNIT разные propImportantForSync"),
                    static_cast<unsigned long>(vidIndex));
            return 0;
        }

    case ACT_NEXT_COMMAND:
        {
            if (m_currentAnimation >= 15)
                return 0;
            if ((m_runtimeFlags&0x7Cu) == 0x40u && m_vid->noAnimCadr[1] != 0)
                ChangeAnimation(1);
            else if (m_currentAnimation != 10)
                ChangeAnimation(0);

            if ((m_runtimeFlags&0x7Cu) != 0x40u || m_stateTimer != 0u || !m_buildVid)
                return 0;

            VECTOR position;
            position.x = static_cast<float>(m_buildX);
            position.y = static_cast<float>(m_buildY);
            if (m_buildX == 0 && m_buildY == 0)
            {
                position.x = m_xyz.x;
                position.y = m_xyz.y;
            }
            if (m_buildX<0)
            {
                const float buildOffset = static_cast<float>(m_buildX);
                const float origin = m_xyz.x;
                const int radius = BitsToSigned32(0u-static_cast<unsigned int>(m_buildX));
                const int random = ((m_vid->runtimeFlags&0x1000u) != 0u)
                    ? ::Random(radius) : ::Random2(radius);
                const int randomOffset = BitsToSigned32(static_cast<unsigned int>(random)*2u);
                position.x = static_cast<float>(static_cast<double>(origin)
                    -buildOffset-randomOffset);
            }
            if (m_buildY<0)
            {
                const float origin = m_xyz.y;
                const int radius = BitsToSigned32(0u-static_cast<unsigned int>(m_buildY));
                const int random = ((m_vid->runtimeFlags&0x1000u) != 0u)
                    ? ::Random(radius) : ::Random2(radius);
                const int randomOffset = BitsToSigned32(static_cast<unsigned int>(random)*2u);
                position.y = static_cast<float>(static_cast<double>(origin)-m_buildY-randomOffset);
            }
            position.z = Z();

            const VECTOR2 halfSize = m_buildVid->halfSizeXY;
            const VECTOR2 corners[4] =
            {
                VECTOR2(position.x-halfSize.x, position.y-halfSize.y),
                VECTOR2(position.x-halfSize.x, position.y+halfSize.y),
                VECTOR2(position.x+halfSize.x, position.y-halfSize.y),
                VECTOR2(position.x+halfSize.x, position.y+halfSize.y)
            };
            for (int corner = 0; corner<4; ++corner)
            {
                const float ground = Map->GetGroundZ(corners[corner]);
                if (ground>position.z)
                    position.z = ground;
            }

            SPRITE_COLLECTOR* const collector = ::Sprites;
            SPRITE* const blocker = collector->CanPlace(m_buildVid, position, m_direction);
            if (blocker)
            {
                m_buildRetryCount = BitsToSigned32(static_cast<unsigned int>(m_buildRetryCount)+1u);
                if (m_buildRetryCount>5)
                {
                    m_stateTimer = 100u;
                    if (m_buildRetryCount>30 && (blocker->Vid()->spriteType&8u) == 0u)
                        blocker->ChangeAnimation(15);
                }
                return 0;
            }

            m_buildRetryCount = 0;
            SetCommand(0, static_cast<SPRITE*>(0));
            SPRITE* created = Map->CreateSprite(m_buildVid, position, m_direction, this);
            if (created)
            {
                if (ActionStackHaveCommand(0, ACT_STOP_STACK))
                    (void) Action(ACT_COPY_STACK_TO, 0, static_cast<int>(PtrToU32(created)), 0);

                if ((m_vid->property&0x00800000u) != 0u)
                {
                    const VECTOR selfScale = BuildingScaleRaw(this);
                    const VECTOR childScale = BuildingScaleRaw(created);
                    created->SetScale(VECTOR(selfScale.x*childScale.x,
                        selfScale.y*childScale.y,
                        selfScale.z*childScale.z));

                    GAMMA gamma = BuildingGammaRaw(created);
                    if (gamma.modulate == 0u && gamma.add == 0u)
                        created->SetGamma(BuildingGammaRaw(this));

                    EX_SPRITE_DATA* const extraData = m_extraData;
                    if (extraData && extraData->numberInName >= 0)
                        created->SetNumberInName(extraData->numberInName);
                }
            }
            m_buildVid = 0;
            ChangeAnimation(0);
            return 0;
        }

    case ACT_SAVE:
            if (!ActionStackHaveCommand(0, ACT_STOP_STACK) && ActionStackHaveCommand(0, ACT_MOVE))
            AddActionImmediate(0, ACT_STOP_STACK, 0, 0, 0);
        (void) UNIT::Action(action, argument1, argument2, argument3);
        return 0;

    default:
            return UNIT::Action(action, argument1, argument2, argument3);
    }
}


