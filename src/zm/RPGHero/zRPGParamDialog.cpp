#include "zm/Utils/zLogConsole.h"
#include "../../engine_globals.h"
#include "mylib.h"

#include "zRPGParamDialog.h"

#include "basetype.h"
#include <new>
#include <cstring>
#include <stdexcept>


int zRPGParamDialog::GetPlayerMenuType() const
{
    return m_playerMenuType;
}

void zRPGParamDialog::SetPlayeR(iPlayer* player)
{
    m_pPlayer = player;
}

void zRPGParamDialog::RecieveNetPlayers()
{
}

int zRPGParamDialog::GetPerkSkill() const
{
    return m_perkSkill;
}

void zRPGParamDialog::operator delete(void* pointer)
{
    CrtFreePointer(pointer);
}

int zRPGParamDialog::iRelease()
{
    if (this)
    {
        delete this;
    }
    return 0;
}


void zRPGParamDialog::FirstHero(int expectedSex)
{
    m_iCurHero = -1;
    SetHero(NextHero(expectedSex, 1));
}

void zRPGParamDialog::PrevHero()
{
    SetHero(NextHero(-1, -1));
}

void zRPGParamDialog::NextHero()
{
    SetHero(NextHero(-1, 1));
}

void zRPGParamDialog::SetPerkSkill(int perkSkill)
{
    m_perkSkill = perkSkill;
    m_pRPGFrameWork->SetPerkForAllHeroes(perkSkill);
}

void* zRPGParamDialog::QueryInterface(unsigned int id) const
{
    if (id == 25u)
        return const_cast<zRPGParamDialog*>(this);
    DebugAssert(5u, "0", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 30u);
    return 0;
}

void zRPGParamDialog::ResetScriptFunctions()
{
    m_scriptSkillAdd = -1;
    (void)m_apHeroes.Erase(m_apHeroes.Begin(), m_apHeroes.End());
}

void zRPGParamDialog::SetPlayerMenuType(int playerMenuType)
{
    m_playerMenuType = playerMenuType;
    (void)m_apHeroes.Erase(m_apHeroes.Begin(), m_apHeroes.End());
    FirstHero(-1);
}

bool zRPGParamDialog::CanDecSkill(int _iParamNum) const
{
    if (_iParamNum<0 || _iParamNum >= RPGHERO_PARAMS_COUNT)
        DebugAssert(5u, "_iParamNum >= 0 && _iParamNum < RPGHERO_PARAMS_COUNT", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 229u);
    return m_aParamIncrease[_iParamNum]>0;
}

void zRPGParamDialog::SetRPGFrameWork(iRPGFrameWork* frameWork)
{
    m_pRPGFrameWork = frameWork;
    m_pTextDB = 0;
    if (frameWork)
        m_pTextDB = static_cast<iTextDB*>(frameWork->QueryInterface(28));
}

zRPGParamDialog::zRPGParamDialog()
    : m_pRPGFrameWork(0), m_pTextDB(0), m_scriptSkillAdd(-1),
      m_perkSkill(-1), m_skillIncrease(0), m_iCurHero(-1),
      m_pPlayer(0), m_playerMenuType(0)
{
    m_heroRef.m_object = 0;
    m_heroRef.m_refObject = 0;
    m_heroRef.m_owner = 0;
    m_childRef.m_object = 0;
    m_childRef.m_refObject = 0;
    m_childRef.m_owner = 0;
    for (int paramIndex = 0;paramIndex<RPGHERO_PARAMS_COUNT;++paramIndex)
        m_aParamIncrease[paramIndex] = 0;
}

void zRPGParamDialog::ChangeCurSex()
{
    iRPGObj* const hero = m_heroRef.m_object;
    const int nextSex = hero ? AddSigned32Wrap(hero->GetSkillValue(RPGHERO_SEX), 1)%2 : 1;
    FirstHero(nextSex);
}

void zRPGParamDialog::RemoveSkillIncrease()
{
    iRPGObj* const hero = m_heroRef.m_object;
    if (!hero)
        return;
    if (m_skillIncrease)
        (void)hero->IncSkillValue(RPGHERO_SKILLSTOADD, m_skillIncrease);
    m_skillIncrease = 0;
    for (int paramIndex = 0;paramIndex<RPGHERO_PARAMS_COUNT;++paramIndex)
    {
        const int skillDelta = m_aParamIncrease[paramIndex];
        if (skillDelta && !m_heroRef.m_object->IncSkillValue(paramIndex, SubSigned32Wrap(0, skillDelta)))
            DebugAssert(4u, "buf", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 338u);
        m_aParamIncrease[paramIndex] = 0;
    }
}

bool zRPGParamDialog::CanIncSkill(int _iParamNum) const
{
    if (_iParamNum<0 || _iParamNum >= RPGHERO_PARAMS_COUNT)
        DebugAssert(5u, "_iParamNum >= 0 && _iParamNum < RPGHERO_PARAMS_COUNT", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 218u);
    iRPGObj* const hero = m_heroRef.m_object;
    const int skillValue = hero ? hero->GetSkillValue(_iParamNum) : 0;
    const int availablePoints = m_heroRef.m_object->GetSkillValue(RPGHERO_SKILLSTOADD);
    return availablePoints>0 && (skillValue<200 || _iParamNum == RPGHERO_MONEY);
}

void zRPGParamDialog::AssignSkills()
{
    iRPGObj* hero = m_heroRef.m_object;
    if (!hero)
        DebugAssert(5u, "m_pHero", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 271u);
    hero = m_heroRef.m_object;
    if (!hero->IsBaseObject())
    {
        for (int paramIndex = 0;paramIndex<RPGHERO_PARAMS_COUNT;++paramIndex)
            m_aParamIncrease[paramIndex] = 0;
        m_skillIncrease = 0;
        return;
    }
    hero = m_heroRef.m_object;
    m_pPlayer->CopyHeroToArmy(hero, -1);
    hero = m_heroRef.m_object;
    const char* heroName = hero ? hero->GetName() : 0;
    if (!heroName)
        heroName = "";
    iLogConsole* const logger = ::gpLogCons;
    (void)logger->LogStr(LT_INFO, "m_pArmyHero - zRPGParamDialog. %s", heroName);
    RemoveSkillIncrease();
}

zRPGParamDialog::~zRPGParamDialog()
{
}

void zRPGParamDialog::SetHeroExternal(iRPGObj* heroObject)
{
    const int baseId = heroObject->GetBaseId();
    SetHero(heroObject);
    LoadHeroesIfNeed();
    int matchingHero = -1;
    for (unsigned int heroIndex = 0; heroIndex < m_apHeroes.size(); ++heroIndex)
    {
        iRPGObj* const candidate = m_apHeroes.at(heroIndex);
        if (candidate->GetBaseId() == baseId)
        {
            matchingHero = static_cast<int>(heroIndex);
            break;
        }
    }
    if (matchingHero >= 0)
    {
        m_iCurHero = matchingHero;
        return;
    }
    (void)m_apHeroes.Erase(m_apHeroes.Begin(), m_apHeroes.End());
    m_iCurHero = -1;
}

void zRPGParamDialog::SetHero(iRPGObj* _pHero)
{
    if (!_pHero)
        DebugAssert(4u, "_pHero", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 257u);
    iRPGObj* const previousHero = m_heroRef.m_object;
    if (previousHero && previousHero != _pHero)
        RemoveSkillIncrease();
    m_heroRef.Set(_pHero);
    iRPGObj* const storedHero = m_heroRef.m_object;
    iRPGHero* const heroInterface = static_cast<iRPGHero*>(storedHero->QueryInterface(0));
    m_childRef.Set(heroInterface);
}

void zRPGParamDialog::AddSkill(int _iParamNum, int stepCount)
{
    iRPGObj* hero = m_heroRef.m_object;
    if (!hero)
        return;
    const int remainingPoints = SubSigned32Wrap(hero->GetSkillValue(RPGHERO_SKILLSTOADD), stepCount);
    if ((remainingPoints<0 || AddSigned32Wrap(m_skillIncrease, stepCount)<0) && !g_iZmDebug)
        return;
    const int pointsPerStep = GetSkillAddValue(_iParamNum);
    int ValToAdd = MultiplySigned32Wrap(pointsPerStep, stepCount);
    hero = m_heroRef.m_object;
    const int skillValue = hero->GetSkillValue(_iParamNum);
    if (AddSigned32Wrap(skillValue, ValToAdd)>200 && _iParamNum != RPGHERO_MONEY)
        ValToAdd = SubSigned32Wrap(200, skillValue);
    int previousDelta = m_aParamIncrease[_iParamNum];
    if (ValToAdd && previousDelta>0 && ValToAdd<0 && (previousDelta%ValToAdd) != 0)
    {
        ValToAdd = SubSigned32Wrap(0, previousDelta)%ValToAdd;
        if (ValToAdd == 0)
            DebugAssert(4u, "ValToAdd != 0", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 88u);
    }
    previousDelta = m_aParamIncrease[_iParamNum];
    m_aParamIncrease[_iParamNum] = AddSigned32Wrap(previousDelta, ValToAdd);
    bool valid = true;
    if (m_aParamIncrease[_iParamNum]<0)
    {
        if (!g_iZmDebug || AddSigned32Wrap(skillValue, ValToAdd)<0)
            valid = false;
        if (!valid)
        {
            m_aParamIncrease[_iParamNum] = previousDelta;
            if (!g_iZmDebug && previousDelta<0)
                DebugAssert(4u, "g_iZmDebug || m_aParamIncrease[ _iParamNum ] >= 0", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 100u);
        }
    }
    if (ValToAdd && valid)
    {
        hero = m_heroRef.m_object;
        (void)hero->IncSkillValue(_iParamNum, ValToAdd);
        hero = m_heroRef.m_object;
        (void)hero->SetSkillValue(RPGHERO_SKILLSTOADD, remainingPoints);
        m_skillIncrease = AddSigned32Wrap(m_skillIncrease, stepCount);
    }
}

iRPGObj* zRPGParamDialog::NextHero(int expectedSex, int step)
{
    LoadHeroesIfNeed();
    if (step != 1 && step != -1)
        DebugAssert(5u, "step == 1 || step == -1", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 401u);
    const int initialHero = m_iCurHero;
    int heroIndex = AddSigned32Wrap(initialHero, step);
    m_iCurHero = heroIndex;
    if (heroIndex == initialHero)
        return 0;
    for (;;)
    {
        const int heroCount = static_cast<int>(m_apHeroes.size());
        heroIndex = m_iCurHero;
        if (heroIndex >= heroCount)
        {
            m_iCurHero = 0;
            if (initialHero == -1)
                return 0;
            heroIndex = 0;
        }
        if (heroIndex<0)
        {
            m_iCurHero = AddSigned32Wrap(heroCount, -1);
            heroIndex = m_iCurHero;
        }
        if (m_apHeroes.at(m_iCurHero)->GetGameObj() != TYPE_HERO)
            DebugAssert(5u, "m_apHeroes[ m_iCurHero ]->GetType() == TYPE_HERO", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 417u);
        if (m_apHeroes.at(m_iCurHero)->GetClone())
            DebugAssert(4u, "!m_apHeroes[ m_iCurHero ]->GetClone()", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 420u);
        const int CurSex = m_apHeroes.at(m_iCurHero)->GetSkillValue(RPGHERO_SEX);
        if (CurSex>1)
            DebugAssert(4u, "CurSex <= 1", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 423u);
        if (expectedSex == -1 || expectedSex == CurSex)
            return m_apHeroes.at(m_iCurHero);
        const int nextHero = AddSigned32Wrap(m_iCurHero, step);
        m_iCurHero = nextHero;
        if (nextHero == initialHero)
            return 0;
    }
}


int zRPGParamDialog::GetSkillAddValue(int paramIndex)
{
    if (m_scriptSkillAdd == -1)
        m_scriptSkillAdd = m_pRPGFrameWork->ScriptGetNFunction("GetIncSkillPoints");
    if (m_scriptSkillAdd == -1)
        return 0;
    return m_pRPGFrameWork->ScriptRun(m_scriptSkillAdd, "%d", paramIndex, 0, 0);
}

void __cdecl GetNew_zRPGParamDialog(iRPGParamDialog** outObject)
{
    zRPGParamDialog* object = static_cast<zRPGParamDialog*>(CrtNew(sizeof(zRPGParamDialog)));
    if (object)
    {
        try
        {
            object = new(object) zRPGParamDialog();
        }
        catch (...)
        {
            CrtFreePointer(object);
            throw;
        }
    }
    *outObject = object;
}

void zRPGParamDialog::LoadHeroesIfNeed()
{
    if (m_apHeroes.size() != 0u)
        return;
    iRPGObj* hero = m_pRPGFrameWork->FirstObject(0x21, 0, 0);
    while (hero)
    {
        const char* const heroName = hero->GetName();
        const bool survivalHero = std::strstr(heroName, "Surv") != 0;
        const bool includeHero = m_playerMenuType == 1 ? survivalHero : !survivalHero;
        if (includeHero)
            m_apHeroes.Append(hero);
        hero = m_pRPGFrameWork->NextObject();
    }
}

iRPGObj* zRPGParamDialog::GetCurHero()
{
    return m_heroRef.m_object;
}
const char* zRPGParamDialog::GetFormattedSkill(int paramIndex)
{
    if (!m_pTextDB)
        DebugAssert(5u, "m_pTextDB", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 121u);
    const iRPGObj* const formattedObject =
        static_cast<iRPGObj*>(m_heroRef.m_object->QueryInterface(9));
    return m_pTextDB->GetFormattedSkill(formattedObject, paramIndex);
}
const char* zRPGParamDialog::GetPlayerName()
{
    return m_pRPGFrameWork->GetItemName(m_heroRef.m_object);
}
const char* zRPGParamDialog::GetHeroInfo()
{
    const int extType = static_cast<int>(m_heroRef.m_object->GetExtType());
    const char** const lines = m_pRPGFrameWork->GetHeroInfo(extType);
    return m_pTextDB->GetSpeciallyDecodedString(lines, -1);
}
const char* zRPGParamDialog::GetObjInfo(iRPGObj* heroObject)
{
    (void)heroObject;
    DebugAssert(5u, "0", "..\\src\\zm\\RPGHero\\zRPGParamDialog.cpp", 200u);
    return 0;
}
const char* zRPGParamDialog::GetVidDirInfo(int infoKind, int infoIndex)
{
    const char** const lines = m_pTextDB->GetExtInfo(0, infoKind, infoIndex);
    return m_pTextDB->GetSpeciallyDecodedString(lines, -1);
}


