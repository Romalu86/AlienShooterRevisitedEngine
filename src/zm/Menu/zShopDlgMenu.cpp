#include "../../engine_globals.h"

#include "zShopDlgMenu.h"
#include "irpg.h"
#include "zHeroInvMenu.h"
#include "zShopItemInfo.h"
#include "zm/Game/ScriptExec/zScriptExec.h"
#include "graph.h"
#include "net.h"
#include "zm/Utils/zVectorOperations.h"
#include "sprite.h"
#include "vid.h"
#include "input.h"
#include "zm/Inventory/zInventory.h"
#include "zm/Inventory/zInventory3D.h"
#include "zm/Inventory/zInventory2D.h"
#include "zm/RPGHero/zRPGWeapon.h"
#include "zm/RPGHero/zRPGAmmo.h"
#include "zm/RPGHero/zRPGArmor.h"
#include "zm/RPGHero/zRPGImplant.h"
#include "mylib.h"
#include <new>
#include <stdexcept>

#include "basetype.h"

void GetNew_zShopDlgMenu(iShopDlgMenu** outObject);


#include "map.h"

namespace
{
    struct zShopActionVector
    {
        zContainerProxy<zShopActionVector>* m_proxy;
        int* m_begin;
        int* m_end;
        int* m_capEnd;

        zShopActionVector()
            : m_proxy(static_cast<zContainerProxy<zShopActionVector>*>(CrtNew(sizeof(zContainerProxy<zShopActionVector>)))),
              m_begin(0), m_end(0), m_capEnd(0)
        {
            if (m_proxy) m_proxy->m_owner = this;
        }

        ~zShopActionVector()
        {
            if (m_begin) CrtFreePointer(m_begin);
            m_begin = m_end = m_capEnd = 0;
            CrtFreePointer(m_proxy);
        }

        void Append(int value)
        {
            const unsigned int count = m_begin ? static_cast<unsigned int>(m_end-m_begin) : 0u;
            const unsigned int reserved = m_begin ? static_cast<unsigned int>(m_capEnd-m_begin) : 0u;
            if (count < reserved)
            {
                *m_end++ = value;
                return;
            }
            if (m_begin > m_end)
                ::_invalid_parameter_noinfo();
            if (m_begin && m_begin != m_end && !m_proxy)
                ::_invalid_parameter_noinfo();
            VectorOperations::InsertOne(m_begin, m_end, m_capEnd, m_end, value);
        }

        unsigned int Size() const
        {
            return static_cast<unsigned int>(m_end-m_begin);
        }

        int& At(unsigned int index)
        {
            if (index >= Size())
                ::_invalid_parameter_noinfo();
            return m_begin[index];
        }
    };
}

void __cdecl GetNew_zShopDlgMenu(iShopDlgMenu** outObject)

{
    void* const storage = CrtNew(sizeof(zShopDlgMenu));
    zShopDlgMenu* object = 0;
    if (storage)
    {
        try
        {
            object = ::new (storage) zShopDlgMenu();
        }
        catch (...)
        {
            CrtFreePointer(storage);
            throw;
        }
    }
    *outObject = object;
}

void  zShopDlgMenu::SetRPGFrameWork(iRPGFrameWork* frameWork)
{
    m_framework = frameWork;
}

void  zShopDlgMenu::SetHeroInvMenu(iHeroInvMenu* heroInventoryMenu)
{
    m_heroInvMenu = heroInventoryMenu;
}

void  zShopDlgMenu::SetPlayer(iPlayer* player)
{
    m_player = player;
}

void  zShopDlgMenu::SetCurLevel(int level)
{
    m_curLevel = level;
}

iBaseDlgMenu*  zShopDlgMenu::AsBaseDlgMenu()
{
    return static_cast<iBaseDlgMenu*>(QueryInterface(11));
}

void  zShopDlgMenu::RefreshAll()
{
    InitLoad();
    RefreshBuyButtons();
}




void  zShopDlgMenu::PageUp(int inventoryId)
{
    iInventory* inventory = m_invServerRef.m_object->GetInv(inventoryId);
    zInventory3D* inventory3D = static_cast<zInventory3D*>(inventory->QueryInterface(48));
    inventory3D->Scroll(0.0f, -100.0f);
}

void  zShopDlgMenu::PageDown(int inventoryId)
{
    iInventory* inventory = m_invServerRef.m_object->GetInv(inventoryId);
    zInventory3D* inventory3D = static_cast<zInventory3D*>(inventory->QueryInterface(48));
    inventory3D->Scroll(0.0f, 100.0f);
}


void*  zShopDlgMenu::QueryInterface(int id) const
{
    if (id == 14) return const_cast<zShopDlgMenu*>(this);
    return zBaseDlgMenu::QueryInterface(id);
}

int  zShopDlgMenu::iRelease()
{
    delete static_cast<zBaseDlgMenu*>(this);
    return 0;
}

bool zShopDlgMenu::ScrollToItem(OBJ_TYPE objectType, OBJ_EXTTYPE extType)
{
    (void)extType;
    if (static_cast<int>(objectType) == 7)
    {
        RefreshBuyButtons();
        return false;
    }
    return true;
}

void  zShopDlgMenu::Control(INPUT* input)
{
    (void) InitLoad();
    zBaseDlgMenu::Control(input);
    RefreshBuyButtons();
}

void  zShopDlgMenu::PageScroll(int scrollPercent, int inventoryId)
{
    if (scrollPercent<0) scrollPercent = 0;
    else if (scrollPercent>1000) scrollPercent = 1000;
    iInventory* inventory = m_invServerRef.m_object->GetInv(inventoryId);
    zInventory3D* inventory3D = static_cast<zInventory3D*>(inventory->QueryInterface(48));
    inventory3D->ScrollPer(scrollPercent);
}

void zShopDlgMenu::DeletePointerToSprite(SPRITE* sprite)
{
    bool found = false;
    for (int i = 0; i < 9; ++i)
        if (sprite && m_categoryButtons[i] == sprite) { found = true; break; }
    if (!found) return;
    for (int i = 0; i < 9; ++i) m_categoryButtons[i] = 0;
    m_buttonsFound = false;
    m_filterButtonCount = 0;
    m_objectType = static_cast<OBJ_TYPE>(15);
    m_extType = static_cast<OBJ_EXTTYPE>(21);
    if (zScriptExec::m_pShopItemInfo) zScriptExec::m_pShopItemInfo->Initialize();
}








void zShopDlgMenu::OnWeaponBought(iRPGObj* item)
{
    if (!item || item->GetGameObj() != TYPE_WEAPON)
        DebugAssert(5u, "item && item->GetType() == TYPE_WEAPON", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1090u);
    zRPGWeapon* weapon = static_cast<zRPGWeapon*>(item->QueryInterface(50));
    zRPGAmmo* ammo = weapon->GetAmmo();
    if (ammo && m_invServerRef.m_object->GetObjectCnt(ammo->GetBaseId()) == 0)
        (void)BuyAmmoForWeapon(item, false);
}

bool zShopDlgMenu::BuyAmmoForWeapon(iRPGObj* item, bool checkOnly)
{
    if (!item)
        DebugAssert(5u, "item", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1174u);
    if (item->GetGameObj() != TYPE_WEAPON && item->GetGameObj() != TYPE_AMMO)
        return false;
    zRPGAmmo* weaponAmmo = 0;
    if (item->GetGameObj() == TYPE_WEAPON)
    {
        zRPGWeapon* const weapon = static_cast<zRPGWeapon*>(item->QueryInterface(50));
        weaponAmmo = weapon->GetAmmo();
    }
    if (item->GetGameObj() == TYPE_AMMO)
        weaponAmmo = static_cast<zRPGAmmo*>(item->QueryInterface(45));
    if (!weaponAmmo)
        return false;

    zRPGAmmo* shopAmmo = 0;
    zShopListLayout::Iterator shopAmmoNode = { 0, 0 };
    const zShopListLayout::Iterator end = m_objects.End();
    for (zShopListLayout::Iterator node = m_objects.Begin();
        node != end; node.Next())
    {
        iRPGObj* object = node.Value();
        if (!object)
            DebugAssert(5u, "*j", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1196u);
        if (object->GetGameObj() != TYPE_AMMO)
            continue;
        zRPGAmmo* candidate = static_cast<zRPGAmmo*>(object->QueryInterface(45));
        const int candidateBaseId = candidate->GetBaseId();
        const int weaponBaseId = weaponAmmo->GetBaseId();
        if (candidateBaseId == weaponBaseId)
        {
            shopAmmo = candidate;
            shopAmmoNode = node;
            break;
        }
    }

    zRPGAmmo* ammoToBuy = shopAmmo;
    if (!ammoToBuy)
    {
        const int inventoryId = item->GetGameObj() == TYPE_AMMO ? INV_SHOP : INV_AMMOSHOP;
        zInventory3D* const inventory = static_cast<zInventory3D*>(
            m_invServerRef.m_object->GetInv(inventoryId)->QueryInterface(48));
        int index = -1;
        zItem* available = inventory->GetItemByBaseId(weaponAmmo->GetBaseId(), index, false);
        if (available)
            ammoToBuy = static_cast<zRPGAmmo*>(available->QueryInterface(45));
    }
    if (!ammoToBuy)
        return false;
    if (!checkOnly)
    {
        (void)TryToBuyItem(ammoToBuy);
        if (ammoToBuy == shopAmmo && ammoToBuy->GetNumInCell() == 0)
        {
            m_objects.Erase(shopAmmoNode);
        }
    }
    return true;
}

void  zShopDlgMenu::GoHome(int inventoryId)
{
    zInventory3D* const inventory =
        static_cast<zInventory3D*>(m_invServerRef.m_object->GetInv(inventoryId)->QueryInterface(48));
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
    inventory->GetLimits(left, top, right, bottom);
    inventory->SetLimits(left, 0.0f, right, bottom);
}

void  zShopDlgMenu::GoEnd(int inventoryId)
{
    zInventory3D* const inventory =
        static_cast<zInventory3D*>(m_invServerRef.m_object->GetInv(inventoryId)->QueryInterface(48));
    float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
    inventory->GetLimits(left, top, right, bottom);
    inventory->SetLimits(left, 100000.0f, right, bottom);
}

int  zShopDlgMenu::GetItemsNum(int inventoryId)
{
    iInventory* group = m_invServerRef.m_object->GetInv(inventoryId);
    zInventory3D* inventory = static_cast<zInventory3D*>(group->QueryInterface(48));
    return MultiplySigned32Wrap(inventory->m_gridHeight, inventory->GetCellSizeY());
}

int  zShopDlgMenu::GetListItemsNum(int inventoryId)
{
    iInventory* group = m_invServerRef.m_object->GetInv(inventoryId);
    zInventory3D* inventory = static_cast<zInventory3D*>(group->QueryInterface(48));
    float left, top, right, bottom;
    inventory->GetLimits(left, top, right, bottom);
    return TruncateFloatViaCrtToInt32(bottom);
}

int  zShopDlgMenu::GetFirstShownItemNum(int inventoryId)
{
    iInventory* group = m_invServerRef.m_object->GetInv(inventoryId);
    zInventory3D* inventory = static_cast<zInventory3D*>(group->QueryInterface(48));
    float left, top, right, bottom;
    inventory->GetLimits(left, top, right, bottom);
    return TruncateFloatViaCrtToInt32(top);
}

void  zShopDlgMenu::SetInvServer(iInvServer* inventoryServer)
{
    m_invServerRef.Set(inventoryServer);

    ReloadAll();
}

bool zShopDlgMenu::ScrollToItem(int objectKey, bool unusedArgument)
{
    (void)unusedArgument;
    if (Net && (Net->m_flags & 0x10u)) return false;
    iRPGObj* const object = m_framework->GetItemById(objectKey);
    if (!object) return false;
    if (object->GetGameObj() == 7)
        RefreshBuyButtons();
    else
    {
        (void)object->GetAddMoney();
        (void)object->GetGameObj();
    }
    return false;
}




void  zShopDlgMenu::ReleaseListOfObjects()
{
    m_objects.Clear();
    m_generated.Clear();
    if (!m_invServerRef.m_object) return;
    for (int inventoryId = INV_SHOP; inventoryId <= INV_AMMOSHOP; ++inventoryId)
    {
        iInventory* inventory = m_invServerRef.m_object->GetInv(inventoryId);
        if (inventory)
        {
            zInventory3D* complete = static_cast<zInventory3D*>(inventory->QueryInterface(48));
            complete->RemoveAllItems();
        }
    }
}

void zShopDlgMenu::TryToFindButtons()
{
    for (int i = 0; i < 9; ++i) m_categoryButtons[i] = 0;
    m_filterButtonCount = 0;
    MENU& menu = Map->MenuState();
    const int count = menu.m_count;
    for (int i = 0; i < count; ++i)
    {
        SPRITE* const sprite = menu.m_data[i];
        VID* const vid = sprite->Vid();
        if (vid->nVid == 997)
        {
            const int index = vid->RealDirection(sprite->Direction().value);
            if (index < 0 || index >= 9)
                DebugAssert(5u, "dir >= 0 && dir < BUTTONS_NUM", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 639u);
            if (static_cast<unsigned int>(index) < 9u) m_categoryButtons[index] = sprite;
            m_buttonsFound = true;
        }
        if (sprite->Vid()->nVid == 715)
        {
            m_filterButtons[m_filterButtonCount++] = sprite;
            m_buttonsFound = true;
        }
    }
    Sort(m_filterButtons, m_filterButtonCount);
}


void  zShopDlgMenu::SetRPGHero(iRPGHero* hero)
{
    m_rpgHeroRef.Set(hero);
    iInvKeeper* keeper = 0;
    if (m_rpgHeroRef.m_object)
        keeper = static_cast<iInvKeeper*>(m_rpgHeroRef.m_object->QueryInterface(1));
    m_keeperRef.Set(keeper);
    if (zScriptExec::m_pShopItemInfo && m_rpgHeroRef.m_object)
        zScriptExec::m_pShopItemInfo->SetInventoryKeeper(m_keeperRef.m_object);
}

void  zShopDlgMenu::ControlInput(INPUT* input)
{
    const VECTOR2 cursor(input->ClientMouseXY().x + Map->m_shiftXY.x,
        input->ClientMouseXY().y + Map->m_shiftXY.y);
    int hoveredCategory = -1;
    VID* const overlay = Map->Vid(3740);
    unsigned int overlaySprites = 0u;
    if (overlay)
        for (int index = 0; index < VID::PlayerSlotCount; ++index)
            overlaySprites += overlay->spriteCountsByPlayer[index];
    if (!overlay || overlaySprites == 0u)
    {
        for (int index = 0; index < 9; ++index)
        {
            if (m_categoryButtons[index] && MenuSpriteContainsPoint(m_categoryButtons[index], cursor))
            {
                hoveredCategory = index;
                break;
            }
        }
    }
    for (int index = 0; index < 9; ++index)
    {
        SPRITE* const tab = Map->MenuState().SpriteWithName(Printf("IS_TAB_%d", index + 1));
        if (tab) tab->Action(ACT_SET_INVISIBLE, index != hoveredCategory, 0, 0);
    }

    unsigned int action = input->GetAction(IM_KEYDOWN);
    if (!action) return;
    action >>= 8;
    const float halfWidth = static_cast<float>(static_cast<double>(Graph->screenWidth) * 0.5);
    const int inventoryId = input->ClientMouseXY().x < halfWidth &&
        input->ClientMouseXY().y < halfWidth ? INV_MAIN : INV_SHOP;
    if (action == 33u) PageUp(inventoryId);
    else if (action == 34u) PageDown(inventoryId);
    else if (action == 36u) GoHome(inventoryId);
    else if (action == 35u) GoEnd(inventoryId);
}



bool zShopDlgMenu::TryToSellItem(iRPGObj* item, unsigned int mode)
{
    if (!item)
    {
        DebugAssert(5u, "_pItem", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1407u);
        return false;
    }
    if (item->GetBuyCostFull() == 0) return false;
    if (item->GetNumInCell() <= 0)
        DebugAssert(4u, "_pItem->GetNumInCell() > 0", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1412u);
    const int soldCount = m_rpgHeroRef.m_object->TryToSellObjj(item, static_cast<unsigned char>(mode) != 0u);
    if (soldCount)
    {
        m_player->PlaySFx(45, false);
        iRPGObj* baseObject = item->GetObjCopy();
        baseObject->SetNumInCell(soldCount);
        m_objects.Append(&baseObject);
        m_objectType = static_cast<OBJ_TYPE>(15);
        m_extType = static_cast<OBJ_EXTTYPE>(21);
        (void)baseObject->GetAddMoney();
        (void)baseObject->GetGameObj();
        m_heroInvMenu->ResetSelItem();
    }
    else
        m_player->PlaySFx(46, false);
    if (static_cast<unsigned char>(mode) != 0u) return soldCount != 0;
    return item->GetNumInCell() == 0;
}

int  zShopDlgMenu::TryToBuyItem(iRPGObj* _pItem)
{
    if (!_pItem || !m_rpgHeroRef.m_object)
        DebugAssert(5u, "_pItem && m_pRPGHero", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1040u);

    iRPGObj* moved = 0;
    const int buyResult = m_rpgHeroRef.m_object->TryToBuyObj(_pItem, &moved);
    if (buyResult)
    {
        m_player->PlaySFx(45, false);
        if (moved)
        {
            if (_pItem->GetNumInCell() == 0 && m_heroInvMenu->GetSelItem() == _pItem)
            {
                m_heroInvMenu->ResetSelItem();
                m_heroInvMenu->HighLightItem(moved);
            }
            return buyResult;
        }
        m_heroInvMenu->ResetSelItem();
        if (_pItem && _pItem->GetNumInCell() == 0)
            zScriptExec::m_pShopItemInfo->ShowObjInfo(0, false, 0);
        return buyResult;
    }

    m_player->PlaySFx(46, false);
    if (!m_player) return buyResult;
    iRPGHero* const moneyHero = m_rpgHeroRef.m_object;
    const int money = moneyHero->GetMoney();
    const int requiredMoney = _pItem->GetBuyCost();
    if (requiredMoney>money)
    {
        m_player->SendInfoMessage(3, 0);
        return buyResult;
    }
    if (m_rpgHeroRef.m_object->CanBuyItem(_pItem))
        m_player->SendInfoMessage(1, 0);
    return buyResult;
}

zShopDlgMenu::zShopDlgMenu()
    : zBaseDlgMenu(), m_filterButtonCount(0), m_buttonsFound(false),
      m_generatedMode(false), m_framework(0), m_heroInvMenu(0),
      m_objectType(static_cast<OBJ_TYPE>(15)), m_extType(static_cast<OBJ_EXTTYPE>(21)),
      m_player(0), m_curLevel(0)
{
    for (int i = 0; i < 9; ++i) m_categoryButtons[i] = 0;
    for (int i = 0; i < 3; ++i) m_buyAnim[i] = 0;
}


zShopListLayout::zShopListLayout()
    : m_proxy(static_cast<zContainerProxy<zShopListLayout>*>(CrtNew(sizeof(zContainerProxy<zShopListLayout>))))
{
    if (m_proxy) m_proxy->m_owner = this;
    try
    {
        m_sentinel = static_cast<zShopListNode*>(CrtNew(sizeof(zShopListNode)));
        m_sentinel->next = m_sentinel;
        m_sentinel->previous = m_sentinel;
        m_size = 0;
    }
    catch (...)
    {
        CrtFreePointer(m_proxy);
        throw;
    }
}

zShopObjectPointerVector::zShopObjectPointerVector()
    : m_proxy(static_cast<zContainerProxy<zShopObjectPointerVector>*>(CrtNew(sizeof(zContainerProxy<zShopObjectPointerVector>)))),
      m_begin(0), m_end(0), m_capEnd(0)
{
    if (m_proxy) m_proxy->m_owner = this;
}

zShopObjectPointerVector::~zShopObjectPointerVector()
{
    if (m_begin)
        CrtFreePointer(m_begin);
    m_begin = m_end = m_capEnd = 0;
    CrtFreePointer(m_proxy);
}

zShopListLayout::~zShopListLayout()
{
    Clear();
    CrtFreePointer(m_sentinel);
    m_sentinel = 0;
    CrtFreePointer(m_proxy);
}

zShopDlgMenu::~zShopDlgMenu()
{
    ReleaseListOfObjects();
}

bool zShopDlgMenu::InitLoad()
{
    if (m_buttonsFound) return false;
    TryToFindButtons();
    if (!m_buttonsFound) return false;
    m_framework->ResetShopItemFlags();
    const zShopListLayout::Iterator end = m_objects.End();
    for (zShopListLayout::Iterator node = m_objects.Begin();
        node != end; node.Next())
        static_cast<zItem*>(node.Value()->QueryInterface(49))->m_priceFlags |= 1u;
    RefreshBuyButtons();
    return true;
}


void zShopDlgMenu::ReloadAll()
{
}







zShopListLayout::Iterator zShopListLayout::Begin() const
{
    Iterator iterator = { m_proxy, m_sentinel->next };
    return iterator;
}

zShopListLayout::Iterator zShopListLayout::End() const
{
    Iterator iterator = { m_proxy, m_sentinel };
    return iterator;
}

bool zShopListLayout::Iterator::operator!=(const Iterator& other) const
{
    if (!m_owner || m_owner != other.m_owner)
        ::_invalid_parameter_noinfo();
    return m_node != other.m_node;
}

iRPGObj* zShopListLayout::Iterator::Value() const
{
    if (!m_owner)
        ::_invalid_parameter_noinfo();
    zShopListLayout* const list = m_owner ? m_owner->m_owner : 0;
    if (m_node == list->m_sentinel)
        ::_invalid_parameter_noinfo();
    return m_node->value;
}

void zShopListLayout::Iterator::Next()
{
    if (!m_owner)
        ::_invalid_parameter_noinfo();
    zShopListLayout* const list = m_owner ? m_owner->m_owner : 0;
    if (m_node == list->m_sentinel)
        ::_invalid_parameter_noinfo();
    m_node = m_node->next;
}

zShopListLayout::Iterator zShopListLayout::Erase(Iterator position)
{
    zShopListNode* const erased = position.m_node;
    position.Next();
    if (erased != m_sentinel)
    {
        erased->previous->next = erased->next;
        erased->next->previous = erased->previous;
        CrtFreePointer(erased);
        --m_size;
    }
    Iterator next = { m_proxy, position.m_node };
    return next;
}

void zShopListLayout::Append(iRPGObj* const* itemReference)
{
    zShopListNode* const next = m_sentinel;
    zShopListNode* const previous = m_sentinel->previous;
    zShopListNode* const node = static_cast<zShopListNode*>(CrtNew(sizeof(zShopListNode)));
    node->next = next;
    node->previous = previous;
    node->value = *itemReference;
    if (0x3fffffffu-m_size < 1u)
        throw std::length_error("list<T> too long");
    ++m_size;
    next->previous = node;
    node->previous->next = node;
}

void zShopListLayout::Clear()
{
    zShopListNode* node = m_sentinel->next;
    m_sentinel->next = m_sentinel;
    m_sentinel->previous = m_sentinel;
    m_size = 0;
    while (node != m_sentinel)
    {
        zShopListNode* const next = node->next;
        CrtFreePointer(node);
        node = next;
    }
}

void zShopObjectPointerVector::Clear()
{
    iRPGObj** const oldEnd = m_end;
    if (m_begin > oldEnd)
        ::_invalid_parameter_noinfo();
    iRPGObj** const first = m_begin;
    if (first > m_end)
        ::_invalid_parameter_noinfo();
    if (m_begin > first || first > m_end)
        ::_invalid_parameter_noinfo();
    zContainerProxy<zShopObjectPointerVector>* const owner = m_proxy;
    if (m_begin > oldEnd || oldEnd > m_end)
        ::_invalid_parameter_noinfo();
    if (!owner || owner != m_proxy)
        ::_invalid_parameter_noinfo();
    if (first != oldEnd)
    {
        const int remaining = static_cast<int>(m_end-oldEnd);
        iRPGObj** const newEnd = first+remaining;
        if (remaining > 0)
            ::memmove_s(first, remaining*sizeof(iRPGObj*), oldEnd, remaining*sizeof(iRPGObj*));
        m_end = newEnd;
    }
}




void zShopDlgMenu::RefreshBuyButtons()
{
    for (int i = 0; i < 3; ++i)
    {
        zScriptExec::m_pShopItemInfo->GetButton(i);
        zScriptExec::m_pShopItemInfo->GetButtonText(i);
    }
    iRPGObj* const item = zScriptExec::m_pShopItemInfo ?
        zScriptExec::m_pShopItemInfo->GetCurrentItem() : 0;
    if (!item || item->GetInvId() == -1)
    {
        for (int i = 0; i < 3; ++i)
        {
            SetItemButton(i, false, true, false, STRING(""), 0);
            m_buyAnim[i] = 0;
        }
        return;
    }
    zShopActionVector actions;
    bool ammoAvailable = true;
    bool affordable = true;
    const bool buying = m_heroInvMenu->GetBuyMode(item);
    if (m_heroInvMenu->IsShopMode())
    {
        if (item->GetGameObj() != 7)
        {
            actions.Append(buying ? 8 : 1);
            if (item->GetBuyCost() > m_rpgHeroRef.m_object->GetMoney()) affordable = false;
        }
        if (item->GetGameObj() == TYPE_WEAPON)
        {
            zRPGAmmo* const ammo = static_cast<zRPGWeapon*>(item->QueryInterface(50))->GetAmmo();
            if (ammo && !static_cast<iRPGObj*>(ammo)->IsInfinite())
            {
                actions.Append(5);
                ammoAvailable = BuyAmmoForWeapon(item, true);
            }
        }
        if (item->GetGameObj() == TYPE_AMMO && !item->IsInfinite())
        {
            actions.Append(5);
            ammoAvailable = BuyAmmoForWeapon(item, true);
        }
        if (item->GetGameObj() == TYPE_ARMOR && !buying &&
            static_cast<iRPGArmor*>(item->QueryInterface(51))->GetRepairCost() != 0)
            actions.Append(4);
    }
    if (item->GetGameObj() != 7 && item->GetGameObj() != TYPE_AMMO && !buying && item->GetInvId() != INV_GRND)
        actions.Append(item->GetInvId() == 0 ? 2 : 3);
    if (!m_heroInvMenu->IsShopMode() && item->GetGameObj() != 7)
    {
        if (item->GetInvId() == INV_GRND) actions.Append(7);
        else if (item->GetGameObj() != TYPE_AMMO) actions.Append(6);
    }
    for (int end = static_cast<int>(actions.Size()) - 1; end >= 0; --end)
    {
        int swapIndex = end;
        for (int i = 0; i < end; ++i)
            if (actions.At(i) < actions.At(end)) swapIndex = i;
        const int saved = actions.At(end);
        actions.At(end) = actions.At(swapIndex);
        actions.At(swapIndex) = saved;
    }
    if (actions.Size() > 3u)
        DebugAssert(5u, "aActs.size() <= IF_BTNS_CNT", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 554u);
    int slot = 2;
    for (int i = static_cast<int>(actions.Size()) - 1; i >= 0 && slot >= 0; --i, --slot)
    {
        const int action = actions.At(i);
        m_buyAnim[slot] = action;
        const bool disabled = action == 5 && !ammoAvailable;
        const bool unaffordable = action == 8 && !affordable;
        int cost = 0;
        if (action == 8) cost = SubSigned32Wrap(0, item->GetBuyCost());
        if (action == 1) cost = item->GetBuyCost();
        if (action == 4)
            cost = SubSigned32Wrap(0, static_cast<iRPGArmor*>(item->QueryInterface(51))->GetRepairCost());
        SetItemButton(slot, true, disabled, unaffordable, GetItemActionText(action), cost);
    }
    for (; slot >= 0; --slot)
    {
        m_buyAnim[slot] = 0;
        SetItemButton(slot, false, true, false, STRING(""), 0);
    }
}

STRING zShopDlgMenu::GetItemActionText(int action)
{
    static const char* const keys[9] = {"", "Sell", "IF_Wear", "IF_UnWear", "Repair",
        "BuyAmmo", "If_Drop", "If_Pickup", "Buy"};
    if (static_cast<unsigned int>(action) > 8u || action == 0) return STRING("");
    return ::Strings->GetString(STRING("menu"), STRING(keys[action]), STRING(""));
}

void zShopDlgMenu::SetItemButton(int index, bool visible, bool disabled, bool unaffordable,
    STRING text, int cost)
{
    if (index < 0 || index >= 3)
        DebugAssert(5u, "_iNum >= 0 && _iNum < IF_BTNS_CNT", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 340u);
    SPRITE* const button = zScriptExec::m_pShopItemInfo->GetButton(index);
    if (button)
    {
        button->Action(ACT_SET_INVISIBLE, visible ? 0 : 1, 0, 0);
        if (!visible || disabled) button->ChangeAnimation(12);
        else if (button->Animation() == 12) button->ChangeAnimation(0);
    }
    SPRITE* const label = Map->MenuState().SpriteWithName(Printf("IF_BtnText_%d", index + 1));
    if (label)
    {
        label->Action(ACT_SET_TEXT, BitsToSigned32(PtrToU32(&text)), 0, 0);
        GAMMA gamma;
        gamma.modulate = 0;
        gamma.add = disabled ? 0x646464u : (unaffordable ? 0xFF0000u : 0u);
        label->SetGamma(gamma);
    }
    SPRITE* const price = Map->MenuState().SpriteWithName(Printf("IF_BtnCost_%d", index + 1));
    if (price)
    {
        STRING value("");
        if (cost > 0) value = Printf("$%d", cost);
        else if (cost < 0) value = Printf("-$%d", SubSigned32Wrap(0, cost));
        price->Action(ACT_SET_TEXT, BitsToSigned32(PtrToU32(&value)), 0, 0);
        if (label && cost != 0)
        {
            VECTOR coor = price->Coor();
            coor.y = static_cast<float>(static_cast<double>(coor.y) - 11.0);
            label->ChangeCoor(coor);
            label->Action(ACT_SET_BEHAVE, 9, 0, 0);
        }
    }
}

void zShopDlgMenu::ProcessItemButton(SPRITE* button)
{
    for (int i = 0; i < 3; ++i)
    {
        if (button == zScriptExec::m_pShopItemInfo->GetButton(i))
        {
            ExecuteItemAction(m_buyAnim[i]);
            return;
        }
    }
    DebugAssert(4u, "0", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 285u);
}

void zShopDlgMenu::ExecuteItemAction(int action)
{
    if (!m_heroInvMenu || !zScriptExec::m_pShopItemInfo)
        DebugAssert(5u, "m_pHeroInvMenu && g_pShopItemInfo", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1101u);
    iRPGObj* const item = zScriptExec::m_pShopItemInfo->GetCurrentItem();
    bool close = false;
    if (action == 8)
        close = (TryToBuyItem(item) & 1) != 0;
    else if (action == 1)
        close = TryToSellItem(item, 0u);
    else if (action == 4)
    {
        if (item->GetGameObj() != TYPE_ARMOR)
            DebugAssert(5u, "item->GetType() == TYPE_ARMOR", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1116u);
        if (item->GetGameObj() == TYPE_ARMOR)
        {
            iRPGArmor* const armor = static_cast<iRPGArmor*>(item->QueryInterface(51));
            iRPGHero* const hero = m_rpgHeroRef.m_object;
            hero->AddMoney(SubSigned32Wrap(0, armor->Repair(hero->GetMoney())), false);
            zScriptExec::m_pShopItemInfo->Refresh();
        }
    }
    else if (action == 5)
        BuyAmmoForWeapon(item, false);
    else if (action == 2 || action == 3)
    {
        m_invServerRef.m_object->PutToActiveInv(item);
        close = true;
    }
    else if (action == 6)
    {
        if (m_invServerRef.m_object->TakeItem(item))
        {
            zInventory* const ground = static_cast<zInventory*>(m_invServerRef.m_object->GetInv(INV_GRND)->QueryInterface(46));
            close = ground->PutItem(static_cast<zItem*>(item->QueryInterface(49)), 0) != 0;
        }
    }
    else if (action == 7)
    {
        if (m_invServerRef.m_object->TakeItem(item))
        {
            zInventory* const main = static_cast<zInventory*>(m_invServerRef.m_object->GetInv(INV_MAIN)->QueryInterface(46));
            if (!(main->PutItem(static_cast<zItem*>(item->QueryInterface(49)), 0) & 1) && item->GetNumInCell() > 0)
            {
                m_rpgHeroRef.m_object->CreateGroundSprite(item);
                m_invServerRef.m_object->ControlGrndInventory(0xFFFFFFFFu);
            }
        }
        close = true;
    }
    else
        DebugAssert(5u, "0", "..\\src\\zm\\Menu\\zShopDlgMenu.cpp", 1161u);
    if (close) zScriptExec::m_pShopItemInfo->ShowObjInfo(0, false, 0);
    RefreshAll();
}





