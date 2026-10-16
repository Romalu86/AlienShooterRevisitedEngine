#pragma once
#include "mylib.h"
#include "basetype.h"
#include <stddef.h>
#include "zm/Utils/zRefObject.h"
#include "zm/Utils/zContainerProxy.h"
#include "resread.h"
#include "../../irpg.h"

class iRPGFrameWork;
class iQuestServer;
class iPlayer;
class iRPGObj;
#include "../../sprite.h"


class zItemTaker : public iItemTaker, public zRefObject {
public:


    template<class T>
    struct PointerStorage
    {
        zContainerProxy<PointerStorage>* owner;
        T** begin;
        T** end;
        T** capacity;

        PointerStorage()
            : owner(static_cast<zContainerProxy<PointerStorage>*>(CrtNew(sizeof(zContainerProxy<PointerStorage>)))),
              begin(0), end(0), capacity(0)
        {
            if (owner)
                owner->m_owner = this;
        }

        ~PointerStorage()
        {
            CrtFreePointer(begin);
            begin = 0;
            end = 0;
            capacity = 0;
            CrtFreePointer(owner);
        }
    };

    PointerStorage<iRPGObj> m_items;
    PointerStorage<SPRITE> m_sprites;
    int m_takeIndex;
    float m_itemTakeDist;
    iPlayer* m_pPlayer;
    iRPGFrameWork* m_pRPGFrameWork;
    iQuestServer* m_pQuestServer;

    void* QueryInterface(int interfaceId) const override;
    int iRelease() override;
    zItemTaker();
    virtual ~zItemTaker();
    void DeletePointerToSprite(SPRITE* spriteToClear) override;
    static unsigned int __cdecl RegisterNetResponseSlots();

private:
    static PTR_SPRITE (&m_aNetRespItems)[500];
    static void __cdecl DestroyNetResponseSlots();
    static int m_iNetRespCnt;
    static bool m_bNeedVidDirty;
    static int m_aiNeededVid[4096];

    void SetRPGFrameWork(iRPGFrameWork* frameWork) override;
    void SetQuestServer(iQuestServer* questServer) override;
    void SetPlayer(iPlayer* player) override;
    bool CreateGroundSprite(iRPGObj* _pItem) const override;
    bool DeleteGroundSprite(iRPGObj* _pItem, bool removeSprite) override;
    bool DeleteGroundSprite(SPRITE* _pSprite, bool removeSprite) override;
    void DeleteOldItems() override;
    void CreateIdsForSprites() override;
    bool IsNeededVid(int _iVidNum) override;
    void CreateIdForSprite(SPRITE* _pSprite);
    void FindIdForSprite(SPRITE* sprite) override;
    bool AddNameToSprite(SPRITE* _pSprite, const char* _pString);
    bool AddObjectToSprite(SPRITE* sprite, iRPGObj* _pObject, bool registerMessage);
    bool IsSpriteFarToTake(SPRITE* sprite);
    bool IsItemFarToTake(iRPGObj* object) override;
    iRPGObj* FirstItemToTake() override;
    iRPGObj* SetNewItemToSprite(iRPGObj* _pBaseObj, SPRITE* _pSprite) override;
    iRPGObj* NextItemToTake() override;
    SPRITE* CurSpriteToTake() override;
    void KillObjects(int handle) override;
    void SetItemTakeDist(float distance) override;
    void GetGrndSpriteCoor(SPRITE* sprite, int& x, int& y, int& z) override;
    void DebugLog(const char* message);
    void SetNetRespItem(int _iNum, SPRITE* value) override;
    SPRITE* GetNetRespItem(int _iNum) override;
};






void GetNew_zItemTaker(iItemTaker** outObject);
