#pragma once
#include "basetype.h"
#include "zBaseDlgMenu.h"
#include "zm/Utils/zRefObject.h"

class SPRITE;
class INPUT;
class iInvKeeper;
class iRPGFrameWork;
class iRPGObj;

class iShopItemInfo
{
public:
    virtual void* QueryInterface(int id) const = 0;
    virtual int iRelease() = 0;
    virtual void Control(INPUT* input) = 0;
    virtual void SetScreenCoordinates(float x, float y) = 0;
    virtual void SetRPGFrameWork(iRPGFrameWork* value) = 0;
    virtual void DeletePointerToSprite(SPRITE* value) = 0;
    virtual void ShowObjInfo(iRPGObj* object, bool notify, iBaseDlgMenu* dialog) = 0;
    virtual void SetBounds(int minX, int minY, int maxX, int maxY) = 0;
    virtual void SetInventoryKeeper(iInvKeeper* value) = 0;
    virtual void Refresh() = 0;
    virtual void PageUp() = 0;
    virtual void PageDown() = 0;
    virtual int GetCurPageNum() = 0;
    virtual int GetPagesCnt() = 0;
    virtual void Initialize() = 0;
    virtual SPRITE* GetButton(int index) = 0;
    virtual SPRITE* GetButtonText(int index) = 0;
    virtual iRPGObj* GetCurrentItem() = 0;
};

class zShopItemInfo : public iShopItemInfo, public zBaseDlgMenu
{
public:
    zShopItemInfo();
    ~zShopItemInfo() override;
    void* QueryInterface(int id) const override;
    int iRelease() override;

    SPRITE* m_textSprite;
    SPRITE* m_auxiliarySprite;
    SPRITE* m_imageSprite;
    SPRITE* m_closeSprite;
    iRPGFrameWork* m_framework;
    iInvKeeper* m_inventoryKeeper;
    iBaseDlgMenu* m_dialog;
    iRef<iRPGObj> m_currentItem;
    iRef<iRPGObj> m_secondaryItem;
    int m_screenX;
    int m_screenY;
    int m_minX;
    int m_minY;
    int m_maxX;
    int m_maxY;
    VECTOR2 m_menuOffset;
    int m_curPage;
    int m_pagesCount;
    float m_pageTop;
    unsigned int m_unresolvedState;
    SPRITE* m_buttons[3];
    SPRITE* m_buttonTexts[3];
    unsigned int m_shownTime;

private:
    void Control(INPUT* input) override;
    void SetScreenCoordinates(float x, float y) override;
    void SetRPGFrameWork(iRPGFrameWork* value) override;
    void DeletePointerToSprite(SPRITE* sprite) override;
    void ShowObjInfo(iRPGObj* object, bool notify, iBaseDlgMenu* dialog) override;
    void SetBounds(int minX, int minY, int maxX, int maxY) override;
    void SetInventoryKeeper(iInvKeeper* value) override;
    void Refresh() override;
    void PageUp() override;
    void PageDown() override;
    int GetCurPageNum() override;
    int GetPagesCnt() override;
    void Initialize() override;
    SPRITE* GetButton(int index) override;
    SPRITE* GetButtonText(int index) override;
    iRPGObj* GetCurrentItem() override;
    void ShowObjInfo(iRPGObj* item, bool notify, iBaseDlgMenu* dialog, bool force);
    void DeleteSprites();
    void Draw();
};


void __cdecl GetNew_zShopItemInfo(iShopItemInfo** outObject);
