
#include "mylib.h"
#include "../../engine_globals.h"
#include "irpg.h"
#include "zm/Menu/zParamDlgMenu.h"
#include "zm/Menu/zShopDlgMenu.h"
#include "zm/Menu/zShopItemInfo.h"
#include "zm/Game/ScriptExec/zScriptExec.h"
#include "zHeroInvMenu.h"
#include "sprite.h"
#include "vid.h"
#include "input.h"
#include "graph.h"
#include "map.h"
#include "zm/Inventory/zInventory3D.h"
#include "zm/Inventory/zInventoryItem.h"
#include "zm/Menu/zSpriteInventory.h"
#include <new>

#include "basetype.h"


iRPGObj* zHeroInvMenu::GetSelItem()
{
    return m_selectedRef.m_object;
}


void  zHeroInvMenu::RemTakenItem()
{
    if (m_invServer->GetTakenObj() != 0)
    {
        if ((m_invServer->TryToPutItemBack() & 1) == 0)
            DebugAssert(4u, "buf & ITEM_ADDEDFULL", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 861u);
        ShowMouseCursor(0);
    }
}

bool zHeroInvMenu::GetBuyMode(const iRPGObj* item)
{
    if (!item)
    {
        DebugAssert(5u, "item", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 1039u);
        return false;
    }
    return item->GetInvId() == INV_SHOP || item->GetInvId() == INV_AMMOSHOP;
}


void  zHeroInvMenu::TryToFindButtons2()
{
    m_extraButtonCount = 0;
    const int menuSpriteCount = Map->m_menu.m_count;
    MENU& menu = Map->m_menu;
    for (int menuSpriteIndex = 0; menuSpriteIndex<menuSpriteCount; ++menuSpriteIndex)
    {
        SPRITE* const sprite = menu.m_data[menuSpriteIndex];
        const unsigned int nVid = static_cast<unsigned int>(sprite->Vid()->nVid);
        if (nVid == 712u || nVid == 713u)
        {
            if (m_extraButtonCount >= 29) DebugAssert(5u, "m_iNumSkillBtn < MAX_BUTTONS_NUM2-1", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 784u);
            m_extraButtons[m_extraButtonCount++] = sprite;
        }
    }
    Sort(m_extraButtons, m_extraButtonCount);
}

void  zHeroInvMenu::MouseCursorTact(float screenX, float screenY)
{
    if (!m_mouseSprite) DebugAssert(4u, "m_pMouseSprite", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 646u);
    const float targetX = screenX+Map->m_shiftXY.x;
    const float targetY = screenY+Map->m_shiftXY.y;
    float currentX = 0.0f, currentY = 0.0f;
    m_invServer->GetMouseDisplacement(currentX, currentY);
    VECTOR delta(targetX-currentX, targetY-currentY+(4003.0f), 4003.0f);
    if (m_mouseSprite) m_mouseSprite->ChangeCoor(delta);
}

void  zHeroInvMenu::SetRPGHero(iRPGHero* hero)
{
    m_rpgHero = hero;
    m_invServer = hero->GetInvServer();
    m_invServer->SetRPGHero(hero);
    for (unsigned int i = 0; i<INV_CNT; ++i)
    {
        if (!m_inventories[i]) continue;
        iInventory* const buf = m_invServer->GetInv(static_cast<int>(i));
        if (!buf) DebugAssert(5u, "buf", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 910u);
        if (buf->QueryInterface(18) != 0)
        {
            m_inventories[i]->SetRPGHero(hero);
            m_inventories[i]->SetInventory(buf);
        }
        else
        {
            m_inventories[i]->iRelease();
            m_inventories[i] = 0;
        }
    }
    iInventory* const selected = m_invServer->GetInv(INV_MAIN);
    for (unsigned int i = INV_WEAPON_1; i <= INV_WEAPON_9; ++i) m_inventories[i]->SetGetDirtyInventory(selected);
    ReloadAll();
}

void zHeroInvMenu::TryToFindButtons()
{
    if (!m_rpgHero) DebugAssert(5u, "m_pRPGHero", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 733u);
    m_buttonCount = 0;
    MENU& menu = Map->MenuState();
    const int count = menu.m_count;
    for (int i = 0; i < count; ++i)
    {
        SPRITE* const sprite = menu.m_data[i];
        if (sprite->Vid()->nVid != 722) continue;
        EX_SPRITE_DATA* const extra = sprite->ExData();
        if (!extra || extra->nameText.value[0] == '\0') continue;
        if (m_buttonCount >= 36)
            DebugAssert(5u, "m_iLightBtnNum < LIGHT_BUTTONS_NUM", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 748u);
        if (m_buttonCount >= 36) continue;
        const STRING name = sprite->GetName();
        const int inventoryId = m_invServer->GetInvByName(name.CharPtr());
        if (inventoryId == -1) continue;
        m_buttonInventoryIndex[m_buttonCount] = inventoryId;
        m_buttons[m_buttonCount] = sprite;
        m_buttons[m_buttonCount]->Action(ACT_SET_INVISIBLE, 1, 0, 0);
        ++m_buttonCount;
    }
}


zHeroInvMenu::zHeroInvMenu(iRPGFrameWork* framework)
    : zBaseDlgMenu(), m_buttonCount(0), m_extraButtonCount(0),
      m_mouseSprite(0), m_hintSprite(0), m_framework(framework), m_invServer(0),
      m_rpgHero(0), m_dragHoldTicks(0), m_itemInfoSource(false), m_itemInfoTime(0),
      m_itemInfoPendingReset(false), m_clickedInventory(-1), m_clickedInvMouseButtonLeft(false)
{
    for (int i = 0; i < INV_CNT; ++i)
        GetNew_zSpriteInventory(&m_inventories[i], m_framework);
    m_inventories[INV_MAIN]->SetDrawCellsType(1);
    m_inventories[INV_GRND]->SetDrawCellsType(2);
    m_inventories[INV_SHOP]->SetSpriteZ(2800);
    m_inventories[INV_MAIN]->SetSpriteZ(2800);
    m_inventories[INV_SHOP]->ShowCost(true);
    m_inventories[INV_AMMOSHOP]->ShowCost(true);
    m_highLightRef.m_owner = static_cast<iRefOwner*>(this);
}



zHeroInvMenu::~zHeroInvMenu()
{
    for (int i = 0; i < INV_CNT; ++i)
    {
        if (m_inventories[i]) m_inventories[i]->iRelease();
        m_inventories[i] = 0;
    }
}


void  zHeroInvMenu::SetRPGFrameWork(iRPGFrameWork* frameWork)
{
    m_framework = frameWork;
}




void zHeroInvMenu::SetShopMode(bool shopMode)
{
    m_shopMode = shopMode;
}


bool zHeroInvMenu::IsShopMode()
{
    return m_shopMode;
}

int zHeroInvMenu::GetClickedInv()
{
    return m_clickedInventory;
}

void zHeroInvMenu::ResetClickedInv()
{
    m_clickedInventory = -1;
}

bool zHeroInvMenu::GetClickedInvMBL()
{
    return m_clickedInvMouseButtonLeft;
}



void  zHeroInvMenu::GetGammaItemDisabled(int& red, int& green, int& blue)
{
    m_framework->GetGammaItemDisabled(&red, &green, &blue);
}

iBaseDlgMenu*  zHeroInvMenu::AsBaseDlgMenu()
{
    return static_cast<iBaseDlgMenu*>(this);
}

void  zHeroInvMenu::DeleteHint()
{
    delete m_hintSprite;
    m_hintSprite = 0;
}

void  zHeroInvMenu::InitOnShow()
{
    FindButtonsIfNeeded();
    ControlInventories(0);
    RefreshItems();
}

void  zHeroInvMenu::OnInvalidate(iRef<iRefObject>* candidate)
{
    if (candidate != reinterpret_cast<iRef<iRefObject>*>(&m_highLightRef))
        return;
    (void) HighLightItem(0);
}


bool  zHeroInvMenu::SetInputActive(bool active)
{
    const bool changed = zBaseDlgMenu::SetInputActive(active);
    if (changed && !active)
        this->RemTakenItem();
    return changed;
}




void  zHeroInvMenu::SetItemTaker(iItemTaker* itemTaker)
{
    m_itemTakerRef.Set(itemTaker);
}

bool zHeroInvMenu::Show(bool action)
{
    const bool changed = zBaseDlgMenu::Show(action);
    if (!changed) return false;
    if (action)
        InitOnShow();
    else
    {
        RemTakenItem();
        ReloadAll();
        if (zScriptExec::m_pShopItemInfo)
        {
            zScriptExec::m_pShopItemInfo->ShowObjInfo(0, false, 0);
            if (zScriptExec::m_pShopDlgMenu)
                zScriptExec::m_pShopDlgMenu->RefreshBuyButtons();
        }
    }
    return true;
}


const char*  zHeroInvMenu::GetMouseTipsString(float x, float y)
{
    if (!m_enabled) return 0;
    if (m_invServer->GetTakenObj() != 0) return 0;
    if (zScriptExec::m_pShopItemInfo->GetCurrentItem()) return 0;
    const iInventory* const candidate = m_invServer->GetInventoryByPoint(x, y);
    if (m_invServer->GetObjByPoint(x, y) != 0 || !candidate) return 0;
    const char* const name = candidate->GetName();
    if (!name) return 0;
    iTextDB* const textDb = m_framework->AsTextDB();
    const char** const lines = textDb->GetExtInfo(1, name);
    return m_framework->AsTextDB()->GetSpeciallyDecodedString(lines, -1);
}

void  zHeroInvMenu::HighLightInventories(const iRPGObj* item)
{
    if (m_buttonCount == 0) return;
    if (m_hoverRef.m_object == item) return;
    for (int inventoryIndex = 0; inventoryIndex<m_buttonCount; ++inventoryIndex)
    {
        unsigned int enabled = 1u;
        if (item && m_buttonInventoryIndex[inventoryIndex] != 0 && m_invServer->CanPutToInv(m_buttonInventoryIndex[inventoryIndex], item))
            enabled = 0u;
        (void) m_buttons[inventoryIndex]->Action(ACT_SET_INVISIBLE, static_cast<int>(enabled), 0, 0);
    }
    m_hoverRef.Set(const_cast<iRPGObj*>(item));
}

bool  zHeroInvMenu::HighLightItem(const iRPGObj* target)
{
    iRPGObj* const mutableTarget = const_cast<iRPGObj*>(target);
    if (!m_selectedRef.m_object && target)
    {
        iRPGObj* const resolved = static_cast<iRPGObj*>(mutableTarget->QueryInterface(9));
        m_selectedRef.Set(resolved);
        if (zScriptExec::m_pShopDlgMenu) zScriptExec::m_pShopDlgMenu->RefreshAll();
    }
    if (m_highLightRef.m_object == target) return false;
    unsigned int changed = 0u;
    for (unsigned int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex)
        if (m_inventories[inventoryIndex] && m_inventories[inventoryIndex]->HighLightItem(target)) changed |= 1u;
    m_highLightRef.Set(mutableTarget);
    return (changed&0xFFu) != 0u;
}

int zHeroInvMenu::TryProcessItemPut(iRPGObj* item, float x, float y, bool alternate)
{
    const float centerX = static_cast<float>(static_cast<double>(Graph->screenWidth) * 0.5);
    const float centerY = static_cast<float>(static_cast<double>(Graph->screenHeight) * 0.5);
    if (item->GetGameObj() == TYPE_AMMO && !IsShopMode())
        return m_invServer->PutItemEx(item, 0);
    const double shopBoundary = static_cast<double>(centerX) + 20.0;
    if (!alternate)
    {
        if (IsShopMode())
        {
            if (x >= shopBoundary)
                return zScriptExec::m_pShopDlgMenu->TryToSellItem(item, 1u) ? 1 : 0;
            if (item->GetGameObj() == TYPE_AMMO)
                return m_invServer->PutItemEx(item, 0);
            return 0;
        }
        if (IsShopMode()) return 0;
        if (x <= static_cast<double>(centerX) - 360.0 ||
            x >= static_cast<double>(centerX) + 360.0 ||
            y <= static_cast<double>(centerY) - 250.0 ||
            y >= static_cast<double>(centerY) + 250.0)
        {
            m_itemTakerRef.m_object->CreateGroundSprite(item);
            return 1;
        }
        return 0;
    }
    if (!(shopBoundary >= x)) return 0;
    const int result = m_invServer->TryToBuyObj(item, x, y);
    if (!result)
    {
        m_framework->PlaySFXCurArmy(46);
        return 0;
    }
    m_framework->PlaySFXCurArmy(45);
    zScriptExec::m_pShopDlgMenu->RefreshAll();
    ResetSelItem();
    return result;
}


void  zHeroInvMenu::ShowMouseCursor(const iRPGObj* enabled)
{
    SPRITE* oldPreview = m_mouseSprite;
    if (!enabled)
    {
        if (oldPreview)
        {
            delete oldPreview;
            m_mouseSprite = 0;
        }
        return;
    }

    iRPGObj* const selected = const_cast<iRPGObj*>(m_invServer->GetTakenObj());
    if (!selected) return;
    oldPreview = m_mouseSprite;
    if (oldPreview) DebugAssert(4u, "m_pMouseSprite == NULL", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 618u);

    const int spriteIndex = selected->GetVidNum(2);
    VID* descriptor = ::EmptyVid;
    if (spriteIndex >= 0 && spriteIndex<Map->m_vidCount)
    {
        VID* const candidate = Map->m_vids[spriteIndex];
        if (candidate) descriptor = candidate;
    }

    VECTOR origin(0.0f, 0.0f, 0.0f);
    ANGLE direction(static_cast<unsigned char>(0));
    SPRITE* const preview = Map->CreateSprite(descriptor, origin, direction, 0);
    m_mouseSprite = preview;
    if (!preview) return;

    const unsigned int frame = static_cast<unsigned int>(selected->GetDirNum(2));
    VID* const previewInfo = m_mouseSprite->Vid();
    const unsigned char directionValue = static_cast<unsigned char>((frame<<8)/static_cast<unsigned int>(previewInfo->noDir));
    m_mouseSprite->ChangeDirection(ANGLE(directionValue));

    iInvKeeper* const activeOwner = m_invServer->Keeper();
    if (activeOwner->CanHoldObj(selected)) return;

    int red = 0, green = 0, blue = 0;
    GetGammaItemDisabled(red, green, blue);
    const int values[3] = {red, green, blue};
    unsigned int negative = 0u, positive = 0u;
    for (unsigned int gammaComponent = 0; gammaComponent<3u; ++gammaComponent)
    {
        int value = values[gammaComponent];
        if (value<-255) value = -255;
        else if (value>255) value = 255;
        const unsigned int shift = (2u-gammaComponent)*8u;
        if (value<0) negative |= static_cast<unsigned int>(-value)<<shift;
        else positive |= static_cast<unsigned int>(value)<<shift;
    }
    GAMMA gamma;
    gamma.modulate = negative;
    gamma.add = positive;
    m_mouseSprite->SetGamma(gamma);
}

void  zHeroInvMenu::ShowHint(const char* _sMsg, float x, float y, bool localize)
{
    if (!_sMsg) DebugAssert(5u, "_sMsg", "..\\src\\zm\\Menu\\zHeroInvMenu.cpp", 1061u);

    SPRITE* hint = m_hintSprite;
    if (!hint)
    {
        VECTOR origin(0.0f, 0.0f, 0.0f);
        ANGLE direction(static_cast<unsigned char>(0));
        VID* const vid = Map->Vid(3);
        hint = Map->CreateSprite(vid, origin, direction, 0);
        m_hintSprite = hint;
    }

    VECTOR position(
        x+Map->m_shiftXY.x,
        y+Map->m_shiftXY.y+(4000.0f),
        4000.0f);
    hint->ChangeCoor(position);

    STRING text;
    if (!localize)
    {
        text = _sMsg;
        (void) hint->Action(ACT_SET_BEHAVE, 2, 0, 0);
    }
    else
    {
        PROFILE* const profile = ::Strings;
        const STRING fallback(StringGlobals::EmptyText());
        const STRING key(_sMsg);
        const STRING section("messages");
        text = profile->GetString(section, key, fallback);
        (void) hint->Action(ACT_SET_BEHAVE, 4, 0, 0);
    }
    (void) hint->Action(ACT_SET_TEXT, static_cast<int>(PtrToU32(&text)), 0, 0);
}

void __cdecl GetNew_zHeroInvMenu(iHeroInvMenu** outObject, iRPGFrameWork* framework)
{
    void* const storage = CrtNew(sizeof(zHeroInvMenu));
    zHeroInvMenu* object = 0;
    try
    {
        if (storage)
            object = ::new (storage) zHeroInvMenu(framework);
    }
    catch (...)
    {
        zBaseDlgMenu::operator delete(storage);
        throw;
    }
    *outObject = object;
}

void  zHeroInvMenu::Control(INPUT* input)
{
    if (m_enabled == 0u)
    {
        this->HighLightItem(0);
        this->HighLightInventories(0);
        this->ResetSelItem();
        return;
    }

    this->FindButtonsIfNeeded();
    this->ControlInventories(input);
    zBaseDlgMenu::Control(input);

    if (this->m_invServer->GetMouseDirty())
        this->ShowMouseCursor(this->m_invServer->GetTakenObj());

    if (this->m_mouseSprite != 0)
        this->MouseCursorTact(input->ClientMouseXY().x, input->ClientMouseXY().y);

    iRPGObj* const overlay = this->m_highLightRef.m_object;
    if (overlay)
    {
        this->HighLightItem(0);
        if (overlay->HaveInv())
            this->HighLightItem(overlay);
    }

    const iRPGObj* candidate = this->m_invServer->GetTakenObj();
    if (!candidate)
    {
        iRPGObj* const fallback = this->m_selectedRef.m_object;
        if (fallback && fallback->GetInvId() == INV_SHOP) candidate = this->m_selectedRef.m_object;
    }
    this->HighLightInventories(candidate);
    SPRITE* const button = Map->MenuState().SelectedSprite();
    if (button && (Map->MenuState().ControlFlags() & 1u) &&
        button->Vid()->nVid == 723 && !m_invServer->GetTakenObj())
        zScriptExec::m_pShopDlgMenu->ProcessItemButton(button);
    this->RefreshItems();
}

void  zHeroInvMenu::ControlInput(INPUT* input)
{
    const float mouseX = input->ClientMouseXY().x;
    const float mouseY = input->ClientMouseXY().y;

    if (input->GetAction(IM_LBUTTONDOWN))
        m_dragHoldTicks = AddSigned32Wrap(m_dragHoldTicks, 1);
    else
    {
        m_dragHoldTicks = 0;
        if (m_itemInfoPendingReset)
            m_itemInfoSelection.Set(0);
        m_itemInfoPendingReset = false;
    }

    (void)m_invServer->GetTakenObj();
    if (this->m_invServer->GetTakenObj() != 0)
    {
        iRPGObj* const selected = const_cast<iRPGObj*>(this->m_invServer->GetTakenObj());
        zItem* const object = static_cast<zItem*>(selected->QueryInterface(49));

        if (zScriptExec::m_pShopDlgMenu && object->GetGameObj() != TYPE_AMMO && this->m_invServer->IsForBuy())
            zScriptExec::m_pShopDlgMenu->ScrollToItem(object->GetBaseId(), false);

        float currentX = 0.0f, currentY = 0.0f;
        this->m_invServer->GetMouseDisplacement(currentX, currentY);
        zItem* routeObject = 0;
        int hoverIndex = this->m_invServer->GetInventoryToPutItem(
            object, mouseX-currentX, mouseY-currentY, routeObject);
        if (hoverIndex == 0 && object->GetGameObj() == TYPE_AMMO) hoverIndex = -1;

        for (int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex)
        {
            if (inventoryIndex == hoverIndex) continue;
            iSpriteInventory* const child = this->m_inventories[static_cast<unsigned int>(inventoryIndex)];
            if (child) child->HideHighLight();
        }
        if (hoverIndex != -1)
        {
            iSpriteInventory* const child = this->m_inventories[static_cast<unsigned int>(hoverIndex)];
            if (child)
            {
                const iRPGObj* const taken = this->m_invServer->GetTakenObj();
                this->m_inventories[static_cast<unsigned int>(hoverIndex)]->ShowPlaceToPut(taken, mouseX-currentX, mouseY-currentY);
            }
        }

        int width = 0, height = 0;
        object->GetSize2DCalced(width, height, 0);
        const float hintX = (mouseX-currentX)+static_cast<float>(MultiplySigned32Wrap(width, 37)/2);
        const float hintY = (mouseY-currentY)-static_cast<float>(MultiplySigned32Wrap(height, 37)/2);

        if (hoverIndex == INV_GRND)
        {
            this->ShowHint("InvThrowToGrndWarn", hintX, hintY, true);
        }
        else
        {
            const iInventory* const candidate = this->m_invServer->GetInventoryByPoint(mouseX, mouseY);
            if (candidate && (candidate->GetId() == INV_SHOP || candidate->GetId() == INV_AMMOSHOP))
            {
                STRING priceText = Printf("$%d", static_cast<unsigned int>(object->GetSellCostFull()));
                this->ShowHint(priceText.CharPtr(), hintX, hintY, false);
            }
            else
            {
                this->DeleteHint();
            }
        }
    }
    else
    {
        this->DeleteHint();
    }


    if (m_invServer->GetTakenObj() && zScriptExec::m_pShopItemInfo->GetCurrentItem())
    {
        if (zScriptExec::m_pShopItemInfo)
        {
            zScriptExec::m_pShopItemInfo->ShowObjInfo(0, false, 0);
            if (zScriptExec::m_pShopDlgMenu)
                zScriptExec::m_pShopDlgMenu->RefreshBuyButtons();
        }
    }

    iRPGObj* const candidate = m_invServer->GetObjByPoint(mouseX, mouseY);
    if (input->GetAction(IM_LBUTTONDOWN) && candidate)
    {
        if (m_dragHoldTicks == 1)
            m_lastClickedItem.Set(candidate);
        else if (m_lastClickedItem.m_object != candidate)
            m_lastClickedItem.Set(0);
    }
    else
    {
        m_lastClickedItem.Set(0);
    }
    if (input->GetAction(IM_LBUTTONUP) && !zScriptExec::m_pShopItemInfo->GetCurrentItem() &&
        m_lastClickedItem.m_object == candidate)
    {
        if (candidate) m_selectedRef.Set(candidate);
        HighLightItem(candidate && candidate->GetGameObj() != 7 ? candidate : 0);
        if (IsShopMode())
        {
            if (candidate)
                zScriptExec::m_pShopDlgMenu->RefreshAll();
            else
            {
                const iInventory* const underMouse = m_invServer->GetInventoryByPoint(mouseX, mouseY);
                if (underMouse)
                {
                    const OBJ_TYPE type = underMouse->GetObjectType();
                    const OBJ_EXTTYPE ext = underMouse->GetObjectExtType();
                    if (static_cast<int>(type) != 15 || static_cast<int>(ext) != 21)
                        zScriptExec::m_pShopDlgMenu->ScrollToItem(type, ext);
                }
            }
        }
        else
        {
            HighLightInventories(candidate);
            ResetSelItem();
        }
    }
    else
    {
        const int wheel = input->GetAction(IA_MOUSEWHEEL);
        if (wheel)
        {
            const iInventory* const underMouse = m_invServer->GetInventoryByPoint(mouseX, mouseY);
            if (underMouse)
            {
                zInventory3D* const inventory = static_cast<zInventory3D*>(const_cast<iInventory*>(underMouse)->QueryInterface(48));
                inventory->Scroll(0.0f, wheel < 0 ? 10.0f : -10.0f);
            }
        }
    }
    ControlItemInfo(candidate, input, false);
    const bool clickedLeft = input->GetAction(IM_LBUTTONDOWN) != 0 && m_dragHoldTicks == 1;
    if (clickedLeft || input->GetAction(IM_RBUTTONCLICK) != 0)
    {
        const iInventory* const inventory = m_invServer->GetInventoryByPoint(mouseX, mouseY);
        if (inventory)
        {
            m_clickedInventory = inventory->GetId();
            m_clickedInvMouseButtonLeft = clickedLeft;
        }
    }
}

void zHeroInvMenu::ControlItemInfo(iRPGObj* selection, INPUT* input, bool source)
{
    iShopItemInfo* const itemInfo = zScriptExec::m_pShopItemInfo;
    itemInfo->SetInventoryKeeper(static_cast<iInvKeeper*>(m_rpgHero->QueryInterface(1)));
    itemInfo->SetBounds(105, 190,
        SubSigned32Wrap(TruncateFloatToInt32(Graph->screenWidth), 155),
        SubSigned32Wrap(TruncateFloatToInt32(Graph->screenHeight), 240));
    if (selection)
    {
        if (m_itemInfoSelection.m_object != selection) m_itemInfoTime = ::CurrentTime;
        m_itemInfoSelection.Set(selection);
        m_itemInfoSource = source;
    }
    else if (m_itemInfoSelection.m_object && m_itemInfoSource == source)
    {
        if (m_shopMode && !m_invServer->GetTakenObj())
            m_itemInfoPendingReset = true;
        else
            m_itemInfoSelection.Set(0);
    }
    if (m_itemInfoSource != source) return;
    if (input->GetAction(IM_LBUTTONDOWN)) m_itemInfoTime = ::CurrentTime;
    const bool wasClosed = itemInfo->GetCurrentItem() == 0;
    if (source)
    {
        static iRPGObj* clickedItem = 0;
        if (input->GetAction(IM_LBUTTONCLICK)) clickedItem = m_itemInfoSelection.m_object;
        if (input->GetAction(IM_LBUTTONUP) && !itemInfo->GetCurrentItem() &&
            clickedItem == m_itemInfoSelection.m_object && m_itemInfoSelection.m_object)
            zScriptExec::m_pShopDlgMenu->ScrollToItem(m_itemInfoSelection.m_object->GetBaseId(), false);
    }
    if (itemInfo->GetCurrentItem())
    {
        if (!wasClosed)
        {
            SPRITE* const selected = Map->MenuState().SelectedSprite();
            if (selected && selected->Vid()->nVid == 723) return;
            itemInfo->Control(input);
        }
        return;
    }
    (void)m_invServer->GetTakenObj();
}


void  zHeroInvMenu::ControlInventories(INPUT* input)
{
    for (unsigned int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex) if (m_inventories[inventoryIndex]) m_inventories[inventoryIndex]->ControlPlayerMenu(input);
    for (unsigned int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex) if (m_inventories[inventoryIndex]) m_inventories[inventoryIndex]->ClearDirty();
}

void  zHeroInvMenu::DeletePointerToSprite(SPRITE* sprite)
{
    for (unsigned int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex)
        if (this->m_inventories[inventoryIndex]) this->m_inventories[inventoryIndex]->DeletePointerToSprite(sprite);
    if (this->m_mouseSprite == sprite)
    {
        this->m_mouseSprite = 0;
    }
    else
    {
        bool found = SpriteInArray(sprite, this->m_buttons, this->m_buttonCount) != 0;
        if (!found) found = SpriteInArray(sprite, this->m_extraButtons, this->m_extraButtonCount) != 0;
        if (found) this->ReloadAll();
    }
    if (this->m_hintSprite == sprite) this->m_hintSprite = 0;
}

void  zHeroInvMenu::FindButtonsIfNeeded()
{
    if (m_buttonCount == 0)
    {
        TryToFindButtons();
        HighLightInventories(0);
    }
    if (m_extraButtonCount == 0) TryToFindButtons2();
}

void  zHeroInvMenu::ReloadAll()
{
    for (unsigned int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex) if (m_inventories[inventoryIndex]) m_inventories[inventoryIndex]->ReloadAll();
    m_buttonCount = 0;
    m_extraButtonCount = 0;
}

void  zHeroInvMenu::RefreshItems()
{
    for (unsigned int inventoryIndex = 0; inventoryIndex<INV_CNT; ++inventoryIndex) if (m_inventories[inventoryIndex]) m_inventories[inventoryIndex]->RefreshItems();
}

void  zHeroInvMenu::ResetSelItem()
{
    m_selectedRef.Set(0);
}

void*  zHeroInvMenu::QueryInterface(int interfaceId) const
{
    if (static_cast<unsigned int>(interfaceId) == 12u)
        return const_cast<zHeroInvMenu*>(this);
    return zBaseDlgMenu::QueryInterface(interfaceId);
}

int  zHeroInvMenu::iRelease()
{
    delete static_cast<zBaseDlgMenu*>(this);
    return 0;
}



