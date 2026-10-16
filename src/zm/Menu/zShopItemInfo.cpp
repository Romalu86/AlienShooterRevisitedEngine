#include "mylib.h"
#include "zShopItemInfo.h"
#include "zShopDlgMenu.h"
#include "sprite.h"
#include "frame.h"
#include "vid.h"
#include "map.h"
#include "graph.h"
#include "input.h"
#include "irpg.h"
#include "engine_globals.h"
#include "zm/Game/ScriptExec/zScriptExec.h"
#include "zm/Utils/String/zString.h"
#include <new>

zShopItemInfo::zShopItemInfo()
    : zBaseDlgMenu(), m_textSprite(0), m_auxiliarySprite(0), m_imageSprite(0), m_closeSprite(0),
      m_framework(0), m_inventoryKeeper(0), m_dialog(0), m_screenX(0), m_screenY(0),
      m_minX(0), m_minY(0), m_maxX(0), m_maxY(0), m_menuOffset(0.0f, 0.0f),
      m_curPage(0), m_pagesCount(0), m_pageTop(0.0f), m_unresolvedState(0), m_shownTime(0)
{
    for (int index = 0; index < 3; ++index)
    {
        m_buttons[index] = 0;
        m_buttonTexts[index] = 0;
    }
}

zShopItemInfo::~zShopItemInfo()
{
}

void* zShopItemInfo::QueryInterface(int id) const
{
    if (id == 27) return const_cast<zShopItemInfo*>(this);
    DebugAssert(5u, "0", "..\\src\\zm\\Menu\\zShopItemInfo.cpp", 70u);
    return 0;
}

int zShopItemInfo::iRelease()
{
    delete static_cast<zBaseDlgMenu*>(this);
    return 0;
}

void zShopItemInfo::SetRPGFrameWork(iRPGFrameWork* value)
{
    m_framework = value;
}

void zShopItemInfo::SetInventoryKeeper(iInvKeeper* value)
{
    m_inventoryKeeper = value;
}

SPRITE* zShopItemInfo::GetButton(int index)
{
    return m_buttons[index];
}

SPRITE* zShopItemInfo::GetButtonText(int index)
{
    return m_buttonTexts[index];
}

iRPGObj* zShopItemInfo::GetCurrentItem()
{
    return m_currentItem.m_object;
}

void zShopItemInfo::Initialize()
{
    m_currentItem.Set(0);
    m_secondaryItem.Set(0);
    m_curPage = 0;
}

void zShopItemInfo::SetScreenCoordinates(float x, float y)
{
    m_screenX = TruncateFloatViaCrtToInt32(x);
    m_screenY = AddSigned32Wrap(TruncateFloatViaCrtToInt32(y), 100);
}

void zShopItemInfo::SetBounds(int minX, int minY, int maxX, int maxY)
{
    m_minX = minX;
    m_minY = minY;
    m_maxX = maxX;
    m_maxY = maxY;
}

void zShopItemInfo::ShowObjInfo(iRPGObj* item, bool notify, iBaseDlgMenu* dialog)
{
    m_curPage = 0;
    ShowObjInfo(item, notify, dialog, false);
}

void zShopItemInfo::ShowObjInfo(iRPGObj* item, bool notify, iBaseDlgMenu* dialog, bool force)
{
    m_dialog = dialog;
    if (m_currentItem.m_object == item && !force) return;
    DeleteSprites();
    if (m_currentItem.m_object != item) m_pagesCount = 0;
    if (notify)
    {
        if (!m_framework)
            DebugAssert(5u, "m_pRPGFrameWork", "..\\src\\zm\\Menu\\zShopItemInfo.cpp", 287u);
        m_framework->PlaySFXCurArmy(47);
    }
    m_currentItem.Set(item);
    if (!m_currentItem.m_object) return;

    const int x = SubSigned32Wrap(m_screenX, TruncateFloatViaCrtToInt32(Graph->screenWidth) / 2);
    const int y = SubSigned32Wrap(m_screenY, TruncateFloatViaCrtToInt32(Graph->screenHeight) / 2);
    m_menuOffset = VECTOR2(static_cast<float>(x), static_cast<float>(y));
    if (Map->MenuState().SpriteWithName(STRING("IF_Btn_1")))
        DebugAssert(5u, "Map->Menu()->SpriteWithName(\"IF_Btn_1\") == 0",
            "..\\src\\zm\\Menu\\zShopItemInfo.cpp", 300u);
    Map->MenuState().Load(STRING("Maps\\item_menu.men"), m_menuOffset);
    Draw();
    m_shownTime = ::CurrentTime;
}

void zShopItemInfo::Refresh()
{
    ShowObjInfo(m_currentItem.m_object, false, m_dialog, true);
}

void zShopItemInfo::PageUp()
{
    if (m_curPage > 0) m_curPage = SubSigned32Wrap(m_curPage, 1);
    Refresh();
}

void zShopItemInfo::PageDown()
{
    const int page = m_curPage;
    if (page < SubSigned32Wrap(GetPagesCnt(), 1)) m_curPage = AddSigned32Wrap(page, 1);
    Refresh();
}

int zShopItemInfo::GetCurPageNum()
{
    return m_curPage;
}

int zShopItemInfo::GetPagesCnt()
{
    if (m_pagesCount > 0) return m_pagesCount;
    int pages = 1;
    if (m_textSprite && static_cast<double>(static_cast<STEXT*>(m_textSprite)->m_textHeight) + m_pageTop > 483.0)
        pages = 2;
    m_pagesCount = pages;
    return pages;
}

void zShopItemInfo::DeletePointerToSprite(SPRITE* sprite)
{
    if (m_textSprite == sprite) m_textSprite = 0;
    if (m_auxiliarySprite == sprite) m_auxiliarySprite = 0;
    if (m_imageSprite == sprite) m_imageSprite = 0;
    if (m_closeSprite == sprite) m_closeSprite = 0;
    for (int index = 0; index < 3; ++index)
    {
        if (m_buttons[index] == sprite) m_buttons[index] = 0;
        if (m_buttonTexts[index] == sprite) m_buttonTexts[index] = 0;
    }
}

void zShopItemInfo::DeleteSprites()
{
    for (int index = 1; index <= 3; ++index)
    {
        zString name;
        name.Format("IF_BtnText_%d", index);
        SPRITE* const sprite = Map->MenuState().SpriteWithName(STRING(static_cast<const char*>(name)));
        if (sprite) delete sprite;
    }
    if (m_imageSprite)
        Map->MenuState().DeleteFromFile(STRING("Maps\\item_menu.men"), m_menuOffset);
    if (m_textSprite)
    {
        delete m_textSprite;
        m_textSprite = 0;
    }
    if (m_auxiliarySprite)
    {
        delete m_auxiliarySprite;
        m_auxiliarySprite = 0;
    }
    if (m_imageSprite)
    {
        delete m_imageSprite;
        m_imageSprite = 0;
    }
}

void zShopItemInfo::Draw()
{
    if (!m_currentItem.m_object)
        DebugAssert(5u, "m_pItem", "..\\src\\zm\\Menu\\zShopItemInfo.cpp", 395u);
    m_imageSprite = Map->MenuState().SpriteWithName(STRING("IF_Img"));
    if (m_imageSprite)
    {
        const int vidNumber = SubSigned32Wrap(m_currentItem.m_object->GetVidNum(2), 1);
        int direction = m_currentItem.m_object->GetDirNum(2);
        VID* const vid = Map->StoredVid(vidNumber);
        if (vid)
        {
            direction = (MultiplySigned32Wrap(direction, 256) / vid->noDir) & 255;
            m_imageSprite->Action(ACT_CHANGE_VID, vidNumber, 0, 0);
            m_imageSprite->Action(ACT_CHANGE_DIRECTION, direction, 0, 0);
            m_imageSprite->Action(ACT_SET_INVISIBLE, 0, 0, 0);
        }
        else
            m_imageSprite->Action(ACT_SET_INVISIBLE, 1, 0, 0);
    }
    m_closeSprite = Map->MenuState().SpriteWithName(STRING("IF_Close"));
    m_buttons[0] = Map->MenuState().SpriteWithName(STRING("IF_Btn_1"));
    m_buttons[1] = Map->MenuState().SpriteWithName(STRING("IF_Btn_2"));
    m_buttons[2] = Map->MenuState().SpriteWithName(STRING("IF_Btn_3"));
    m_buttonTexts[0] = Map->MenuState().SpriteWithName(STRING("IF_BtnText_1"));
    m_buttonTexts[1] = Map->MenuState().SpriteWithName(STRING("IF_BtnText_2"));
    m_buttonTexts[2] = Map->MenuState().SpriteWithName(STRING("IF_BtnText_3"));
    if (zScriptExec::m_pShopDlgMenu)
        zScriptExec::m_pShopDlgMenu->RefreshBuyButtons();

    static const char* const names[3] = {"IF_Text_1", "IF_Text_2", "IF_Info"};
    STRING text;
    for (int index = 0; index < 3; ++index)
    {
        SPRITE* const sprite = Map->MenuState().SpriteWithName(STRING(names[index]));
        if (!sprite) continue;
        text = "";
        const char* const description = m_framework->AsTextDB()->GetObjInfo(m_currentItem.m_object, true, index + 1);
        if (description) text = description;
        sprite->Action(ACT_SET_TEXT, static_cast<int>(PtrToU32(&text)), 0, 0);
    }
}

void zShopItemInfo::Control(INPUT* input)
{
    static bool closePressed = false;
    if (!m_closeSprite || !m_currentItem.m_object) return;
    const unsigned int pressed = Map->MenuState().ControlFlags() & 1u;
    if (pressed && Map->MenuState().SelectedSprite() == m_closeSprite) closePressed = true;
    if (!pressed && closePressed)
    {
        closePressed = false;
        ShowObjInfo(0, false, 0);
        input->RemoveEvent(IM_LBUTTONCLICK, 0u);
        input->RemoveEvent(IM_LBUTTONUP, 0u);
    }
}

void __cdecl GetNew_zShopItemInfo(iShopItemInfo** outObject)
{
    void* const storage = CrtNew(sizeof(zShopItemInfo));
    zShopItemInfo* object = 0;
    if (storage)
    {
        try
        {
            object = ::new (storage) zShopItemInfo();
        }
        catch (...)
        {
            CrtFreePointer(storage);
            throw;
        }
    }
    *outObject = object;
}
