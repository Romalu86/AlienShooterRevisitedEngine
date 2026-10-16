#pragma once
#include <stddef.h>
#include "zBaseDlgMenu.h"
#include "irpg.h"
#include "zm/Utils/zRefObject.h"

class INPUT;
class SPRITE;
class iBaseDlgMenu;
class iItemTaker;
class iInvServer;
class iRPGFrameWork;
class iRPGHero;
class iRPGObj;
class iShopDlgMenu;
class iShopItemInfo;
class iSpriteInventory;


class iHeroInvMenu {
public:
    virtual void* QueryInterface(int id) const = 0;
    virtual int iRelease() = 0;
    virtual void SetRPGFrameWork(iRPGFrameWork* framework) = 0;
    virtual void SetItemTaker(iItemTaker* taker) = 0;
    virtual void SetRPGHero(iRPGHero* hero) = 0;
    virtual iBaseDlgMenu* AsBaseDlgMenu() = 0;
    virtual void ReloadAll() = 0;
    virtual int TryProcessItemPut(iRPGObj* item, float x, float y, bool alternate) = 0;
    virtual const char* GetMouseTipsString(float x, float y) = 0;
    virtual iRPGObj* GetSelItem() = 0;
    virtual void ResetSelItem() = 0;
    virtual bool GetBuyMode(const iRPGObj* item) = 0;
    virtual bool HighLightItem(const iRPGObj* item) = 0;
    virtual void ControlItemInfo(iRPGObj* selection, INPUT* input, bool source) = 0;
    virtual void InitOnShow() = 0;
    virtual void SetShopMode(bool shopMode) = 0;
    virtual bool IsShopMode() = 0;
    virtual int GetClickedInv() = 0;
    virtual void ResetClickedInv() = 0;
    virtual bool GetClickedInvMBL() = 0;
};

class zHeroInvMenu : public iHeroInvMenu, public zBaseDlgMenu, public iRefOwner {
public:
    explicit zHeroInvMenu(iRPGFrameWork* framework);
    ~zHeroInvMenu() override;

    void* QueryInterface(int id) const override;
    int iRelease() override;
private:
    void OnInvalidate(iRef<iRefObject>* value) override;

public:
    iSpriteInventory* m_inventories[INV_CNT];
    iRef<iItemTaker> m_itemTakerRef;
    int m_buttonCount;
    SPRITE* m_buttons[36];
    int m_buttonInventoryIndex[36];
    SPRITE* m_extraButtons[30];
    int m_extraButtonCount;
    SPRITE* m_mouseSprite;
    SPRITE* m_hintSprite;
    iRPGFrameWork* m_framework;
    iInvServer* m_invServer;
    iRef<iRPGObj> m_lastClickedItem;
    iRPGHero* m_rpgHero;
    iRef<iRPGObj> m_highLightRef;
    iRef<iRPGObj> m_hoverRef;
    iRef<iRPGObj> m_selectedRef;
    int m_dragHoldTicks;
    bool m_shopMode;
    iRef<iRPGObj> m_itemInfoSelection;
    bool m_itemInfoSource;
    unsigned int m_itemInfoTime;
    bool m_itemInfoPendingReset;
    int m_clickedInventory;
    bool m_clickedInvMouseButtonLeft;
private:
    iRPGObj* GetSelItem() override;
    void RemTakenItem();
    bool GetBuyMode(const iRPGObj* item) override;
    void TryToFindButtons2();
    void MouseCursorTact(float screenX, float screenY);
    void SetRPGHero(iRPGHero* hero) override;
    void TryToFindButtons();
    void SetRPGFrameWork(iRPGFrameWork* frameWork) override;
    void SetShopMode(bool shopMode) override;
    bool IsShopMode() override;
    int GetClickedInv() override;
    void ResetClickedInv() override;
    bool GetClickedInvMBL() override;
    void GetGammaItemDisabled(int& red, int& green, int& blue);
    iBaseDlgMenu* AsBaseDlgMenu() override;
    void DeleteHint();
    void InitOnShow() override;
    void FindButtonsIfNeeded();
    void ControlInventories(INPUT* input);
    void RefreshItems();
    bool SetInputActive(bool active) override;
    void SetItemTaker(iItemTaker* itemTaker) override;
    bool Show(bool show) override;
    const char* GetMouseTipsString(float x, float y) override;
    void HighLightInventories(const iRPGObj* item);
    bool HighLightItem(const iRPGObj* item) override;
    int TryProcessItemPut(iRPGObj* item, float x, float y, bool alternate) override;
    void ShowMouseCursor(const iRPGObj* item);
    void ShowHint(const char* _sMsg, float x, float y, bool localize);
    void Control(INPUT* input) override;
    void ControlInput(INPUT* input) override;
    void ControlItemInfo(iRPGObj* selection, INPUT* input, bool source) override;
    void DeletePointerToSprite(SPRITE* sprite) override;
    void ReloadAll() override;
    void ResetSelItem() override;
};

