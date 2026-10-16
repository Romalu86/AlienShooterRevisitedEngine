#pragma once

#include <stddef.h>
#include "irpg.h"
#include "zm/Utils/zContainerProxy.h"
#include "zm/Utils/String/zString.h"

class zItem;
class iInvKeeper;
enum OBJ_TYPE : int;
enum OBJ_EXTTYPE : int;


class zInventory : public iInventory
{

    friend class zSpriteInventory;
    friend class zInventory3D;
public:
    struct ItemStorage
    {
        zContainerProxy<ItemStorage>* owner;
        zItem** begin;
        zItem** end;
        zItem** capacity;

        struct Iterator
        {
            zContainerProxy<ItemStorage>* owner;
            zItem** position;
        };

        ItemStorage();
        ~ItemStorage();
        zItem* At(unsigned int index) const;
        void Append(zItem* item);
        Iterator Insert(Iterator position, zItem* item);
        void InsertValue(Iterator position, zItem* item);
        Iterator Erase(Iterator position);
    };

    ItemStorage m_apItems;
    unsigned char m_active;
    int m_inputAction;
    OBJ_TYPE m_objectType;
    OBJ_EXTTYPE m_objectExtType;
    unsigned char m_assertTypeMismatch;
    unsigned char m_dirty;
    unsigned char m_ammoDirty;
    iInvKeeper* m_keeper;
    unsigned char m_preserveItemInventory;
    unsigned char m_mergeStacks;
    OBJ_TYPE m_searchObjectType;
    OBJ_EXTTYPE m_searchObjectExtType;
    int m_searchIndex;
    zString m_name;
    int m_id;
    unsigned char m_unlimited;
    unsigned char m_bCanPutBaseDebug;

    explicit zInventory(int inventoryId);
    virtual ~zInventory();
    static void operator delete(void* pointer);

    void* QueryInterface(int interfaceId) const override;
    int iRelease() override;
    unsigned int GetInpAction() override;
    const char* GetName() const override;
    bool IsActive() override;
    void SetUnlimited(bool enabled) override;
    OBJ_EXTTYPE GetObjectExtType() const override;
    OBJ_TYPE GetObjectType() const override;
    int GetId() const override;
    int GetItemCount() const;
    zItem* GetItem(int index);
protected:
    int GetItemNum(const zItem* item);
public:
    int IfObjectCanBePut(const zItem* item, bool checkCapacity) override;
    int PutItem(zItem* item, zItem** outItem) override;
protected:
    int PutItem(zItem* _pItem, bool mergeStacks, bool checkCapacity, zItem** _pNewItm) override;
public:
    zItem* TakeItem(zItem* _pItem);
    zItem* RemoveItem(int index);
private:
    zItem* TakeItem(int _iItemNum);
protected:
    virtual zItem* DeleteItem(int index) override;
public:
    void RemoveAllItems() override;
    void SetObjectType(OBJ_TYPE objectType, OBJ_EXTTYPE objectExtType, bool debug) override;
    void SetKeeper(iInvKeeper* keeper);
    zItem* FirstItem(OBJ_TYPE objectType, OBJ_EXTTYPE objectExtType);
    zItem* NextItem();
    zItem* GetItemByBaseId(int baseId, int& inOutIndex, bool findLowestRank) override;
    int GetItemByBaseIdCnt(int baseId) override;
    void SetDirty();
    bool GetDirty();
    bool GetDirtyNotErase();
    void SetAmmoDirty();
    bool GetAmmoDirty();
    bool GetAmmoDirtyNotErase();
protected:
    int CanAddToItems(const zItem* itemToAdd);
};

