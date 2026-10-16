
#include "myerror.h"
#include "engine_globals.h"
#include <new>
#include <cmath>
#include "menu.h"
#include "mylib.h"
#include "script.h"
#include "sprite.h"
#include "vid.h"
#include "resread.h"
#include "input.h"
#include "sprite_list.h"
#include "graph.h"
#include "map.h"

namespace {
SPRITE* pressedMenuSprite = 0;

int MenuReferenceHeight(int width)
{
    return width == 1024 ? 768 : 600;
}

void MenuAnchorSprite(SPRITE* sprite, int referenceWidth)
{
    if (referenceWidth == 0)
        return;

    const float halfWidth = Graph->screenWidth*0.5f;
    const float screenX = sprite->X()-Map->ShiftXY().x;
    const double dx = static_cast<double>(halfWidth)-static_cast<double>(static_cast<float>(referenceWidth))*0.5;
    const float x = static_cast<float>(screenX < halfWidth
        ? static_cast<double>(sprite->X())-dx : static_cast<double>(sprite->X())+dx);
    sprite->ChangeCoor(VECTOR(x, sprite->Y(), sprite->Z()));

    const float halfHeight = Graph->screenHeight*0.5f;
    const float screenY = static_cast<float>(static_cast<double>(sprite->Y())-sprite->Z()-Map->ShiftXY().y);
    const double dy = static_cast<double>(halfHeight)-static_cast<double>(MenuReferenceHeight(referenceWidth))*0.5;
    const float y = static_cast<float>(screenY < halfHeight
        ? static_cast<double>(sprite->Y())-dy : static_cast<double>(sprite->Y())+dy);
    sprite->ChangeCoor(VECTOR(sprite->X(), y, sprite->Z()));
}

void MenuAnchorDeletedPosition(VECTOR& authored, const VECTOR2& translation, int referenceWidth)
{
    if (referenceWidth == 0)
        return;

    const float halfWidth = Graph->screenWidth*0.5f;
    float screenX = authored.x+translation.x;
    screenX -= Map->ShiftXY().x;
    const double dx = static_cast<double>(halfWidth)-static_cast<double>(static_cast<float>(referenceWidth))*0.5;
    authored.x = static_cast<float>(screenX < halfWidth
        ? static_cast<double>(authored.x)-dx : static_cast<double>(authored.x)+dx);

    const float halfHeight = Graph->screenHeight*0.5f;
    float screenY = authored.y+translation.y;
    const float z = authored.z+0.0f;
    screenY -= z;
    screenY -= Map->ShiftXY().y;
    const double dy = static_cast<double>(halfHeight)-static_cast<double>(MenuReferenceHeight(referenceWidth))*0.5;
    authored.y = static_cast<float>(screenY < halfHeight
        ? static_cast<double>(authored.y)-dy : static_cast<double>(authored.y)+dy);
}
}

STRING SPRITE::GetName() const
{
    STRING emptyName;
    const STRING* nameSource = 0;

    EX_SPRITE_DATA* const extraData = ExtraData();
    if (extraData)
    {
        nameSource = &extraData->nameText;
    }
    else
    {
        emptyName = StringGlobals::EmptyText();
        nameSource = &emptyName;
    }

    return STRING(*nameSource);
}

int VID::RealDirection(unsigned char direction)
{
    const unsigned int wrapped = (static_cast<unsigned int>(directionQuantizationOffset)+static_cast<unsigned int>(direction))&0xFFu;
    return static_cast<int>((wrapped*static_cast<unsigned int>(noDir))>>8);
}


#include "region.h"

VECTOR2 MAP::ToScreen(const VECTOR& worldCoor) const
{
    VECTOR2 result;
    result.x = worldCoor.x-m_shiftXY.x;

    result.y = static_cast<float>(static_cast<double>(worldCoor.y)-static_cast<double>(worldCoor.z)
        -static_cast<double>(m_shiftXY.y));
    return result;
}

int MENU::NVidUnderCursor() const
{
    if (!m_selectedSprite) return 0;
    VID* const vid = m_selectedSprite->Vid();
    return vid->nVid;
}


int MENU::NDirUnderCursor() const
{
    if (!m_selectedSprite) return 0;
    VID* const vid = m_selectedSprite->Vid();
    return vid->RealDirection(m_selectedSprite->Direction().value);
}


#include "basetype.h"

MENU::MENU()
{
    m_controlFlags = *reinterpret_cast<volatile unsigned int*>(&m_controlFlags)&0xFFFFFFFCu;
    m_selectedSprite = 0;
}


void MENU::Error(TYPE_ERROR type, const STRING& subject, unsigned long code)
{
    const STRING category("MENU");
        ::Error->Error(category, type, subject, code);
}

SPRITE* MENU::SpriteWithName(const STRING& name) const
{
    for (int spriteIndex = 0; spriteIndex<m_count; ++spriteIndex)
    {
        bool equal;
        {
            const STRING childName = m_data[spriteIndex]->GetName();
            equal = std::strcmp(childName.value, name.value) == 0;
        }
        if (equal) return m_data[spriteIndex];
    }
    return 0;
}

bool MenuSpriteContainsPoint(const SPRITE* sprite, const VECTOR2& cursor)
{
    const VID* const vid = sprite->Vid();
    const double screenX = static_cast<double>(sprite->X());
    const double screenY = static_cast<double>(sprite->Y())-static_cast<double>(sprite->Z());
    return screenX-static_cast<double>(vid->halfSizeXY.x)<=static_cast<double>(cursor.x) &&
        static_cast<double>(cursor.x)<=screenX+static_cast<double>(vid->halfSizeXY.x) &&
        screenY-static_cast<double>(vid->sizeXYZ.z)-static_cast<double>(vid->halfSizeXY.y)<static_cast<double>(cursor.y) &&
        screenY+static_cast<double>(vid->halfSizeXY.y)>static_cast<double>(cursor.y);
}

void MENU::Control(INPUT* input)
{
    const VECTOR2 cursor = input->ClientMouseXY()+Map->ShiftXY();

    m_selectedSprite = 0;
    m_controlFlags &= ~3u;

    int spriteIndex = 0;
    if (m_count != 0) do
        {
            SPRITE* const sprite = m_data[spriteIndex];
            if (!sprite) break;

            VID* const vid = sprite->Vid();
            if (vid->moveMask == 0u) continue;

            const int animation = sprite->Animation();
            if (animation == 14 || animation >= 15 || animation == 13 || animation == 12)
                continue;

            const bool cursorInsideSprite = MenuSpriteContainsPoint(sprite, cursor);

            if (!cursorInsideSprite || (m_selectedSprite && !(m_selectedSprite->Z()<sprite->Z())))
            {
                sprite->ChangeAnimation(animation&1);
                continue;
            }

            if (m_selectedSprite)
                m_selectedSprite->ChangeAnimation(m_selectedSprite->Animation()&1);
            m_selectedSprite = sprite;
        }
        while (++spriteIndex<m_count);

    if (input->GetAction(IM_LBUTTONCLICK) != 0 || input->GetAction(IM_RBUTTONCLICK) != 0)
        pressedMenuSprite = m_selectedSprite;

    if (m_selectedSprite)
    {
        const int originalAnimation = m_selectedSprite->Animation();

        if (pressedMenuSprite == m_selectedSprite && originalAnimation <= 7)
        {
            if (input->GetAction(IM_LBUTTONUP) != 0)
            {
                SPRITE* const selected = m_selectedSprite;
                VID* const vid = selected->Vid();
                if (vid->noAnimCadr[1] != 0 && vid->noAnimCadr[0] != 0)
                {
                    selected->ChangeAnimation(((~selected->Animation())&1) | 2);
                    if ((originalAnimation&~1) == 4)
                        m_selectedSprite->AddRuntimeFlags(SPRITE::FlagAnimationSoundPlayed);
                }
            }
            else if (input->GetAction(IM_LBUTTONCLICK) != 0)
            {
                SPRITE* const selected = m_selectedSprite;
                VID* const vid = selected->Vid();
                int nextAnimation;
                if (vid->noAnimCadr[10] != 0 && vid->noAnimCadr[11] != 0)
                    nextAnimation = (selected->X()>cursor.x)?10:11;
                else
                    nextAnimation = (selected->Animation()&1) | 4;
                selected->ChangeAnimation(nextAnimation);
            }
        }

        if (input->GetAction(IM_LBUTTONDOWN) == 0 && input->GetAction(IM_LBUTTONUP) == 0)
        {
            SPRITE* const selected = m_selectedSprite;
            const int currentAnimation = selected->Animation();
            bool mayAdvance = true;
            if ((currentAnimation&~1) == 4)
            {
                if (::CurrentTime-selected->LastAnimationTick() == 0u ||
                    selected->CurrentFrame() != selected->CurrentFrameEnd())
                    mayAdvance = false;
            }

            if (mayAdvance)
            {
                VID* const vid = selected->Vid();
                if (vid->noAnimCadr[8] != 0 && vid->noAnimCadr[9] != 0)
                {
                    selected->ChangeAnimation((selected->X()>cursor.x)?8:9);
                    if ((originalAnimation&~1) == 10)
                        m_selectedSprite->AddRuntimeFlags(SPRITE::FlagAnimationSoundPlayed);
                }
                else
                {
                    selected->ChangeAnimation((currentAnimation&1) | 2);
                    if ((originalAnimation&~1) == 4)
                        m_selectedSprite->AddRuntimeFlags(SPRITE::FlagAnimationSoundPlayed);
                }
            }
        }
        input->RemoveEvent(IM_LBUTTONCLICK, 0u);
        input->RemoveEvent(IM_RBUTTONCLICK, 0u);
        if (input->GetAction(IM_LBUTTONUP) != 0 &&
            pressedMenuSprite == m_selectedSprite)
            m_controlFlags |= 1u;
        if (input->GetAction(IM_RBUTTONUP) != 0 &&
            pressedMenuSprite == m_selectedSprite)
            m_controlFlags |= 2u;
    }
}

int MENU::Load(const STRING& fileName, VECTOR2 offset, int layoutWidth)
{
    RESOURCE resource;

    if (resource.OpenForRead(fileName, ResourceType::Menu) != 0)
    {
        Error(E_OPEN, fileName, 0u);
        return 1;
    }

    if (resource.GoBegin(ResourceType::Header) != 0)
    {
        STRING section("'HEAD'in menu");
        Error(E_SECTION, section, 0u);
        return 1;
    }

    int version;
    union HEADER_VALUE
    {
        float f;
        int i;
    }
    sizeX, sizeY, shiftX, shiftY;
    (void) resource.RESOURCE::Read(&version, 4);
    if (version >= 15)
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
    }
    else
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        sizeX.f = static_cast<float>(sizeX.i);
        sizeY.f = static_cast<float>(sizeY.i);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
        shiftX.f = static_cast<float>(shiftX.i);
        shiftY.f = static_cast<float>(shiftY.i);
    }

    const float screenHalfWidth = Graph->screenWidth*0.5f;
    const float screenHalfHeight = Graph->screenHeight*0.5f;
    const float menuHalfWidth = sizeX.f*0.5f;
    const float menuHalfHeight = sizeY.f*0.5f;
    float translationX = -shiftX.f;
    translationX -= menuHalfWidth;
    float translationY = -shiftY.f;
    translationY -= menuHalfHeight;
    translationX += screenHalfWidth;
    translationY += screenHalfHeight;
    translationX += offset.x;
    translationY = offset.y+translationY;

    SPRITE* const endSprite = reinterpret_cast<SPRITE*>(U32ToPtr(0xFFFFFFFFu));
    if (resource.GoNext(ResourceType::Sprites) == 0)
    {
        for (; ;)
        {
            SPRITE* sprite = Map->LoadSprite(&resource, version);
            if (sprite == endSprite) break;
            if (sprite)
            {
                const VECTOR position(sprite->X()+translationX, sprite->Y()+translationY, sprite->Z()+0.0f);
                sprite->ChangeCoor(position);
                MenuAnchorSprite(sprite, layoutWidth);
                (void) sprite->Action(ACT_RESTORE, static_cast<int>(PtrToU32(&resource)), version, 0);
            }
            (void) resource.GoNextSub(ResourceType::Sprites);
        }
        resource.Close();

        return 0;
    }

    if (resource.GoBegin(ResourceType::SpriteIndices) == 0)
    {
        for (; ;)
        {
            SPRITE* sprite = Map->LoadSprite(&resource, version);
            if (sprite == endSprite) break;
            if (sprite)
            {
                VECTOR translation(translationX, translationY, 0.0f);
                if (layoutWidth != 0)
                {
                    translation.x = -shiftX.f+offset.x;
                    translation.y = -shiftY.f+offset.y;
                    const double screenX = static_cast<double>(sprite->X())-shiftX.f;
                    if (screenX > Graph->screenWidth*0.5f)
                        translation.x = static_cast<float>(static_cast<double>(Graph->screenWidth)-sizeX.f+translation.x);
                    const double screenY = static_cast<double>(sprite->Y())-shiftY.f-sprite->Z();
                    if (screenY > Graph->screenHeight*0.5f)
                        translation.y = static_cast<float>(static_cast<double>(Graph->screenHeight)-sizeY.f+translation.y);
                }
                const VECTOR position(sprite->X()+translation.x, sprite->Y()+translation.y, sprite->Z()+translation.z);
                sprite->ChangeCoor(position);
            }
        }
        resource.Close();

        return 0;
    }

    STRING section("'SPR ' or 'SPRI' in menu");
    Error(E_SECTION, section, 0u);
    return 1;
}


int MENU::DeleteFromFile(const STRING& fileName, VECTOR2 offset, int layoutWidth)
{
    RESOURCE resource;
    if (resource.OpenForRead(fileName, ResourceType::Menu) != 0)
    {
        Error(E_OPEN, fileName, 0u);
        return 1;
    }

    if (resource.GoBegin(ResourceType::Header) != 0)
    {
        STRING section("'HEAD'in menu");
        Error(E_SECTION, section, 0u);
        return 1;
    }

    int version;
    union HEADER_VALUE
    {
        float f;
        int i;
    }
    sizeX, sizeY, shiftX, shiftY;
    (void) resource.RESOURCE::Read(&version, 4);
    if (version >= 15)
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
    }
    else
    {
        (void) resource.Read(&sizeX, 4);
        (void) resource.Read(&sizeY, 4);
        sizeX.f = static_cast<float>(sizeX.i);
        sizeY.f = static_cast<float>(sizeY.i);
        (void) resource.Read(&shiftX, 4);
        (void) resource.Read(&shiftY, 4);
        shiftX.f = static_cast<float>(shiftX.i);
        shiftY.f = static_cast<float>(shiftY.i);
    }

    if (resource.GoNext(ResourceType::Sprites) != 0)
    {
        STRING section("'SPR ' in MENU::DeleteFromFile");
        Error(E_SECTION, section, 0u);
        return 1;
    }

    GRAPH* const graph = Graph;
    const float screenHalfHeight = graph->screenHeight*0.5f;
    HEADER_VALUE persistentId, nvid;
    persistentId.f = graph->screenWidth*0.5f;
    const float menuHalfHeight = sizeY.f*0.5f;
    const float menuHalfWidth = sizeX.f*0.5f;
    nvid.f = -shiftX.f;
    nvid.f -= menuHalfWidth;
    float offsetY = -shiftY.f;
    offsetY -= menuHalfHeight;
    offsetY += screenHalfHeight;
    offsetY = offset.y+offsetY;
    float offsetX = nvid.f+persistentId.f;
    offsetX += offset.x;
    const double coordinateTolerance = 1.600000023841858;

    float authoredX, authoredY, authoredZ;
    for (; ;)
    {
        (void) resource.RESOURCE::Read(&persistentId, 4);
        if (persistentId.i == -1) break;

        (void) resource.RESOURCE::Read(&nvid, 4);
        (void) resource.Read(&authoredX, 4);
        (void) resource.Read(&authoredY, 4);
        (void) resource.Read(&authoredZ, 4);
        authoredX = Map->ShiftXY().x+authoredX;
        authoredY = Map->ShiftXY().y+authoredY;
        VECTOR authored(authoredX, authoredY, authoredZ);
        MenuAnchorDeletedPosition(authored, VECTOR2(offsetX, offsetY), layoutWidth);

        SPRITE_LIST* const spriteList = static_cast<SPRITE_LIST*>(this);
        for (int spriteIndex = 0; spriteIndex<spriteList->m_count; ++spriteIndex)
        {
            SPRITE* sprite = spriteList->m_data[spriteIndex];
            VID* vid = sprite->Vid();
            if (vid->nVid != nvid.i) continue;

            const VECTOR& loadedPosition = sprite->Coor();
            const float deltaX = loadedPosition.x-authored.x;
            const float deltaY = loadedPosition.y-authored.y;
            const float deltaZ = loadedPosition.z-authored.z;
            const float translatedX = deltaX-offsetX;
            const float translatedY = deltaY-offsetY;
            if (std::fabs(translatedX) <= coordinateTolerance &&
                std::fabs(translatedY) <= coordinateTolerance &&
                std::fabs(deltaZ) <= coordinateTolerance)
            {
                (void) spriteList->DeleteSpriteNumber(spriteIndex);
                --spriteIndex;
            }
        }

        (void) resource.GoNextSub(ResourceType::Sprites);
    }

    resource.Close();
    return 0;
}


