#include "mylib.h"
#include "../../engine_globals.h"
#include "list.h"

#include "zInvServer.h"
#include "myerror.h"
#include "zm/Utils/zRefObject.h"
#include "zm/Utils/zLogConsole.h"
#include <functional>
#include <stdexcept>
#include "zm/Inventory/zInventory.h"
#include "zm/Inventory/zInventory2D.h"
#include "zm/Inventory/zInventory3D.h"
#include "zm/Inventory/zInventoryItem.h"
#include "zm/Utils/String/zString.h"
#include "zm/RPGHero/zRPGWeapon.h"
#include "zm/RPGHero/zRPGAmmo.h"
#include "zm/RPGHero/zRPGEquip.h"
#include "zm/RPGHero/zRPGArmor.h"
#include "zm/Menu/zHeroInvMenu.h"
#include "zm/Menu/zShopDlgMenu.h"
#include "zm/Game/ScriptExec/zScriptExec.h"
#include <new>
#include <cstring>

#include "basetype.h"

iInvKeeper* zInvServer::Keeper() const
{
    return static_cast<iInvKeeper*>(m_heroRef.m_object->QueryInterface(1));
}

bool zInvServer::IsForBuy()
{
    return m_bForBuy;
}

const zItem* zInvServer::GetTakenItem() const
{
    return m_takenRef.m_object;
}

bool zInvServer::GetMouseDirty()
{
    const bool result = m_mouseDirty != 0;
    m_mouseDirty = 0;
    return result;
}

void zInvServer::GetMouseDisplacement(float& outX, float& outY) const
{
    outX = m_mouseOffsetX;
    outY = m_mouseOffsetY;
}

void zInvServer::SetParent(iHeroInvMenu* parent)
{
    m_parentMenu = parent;
}

void zInvServer::SetPlayer(iPlayer* player)
{
    m_player = player;
}

iInventory* zInvServer::GetInv(int index)
{
    return m_apInv[index] ? static_cast<iInventory*>(m_apInv[index]) : 0;
}

const iRPGObj* zInvServer::GetTakenObj() const
{
    return static_cast<const iRPGObj*>(GetTakenItem());
}


void __cdecl GetNew_zInvServer(iInvServer** outObject)
{
    void* storage = static_cast<unsigned char*>(CrtNew(sizeof(zInvServer)));
    zInvServer* object = storage ? new (storage) zInvServer() : 0;
    *outObject = object ? static_cast<iInvServer*>(object) : 0;
}

zInvServer::zInvServer()
    : m_heroRef
{
    0, 0, 0
},
      m_takeOriginX(0.0f), m_takeOriginY(0.0f), m_pItemTaker(0),
      m_takenRef
{
    0, 0, static_cast<iRefOwner*>(this)
},
      m_mouseOffsetX(0.0f), m_mouseOffsetY(0.0f), m_mouseDirty(0), m_parentMenu(0),
      m_bForBuy(0), m_failedBuyPlacement(0), m_player(0)
{
}

void zInvServer::SetInventoryCoord(int _iNum, float x, float y)
{
    if (_iNum >= INV_CNT)
        DebugAssert(5, "_iNum < INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp",
            104);

    zInventory3D* target = m_inventory3D[_iNum];
    if (target)
    {
        target->SetCoord(x, y);
    }
}

void* zInvServer::QueryInterface(int interfaceId) const
{
    if (interfaceId == 31)
        return const_cast<iInvServer*>(static_cast<const iInvServer*>(this));
    return zRefObject::QueryInterface(interfaceId);
}

int zInvServer::iRelease()
{
    if (this)
        delete this;
    return 0;
}

zInvServer::~zInvServer()
{
    for (int i = INV_MAIN; i != INV_CNT; ++i)
    {
        if (m_apInv[i] != 0)
        {
            m_apInv[i]->iRelease();
            m_apInv[i] = 0;

            m_inventory3D[i] = 0;
        }
    }

}

iRPGObj* zInvServer::TryToTakeItem(float x, float y)
{
    zItem* found = 0;
    int index = 0;
    for (; index < INV_CNT; ++index)
    {
        zInventory3D* inventory = m_inventory3D[index];
        if (inventory == 0 || inventory->m_allowPointTake == 0)
            continue;
        found = inventory->GetItemByPoint(x, y);
        if (found != 0)
            break;
    }
    if (found == 0)
        return 0;

    zInventory3D* inventory = m_inventory3D[index];
    (void) inventory->GetItemPos(found, &m_takeOriginX, &m_takeOriginY);
    inventory = m_inventory3D[index];
    inventory->TakeItem(found);
    m_mouseOffsetX = x - m_takeOriginX;
    m_mouseOffsetY = y - m_takeOriginY;

    m_takenRef.Set(found);

    m_mouseDirty = 1;
    if (index == INV_SHOP || index == INV_AMMOSHOP)
        m_bForBuy = 1;
    return static_cast<iRPGObj*>(found);
}

int zInvServer::GetInventoryToPutItem(
    zItem* item, float x, float y, zItem*& outOwner)
{
    int result = -1;
    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        zInventory3D* inventory = m_inventory3D[i];
        if (inventory == 0)
            continue;
        const unsigned char fits = inventory->CanPutItemToPoint(item, x, y) ? 1u : 0u;
        inventory = m_inventory3D[i];
        const unsigned char clipped = inventory->m_pointClipped;
        outOwner = inventory->m_displacedItem;
        if (fits != 0)
        {
            result = i;
            if (clipped == 0)
                break;
        }
    }
    return result;
}

int zInvServer::TryToPutItemBack()
{
    if (m_takenRef.m_object == 0)
        DebugAssert(4, "m_pTakenItem",
            "..\\src\\zm\\Inventory\\zInvServer.cpp",
            345);

    int result = 0;
    if (m_takeOriginX != 0.0f || m_takeOriginY != 0.0f)
    {
        const float x = m_takeOriginX;
        const float y = m_takeOriginY;
        m_takeOriginX = 0.0f;
        m_takeOriginY = 0.0f;
        result = TryToPutItem(x, y, false);
    }
    if ((result & 1) != 0)
        m_takenRef.Set(static_cast<zItem*>(0));
    return result;
}

zItem* zInvServer::GetItemByPoint(float x, float y) const
{
    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        if (m_inventory3D[i] == 0)
            continue;
        zItem* const result = m_inventory3D[i]->GetItemByPoint(x, y);
        if (result != 0)
            return result;
    }
    return 0;
}

iRPGObj* zInvServer::GetObjByPoint(float x, float y) const
{
    return static_cast<iRPGObj*>(GetItemByPoint(x, y));
}

const iInventory* zInvServer::GetInventoryByPoint(float x, float y) const
{
    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        zInventory3D* inventory = m_inventory3D[i];
        if (inventory == 0)
            continue;
        const int pixelY = TruncateDoubleViaCrtToInt32(static_cast<double>(y) - inventory->m_originY + inventory->m_scrollY);
        const int pixelX = TruncateDoubleViaCrtToInt32(static_cast<double>(x) - inventory->m_originX + inventory->m_scrollX);
        if (inventory->SmoothToGrid(false, pixelX, pixelY, 0, 0))
            return m_apInv[i] ? static_cast<const iInventory*>(m_apInv[i]) : 0;
    }
    return 0;
}

bool zInvServer::CanPutToInv(int _iInvNum, const zItem* value) const
{
    if (_iInvNum < 0 || _iInvNum >= INV_CNT)
        DebugAssert(5, "_iInvNum >= 0 && _iInvNum < INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp",
            420);
    zInventory* inventory = m_apInv[_iInvNum];
    return inventory->IfObjectCanBePut(value, false) != 0;
}

bool zInvServer::CanPutToInv(int index, const iRPGObj* object) const
{
    zItem* const value = static_cast<zItem*>(
        const_cast<iRPGObj*>(object)->QueryInterface(49));
    return CanPutToInv(index, value);
}


int zInvServer::TryToPutItem(float x, float y, bool action)
{
    if (m_takenRef.m_object == 0)
        DebugAssert(4, "m_pTakenItem",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 170);

    const float localX = x - m_mouseOffsetX;
    const float localY = y - m_mouseOffsetY;
    zItem* placedOwner = 0;
    int index = GetInventoryToPutItem(
        m_takenRef.m_object, localX, localY, placedOwner);

    const int sellCost = m_takenRef.m_object->GetBuyCostFull();

    unsigned char notifyHero = m_bForBuy == 0 ? 1 : 0;
    if (notifyHero != 0)
    {
        if (index == INV_SHOP || index == INV_AMMOSHOP)
        {
            if (m_failedBuyPlacement != 0)
                notifyHero = 0;
        }
        else
        {
            notifyHero = 0;
        }
    }
    else if (m_failedBuyPlacement != 0)
    {
        notifyHero = 0;
    }

    const unsigned char initialBuy = m_bForBuy;
    m_failedBuyPlacement = 0;
    if (initialBuy != 0 && (index == INV_SHOP || index == INV_AMMOSHOP))
        m_bForBuy = 0;

    int result = 0;
    if (m_bForBuy == 0 && index >= 0)
    {
        zInventory3D* inventory = m_inventory3D[index];
        result = inventory->PutItemToPoint(m_takenRef.m_object, localX, localY);
        if (result == 0)
            DebugAssert(4, "ret != ITEM_ADDEDNOT",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 211);

        zItem* const placedItem = inventory->m_displacedItem;
        if ((result & 2) != 0 && (result & 4) != 0)
        {
            const int buff = TryToPutItemBack();
            if ((buff & 1) == 0)
                DebugAssert(4, "buf & ITEM_ADDEDFULL",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 203);
        }
        else if (placedItem != 0 && (((result & 2) == 0) || ((result & 4) != 0)))
        {
            zItem* const PrevTakenItem = m_takenRef.m_object;
            m_takenRef.Set(placedItem);

            int buff = TryToPutItemBack();
            if (buff == 0)
            {
                buff = PutItemEx(static_cast<iRPGObj*>(m_takenRef.m_object), 0);
                if ((buff & 1) != 0)
                    result = buff;
            }
            if (buff == 0)
            {
                int retry = inventory->PutItemToPoint(m_takenRef.m_object, localX, localY);
                if (retry == 0)
                    DebugAssert(4, "ret != ITEM_ADDEDNOT",
                        "..\\src\\zm\\Inventory\\zInvServer.cpp", 247);
                zItem* const PlacedItem = inventory->m_displacedItem;
                if (PlacedItem == 0 || PlacedItem != PrevTakenItem)
                    DebugAssert(4, "PlacedItem && PlacedItem == PrevTakenItem",
                        "..\\src\\zm\\Inventory\\zInvServer.cpp", 251);
                m_takenRef.Set(PlacedItem);
                result = 0;
            }
        }
    }
    else
    {
        zItem* const takenItem = m_takenRef.m_object;
        result = m_parentMenu->TryProcessItemPut(
            static_cast<iRPGObj*>(takenItem), localX, localY, m_bForBuy != 0);
        if (takenItem != 0 && takenItem->GetNumInCell() == 0)
            result |= 1;
        if ((result & 4) != 0 && (result & 1) == 0)
        {
            const int buff = TryToPutItemBack();
            if ((buff & 1) == 0)
                DebugAssert(4, "buf & ITEM_ADDEDFULL",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 203);
        }
    }

    if (result == 0 && action)
    {
        if (m_bForBuy != 0)
            m_failedBuyPlacement = 1;
        m_bForBuy = 0;

        int buff = TryToPutItemBack();
        if ((buff & 1) != 0)
        {
            return result;
        }

        buff = PutItemEx(static_cast<iRPGObj*>(m_takenRef.m_object), 0);
        if ((buff & 1) != 0)
        {
            result = buff;
        }
        else
        {
            zInventory* groundInventory = m_apInv[INV_GRND];
            buff = groundInventory->PutItem(m_takenRef.m_object, static_cast<zItem**>(0));
            if ((buff & 1) != 0)
            {
                result = buff;
            }
            else
            {
                zItem* takenItem = m_takenRef.m_object;
                if (!m_pItemTaker->CreateGroundSprite(static_cast<iRPGObj*>(takenItem)))
                    DebugAssert(4, "buff",
                        "..\\src\\zm\\Inventory\\zInvServer.cpp", 279);
                result = 1;
            }
        }
    }

    if (result != 0)
    {
        if (notifyHero != 0)
        {
            if ((result & 1) == 0)
                DebugAssert(4, "ret&ITEM_ADDEDFULL",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 289);
            (void) m_heroRef.m_object->AddMoney(sellCost, true);
            if (m_player != 0)
                m_player->PlaySFx(45, false);
        }
        m_mouseDirty = 1;
        m_takenRef.Set(static_cast<zItem*>(0));
        m_bForBuy = 0;
    }
    return result;
}

int zInvServer::TryToBuyObj(iRPGObj* object, float x, float y)
{
    zItem* itemInterface = static_cast<zItem*>(object->QueryInterface(49));
    return TryToBuyItem(itemInterface, x, y);
}

int zInvServer::TryToBuyItem(zItem* _pItem, float x, float y)
{
    if (m_heroRef.m_object == 0 || _pItem == 0)
        DebugAssert(5, "m_pRPGHero && _pItem",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 475);
    if (_pItem->IsBaseObject())
        DebugAssert(5u, "!_pItem->IsBaseObject()", "..\\src\\zm\\Inventory\\zInvServer.cpp", 476u);

    zItem* routeObject;
    int index = GetInventoryToPutItem(_pItem, x, y, routeObject);
    if (_pItem->GetGameObj() == TYPE_AMMO)
        index = -1;
    if (routeObject != 0 && index == INV_MAIN)
        index = -1;

    iRPGHero* const moneyHero = m_heroRef.m_object;
    const int itemValue = _pItem->GetBuyCostFull();
    if (itemValue > moneyHero->GetMoney())
    {
        if (m_player != 0)
            m_player->SendInfoMessage(3, 0);
        return 0;
    }

    if (m_heroRef.m_object->CanBuyItem(_pItem) == 0)
    {
        if (m_player != 0)
            m_player->SendInfoMessage(2, 0);
        return 0;
    }

    int resultFlags = 0;
    if (index < 0)
    {
        iRPGObj* routed = 0;
        resultFlags = m_heroRef.m_object->BuyItem(_pItem, true, &routed, true);
        (void) m_parentMenu->HighLightItem(routed);
    }
    else
    {
        iRPGObj* routedObject = 0;
        (void) m_heroRef.m_object->BuyItem(_pItem, false, &routedObject, true);
        if (_pItem->GetGameObj() == TYPE_WEAPON)
            zScriptExec::m_pShopDlgMenu->OnWeaponBought(_pItem);

        if (routedObject != 0)
        {
            (void) m_parentMenu->HighLightItem(routedObject);
        }

        zInventory3D* inventory = m_inventory3D[index];
        resultFlags = inventory->PutItemToPoint(_pItem, x, y);

        if ((resultFlags & 2) == 0 || (resultFlags & 4) != 0)
        {
            inventory = m_inventory3D[index];
            zItem* placedItem = inventory->m_displacedItem;
            if (placedItem != 0 && (resultFlags & 1) != 0)
            {
                zInventory* firstInventory = m_apInv[INV_MAIN];
                if ((firstInventory->PutItem(placedItem, static_cast<zItem**>(0)) & 1) == 0)
                {
                    inventory = m_inventory3D[index];
                    zItem* bbuf = inventory->TakeItem(_pItem);
                    if (bbuf == 0)
                        DebugAssert(4, "bbuf",
                            "..\\src\\zm\\Inventory\\zInvServer.cpp", 539);

                    resultFlags = 0;
                    inventory = m_inventory3D[index];
                    const int buf2 = inventory->PutItem(
                        placedItem, static_cast<zItem**>(0));
                    if (buf2 != 1)
                        DebugAssert(4, "buf2 == ITEM_ADDEDFULL",
                            "..\\src\\zm\\Inventory\\zInvServer.cpp", 543);
                }
            }

            if (_pItem->IsBaseObject())
                DebugAssert(5, "!_pItem->IsBaseObject()",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 551);

            if ((resultFlags & 1) == 0)
            {
                zInventory* firstInventory = m_apInv[INV_MAIN];
                resultFlags = firstInventory->PutItem(_pItem, static_cast<zItem**>(0));
                if ((resultFlags & 1) == 0)
                    resultFlags = m_heroRef.m_object->PutItem(_pItem);
            }
        }
    }

    if (resultFlags == 0)
    {
        DebugAssert(2, "0",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 566);
        if (m_player != 0)
            m_player->SendInfoMessage(1, 0);
    }

    return resultFlags;
}

bool zInvServer::TryToTakeObj(iRPGObj* object, bool forBuy)
{
    zItem* itemInterface = static_cast<zItem*>(object->QueryInterface(49));
    return TryToTakeItem(itemInterface, forBuy);
}

bool zInvServer::TryToTakeItem(zItem* _pItem, bool forBuy)
{
    if (m_takenRef.m_object != 0 && _pItem != 0)
        DebugAssert(4, "!m_pTakenItem || !_pItem",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 589);
    m_mouseDirty = 1;
    m_takenRef.Set(_pItem);
    m_bForBuy = (unsigned char) forBuy;
    return 1;
}

bool zInvServer::PutToActiveInv(float x, float y)
{
    zItem* item = GetItemByPoint(x, y);
    if (item == 0)
        return 0;
    return PutToActiveInv(static_cast<iRPGObj*>(item));
}

bool zInvServer::PutToActiveInv(iRPGObj* object)
{
    if (object == 0)
        return 0;
    if (object->GetGameObj() == 7)
        return 0;

    zItem* itemInterface = static_cast<zItem*>(object->QueryInterface(49));
    zInventory* ownerInventory = itemInterface->m_pInventory;
    if (ownerInventory->GetId() == INV_SHOP)
        return 0;

    if (ownerInventory != 0 && ownerInventory->IsActive())
    {
        zInventory* inventory0 = m_apInv[INV_MAIN];
        if (inventory0->IfObjectCanBePut(itemInterface, true) != 0)
        {
            ownerInventory->TakeItem(itemInterface);
            inventory0 = m_apInv[INV_MAIN];
            inventory0->PutItem(itemInterface, static_cast<zItem**>(0));
        }
        return 0;
    }

    int selected = -1;
    for (int i = INV_MAIN; i < INV_GRND; ++i)
    {
        zInventory* inventory = m_apInv[i];
        if (!inventory->IsActive())
            continue;
        inventory = m_apInv[i];
        if (inventory->IfObjectCanBePut(itemInterface, false) == 0)
            continue;
        selected = i;
        inventory = m_apInv[i];
        if (inventory->IfObjectCanBePut(itemInterface, true) != 0)
            break;
    }
    if (selected >= 0)
        (void) ChangeWpn_(selected, itemInterface);
    return 0;
}

iRPGObj* zInvServer::GetObj(int index, OBJ_TYPE objectType, OBJ_EXTTYPE objectExtType, int flags) const
{
    return static_cast<iRPGObj*>(GetItem(index, objectType, objectExtType, flags));
}

zItem* zInvServer::GetItem(int _iInvNum, OBJ_TYPE objectType, OBJ_EXTTYPE objectExtType, int flags) const
{
    if (_iInvNum < 0 || _iInvNum > INV_CNT)
        DebugAssert(5, "_iInvNum >= 0 && _iInvNum <= INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 670);

    zInventory* inventory = _iInvNum == INV_CNT
        ? static_cast<zInventory*>(m_inventory3D[INV_MAIN]) : m_apInv[_iInvNum];
    if ((flags & 1) != 0 && !inventory->IsActive())
        return 0;

    zItem* candidate = inventory->FirstItem(objectType, objectExtType);
    if (candidate == 0)
        return 0;
    if ((flags & 2) == 0)
        return candidate;

    for (; ;)
    {
        iInvKeeper* acceptor = Keeper();
        if (acceptor->CanHoldItm(candidate))
            return candidate;
        candidate = inventory->NextItem();
        if (candidate == 0)
            return 0;
    }
}

bool zInvServer::ChangeWpn_(
    int _iInvNum1, int _iInvNum2)
{
    if (_iInvNum1 < 0 || _iInvNum1 > INV_CNT)
        DebugAssert(5, "_iInvNum1 >= 0 && _iInvNum1 <= INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 694);
    if (_iInvNum2 < 0 || _iInvNum2 > INV_CNT)
        DebugAssert(5, "_iInvNum2 >= 0 && _iInvNum2 <= INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 695);
    if (_iInvNum1 == _iInvNum2)
        DebugAssert(5, "_iInvNum1 != _iInvNum2",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 696);

    zItem* sourceItem = GetItem(
        _iInvNum1, static_cast<OBJ_TYPE>(15), static_cast<OBJ_EXTTYPE>(21), 0);
    int value;
    if (sourceItem != 0)
    {
        value = sourceItem->GetAddMoney();
    }
    else
    {
        zInventory* sourceInventory = _iInvNum1 == INV_CNT
            ? static_cast<zInventory*>(m_inventory3D[INV_MAIN]) : m_apInv[_iInvNum1];
        value = sourceInventory->GetObjectExtType();
    }

    zItem* destinationItem = GetItem(
        _iInvNum2, static_cast<OBJ_TYPE>(15), static_cast<OBJ_EXTTYPE>(value), 0);
    if (destinationItem == 0)
        return 0;

    if (sourceItem != 0)
    {
        unsigned char result = ChangeItems(sourceItem, destinationItem, false) ? 1u : 0u;
        if (result == 0)
        {
            destinationItem = GetItem(
                _iInvNum2, static_cast<OBJ_TYPE>(15), static_cast<OBJ_EXTTYPE>(value), 0);
            if (destinationItem != 0)
                result = ChangeItems(sourceItem, destinationItem, false) ? 1u : 0u;
        }
        return result;
    }

    return ChangeWpn_(_iInvNum1, destinationItem);
}

bool zInvServer::ChangeWpn_(int _iInvNum1, zItem* item)
{
    if (_iInvNum1 < 0 || _iInvNum1 > INV_CNT)
        DebugAssert(5, "_iInvNum1 >= 0 && _iInvNum1 <= INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 746);

    const int value = item->GetAddMoney();
    zItem* existing = GetItem(
        _iInvNum1, static_cast<OBJ_TYPE>(15), static_cast<OBJ_EXTTYPE>(value), 0);

    if (existing == 0)
    {
        if (item != 0)
        {
            item->m_pInventory->TakeItem(item);
            zInventory* inventory = m_apInv[_iInvNum1];
            if ((inventory->PutItem(item, static_cast<zItem**>(0)) & 1) == 0)
                DebugAssert(4, "buf & ITEM_ADDEDFULL",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 755);
            return 1;
        }
        return 0;
    }

    unsigned char result = 0;
    if (item != 0)
    {
        result = ChangeItems(existing, item, false) ? 1u : 0u;
        if (result == 0)
            DebugAssert(4, "ret",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 762);
    }
    return result;
}

static int InvSignedHalfProduct(int first, int second, int factor)
{
    const unsigned int delta = static_cast<unsigned int>(first) - static_cast<unsigned int>(second);
    const unsigned int product = delta * static_cast<unsigned int>(factor);
    if ((product & 0x80000000u) == 0)
        return static_cast<int>(product >> 1);
    const unsigned int magnitude = (~product) + 1u;
    return -static_cast<int>(magnitude >> 1);
}

bool zInvServer::ChangeItems(zItem* _pItem1, zItem* _pItem2, bool _bGrnd)
{
    if (_pItem1 == 0)
        DebugAssert(5, "_pItem1",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 775);
    if (_pItem2 == 0)
        DebugAssert(5, "_pItem2",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 776);

    zInventory* firstInventory = _pItem1->m_pInventory;
    zInventory* secondInventory = _pItem2->m_pInventory;
    unsigned char ExactPlace1 = 1;
    unsigned char ExactPlace2 = 1;
    unsigned char bothHaveInventories = 1;

    if (firstInventory == 0)
    {
        ExactPlace1 = 0;
        bothHaveInventories = 0;
        firstInventory = m_apInv[INV_MAIN];
    }
    if (secondInventory == 0)
    {
        ExactPlace2 = 0;
        bothHaveInventories = 0;
        secondInventory = m_apInv[INV_MAIN];
    }

    if (firstInventory->IsActive())
    {
        zInventory* previousInventory = firstInventory;
        firstInventory = secondInventory;
        secondInventory = previousInventory;
        zItem* previousItem = _pItem1;
        _pItem1 = _pItem2;
        _pItem2 = previousItem;
        const unsigned char previousHasInventory = ExactPlace1;
        ExactPlace1 = ExactPlace2;
        ExactPlace2 = previousHasInventory;
    }

    zInventory3D* firstGrid =
        static_cast<zInventory3D*>(firstInventory->QueryInterface(48));
    zInventory3D* secondGrid =
        static_cast<zInventory3D*>(secondInventory->QueryInterface(48));

    if (_pItem1 == _pItem2 || firstInventory == secondInventory)
        return 0;

    if ((secondInventory->IfObjectCanBePut(_pItem1, false) & 1) == 0 && _bGrnd == 0)
        return 0;
    if ((firstInventory->IfObjectCanBePut(_pItem2, false) & 1) == 0 && _bGrnd == 0)
        return 0;

    float firstX;
    float firstY;
    float secondX;
    float secondY;
    if (bothHaveInventories != 0)
    {
        (void) firstGrid->GetItemPos(_pItem1, &firstX, &firstY);
        (void) secondGrid->GetItemPos(_pItem2, &secondX, &secondY);
    }

    (void) firstInventory->TakeItem(_pItem1);
    (void) secondInventory->TakeItem(_pItem2);

    unsigned char firstPlaced = 0;
    unsigned char secondPlaced = 0;
    if (bothHaveInventories != 0)
    {
        int firstWidth;
        int firstHeight;
        int secondWidth;
        int secondHeight;
        _pItem1->GetSize2DBase(firstWidth, firstHeight);
        _pItem2->GetSize2DBase(secondWidth, secondHeight);

        if (firstWidth > secondWidth || firstHeight > secondHeight)
        {
            const int cellWidth = firstGrid->GetCellSizeX();
            const int cellHeight = firstGrid->GetCellSizeY();
            const int halfWidthDifference =
                InvSignedHalfProduct(firstWidth, secondWidth, cellWidth);
            const int halfHeightDifference =
                InvSignedHalfProduct(firstHeight, secondHeight, cellHeight);
            firstX = static_cast<float>(static_cast<double>(firstX)-halfWidthDifference);
            firstY = static_cast<float>(static_cast<double>(firstY)-halfHeightDifference);
        }

        firstPlaced = secondGrid->PutItemToPoint(_pItem1, secondX, secondY) == 1u ? 1u : 0u;
        if (firstPlaced == 0)
            firstPlaced = secondInventory->PutItem(_pItem1, static_cast<zItem**>(0)) == 1 ? 1u : 0u;

        secondPlaced = firstGrid->PutItemToPoint(_pItem2, firstX, firstY) == 1u ? 1u : 0u;
        if (secondPlaced == 0)
            secondPlaced = firstInventory->PutItem(_pItem2, static_cast<zItem**>(0)) == 1 ? 1u : 0u;
    }
    else
    {
        firstPlaced = secondInventory->PutItem(_pItem1, static_cast<zItem**>(0)) == 1 ? 1u : 0u;
        secondPlaced = firstInventory->PutItem(_pItem2, static_cast<zItem**>(0)) == 1 ? 1u : 0u;
    }

    if (firstPlaced == 0 && m_pItemTaker != 0 && _bGrnd != 0)
    {
        if (m_player->GetArcadeMod_() != 0)
            firstPlaced = 1;
        else
            firstPlaced = m_pItemTaker->CreateGroundSprite(static_cast<iRPGObj*>(_pItem1));
    }

    if (secondPlaced == 0 && m_pItemTaker != 0 && _bGrnd != 0)
    {
        if (m_player->GetArcadeMod_() != 0)
            secondPlaced = 1;
        else
            secondPlaced = m_pItemTaker->CreateGroundSprite(static_cast<iRPGObj*>(_pItem2));
    }

    if (firstPlaced != 0 && secondPlaced != 0)
        return 1;

    if (_bGrnd != 0)
        DebugAssert(4, "!_bGrnd",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 903);

    if (firstPlaced != 0)
    {
        zInventory* inv = _pItem1->m_pInventory;
        if (inv == 0)
        {
            DebugAssert(4, "inv",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 910);
        }
        else if (inv->TakeItem(_pItem1) == 0)
        {
            DebugAssert(4, "buf",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 914);
        }
    }

    if (secondPlaced != 0)
    {
        zInventory* inv = _pItem2->m_pInventory;
        if (inv == 0)
        {
            DebugAssert(4, "inv",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 920);
        }
        else if (inv->TakeItem(_pItem2) == 0)
        {
            DebugAssert(4, "buf",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 924);
        }
    }

    if ((firstInventory->PutItem(_pItem1, static_cast<zItem**>(0)) & 1) == 0 && ExactPlace1 != 0)
        DebugAssert(4, "aaa & ITEM_ADDEDFULL || !ExactPlace1",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 929);

    if ((secondInventory->PutItem(_pItem2, static_cast<zItem**>(0)) & 1) == 0 && ExactPlace2 != 0)
        DebugAssert(4, "aaa & ITEM_ADDEDFULL || !ExactPlace2",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 931);

    return firstPlaced != 0 && secondPlaced != 0 ? 1 : 0;
}

void zInvServer::OnInvalidate(iRef<iRefObject>* reference)
{
    if (static_cast<const void*>(reference) == static_cast<const void*>(&m_takenRef))
        m_mouseDirty = 1;
}

void zInvServer::SetItemTaker(iItemTaker* _pItemTaker)
{
    if (_pItemTaker == 0)
        DebugAssert(5, "_pItemTaker",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 952);
    m_pItemTaker = _pItemTaker;
}

void zInvServer::ControlGrndInventory(unsigned long tactTime)
{
    if (m_pItemTaker == 0)
        DebugAssert(5, "m_pItemTaker",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 962);

    zInventory* inventory = m_apInv[INV_GRND];
    const int count = inventory->m_apItems.begin == 0 ? 0 :
        static_cast<int>(inventory->m_apItems.end - inventory->m_apItems.begin);
    for (int i = count - 1; i >= 0; --i)
    {
        zItem* value = inventory->GetItem(i);
        if (m_pItemTaker->IsItemFarToTake(static_cast<iRPGObj*>(value)))
            (void) inventory->RemoveItem(i);
    }

    iRPGObj* object = m_pItemTaker->FirstItemToTake();
    while (object != 0)
    {
        const unsigned int stamp = static_cast<unsigned int>(object->GetThrowTime());
        bool process = false;
        if (static_cast<unsigned int>(tactTime) != 0xffffffffu)
        {
            if (stamp == 0 || object->ItemIsNotReal() ||
                static_cast<unsigned int>(tactTime) - stamp > 10000u)
                process = true;
        }

        zItem* item = static_cast<zItem*>(object->QueryInterface(49));
        if (!process)
        {
            inventory->PutItem(item, static_cast<zItem**>(0));
        }
        else
        {
            const int takenItemCount = item->GetNumInCell();

            zRPGAmmo* optionalAmmo = 0;
            if (item->GetGameObj() == TYPE_WEAPON)
            {
                zRPGWeapon* weapon = static_cast<zRPGWeapon*>(item->QueryInterface(50));
                if (weapon->IfIncAmmoOnPut())
                {
                    optionalAmmo = weapon->GetAmmo();
                    if (static_cast<zRPGObj*>(optionalAmmo)->IsInfinite())
                        optionalAmmo = 0;
                }
            }

            if (m_heroRef.m_object->PutItem(object) != 0)
            {
                int groundX;
                int groundY;
                int groundZ;

                SPRITE* routeSprite = item->GetGrndSpr();
                if (routeSprite == 0)
                {
                    routeSprite = m_pItemTaker->CurSpriteToTake();
                    m_pItemTaker->GetGrndSpriteCoor(routeSprite, groundX, groundY, groundZ);
                    m_pItemTaker->DeleteGroundSprite(routeSprite, true);
                }
                else
                {
                    routeSprite = object->GetGrndSpr();
                    m_pItemTaker->GetGrndSpriteCoor(routeSprite, groundX, groundY, groundZ);
                    m_pItemTaker->DeleteGroundSprite(object, true);
                }

                if (m_player != 0)
                    m_player->OnAutoTakeItem(object, takenItemCount, groundX, groundY, groundZ);

                if (optionalAmmo != 0)
                {
                    iRPGObj* target = static_cast<zRPGObj*>(optionalAmmo)->GetObjCopy();
                    int ammoCount;
                    if (m_player != 0 && (m_player->GetArcadeMod_() & 2) != 0)
                    {
                        ammoCount = target->GetSkillValue(RPGAMMO_DMGRND) / 2;
                        if (ammoCount == 0)
                            ammoCount = target->GetGrndTakeNum();
                    }
                    else
                    {
                        ammoCount = target->GetGrndTakeNum();
                    }
                    target->SetNumInCell(ammoCount);
                    m_heroRef.m_object->PutItem(target);
                }
            }
            else if (!object->ItemIsNotReal())
            {
                inventory->PutItem(item, static_cast<zItem**>(0));
                if (m_player != 0)
                    m_player->SendInfoMessage(4, 0);
            }
        }

        object = m_pItemTaker->NextItemToTake();
    }
}

int zInvServer::PutItemEx(iRPGObj* _pObj, iRPGObj** outObject)
{
    if (_pObj == 0)
        DebugAssert(5, "_pObj",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1059);

    zItem* item = static_cast<zItem*>(_pObj->QueryInterface(49));
    if (outObject != 0)
        *outObject = _pObj;

    if (_pObj->ItemIsNotReal())
    {
        if (item->GetId() != 0)
        {
            iRPGHero* const recipient = m_heroRef.m_object;
            const int value = item->GetId();
            (void) recipient->AddMoney(value, false);
            return 1;
        }

        const int hp = _pObj->GetSkillValue(RPGEQUIP_ADDHP);
        if (hp != 0)
        {
            const bool flag = _pObj->GetSkillValue(RPGEQUIP_IGNOREMAXHP) != 0;
            return m_heroRef.m_object->AddHp(hp, flag) ? 1 : 0;
        }

        if (_pObj->GetSkillValue(RPGEQUIP_ADDEXISTINGAMMO) != 0)
            return AddExistingAmmo();

        iRPGEquip* const equip =
            static_cast<iRPGEquip*>(_pObj->QueryInterface(5));
        iRPGObj* const buff = equip->GetAddAmmo();
        if (buff != 0)
        {
            if (buff->GetGameObj() != TYPE_AMMO)
                DebugAssert(5, "buff->GetType() == TYPE_AMMO",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 1093);

            if (buff->GetNumInCell() <= 0)
                DebugAssert(4, "buff->GetNumInCell() > 0",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 1094);
            return PutItemEx(buff, 0);
        }

        const int amount = _pObj->GetSkillValue(RPGEQUIP_ADDLIFE);
        if (amount != 0)
        {
            iRPGObj* const target =
                static_cast<iRPGObj*>(m_heroRef.m_object->QueryInterface(9));
            if (AddSigned32Wrap(target->GetSkillValue(RPGHERO_LIVES), amount) > 6)
                return 0;
            return target->IncSkillValue(RPGHERO_LIVES, amount) ? 1 : 0;
        }

        if (_pObj->GetType() == static_cast<OBJ_TYPE>(11))
        {
            zInventory* inventory = _pObj->GetBPInv();
            if (inventory == 0)
                return 1;

            while (inventory->m_apItems.begin != 0 &&
                ((PtrToU32(inventory->m_apItems.end) -
                PtrToU32(inventory->m_apItems.begin)) & 0xFFFFFFFCu) != 0u)
            {
                zItem* firstItem = inventory->GetItem(0);
                (void) inventory->TakeItem(firstItem);
                (void) m_heroRef.m_object->PutItem(firstItem);
            }
            return 1;
        }

        if (_pObj->GetSkillValue(RPGEQUIP_RepairArmor) == 0)
            return 1;

        iRPGObj* owner = GetArmor();
        if (owner == 0)
            return 0;
        zRPGArmor* const armor =
            static_cast<zRPGArmor*>(GetArmor()->QueryInterface(51));
        if (armor == 0)
            return 0;
        return static_cast<zRPGObj*>(armor)->GetSkillValue(RPGARMOR_HP) != 100000 ? 1 : 0;
    }

    int result = 0;
    if (_pObj->IsBaseObject())
    {
        if (_pObj->GetGameObj() == TYPE_AMMO)
        {
            zItem* placedObject = 0;
            m_apInv[INV_MAIN]->m_bCanPutBaseDebug = 1;
            result = m_apInv[INV_MAIN]->PutItem(item, &placedObject);
            m_apInv[INV_MAIN]->m_bCanPutBaseDebug = 0;
            if ((result & 1) != 0)
            {
                if (outObject != 0)
                    *outObject = placedObject;
                return result;
            }
        }
    }

    if (_pObj->IsBaseObject())
    {
        _pObj = _pObj->GetObjCopy();
        if (outObject != 0)
            *outObject = _pObj;
    }

    item = static_cast<zItem*>(_pObj->QueryInterface(49));

    for (int i = INV_WEAPON_HAND; i < INV_GRND; ++i)
    {
        if (item->GetNumInCell() <= 0)
            DebugAssert(4, "item->GetNumInCell() > 0",
                "..\\src\\zm\\Inventory\\zInvServer.cpp", 1178);

        result |= m_apInv[i]->PutItem(item, static_cast<zItem**>(0));
        if ((result & 1) != 0)
            return result;
    }

    if (result == 0 || (result == 4 && _pObj->GetGameObj() == TYPE_AMMO))
        result |= m_apInv[INV_MAIN]->PutItem(item, static_cast<zItem**>(0));

    if (result == 0 && _pObj->GetGameObj() == TYPE_WEAPON)
    {
        zRPGWeapon* const weapon = static_cast<zRPGWeapon*>(_pObj->QueryInterface(50));
        zRPGAmmo* const ammo = weapon->GetAmmo();
        if (ammo != 0 && !ammo->IsInfinite())
        {
            m_apInv[INV_MAIN]->m_bCanPutBaseDebug = 1;
            (void)m_apInv[INV_MAIN]->PutItem(ammo, static_cast<zItem**>(0));
            m_apInv[INV_MAIN]->m_bCanPutBaseDebug = 0;
            result = 1;
            if (outObject != 0)
                *outObject = 0;
        }
    }
    return result;
}

bool zInvServer::TakeItem(iRPGObj* _pObj)
{
    if (_pObj == 0)
        DebugAssert(5, "_pObj",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1218);

    zItem* item = static_cast<zItem*>(_pObj->QueryInterface(49));
    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        zItem* TakenItem = m_apInv[i]->TakeItem(item);
        if (TakenItem != 0)
        {
            if (TakenItem != item)
                DebugAssert(4, "TakenItem == item",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 1228);
            return 1;
        }
    }
    return 0;
}

bool zInvServer::TakeItem(int baseId)
{
    for (int i = INV_MAIN; i < INV_GRND; ++i)
    {
        int slot = -1;
        zItem* item = m_apInv[i]->GetItemByBaseId(baseId, slot, false);
        if (item != 0)
        {
            (void) m_apInv[i]->TakeItem(item);
            return 1;
        }
    }
    return 0;
}

int zInvServer::GetInvByName(const char* _sName)
{
    if (_sName == 0)
        DebugAssert(4, "_sName",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1265);

    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        zInventory* inventory = m_apInv[i];
        const char* current = inventory->GetName();
        if (std::strcmp(current, _sName) == 0)
            return i;
    }
    return -1;
}

void zInvServer::CloneInv(zInventory* _pInvDest, zInventory* _pInvSrc)
{
    if (_pInvDest == 0 || _pInvSrc == 0)
        DebugAssert(5, "_pInvDest && _pInvSrc",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1289);

    _pInvDest->RemoveAllItems();

    const int count = _pInvSrc->GetItemCount();
    for (int i = 0; i < count; ++i)
    {
        zItem* item = _pInvSrc->GetItem(i);

        iRPGFrameWork* const owner = item->GetFW();
        const OBJ_TYPE objectType = static_cast<OBJ_TYPE>(item->GetGameObj());
        iRPGObj* target = owner->GetNewRPGObj(objectType, false);

        if (item->GetGameObj() == TYPE_IMPLANT)
        {
            iInvKeeper* gate = _pInvSrc->m_keeper;
            if (gate != 0 && gate->CanHoldItm(item))
                (void) item->SetSkillValue(RPGIMPLANT_ENABLED, 1);
        }

        target->CopyParametersFrom(item);
        zItem* clonedItem = static_cast<zItem*>(target->QueryInterface(49));

        zInventory3D* destination3D =
            static_cast<zInventory3D*>(_pInvDest->QueryInterface(48));
        zInventory3D* source3D =
            static_cast<zInventory3D*>(_pInvSrc->QueryInterface(48));

        unsigned char putResult;
        zRPGObj::m_ignoreAmmoLimits = 1;
        if (destination3D != 0 && source3D != 0)
        {
            int x;
            int y;
            if (source3D->GetItemCellPos(item, &x, &y))
                putResult = static_cast<unsigned char>(
                    destination3D->PutItemToCellEx(clonedItem, x, y));
            else
                putResult = static_cast<unsigned char>(
                    _pInvDest->PutItem(clonedItem, static_cast<zItem**>(0)));
        }
        else
        {
            putResult = static_cast<unsigned char>(
                _pInvDest->PutItem(clonedItem, static_cast<zItem**>(0)));
        }

        zRPGObj::m_ignoreAmmoLimits = 0;
        if ((putResult & 1) == 0)
        {
            const char* const itemName = item->GetName();
            iLogConsole* const logger = ::gpLogCons;
            logger->LogStr(LT_WARN, "zInvServer : Object %s was not put!", itemName);
        }
    }
}

void zInvServer::CopyParametersFrom(iInvServer* sourceValue)
{
    zInvServer* _pInvServer = static_cast<zInvServer*>(sourceValue);
    if (_pInvServer == 0)
        DebugAssert(5, "_pInvServer",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1340);

    if (_pInvServer->m_pItemTaker != 0)
        m_pItemTaker = _pInvServer->m_pItemTaker;
    if (_pInvServer->m_parentMenu != 0)
        m_parentMenu = _pInvServer->m_parentMenu;
    if (_pInvServer->m_player != 0)
        m_player = _pInvServer->m_player;

    for (int i = INV_IMPL1; i <= INV_IMPL4; ++i)
        CloneInv(m_apInv[i], _pInvServer->m_apInv[i]);
    for (int i = INV_MAIN; i < INV_GRND; ++i)
    {
        if (i < INV_IMPL1 || i > INV_IMPL4)
            CloneInv(m_apInv[i], _pInvServer->m_apInv[i]);
    }
}

iRPGObj* zInvServer::GetArmor()
{
    return static_cast<iRPGObj*>(m_apInv[INV_ARMOR]->GetItem(0));
}

iRPGObj* zInvServer::GetEquip(int _iInvNum)
{
    if (_iInvNum >= INV_CNT)
        DebugAssert(5, "_iInvNum < INV_CNT",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1565);
    return static_cast<iRPGObj*>(m_apInv[_iInvNum]->GetItem(0));
}

bool zInvServer::GetDirty()
{
    unsigned char result = 0;
    for (int i = INV_MAIN; i < INV_GRND; ++i)
    {
        if (result == 0)
        {
            result = m_apInv[i]->GetDirty() ? 1u : 0u;
            if (result != 0)
                result = 1;
        }
    }
    return result;
}

int zInvServer::GetObjectCnt(int baseId)
{
    int result = 0;
    for (int inv = INV_MAIN; inv < INV_GRND; ++inv)
    {
        const int count = m_apInv[inv]->GetItemCount();
        for (int i = 0; i < count; ++i)
        {
            zItem* item = m_apInv[inv]->GetItem(i);
            if (item == 0)
                DebugAssert(5, "item",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 1651);
            if (item->GetBaseId() == baseId)
                result = AddSigned32Wrap(result, item->GetNumInCell());
        }
    }
    return result;
}

void zInvServer::SetActCellsCnt(int activeCellPercent)
{
    if (m_apInv[INV_MAIN] == 0)
        DebugAssert(5, "m_apInv[ INV_MAIN ]",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1663);
    (void) m_inventory3D[INV_MAIN]->SetActiveCellsCnt(activeCellPercent);
}

const iRPGObj* zInvServer::GetBestWeapon(bool compareArg) const
{
    zItem* best = 0;
    for (int inv = INV_MAIN; inv < INV_GRND; ++inv)
    {
        zInventory* inventory = m_apInv[inv];
        if (!inventory->IsActive())
            continue;

        zItem* candidate = inventory->FirstItem(
            static_cast<OBJ_TYPE>(2), static_cast<OBJ_EXTTYPE>(21));
        while (candidate != 0)
        {
            if (best == 0 || candidate->IsBetter(best, compareArg))
                best = candidate;
            candidate = inventory->NextItem();
        }
    }
    return static_cast<const iRPGObj*>(best);
}

iRPGObj* zInvServer::GetEquipEx(int compareValue, int _iInvType)
{
    if (_iInvType == 0)
        DebugAssert(4, "_iInvType",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1577);

    zItem* result = 0;
    for (int inv = INV_MAIN; inv < INV_GRND; ++inv)
    {
        zInventory* inventory = m_apInv[inv];
        const unsigned char active = inventory->IsActive() ? 1u : 0u;
        if ((active != 0 && (_iInvType & 1) == 0) ||
            (active == 0 && (_iInvType & 2) == 0))
            continue;

        const int count = m_apInv[inv]->GetItemCount();
        for (int i = 0; i < count; ++i)
        {
            zItem* item = m_apInv[inv]->GetItem(i);
            if (item->GetType() == static_cast<OBJ_TYPE>(compareValue))
            {
                result = item;
                break;
            }
        }
    }
    return static_cast<iRPGObj*>(result);
}

bool zInvServer::DecAmmo(int baseId, int amount)
{
    if (amount == 0)
        return 1;

    int remaining = amount;
    bool usedItem = false;
    for (; ;)
    {
        int slot = -1;
        zItem* item = m_apInv[INV_MAIN]->GetItemByBaseId(baseId, slot, true);
        if (item == 0)
        {
            if (!usedItem)
                DebugAssert(4, "item || t",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp", 1616);
            return false;
        }
        remaining = item->AddNumInCell(SubSigned32Wrap(0, remaining));
        usedItem = true;
        if (remaining == 0)
            return true;
    }
}

iRPGObj* zInvServer::GetItemByBaseId(int baseId, int _iInvType)
{
    if (_iInvType == 0)
        DebugAssert(4, "_iInvType",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1701);

    for (int inv = INV_MAIN; inv < INV_GRND; ++inv)
    {
        zInventory* inventory = m_apInv[inv];
        const unsigned char active = inventory->IsActive() ? 1u : 0u;
        if ((active != 0 && (_iInvType & 1) == 0) ||
            (active == 0 && (_iInvType & 2) == 0))
            continue;

        int slot = -1;
        zItem* item = m_apInv[inv]->GetItemByBaseId(baseId, slot, false);
        if (item != 0)
            return static_cast<iRPGObj*>(item);
    }
    return 0;
}

int zInvServer::IfObjectCanBePut(const iRPGObj* object, bool flags)
{
    zItem* item = static_cast<zItem*>(
        const_cast<iRPGObj*>(object)->QueryInterface(49));
    int result = 0;
    for (int inv = INV_MAIN; inv < INV_GRND; ++inv)
    {
        const int placement = m_apInv[inv]->IfObjectCanBePut(item, flags);
        if (result == 0 || placement != 0)
            result = placement;
        if ((result & 1) != 0)
            break;
    }
    return result;
}

int zInvServer::PutItemIfBetter(iRPGObj* _pItem)
{
    if (_pItem == 0)
        DebugAssert(5, "_pItem",
            "..\\src\\zm\\Inventory\\zInvServer.cpp", 1725);

    int foundIndex = -1;
    iRPGObj* candidate = 0;
    for (int inv = INV_MAIN; inv < INV_GRND; ++inv)
    {
        candidate = GetObj(
            inv,
            static_cast<OBJ_TYPE>(_pItem->GetGameObj()),
            static_cast<OBJ_EXTTYPE>(_pItem->GetAddMoney()),
            1);
        if (candidate != 0)
        {
            foundIndex = inv;
            break;
        }
    }

    (void) _pItem->GetBaseId();
    if (candidate != 0)
        (void) candidate->GetBaseId();

    unsigned char arcadeModeBit1 = 0;
    unsigned char arcadeModeBit0 = 0;
    if (m_player != 0)
        arcadeModeBit1 = static_cast<unsigned char>((m_player->GetArcadeMod_() >> 1) & 1);
    if (m_player != 0)
        arcadeModeBit0 = static_cast<unsigned char>(m_player->GetArcadeMod_() & 1);

    if (candidate == 0 || candidate == _pItem)
        return foundIndex;

    zItem* const incomingItem = static_cast<zItem*>(_pItem->QueryInterface(49));
    zItem* const equippedItem = static_cast<zItem*>(candidate->QueryInterface(49));

    const bool special =
        arcadeModeBit0 != 0 && incomingItem->GetGameObj() == TYPE_WEAPON;

    if (!incomingItem->IsBetter(equippedItem, true))
    {
        if (!special || incomingItem->GetBaseId() == equippedItem->GetBaseId())
        {
            foundIndex = -1;
            if (arcadeModeBit1 != 0 &&
                incomingItem->GetGameObj() == TYPE_WEAPON &&
                incomingItem->m_pInventory == 0)
                return -2;
            return foundIndex;
        }
    }

    const bool reverseCompare = equippedItem->IsBetter(incomingItem, false);
    const bool swapped = ChangeItems(
        equippedItem,
        incomingItem,
        !(reverseCompare && !special));
    if (!swapped)
        foundIndex = -1;
    return foundIndex;
}


void zInvServer::SetRPGHero(iRPGHero* hero)
{
    m_heroRef.Set(hero);

    for (int i = INV_MAIN; i != INV_CNT; ++i)
    {
        iInvKeeper* const keeper =
            static_cast<iInvKeeper*>(m_heroRef.m_object->QueryInterface(1));
        m_apInv[i]->SetKeeper(keeper);
    }
}


namespace
{
struct InventoryAmmoTreeNode
{
    InventoryAmmoTreeNode* left;
    InventoryAmmoTreeNode* parent;
    InventoryAmmoTreeNode* right;
    zRPGAmmo* value;
    unsigned char color;
    unsigned char isNil;
};

class InventoryAmmoSet
{
public:
    zContainerProxy<InventoryAmmoSet>* owner;
    InventoryAmmoTreeNode* head;
    unsigned int size;

    InventoryAmmoSet();
    ~InventoryAmmoSet() noexcept(false);
    void insert(zRPGAmmo* value);
};

struct InventoryAmmoIterator
{
    zContainerProxy<InventoryAmmoSet>* owner;
    InventoryAmmoTreeNode* node;
};

struct InventoryAmmoInsertResult
{
    InventoryAmmoIterator position;
    unsigned char inserted;
};

static void AmmoTreeInvalidIterator()
{
    ::_invalid_parameter_noinfo();
}

static InventoryAmmoTreeNode* AmmoTreeAllocateNode()
{
    void* const storage = CrtNew(sizeof(InventoryAmmoTreeNode));
    return ::new (storage) InventoryAmmoTreeNode;
}

static void AmmoTreeInit(InventoryAmmoSet* tree)
{
    zContainerProxy<InventoryAmmoSet>* const proxy =
        static_cast<zContainerProxy<InventoryAmmoSet>*>(CrtNew(sizeof(zContainerProxy<InventoryAmmoSet>)));
    if (proxy)
        proxy->m_owner = tree;
    tree->owner = proxy;
    InventoryAmmoTreeNode* head;
    try
    {
        head = AmmoTreeAllocateNode();
    }
    catch (...)
    {
        CrtFreePointer(tree->owner);
        throw;
    }
    head->left = head;
    head->parent = head;
    head->right = head;
    head->color = 1u;
    head->isNil = 1u;
    tree->head = head;
    tree->size = 0u;
}

static InventoryAmmoIterator AmmoTreeBegin(InventoryAmmoSet* tree)
{
    return InventoryAmmoIterator{tree->owner, tree->head->left};
}

static InventoryAmmoIterator AmmoTreeEnd(InventoryAmmoSet* tree)
{
    return InventoryAmmoIterator{tree->owner, tree->head};
}

static bool AmmoTreeEqual(InventoryAmmoIterator first, InventoryAmmoIterator last)
{
    if (!first.owner || first.owner != last.owner)
        AmmoTreeInvalidIterator();
    return first.node == last.node;
}

static InventoryAmmoTreeNode* AmmoTreeDereference(InventoryAmmoIterator position)
{
    if (!position.owner)
        AmmoTreeInvalidIterator();
    if (position.node == position.owner->m_owner->head)
        AmmoTreeInvalidIterator();
    return position.node;
}

static InventoryAmmoTreeNode* AmmoTreeMinimum(InventoryAmmoTreeNode* node)
{
    while (!node->left->isNil)
        node = node->left;
    return node;
}

static InventoryAmmoTreeNode* AmmoTreeMaximum(InventoryAmmoTreeNode* node)
{
    while (!node->right->isNil)
        node = node->right;
    return node;
}

static void AmmoTreeIncrement(InventoryAmmoIterator& position)
{
    if (!position.owner)
        AmmoTreeInvalidIterator();
    if (position.node->isNil)
    {
        AmmoTreeInvalidIterator();
        return;
    }
    if (!position.node->right->isNil)
    {
        position.node = AmmoTreeMinimum(position.node->right);
        return;
    }
    InventoryAmmoTreeNode* parent = position.node->parent;
    while (!parent->isNil && position.node == parent->right)
    {
        position.node = parent;
        parent = parent->parent;
    }
    position.node = parent;
}

static void AmmoTreeDecrement(InventoryAmmoIterator& position)
{
    if (!position.owner)
        AmmoTreeInvalidIterator();
    if (position.node->isNil)
    {
        position.node = position.node->right;
        if (position.node->isNil)
            AmmoTreeInvalidIterator();
        return;
    }
    if (!position.node->left->isNil)
    {
        position.node = AmmoTreeMaximum(position.node->left);
        return;
    }
    InventoryAmmoTreeNode* parent = position.node->parent;
    while (!parent->isNil && position.node == parent->left)
    {
        position.node = parent;
        parent = parent->parent;
    }
    if (position.node->isNil)
    {
        AmmoTreeInvalidIterator();
        return;
    }
    position.node = parent;
}

static void AmmoTreeRotateLeft(InventoryAmmoSet* tree, InventoryAmmoTreeNode* node)
{
    InventoryAmmoTreeNode* const pivot = node->right;
    node->right = pivot->left;
    if (!pivot->left->isNil)
        pivot->left->parent = node;
    pivot->parent = node->parent;
    if (node == tree->head->parent)
        tree->head->parent = pivot;
    else if (node == node->parent->left)
        node->parent->left = pivot;
    else
        node->parent->right = pivot;
    pivot->left = node;
    node->parent = pivot;
}

static void AmmoTreeRotateRight(InventoryAmmoSet* tree, InventoryAmmoTreeNode* node)
{
    InventoryAmmoTreeNode* const pivot = node->left;
    node->left = pivot->right;
    if (!pivot->right->isNil)
        pivot->right->parent = node;
    pivot->parent = node->parent;
    if (node == tree->head->parent)
        tree->head->parent = pivot;
    else if (node == node->parent->right)
        node->parent->right = pivot;
    else
        node->parent->left = pivot;
    pivot->right = node;
    node->parent = pivot;
}

static InventoryAmmoIterator AmmoTreeInsertNode(InventoryAmmoSet* tree, bool putLeft,
    InventoryAmmoTreeNode* parent, zRPGAmmo* value)
{
    if (tree->size >= 0x3FFFFFFEu)
        throw std::length_error("map/set<T> too long");
    InventoryAmmoTreeNode* const node = AmmoTreeAllocateNode();
    node->left = tree->head;
    node->parent = parent;
    node->right = tree->head;
    node->value = value;
    node->color = 0u;
    node->isNil = 0u;
    ++tree->size;
    if (parent == tree->head)
    {
        tree->head->parent = node;
        tree->head->left = node;
        tree->head->right = node;
    }
    else if (putLeft)
    {
        parent->left = node;
        if (parent == tree->head->left)
            tree->head->left = node;
    }
    else
    {
        parent->right = node;
        if (parent == tree->head->right)
            tree->head->right = node;
    }
    InventoryAmmoTreeNode* current = node;
    while (current->parent->color == 0u)
    {
        InventoryAmmoTreeNode* parentNode = current->parent;
        InventoryAmmoTreeNode* grandparent = parentNode->parent;
        if (parentNode == grandparent->left)
        {
            InventoryAmmoTreeNode* const uncle = grandparent->right;
            if (uncle->color == 0u)
            {
                parentNode->color = 1u;
                uncle->color = 1u;
                grandparent->color = 0u;
                current = grandparent;
            }
            else
            {
                if (current == parentNode->right)
                {
                    current = parentNode;
                    AmmoTreeRotateLeft(tree, current);
                    parentNode = current->parent;
                    grandparent = parentNode->parent;
                }
                parentNode->color = 1u;
                grandparent->color = 0u;
                AmmoTreeRotateRight(tree, grandparent);
            }
        }
        else
        {
            InventoryAmmoTreeNode* const uncle = grandparent->left;
            if (uncle->color == 0u)
            {
                parentNode->color = 1u;
                uncle->color = 1u;
                grandparent->color = 0u;
                current = grandparent;
            }
            else
            {
                if (current == parentNode->left)
                {
                    current = parentNode;
                    AmmoTreeRotateRight(tree, current);
                    parentNode = current->parent;
                    grandparent = parentNode->parent;
                }
                parentNode->color = 1u;
                grandparent->color = 0u;
                AmmoTreeRotateLeft(tree, grandparent);
            }
        }
    }
    tree->head->parent->color = 1u;
    return InventoryAmmoIterator{tree->owner, node};
}

static InventoryAmmoInsertResult AmmoTreeInsertUnique(InventoryAmmoSet* tree, zRPGAmmo* value)
{
    InventoryAmmoTreeNode* node = tree->head->parent;
    InventoryAmmoTreeNode* parent = tree->head;
    bool insertLeft = true;
    while (!node->isNil)
    {
        parent = node;
        insertLeft = std::less<zRPGAmmo*>()(value, node->value);
        node = insertLeft ? node->left : node->right;
    }
    InventoryAmmoIterator position = {tree->owner, parent};
    if (insertLeft)
    {
        if (!position.owner || position.owner != tree->owner)
            AmmoTreeInvalidIterator();
        if (parent == tree->head->left)
        {
            InventoryAmmoInsertResult result;
            result.position = AmmoTreeInsertNode(tree, true, parent, value);
            result.inserted = 1u;
            return result;
        }
        AmmoTreeDecrement(position);
    }
    InventoryAmmoInsertResult result;
    if (std::less<zRPGAmmo*>()(position.node->value, value))
    {
        result.position = AmmoTreeInsertNode(tree, insertLeft, parent, value);
        result.inserted = 1u;
    }
    else
    {
        result.position = position;
        result.inserted = 0u;
    }
    return result;
}

static InventoryAmmoIterator AmmoTreeEraseNode(InventoryAmmoSet* tree, InventoryAmmoIterator position)
{
    InventoryAmmoTreeNode* const eraseNode = position.node;
    if (eraseNode->isNil)
        throw std::out_of_range("invalid map/set<T> iterator");
    AmmoTreeIncrement(position);
    InventoryAmmoTreeNode* releaseNode = eraseNode;
    InventoryAmmoTreeNode* replacement;
    InventoryAmmoTreeNode* replacementParent;
    if (eraseNode->left->isNil)
        replacement = eraseNode->right;
    else if (eraseNode->right->isNil)
        replacement = eraseNode->left;
    else
    {
        releaseNode = position.node;
        replacement = releaseNode->right;
    }
    if (releaseNode == eraseNode)
    {
        replacementParent = eraseNode->parent;
        if (!replacement->isNil)
            replacement->parent = replacementParent;
        if (tree->head->parent == eraseNode)
            tree->head->parent = replacement;
        else if (replacementParent->left == eraseNode)
            replacementParent->left = replacement;
        else
            replacementParent->right = replacement;
        if (tree->head->left == eraseNode)
            tree->head->left = replacement->isNil ? replacementParent : AmmoTreeMinimum(replacement);
        if (tree->head->right == eraseNode)
            tree->head->right = replacement->isNil ? replacementParent : AmmoTreeMaximum(replacement);
    }
    else
    {
        eraseNode->left->parent = releaseNode;
        releaseNode->left = eraseNode->left;
        if (releaseNode == eraseNode->right)
            replacementParent = releaseNode;
        else
        {
            replacementParent = releaseNode->parent;
            if (!replacement->isNil)
                replacement->parent = replacementParent;
            replacementParent->left = replacement;
            releaseNode->right = eraseNode->right;
            eraseNode->right->parent = releaseNode;
        }
        if (tree->head->parent == eraseNode)
            tree->head->parent = releaseNode;
        else if (eraseNode->parent->left == eraseNode)
            eraseNode->parent->left = releaseNode;
        else
            eraseNode->parent->right = releaseNode;
        releaseNode->parent = eraseNode->parent;
        const unsigned char oldColor = eraseNode->color;
        eraseNode->color = releaseNode->color;
        releaseNode->color = oldColor;
    }
    if (eraseNode->color == 1u)
    {
        while (replacement != tree->head->parent && replacement->color == 1u)
        {
            if (replacement == replacementParent->left)
            {
                InventoryAmmoTreeNode* sibling = replacementParent->right;
                if (sibling->color == 0u)
                {
                    sibling->color = 1u;
                    replacementParent->color = 0u;
                    AmmoTreeRotateLeft(tree, replacementParent);
                    sibling = replacementParent->right;
                }
                if (sibling->isNil)
                {
                    replacement = replacementParent;
                    replacementParent = replacementParent->parent;
                }
                else if (sibling->left->color == 1u && sibling->right->color == 1u)
                {
                    sibling->color = 0u;
                    replacement = replacementParent;
                    replacementParent = replacementParent->parent;
                }
                else
                {
                    if (sibling->right->color == 1u)
                    {
                        sibling->left->color = 1u;
                        sibling->color = 0u;
                        AmmoTreeRotateRight(tree, sibling);
                        sibling = replacementParent->right;
                    }
                    sibling->color = replacementParent->color;
                    replacementParent->color = 1u;
                    sibling->right->color = 1u;
                    AmmoTreeRotateLeft(tree, replacementParent);
                    break;
                }
            }
            else
            {
                InventoryAmmoTreeNode* sibling = replacementParent->left;
                if (sibling->color == 0u)
                {
                    sibling->color = 1u;
                    replacementParent->color = 0u;
                    AmmoTreeRotateRight(tree, replacementParent);
                    sibling = replacementParent->left;
                }
                if (sibling->isNil)
                {
                    replacement = replacementParent;
                    replacementParent = replacementParent->parent;
                }
                else if (sibling->right->color == 1u && sibling->left->color == 1u)
                {
                    sibling->color = 0u;
                    replacement = replacementParent;
                    replacementParent = replacementParent->parent;
                }
                else
                {
                    if (sibling->left->color == 1u)
                    {
                        sibling->right->color = 1u;
                        sibling->color = 0u;
                        AmmoTreeRotateLeft(tree, sibling);
                        sibling = replacementParent->left;
                    }
                    sibling->color = replacementParent->color;
                    replacementParent->color = 1u;
                    sibling->left->color = 1u;
                    AmmoTreeRotateRight(tree, replacementParent);
                    break;
                }
            }
        }
        replacement->color = 1u;
    }
    CrtFreePointer(eraseNode);
    if (tree->size != 0u)
        --tree->size;
    return position;
}

static void AmmoTreeDestroySubtree(InventoryAmmoTreeNode* node)
{
    while (!node->isNil)
    {
        AmmoTreeDestroySubtree(node->right);
        InventoryAmmoTreeNode* const left = node->left;
        CrtFreePointer(node);
        node = left;
    }
}

static InventoryAmmoIterator AmmoTreeEraseRange(InventoryAmmoSet* tree,
    InventoryAmmoIterator first, InventoryAmmoIterator last)
{
    if (AmmoTreeEqual(first, AmmoTreeBegin(tree)) && AmmoTreeEqual(last, AmmoTreeEnd(tree)))
    {
        AmmoTreeDestroySubtree(tree->head->parent);
        tree->head->parent = tree->head;
        tree->size = 0u;
        tree->head->left = tree->head;
        tree->head->right = tree->head;
        return AmmoTreeBegin(tree);
    }
    while (!AmmoTreeEqual(first, last))
    {
        const InventoryAmmoIterator old = first;
        AmmoTreeIncrement(first);
        (void)AmmoTreeEraseNode(tree, old);
    }
    return first;
}

static void AmmoTreeClear(InventoryAmmoSet* tree)
{
    (void)AmmoTreeEraseRange(tree, AmmoTreeBegin(tree), AmmoTreeEnd(tree));
}

static void AmmoTreeReleaseStorage(InventoryAmmoSet* tree)
{
    AmmoTreeClear(tree);
    CrtFreePointer(tree->head);
    tree->head = 0;
    tree->size = 0u;
}

InventoryAmmoSet::InventoryAmmoSet()
{
    AmmoTreeInit(this);
}

InventoryAmmoSet::~InventoryAmmoSet() noexcept(false)
{
    try
    {
        AmmoTreeReleaseStorage(this);
    }
    catch (...)
    {
        CrtFreePointer(owner);
        throw;
    }
    CrtFreePointer(owner);
}

void InventoryAmmoSet::insert(zRPGAmmo* value)
{
    (void)AmmoTreeInsertUnique(this, value);
}
}

int zInvServer::AddExistingAmmo()
{
    InventoryAmmoSet ammoSet;

    for (int invIndex = INV_MAIN; invIndex != INV_GRND; ++invIndex)
    {
        for (int itemIndex = 0;
            itemIndex < m_apInv[invIndex]->GetItemCount(); ++itemIndex)
        {
            zItem* const item = m_apInv[invIndex]->GetItem(itemIndex);
            if (item->GetGameObj() != TYPE_WEAPON)
                continue;
            if (item->GetAddMoney() == 0)
                continue;

            zRPGWeapon* const weapon =
                static_cast<zRPGWeapon*>(item->QueryInterface(50));
            zRPGAmmo* ammo = weapon->GetAmmo();

            if (static_cast<zRPGObj*>(ammo)->IsInfinite())
            {
                if (ammo->m_addAmmo == 0)
                {
                    ammo = 0;
                }
                else
                {
                    ammo = static_cast<zRPGAmmo*>(
                        ammo->m_addAmmo->QueryInterface(45));
                }
            }

            if (ammo == 0)
                continue;

            if (!static_cast<zRPGObj*>(ammo)->IsBaseObject())
            {
                DebugAssert(4, "ammo->IsBaseObject()",
                    "..\\src\\zm\\Inventory\\zInvServer.cpp",
                    1838);
            }

            ammoSet.insert(ammo);
        }
    }

    int result = 0;
    InventoryAmmoIterator position = AmmoTreeBegin(&ammoSet);
    while (!AmmoTreeEqual(position, AmmoTreeEnd(&ammoSet)))
    {
        zRPGAmmo* const ammo = AmmoTreeDereference(position)->value;
        m_apInv[INV_MAIN]->m_bCanPutBaseDebug = 1;

        zItem* const ammoItem = ammo != 0
            ? static_cast<zItem*>(static_cast<zRPGObj*>(ammo))
            : 0;
        const int mergeResult =
            m_apInv[INV_MAIN]->PutItem(ammoItem, static_cast<zItem**>(0));
        m_apInv[INV_MAIN]->m_bCanPutBaseDebug = 0;

        result |= mergeResult;
        if ((mergeResult & 1) == 0 && (result & 1) != 0)
            result = (result & ~1) | 4;

        AmmoTreeIncrement(position);
    }

    return result;
}

void zInvServer::InitializeInventories()
{
    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        const unsigned int bytes = (i == INV_WEAPON_HAND) ? sizeof(zInventory) : sizeof(zInventory3D);
        void* storage = static_cast<unsigned char*>(CrtNew(bytes));
        zInventory* inventory = 0;
        if (storage)
            inventory = (i == INV_WEAPON_HAND)
                ? new (storage) zInventory(i)
                : static_cast<zInventory*>(new (storage) zInventory3D(i));
        m_apInv[i] = inventory;
        m_inventory3D[i] = static_cast<zInventory3D*>(inventory->QueryInterface(48));
    }

    for (int i = INV_WEAPON_HAND; i <= INV_ITEM_3; ++i)
        m_apInv[i]->m_inputAction = i + 8;

    for (int i = INV_MAIN; i < INV_CNT; ++i)
    {
        zInventory* inventory = m_apInv[i];
        inventory->m_active = 1u;
        if (i == INV_WEAPON_HAND)
            continue;
        zInventory3D* grid3D = m_inventory3D[i];
        grid3D->m_allowPointTake = 1u;
        zInventory2D* grid2D = static_cast<zInventory2D*>(inventory->QueryInterface(47));
        grid2D->SetCellSize(37, 37);
        (void)grid2D->SetActiveCellsCnt(100);
        (void)grid2D->SetSizeInCells(1, 1);
        grid2D->SetItemAlwaysInCenter(true);
    }

    for (int i = INV_OBJECTS1; i <= INV_OBJECTS4; ++i)
        m_inventory3D[i]->m_allowPointTake = 0u;

    (void)m_inventory3D[INV_WEAPON_2]->SetSizeInCells(2, 1);
    for (int i = INV_WEAPON_3; i <= INV_WEAPON_9; ++i)
        (void)m_inventory3D[i]->SetSizeInCells(3, 1);
    m_inventory3D[INV_ARMOR]->SetCellSize(73, 73);

    zInventory3D* mainInventory = m_inventory3D[INV_MAIN];
    mainInventory->SetUnlimited(true);
    mainInventory->SetAmmoOutsideGrid();
    (void)mainInventory->SetSizeInCells(1, 1);
    mainInventory->SetItemAlwaysInCenter(false);
    mainInventory->SetLimits(0.0f, 0.0f, 0.0f, 153.0f);
    m_apInv[INV_MAIN]->m_active = 0u;
    m_apInv[INV_MAIN]->SetObjectType(static_cast<OBJ_TYPE>(15), static_cast<OBJ_EXTTYPE>(21), true);

    for (int i = INV_OBJECTS1; i <= INV_OBJECTS4; ++i)
        m_inventory3D[i]->SetCellSize(27, 27);

    for (int i = INV_WEAPON_HAND; i <= INV_WEAPON_9; ++i)
        m_apInv[i]->SetObjectType(static_cast<OBJ_TYPE>(TYPE_WEAPON), static_cast<OBJ_EXTTYPE>(i - INV_WEAPON_HAND), false);
    m_apInv[INV_ARMOR]->SetObjectType(static_cast<OBJ_TYPE>(TYPE_ARMOR), static_cast<OBJ_EXTTYPE>(21), false);
    for (int i = INV_ITEM_3; i <= INV_ITEM_6; ++i)
        m_apInv[i]->SetObjectType(static_cast<OBJ_TYPE>(TYPE_EQUIP), static_cast<OBJ_EXTTYPE>(15 + i - INV_ITEM_3), false);
    for (int i = INV_IMPL1; i <= INV_IMPL4; ++i)
        m_apInv[i]->SetObjectType(static_cast<OBJ_TYPE>(TYPE_IMPLANT), static_cast<OBJ_EXTTYPE>(10 + i - INV_IMPL1), false);
    for (int i = INV_OBJECTS1; i <= INV_OBJECTS4; ++i)
        m_apInv[i]->SetObjectType(static_cast<OBJ_TYPE>(7), static_cast<OBJ_EXTTYPE>(21), false);
    m_apInv[INV_AMMOSHOP]->SetObjectType(static_cast<OBJ_TYPE>(TYPE_AMMO), static_cast<OBJ_EXTTYPE>(21), false);

    static const char* const inventoryNames[INV_CNT] =
    {
        "INV_MAIN", "INV_WEAPON_HAND", "INV_WEAPON_1", "INV_WEAPON_2", "INV_WEAPON_3",
        "INV_WEAPON_4", "INV_WEAPON_5", "INV_WEAPON_6", "INV_WEAPON_7", "INV_WEAPON_8", "INV_WEAPON_9",
        "INV_ITEM_3", "INV_ITEM_4", "INV_ITEM_5", "INV_ITEM_6", "INV_ARMOR",
        "INV_ITEM_IMPLANT1", "INV_ITEM_IMPLANT2", "INV_ITEM_IMPLANT3", "INV_ITEM_IMPLANT4",
        "INV_UNIQ", "INV_a2", "INV_a3", "INV_a4", "INV_GRND", "INV_SHOP", "INV_AMMO"
    };
    for (int i = INV_MAIN; i < INV_CNT; ++i)
        m_apInv[i]->m_name = inventoryNames[i];

    zInventory* shopBase = m_apInv[INV_SHOP];
    shopBase->m_active = 0u;
    shopBase->m_mergeStacks = 1u;
    zInventory3D* shop = m_inventory3D[INV_SHOP];
    shop->SetCellSize(37, 37);
    (void)shop->SetActiveCellsCnt(100);
    (void)shop->SetSizeInCells(7, 13);
    shop->SetItemAlwaysInCenter(false);
    shop->SetLimits(0.0f, 0.0f, 0.0f, 309.0f);
    shop->m_allowReplacement = 0u;

    m_apInv[INV_AMMOSHOP]->m_active = 0u;
    zInventory3D* ammo = m_inventory3D[INV_AMMOSHOP];
    ammo->SetCellSize(37, 37);
    (void)ammo->SetActiveCellsCnt(100);
    (void)ammo->SetSizeInCells(7, 1);
    ammo->SetItemAlwaysInCenter(false);
    ammo->m_allowReplacement = 0u;

    zInventory* groundBase = m_apInv[INV_GRND];
    groundBase->m_mergeStacks = 0u;
    groundBase->m_active = 0u;
    zInventory2D* ground = static_cast<zInventory2D*>(groundBase->QueryInterface(47));
    ground->SetCellSize(37, 37);
    (void)ground->SetActiveCellsCnt(100);
    (void)ground->SetSizeInCells(9, 2);
    ground->SetItemAlwaysInCenter(false);
}
