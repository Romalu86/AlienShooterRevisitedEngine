#include "mylib.h"
#include "basetype.h"
#include <new>
#include <cstring>
#include <stdexcept>
#include "myerror.h"

#include "zInventory.h"
#include "zInventoryItem.h"
#include "zm/Utils/String/zString.h"


static int InventoryPointerRangeCount(zItem* const* begin, zItem* const* end)
{
    return begin == end ? 0 : static_cast<int>(end - begin);
}





zInventory::ItemStorage::ItemStorage()
{
    zContainerProxy<ItemStorage>* const proxy =
        static_cast<zContainerProxy<ItemStorage>*>(CrtNew(sizeof(zContainerProxy<ItemStorage>)));
    if (proxy)
        proxy->m_owner = this;
    owner = proxy;
    begin = 0;
    end = 0;
    capacity = 0;
}

zInventory::ItemStorage::~ItemStorage()
{
    if (begin)
        CrtFreePointer(begin);
    begin = 0;
    end = 0;
    capacity = 0;
    CrtFreePointer(owner);
}

zItem* zInventory::ItemStorage::At(unsigned int index) const
{
    const unsigned int count = static_cast<unsigned int>(InventoryPointerRangeCount(begin, end));
    if (index >= count)
        ::_invalid_parameter_noinfo();
    return begin[index];
}

void zInventory::ItemStorage::Append(zItem* item)
{
    const unsigned int count = static_cast<unsigned int>(InventoryPointerRangeCount(begin, end));
    const unsigned int reserved = begin ? static_cast<unsigned int>(capacity - begin) : 0u;
    if (count < reserved)
    {
        *end++ = item;
        return;
    }
    if (begin > end)
        ::_invalid_parameter_noinfo();
    Iterator position = { owner, end };
    Insert(position, item);
}

zInventory::ItemStorage::Iterator zInventory::ItemStorage::Insert(Iterator position, zItem* item)
{
    int index = 0;
    if (((PtrToU32(end) - PtrToU32(begin)) & 0xFFFFFFFCu) != 0u)
    {
        if (begin > end)
            ::_invalid_parameter_noinfo();
        if (!position.owner || position.owner != owner)
            ::_invalid_parameter_noinfo();
        index = static_cast<int>(position.position - begin);
    }
    InsertValue(position, item);
    if (begin > end)
        ::_invalid_parameter_noinfo();
    if (!owner)
        ::_invalid_parameter_noinfo();
    ItemStorage* const container = owner ? owner->m_owner : 0;
    zItem** const inserted = begin + index;
    if (inserted > container->end || inserted < container->begin)
        ::_invalid_parameter_noinfo();
    Iterator result = { owner, inserted };
    return result;
}

zInventory::ItemStorage::Iterator zInventory::ItemStorage::Erase(Iterator position)
{
    zItem** const next = position.position + 1;
    const int remaining = static_cast<int>(end - next);
    if (remaining > 0)
        (void) ::memmove_s(position.position, remaining * sizeof(zItem*), next, remaining * sizeof(zItem*));
    --end;

    Iterator result;
    result.owner = 0;
    if (position.position < begin || position.position > end)
        ::_invalid_parameter_noinfo();
    result.owner = owner;
    result.position = position.position;
    return result;
}

void zInventory::ItemStorage::InsertValue(Iterator position, zItem* item)
{
    zItem* const value = item;
    const unsigned int reserved = begin ? static_cast<unsigned int>(capacity - begin) : 0u;
    const unsigned int count = static_cast<unsigned int>(InventoryPointerRangeCount(begin, end));
    if (0x3fffffffu - count < 1u)
        throw std::length_error("vector<T> too long");
    if (reserved < count + 1u)
    {
        const unsigned int half = reserved / 2u;
        unsigned int newCapacity = reserved > 0x3fffffffu - half ? 0u : reserved + half;
        if (newCapacity < count + 1u)
            newCapacity = count + 1u;
        zItem** const newBegin = static_cast<zItem**>(CrtNew(newCapacity * sizeof(zItem*)));
        const unsigned int prefix = position.position == begin ? 0u
            : static_cast<unsigned int>(position.position - begin);
        if (prefix)
            ::memmove_s(newBegin, prefix * sizeof(zItem*), begin, prefix * sizeof(zItem*));
        newBegin[prefix] = value;
        const unsigned int suffix = position.position == end ? 0u
            : static_cast<unsigned int>(end - position.position);
        if (suffix)
            ::memmove_s(newBegin + prefix + 1u, suffix * sizeof(zItem*), position.position, suffix * sizeof(zItem*));
        if (begin)
            CrtFreePointer(begin);
        capacity = newBegin + newCapacity;
        end = newBegin + count + 1u;
        begin = newBegin;
        return;
    }

    zItem** const oldEnd = end;
    const unsigned int tail = static_cast<unsigned int>(oldEnd - position.position);
    if (tail < 1u)
    {
        *oldEnd = value;
        ++end;
        return;
    }
    *oldEnd = *(oldEnd - 1);
    ++end;
    const int shift = static_cast<int>(tail - 1u);
    if (shift > 0)
        ::memmove_s(position.position + 1, shift * sizeof(zItem*), position.position, shift * sizeof(zItem*));
    *position.position = value;
}

zInventory::zInventory(int inventoryId)
    : m_apItems(),
      m_active(0), m_inputAction(65535), m_objectType(static_cast<OBJ_TYPE>(15)), m_objectExtType(static_cast<OBJ_EXTTYPE>(21)),
      m_assertTypeMismatch(0), m_dirty(1), m_ammoDirty(1), m_keeper(0),
      m_preserveItemInventory(0), m_mergeStacks(1), m_searchObjectType(static_cast<OBJ_TYPE>(15)), m_searchObjectExtType(static_cast<OBJ_EXTTYPE>(21)), m_searchIndex(-1),
      m_name(), m_id(inventoryId), m_unlimited(0), m_bCanPutBaseDebug(0)
{
}

zInventory::~zInventory()
{
    for (int index = 0; index < GetItemCount(); ++index)
    {
        zItem* const item = GetItem(index);
        if (item && item->m_pInventory == this)
            item->SetInventory(0);
    }
}

void zInventory::operator delete(void* pointer)
{
    CrtFreePointer(pointer);
}

int zInventory::GetItemCount() const
{
    return m_apItems.begin ? InventoryPointerRangeCount(m_apItems.begin, m_apItems.end) : 0;
}

unsigned int zInventory::GetInpAction()
{
    return static_cast<unsigned int>(m_inputAction);
}

int zInventory::GetId() const
{
    return m_id;
}


bool zInventory::IsActive()
{
    return m_active != 0u;
}


void zInventory::SetUnlimited(bool enabled)
{
    m_unlimited = enabled ? 1u : 0u;
}


OBJ_EXTTYPE zInventory::GetObjectExtType() const
{
    return m_objectExtType;
}


OBJ_TYPE zInventory::GetObjectType() const
{
    return m_objectType;
}


zItem* zInventory::GetItem(int index)
{
    const int count = InventoryPointerRangeCount(m_apItems.begin, m_apItems.end);
    if (index < 0 || index >= count)
        return 0;
    return m_apItems.At(static_cast<unsigned int>(index));
}


int zInventory::GetItemNum(const zItem* item)
{
    for (int index = 0; index < InventoryPointerRangeCount(m_apItems.begin, m_apItems.end); ++index)
        if (m_apItems.At(static_cast<unsigned int>(index)) == item)
            return index;
    return -1;
}


void zInventory::SetObjectType(OBJ_TYPE objectType, OBJ_EXTTYPE objectExtType, bool debug)
{
    m_objectType = objectType;
    m_objectExtType = objectExtType;
    m_assertTypeMismatch = debug ? 1u : 0u;
}

#if !defined(_MSC_VER) && !defined(__cdecl)
#define __cdecl
#endif
#include "myerror.h"

zItem* zInventory::TakeItem(zItem* _pItem)
{
    if (_pItem == 0)
        DebugAssert(4, "_pItem",
            "..\\src\\zm\\Inventory\\zInventory.cpp",
            261);

    const int index = GetItemNum(_pItem);
    if (index < 0)
        return 0;

    zItem* removed = DeleteItem(index);
    if (removed != 0 && m_keeper != 0)
        m_keeper->OnTakeItem(removed, this, 0);
    return removed;
}


int zInventory::iRelease()
{
    delete this;
    return 0;
}


const char* zInventory::GetName() const
{
    return static_cast<const char*>(m_name);
}


int zInventory::PutItem(zItem* item, zItem** outItem)
{
    return PutItem(item, m_mergeStacks != 0, true, outItem);
}

void* zInventory::QueryInterface(int interfaceId) const
{
    if (interfaceId == 16 || interfaceId == 46)
        return const_cast<zInventory*>(this);
    return 0;
}

void zInventory::SetKeeper(iInvKeeper* keeper)
{
    m_keeper = keeper;
}


void zInventory::RemoveAllItems()
{
    while (m_apItems.begin && m_apItems.begin != m_apItems.end)
        RemoveItem(SubSigned32Wrap(InventoryPointerRangeCount(m_apItems.begin, m_apItems.end), 1));
}






zItem* zInventory::TakeItem(int _iItemNum)
{
    const int count = m_apItems.begin ? InventoryPointerRangeCount(m_apItems.begin, m_apItems.end) : 0;
    if (_iItemNum < 0 || _iItemNum >= count)
        DebugAssert(5u, "_iItemNum >= 0 && _iItemNum < (int)m_apItems.size()", "..\\src\\zm\\Inventory\\zInventory.cpp", 304u);
    const unsigned int checkedCount = m_apItems.begin
        ? static_cast<unsigned int>(InventoryPointerRangeCount(m_apItems.begin, m_apItems.end)) : 0u;
    if (static_cast<unsigned int>(_iItemNum) >= checkedCount)
        ::_invalid_parameter_noinfo();
    zItem* const removed = m_apItems.begin[_iItemNum];
    if (m_apItems.begin > m_apItems.end)
        ::_invalid_parameter_noinfo();
    zContainerProxy<ItemStorage>* const owner = m_apItems.owner;
    if (!owner)
        ::_invalid_parameter_noinfo();
    ItemStorage* const container = owner ? owner->m_owner : 0;
    zItem** const current = m_apItems.begin + _iItemNum;
    if (current > container->end || current < container->begin)
        ::_invalid_parameter_noinfo();
    ItemStorage::Iterator position = { owner, current };
    (void) m_apItems.Erase(position);

    if (m_preserveItemInventory == 0)
        removed->SetInventory(0);
    SetDirty();
    return removed;
}


zItem* zInventory::DeleteItem(int index)
{
    return TakeItem(index);
}


zItem* zInventory::GetItemByBaseId(int baseId, int& inOutIndex, bool findLowestRank)
{
    const int count = m_apItems.begin ? InventoryPointerRangeCount(m_apItems.begin, m_apItems.end) : 0;
    zItem* best = 0;
    for (int index = AddSigned32Wrap(inOutIndex, 1); index < count; index = AddSigned32Wrap(index, 1))
    {
        zItem* item = GetItem(index);
        if (!item)
            DebugAssert(5u, "item", "..\\src\\zm\\Inventory\\zInventory.cpp", 397u);
        if (item->GetBaseId() != baseId)
            continue;
        if (best)
        {
            const int bestRank = best->GetNumInCell();
            const int itemRank = item->GetNumInCell();
            if (itemRank >= bestRank)
                continue;
        }
        best = item;
        inOutIndex = index;
        if (!findLowestRank)
            break;
    }
    return best;
}

zItem* zInventory::NextItem()
{
    const int count = m_apItems.begin ? InventoryPointerRangeCount(m_apItems.begin, m_apItems.end) : 0;
    for (; ;)
    {
        m_searchIndex = AddSigned32Wrap(m_searchIndex, 1);
        const int index = m_searchIndex;
        if (index >= count)
            return 0;
        zItem* item = GetItem(index);
        if (!item)
            DebugAssert(4u, "item", "..\\src\\zm\\Inventory\\zInventory.cpp", 368u);
        if (m_searchObjectType != 15 && item->GetGameObj() != m_searchObjectType)
            continue;
        if (m_searchObjectExtType != 21 && item->GetAddMoney() != m_searchObjectExtType)
            continue;
        return item;
    }
}


int zInventory::IfObjectCanBePut(const zItem* item, bool checkCapacity)
{
    (void) checkCapacity;
    bool match = true;
    if (m_objectExtType != 21)
        match = const_cast<zItem*>(item)->GetAddMoney() == m_objectExtType;
    if (m_objectType != 15 && (!match || const_cast<zItem*>(item)->GetGameObj() != m_objectType))
        match = false;
    int result = match ? 1 : 0;
    if (!match || m_assertTypeMismatch == 0)
        return result;
    if (const_cast<zItem*>(item)->GetGameObj() != TYPE_AMMO)
        return result;
    const int existingCount = GetItemByBaseIdCnt(item->GetBaseId());
    const int maximumCount = item->GetMaxInvAmount();
    const int remainingCapacity = SubSigned32Wrap(maximumCount, existingCount);
    if (maximumCount == 0)
        return result;
    if (remainingCapacity <= 0)
        return 0;
    (void) item->GetNumInCell();
    return 4;
}

void zInventory::SetDirty()
{
    m_dirty = 1u;
}

bool zInventory::GetDirtyNotErase()
{
    return m_dirty != 0u;
}


void zInventory::SetAmmoDirty()
{
    m_ammoDirty = 1u;
}


bool zInventory::GetAmmoDirtyNotErase()
{
    return m_ammoDirty != 0u;
}


bool zInventory::GetDirty()
{
    const bool dirty = m_dirty != 0u;
    if (dirty)
        m_dirty = 0u;
    return dirty;
}


bool zInventory::GetAmmoDirty()
{
    const bool dirty = m_ammoDirty != 0u;
    if (dirty)
        m_ammoDirty = 0u;
    return dirty;
}







zItem* zInventory::FirstItem(OBJ_TYPE objectType, OBJ_EXTTYPE objectExtType)
{
    m_searchObjectType = objectType;
    m_searchObjectExtType = objectExtType;
    m_searchIndex = -1;
    return NextItem();
}

zItem* zInventory::RemoveItem(int index)
{
    zItem* item = DeleteItem(index);
    if (item && m_keeper)
        m_keeper->OnTakeItem(item, this, 1);
    return item;
}

int zInventory::GetItemByBaseIdCnt(int baseId)
{
    int cursor = -1;
    int total = 0;
    for (zItem* item = GetItemByBaseId(baseId, cursor, false); item; item = GetItemByBaseId(baseId, cursor, false))
        total = AddSigned32Wrap(total, item->GetNumInCell());
    return total;
}


int zInventory::CanAddToItems(const zItem* itemToAdd)
{
    int remaining = itemToAdd->GetNumInCell();

    for (int i = 0; i < GetItemCount(); i = AddSigned32Wrap(i, 1))
    {
        zItem* item = GetItem(i);
        const int itemBaseId = item->GetBaseId();
        const int targetBaseId = itemToAdd->GetBaseId();
        if (targetBaseId == itemBaseId)
        {
            const int itemLimit = item->GetMaxNumInCell();
            const int itemCount = item->GetNumInCell();
            const int delta = SubSigned32Wrap(itemCount, itemLimit);
            remaining = AddSigned32Wrap(remaining, delta);
            if (remaining <= 0)
                return 3;
        }
    }
    return remaining > 0 ? 0 : 3;
}

int zInventory::PutItem(zItem* _pItem, bool mergeStacks, bool checkCapacity, zItem** _pNewItm)
{
    if (!_pItem || _pItem->GetNumInCell() <= 0)
        DebugAssert(5u, "_pItem && _pItem->GetNumInCell() > 0", "..\\src\\zm\\Inventory\\zInventory.cpp", 155u);

    const bool bPutBase = _pItem->IsBaseObject();
    if (bPutBase && !m_bCanPutBaseDebug)
        DebugAssert(5u, "!bPutBase || m_bCanPutBaseDebug", "..\\src\\zm\\Inventory\\zInventory.cpp", 158u);
    if (GetItemNum(_pItem) != -1)
        DebugAssert(4u, "ItemWasAlreadyHere = ( GetItemNum( _pItem ) == -1 )", "..\\src\\zm\\Inventory\\zInventory.cpp", 161u);

    const int originalCount = _pItem->GetNumInCell();
    const int placement = IfObjectCanBePut(_pItem, checkCapacity);
    if (!placement)
        return 0;

    int countToAdd = 0;
    if (placement & 4)
    {
        const int currentByBaseId = GetItemByBaseIdCnt(_pItem->GetBaseId());
        countToAdd = SubSigned32Wrap(_pItem->GetMaxInvAmount(), currentByBaseId);
        if (countToAdd <= 0)
            DebugAssert(4u, "CntToAdd > 0", "..\\src\\zm\\Inventory\\zInventory.cpp", 181u);
        if (countToAdd > _pItem->GetNumInCell())
            countToAdd = _pItem->GetNumInCell();
    }

    int result = 0;
    if (mergeStacks)
    {
        for (int index = 0; index < GetItemCount(); ++index)
        {
            result |= GetItem(index)->TryToAddItem(_pItem, countToAdd);
            if (result & 1)
                break;
        }
    }

    if (result & 1)
    {
        result &= ~4;
    }
    else
    {
        zItem* placedItem = _pItem;
        if (bPutBase || countToAdd > 0)
        {
            placedItem = static_cast<zItem*>(_pItem->GetObjCopy()->QueryInterface(49));
            placedItem->SetGrndSpr(0);
            if (countToAdd > 0)
            {
                placedItem->SetNumInCell(countToAdd);
                if (!bPutBase)
                    (void) _pItem->AddNumInCell(SubSigned32Wrap(0, countToAdd));
            }
            if (_pNewItm)
                *_pNewItm = placedItem;
            else if (bPutBase)
                DebugAssert(5u, "_pNewItm", "..\\src\\zm\\Inventory\\zInventory.cpp", 218u);
        }
        if (!bPutBase)
        {
            if (!m_preserveItemInventory)
                placedItem->SetInventory(this);
            m_apItems.Append(placedItem);
            if (m_keeper)
                m_keeper->OnPutItem(placedItem, this);
        }
        result |= 1;
    }

    if (countToAdd > 0 && (result & 1))
        result = (result & ~1) | 4;
    if (result)
        SetDirty();
    if (bPutBase)
        _pItem->SetNumInCell(originalCount);
    return result;
}
