#pragma once
#include <stddef.h>
#include "zBaseDlgMenu.h"
#include "basetype.h"
#include "zm/Utils/zRefObject.h"
#include "zm/Utils/zContainerProxy.h"

class iShopItemInfo;
class iBaseDlgMenu;
class iRPGFrameWork;
class iRPGHero;
class iHeroInvMenu;
class iInvServer;
class iInvKeeper;
class iPlayer;
class iRPGObj;
class STRING;
enum OBJ_TYPE : int;
enum OBJ_EXTTYPE : int;

class iShopDlgMenu {
public:
    virtual void* QueryInterface(int id) const = 0;
    virtual int iRelease() = 0;
    virtual void ReloadAll() = 0;
    virtual void SetRPGFrameWork(iRPGFrameWork* value) = 0;
    virtual void SetRPGHero(iRPGHero* object) = 0;
    virtual void SetHeroInvMenu(iHeroInvMenu* value) = 0;
    virtual bool ScrollToItem(OBJ_TYPE objectType, OBJ_EXTTYPE extType) = 0;
    virtual bool ScrollToItem(int objectKey, bool unusedArgument) = 0;
    virtual iBaseDlgMenu* AsBaseDlgMenu() = 0;
    virtual void ReleaseListOfObjects() = 0;
    virtual void SetInvServer(iInvServer* object) = 0;
    virtual bool TryToSellItem(iRPGObj* item, unsigned int mode) = 0;
    virtual void RefreshAll() = 0;
    virtual int GetItemsNum(int inventoryId) = 0;
    virtual int GetListItemsNum(int inventoryId) = 0;
    virtual int GetFirstShownItemNum(int inventoryId) = 0;
    virtual void PageUp(int inventoryId) = 0;
    virtual void PageDown(int inventoryId) = 0;
    virtual void PageScroll(int value, int inventoryId) = 0;
    virtual void SetPlayer(iPlayer* value) = 0;
    virtual void SetCurLevel(int value) = 0;
    virtual void RefreshBuyButtons() = 0;
    virtual void ProcessItemButton(SPRITE* button) = 0;
    virtual void OnWeaponBought(iRPGObj* item) = 0;
    virtual int TryToBuyItem(iRPGObj* item) = 0;
};

struct zShopListNode { zShopListNode* next; zShopListNode* previous; iRPGObj* value; };

struct zShopListLayout {
    zContainerProxy<zShopListLayout>* m_proxy;
    zShopListNode* m_sentinel;
    unsigned int m_size;

    struct Iterator {
        zContainerProxy<zShopListLayout>* m_owner;
        zShopListNode* m_node;

        bool operator!=(const Iterator& other) const;
        iRPGObj* Value() const;
        void Next();
    };

    zShopListLayout();
    ~zShopListLayout();
    Iterator Begin() const;
    Iterator End() const;
    Iterator Erase(Iterator position);
    void Append(iRPGObj* const* value);
    void Clear();
};

struct zShopObjectPointerVector {
    zContainerProxy<zShopObjectPointerVector>* m_proxy;
    iRPGObj** m_begin;
    iRPGObj** m_end;
    iRPGObj** m_capEnd;

    zShopObjectPointerVector();
    ~zShopObjectPointerVector();
    void Clear();
};

class zShopDlgMenu : public iShopDlgMenu, public zBaseDlgMenu {
public:
    zShopDlgMenu();
    ~zShopDlgMenu() override;
    SPRITE* m_categoryButtons[9];
    int m_filterButtonCount;
    SPRITE* m_filterButtons[9];
    bool m_buttonsFound;
    int m_buyAnim[3];
    zShopListLayout m_objects;
    zShopObjectPointerVector m_generated;
    bool m_generatedMode;
    iRPGFrameWork* m_framework;
    iRef<iRPGHero> m_rpgHeroRef;
    iRef<iInvKeeper> m_keeperRef;
    iHeroInvMenu* m_heroInvMenu;
    OBJ_TYPE m_objectType;
    OBJ_EXTTYPE m_extType;
    iRef<iInvServer> m_invServerRef;
    iPlayer* m_player;
    int m_curLevel;


    void* QueryInterface(int id) const override;
    int iRelease() override;
private:
    void Control(INPUT* input) override;
    void ControlInput(INPUT* input) override;
    void DeletePointerToSprite(SPRITE* sprite) override;
    bool ScrollToItem(int objectKey, bool unusedArgument) override;
    bool ScrollToItem(OBJ_TYPE objectType, OBJ_EXTTYPE extType) override;
    void ReleaseListOfObjects() override;
    bool TryToSellItem(iRPGObj* _pItem, unsigned int mode) override;
public:
    void SetRPGFrameWork(iRPGFrameWork* frameWork) override;
    void SetHeroInvMenu(iHeroInvMenu* heroInventoryMenu) override;
private:
    void SetPlayer(iPlayer* player) override;
    void SetCurLevel(int level) override;
    iBaseDlgMenu* AsBaseDlgMenu() override;
    void RefreshAll() override;
public:
    void OnWeaponBought(iRPGObj* item) override;
    void RefreshBuyButtons() override;
    void ProcessItemButton(SPRITE* button) override;
private:
    void PageUp(int inventoryId) override;
    void PageDown(int inventoryId) override;
    void PageScroll(int scrollPercent, int inventoryId) override;
    int GetItemsNum(int inventoryId) override;
    int GetListItemsNum(int inventoryId) override;
    int GetFirstShownItemNum(int inventoryId) override;
    void SetInvServer(iInvServer* inventoryServer) override;
    void TryToFindButtons();
public:
    void SetRPGHero(iRPGHero* hero) override;
public:
    int TryToBuyItem(iRPGObj* item) override;
private:
    bool BuyAmmoForWeapon(iRPGObj* item, bool checkOnly);
    bool InitLoad();
public:
    void ReloadAll() override;
private:
    void GoHome(int inventoryId);
    void GoEnd(int inventoryId);
    void ExecuteItemAction(int action);
    static STRING GetItemActionText(int action);
    static void SetItemButton(int index, bool visible, bool disabled, bool unaffordable,
        STRING text, int cost);
};


