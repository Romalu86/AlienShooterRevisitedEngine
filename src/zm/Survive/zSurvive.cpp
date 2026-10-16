#include "mylib.h"
#include "../../engine_globals.h"
#include "zSurvive.h"
#include "zm/Utils/zLogConsole.h"
#include "zm/Utils/zStaticStorage.h"
#include "zm/Game/zGroundItem.h"
#include "zm/RPGHero/zRPGBaseObject.h"
#include "map.h"
#include <new>
#include <cstring>
#include "myerror.h"

bool zSurvive::m__abChangeableParams[MAX_RPGPARAMSCOUNT] = {};
bool zSurvive::m__abBaseParams[MAX_RPGPARAMSCOUNT] = {};
bool zSurvive::m__abArrayParams[MAX_RPGPARAMSCOUNT] = {};
int zSurvive::ms_MonstCntCoeff = 0;
int zSurvive::ms_EmptyBonusWeightCoeff = 0;
zSurvive* zSurvive::m_pM2CInterval = 0;
int zSurvive::m_iM2CPeriod = 0;

namespace
{
StaticObjectStorage<zSurviveVector<int> > monsterCountCache;
StaticObjectStorage<zSurviveVector<int> > monsterTimeCache;
}

zSurviveVector<int>& zSurvive::m_aM2CMonst =
    *reinterpret_cast<zSurviveVector<int>*>(&monsterCountCache.memory);
zSurviveVector<int>& zSurvive::m_aM2CLastTime =
    *reinterpret_cast<zSurviveVector<int>*>(&monsterTimeCache.memory);

namespace {
enum SurviveObjectKind : int
{
    TYPE_SURVIVEMAP = 10,
    TYPE_SURVIVEINTERVAL = 11
};


}


static int SubtractSigned32Wrap(int left, int right)
{
    return BitsToSigned32(static_cast<unsigned int>(left)-static_cast<unsigned int>(right));
}

void* zSurvive::QueryInterface(int interfaceId) const
{
    if (interfaceId == 29)
        return static_cast<iSurvive*>(const_cast<zSurvive*>(this));
    if (interfaceId == 53)
        return const_cast<zSurvive*>(this);
    return zGroundItem::QueryInterface(interfaceId);
}


void zSurvive::SetDataArrays()
{
    if (m_changeableParams != 0)
        return;

    m_changeableParams = m__abChangeableParams;
    m_baseParams = m__abBaseParams;
    m_arrayParams = m__abArrayParams;
    for (int parameterIndex = 0; parameterIndex < MAX_RPGPARAMSCOUNT; ++parameterIndex)
    {
        m_changeableParams[parameterIndex] = 0;
        m_baseParams[parameterIndex] = 0;
        m_arrayParams[parameterIndex] = 0;
    }
}

int zSurvive::iRelease()
{
    if (this)
    {
        this->~zSurvive();
        CrtFreePointer(this);
    }
    return 0;
}


zSurvive::zSurvive(OBJ_TYPE _eType, iRPGFrameWork* frameWork)
    : zRPGObj(_eType, RPGSURVIVE_PARAMS_COUNT),
      m_frameWork(frameWork),
      m_iScoresCoeff(0)
{
    if (_eType != TYPE_SURVIVEMAP && _eType != TYPE_SURVIVEINTERVAL)
        DebugAssert(5,
            "_eType == TYPE_SURVIVEMAP || _eType == TYPE_SURVIVEINTERVAL",
            "..\\src\\zm\\Survive\\zSurvive.cpp",
            36);

    m_iBossColor[0] = 0;
    m_iBossColor[1] = 0;
    m_iBossColor[2] = 0;
}


zSurvive::~zSurvive()
{
}


void zSurvive::OnSkillChange(int skillSelector, int oldValue, int newValue)
{
    zRPGObj::OnSkillChange(skillSelector, oldValue, newValue);

    zSurviveVector<int>* target = 0;
    if (skillSelector == RPGSURVIVE_MONSTERS)
    {
        const unsigned int count = m_aiMonstVids.size();
        if (count == 0)
            DebugAssert(5, "m_aiMonstVids.size() > 0",
                "..\\src\\zm\\Survive\\zSurvive.cpp", 123);
        target = &m_aiMonstVids.at(m_aiMonstVids.size() - 1);
    }
    else if (skillSelector == RPGSURVIVE_MONSTERSCOUNT)
    {
        const unsigned int count = m_aiMonstCnt.size();
        if (count == 0)
            DebugAssert(5, "m_aiMonstCnt.size() > 0",
                "..\\src\\zm\\Survive\\zSurvive.cpp", 129);
        target = &m_aiMonstCnt.at(m_aiMonstCnt.size() - 1);
    }
    else if (skillSelector == RPGSURVIVE_PERIODS)
    {
        target = &m_aiPeriodsLen;
    }
    else if (skillSelector == RPGSURVIVE_PAUSES)
    {
        target = &m_aiPauseLen;
    }
    else if (skillSelector == RPGSURVIVE_BONUSES)
    {
        target = &m_aiBonus;
    }
    else if (skillSelector == RPGSURVIVE_BONUSRANDOM)
    {
        target = &m_aiBonusRand;
    }
    else if (skillSelector == RPGSURVIVE_WITHOUTMENU)
    {
        target = &m_withoutMenu;
    }

    if (target != 0)
    {
        target->push_back(newValue);
    }
}


zSurvive* zSurvive::GetNextInt()
{
    const int nextBaseId = AddSigned32Wrap(GetBaseId(), 1);
    iRPGObj* object = m_frameWork->GetItemById(nextBaseId);
    if (object == 0 || object->GetGameObj() != TYPE_SURVIVEINTERVAL)
        return 0;
    return static_cast<zSurvive*>(object->QueryInterface(53));
}

static const char* SurviveSourceFile()
{
    return "..\\src\\zm\\Survive\\zSurvive.cpp";
}

static int SurviveAbsScaled(int value, int scale)
{
    if (scale != 0)
        value = MultiplySigned32Wrap(value, scale) / 100;
    const int sign = value >> 31;
    return SubtractSigned32Wrap(value ^ sign, sign);
}


int zSurvive::GetBonusBaseId(int _iNum)
{
    const unsigned int randCount = m_aiBonusRand.size();
    if (_iNum < 0 || _iNum >= BitsToSigned32(randCount))
        DebugAssert(4, "_iNum >= 0 && _iNum < (int)m_aiBonusRand.size()", SurviveSourceFile(), 631);

    int multiplier = 100;
    const int globalScale = ms_EmptyBonusWeightCoeff;
    if (globalScale != 0)
    {
        if (m_aiBonus.at(_iNum) == 0)
            multiplier = globalScale;
    }
    const int bonusValue = m_aiBonusRand.at(_iNum);
    return MultiplySigned32Wrap(bonusValue, multiplier) / 100;
}


void zSurvive::InitParams()
{
    zRPGObj* base = this;
    base->SetArrayParam(RPGSURVIVE_MONSTERS);
    base->SetArrayParam(RPGSURVIVE_MONSTERSCOUNT);
    base->SetArrayParam(RPGSURVIVE_PERIODS);
    base->SetArrayParam(RPGSURVIVE_PAUSES);
    base->SetArrayParam(RPGSURVIVE_BOSSBONUSES);
    base->SetArrayParam(RPGSURVIVE_BONUSES);
    base->SetArrayParam(RPGSURVIVE_BONUSRANDOM);
    base->SetArrayParam(RPGSURVIVE_WITHOUTMENU);
}


bool zSurvive::GetParams(
    const char* name, int nameLength, int& outCount, int* outKind) const
{
    outCount = 1;
    *outKind = -1;

    if (std::strncmp("Monsters", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_MONSTERS;
    else if (std::strncmp("MonstCount", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_MONSTERSCOUNT;
    else if (std::strncmp("Periods", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_PERIODS;
    else if (std::strncmp("Pauses", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_PAUSES;
    else if (std::strncmp("WithoutMenu", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_WITHOUTMENU;
    else if (std::strncmp("BossVid", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_BOSSVID;
    else if (std::strncmp("BonusRandom", name, static_cast<unsigned int>(nameLength)) == 0)
        *outKind = RPGSURVIVE_BONUSRANDOM;

    return *outKind != -1;
}


void zSurvive::SetCharValue(int stringParam, const char* textValue)
{
    zRPGObj::SetCharValue(stringParam, textValue);
}


iSurvive* zSurvive::GetPrevInt()
{
    zSurvive* owner = this;
    iRPGObj* cursor = owner;
    for (;;)
    {
        const int previousBaseId = SubtractSigned32Wrap(cursor->GetBaseId(), 1);
        iRPGObj* object = owner->m_frameWork->GetItemById(previousBaseId);
        if (object == 0)
            return 0;
        if (object->GetGameObj() != TYPE_SURVIVEINTERVAL)
        {
            cursor = object;
            continue;
        }
        return static_cast<iSurvive*>(object->QueryInterface(29));
    }
}


int zSurvive::GetIntrval(
    int _iDeltaTime, iSurvive*& outInterval, int& outIndex, int* outDepth)
{
    outIndex = -1;
    outInterval = 0;
    int depth = 0;

    zSurvive* current = this;
    if (current == 0 || _iDeltaTime < 0)
        return 0;

    while (current != 0)
    {
        if (_iDeltaTime < 0)
            break;
        iSurvive* currentSecondary = static_cast<iSurvive*>(current);
        const int totalLength = currentSecondary->GetFullLen();
        if (totalLength <= _iDeltaTime)
        {
            _iDeltaTime = SubtractSigned32Wrap(_iDeltaTime, totalLength);
            current = current->GetNextInt();
            ++depth;
            continue;
        }

        int index = -1;
        int i = 0;
        while (i < currentSecondary->GetPerCnt())
        {
            ++index;
            ++i;
            _iDeltaTime = SubtractSigned32Wrap(_iDeltaTime, current->m_aiPeriodsLen.at(index));
            _iDeltaTime = SubtractSigned32Wrap(_iDeltaTime, current->m_aiPauseLen.at(index));
            if (_iDeltaTime < 0)
                break;
        }
        if (_iDeltaTime >= 0)
            DebugAssert(4, "_iDeltaTime < 0", SurviveSourceFile(), 234);
        outIndex = index;
    }

    if (outIndex == -1)
        return 0;

    if (current != 0)
        outInterval = static_cast<iSurvive*>(current);
    else
        outInterval = 0;

    const unsigned int index = static_cast<unsigned int>(outIndex);
    int ret = current->m_aiPeriodsLen.at(index);
    ret = AddSigned32Wrap(ret, current->m_aiPauseLen.at(index));
    ret = AddSigned32Wrap(ret, _iDeltaTime);
    if (outDepth != 0)
        *outDepth = depth;
    if (ret < 0)
        DebugAssert(4, "ret >= 0", SurviveSourceFile(), 248);
    return ret;
}


int zSurvive::GetWpnIdForClass(int period, int num, iRPGHero* hero, iRPGFrameWork* frameWork)
{
    (void)hero;
    if (period < 0 || m_addedWeaponLists.m_begin == 0)
        return 0;

    const unsigned int outerCount = m_addedWeaponLists.size();
    if (static_cast<unsigned int>(period) >= outerCount)
        return 0;

    for (unsigned int i = 0;; ++i)
    {
        const zSurviveVector<int>* outer = &m_addedWeaponLists.at(period);
        if (i >= outer->size())
            return 0;
        const int value = m_addedWeaponLists.at(period).at(i);
        iRPGObj* wpn = frameWork->GetItemById(value);
        if (wpn == 0)
        {
            DebugAssert(3, "wpn", SurviveSourceFile(), 301);
            continue;
        }
        if (wpn->GetSkillValue(RPGWEAPON_CLASSS) == AddSigned32Wrap(num, 1))
            return value;
    }
    return 0;
}


int zSurvive::GetAddWpnId(
    int period, int num, iRPGHero* hero, iRPGFrameWork* frameWork)
{
    iSurvive* const interval = this;
    iSurvive* pSurvive = 0;
    int resolvedIndex = 0;
    const int resolvedValue = interval->GetIntrval(period, pSurvive, resolvedIndex, 0);

    const int periodLength = pSurvive->GetPeriodLength(resolvedIndex);
    if (resolvedValue < periodLength)
    {
        if (resolvedIndex > 0)
        {
            --resolvedIndex;
        }
        else
        {
            pSurvive = pSurvive->GetPrevInt();
            if (pSurvive == 0)
                DebugAssert(5, "pSurvive", SurviveSourceFile(), 337);
            if (pSurvive != 0)
                resolvedIndex = SubtractSigned32Wrap(pSurvive->GetPerCnt(), 1);
        }
    }

    if (pSurvive == 0)
    {
        DebugAssert(5, "pSurvive", SurviveSourceFile(), 343);
        if (pSurvive == 0)
            return 0;
    }

    return pSurvive->ResolveWeaponIdForClass(resolvedIndex, num, hero, frameWork);
}


int zSurvive::GetAbsPeriodNum(int value)
{
    iRPGObj* current = this;
    if (current == 0)
        return value;

    for (;;)
    {
        const int previousBaseId = SubtractSigned32Wrap(current->GetBaseId(), 1);
        iRPGObj* object = m_frameWork->GetItemById(previousBaseId);
        if (object == 0 || object->GetGameObj() != TYPE_SURVIVEINTERVAL)
            return value;

        current = object;
        zSurvive* const primary = static_cast<zSurvive*>(object->QueryInterface(53));
        iSurvive* interval = static_cast<iSurvive*>(primary);
        value = AddSigned32Wrap(value, interval->GetPerCnt());
    }
}


int zSurvive::GetMonst2Create(int _iPeriod, int tactTime, int _iMonstNum)
{
    if (_iMonstNum >= GetMonsterVidsCnt(_iPeriod))
        DebugAssert(5,
            "_iMonstNum < GetMonsterVidsCnt( _iPeriod )", SurviveSourceFile(), 715);

    const int periodLength = GetPeriodLength(_iPeriod);
    const int tactSeconds = tactTime / 1000;
    if (tactSeconds >= periodLength)
        return 0;

    const zSurviveVector<int>* nested =
        &m_aiMonstCnt.at(_iPeriod);
    const int configuredCount = nested->at(_iMonstNum);

    return configuredCount < 0
        ? GetMonst2Create_type1(_iPeriod, tactTime, _iMonstNum)
        : GetMonst2Create_type2(_iPeriod, tactTime, _iMonstNum);
}


int zSurvive::GetWithoutMenu(int index)
{
    const unsigned int count = m_withoutMenu.size();
    if (index >= BitsToSigned32(count))
        return 0;
    return m_withoutMenu.at(index);
}


void zSurvive::ResetMonst2Create()
{
    m_pM2CInterval = 0;
    m_iM2CPeriod = -777;
}

static int SurviveMulDivSigned32(int lhs, int rhs, int divisor)
{

    const unsigned int wrapped =
        static_cast<unsigned int>(lhs) * static_cast<unsigned int>(rhs);
    return static_cast<int>(wrapped) / divisor;
}


int zSurvive::GetMonst2Create_type1(int period, int tactTime, int monstNum)
{
    zSurviveVector<int>* const producedCache = &m_aM2CMonst;
    zSurviveVector<int>* const tactCache = &m_aM2CLastTime;

    if (m_pM2CInterval != this || m_iM2CPeriod != period)
    {
        m_pM2CInterval = this;
        m_iM2CPeriod = period;
        producedCache->clear();
        tactCache->clear();

        iSurvive* const interval = static_cast<iSurvive*>(this);
        unsigned int index = 0;

        if (interval->GetMonsterVidsCnt(period) != 0)
        {
            do
            {
                producedCache->push_back(0);
                tactCache->push_back(0);

                ++index;
            }
            while (index < static_cast<unsigned int>(
                interval->GetMonsterVidsCnt(period)));
        }
    }

    iSurvive* const interval = static_cast<iSurvive*>(this);
    const int target = interval->GetMonstersCount(period, monstNum);

    const int elapsedSeconds = tactTime / 1000;
    const int periodLength = interval->GetPeriodLength(period);

    int deltaTact = SubtractSigned32Wrap(tactTime, tactCache->at(monstNum));
    if (elapsedSeconds >= periodLength)
        deltaTact = 0;
    tactCache->at(monstNum) = tactTime;

    if ((deltaTact / 1000) >= periodLength)
        DebugAssert(4,
            "iTactTime/1000 < PeriodLength", SurviveSourceFile(), 760);

    const int scaled = SurviveMulDivSigned32(deltaTact, target, periodLength);
    int result = scaled / 1000;
    const int remainder = SubtractSigned32Wrap(scaled, MultiplySigned32Wrap(result, 1000));

    if (AddSigned32Wrap(producedCache->at(monstNum), result) < target && remainder != 0)
    {
        if (Random(999) < remainder)
            result = AddSigned32Wrap(result, 1);
    }

    const int alreadyProduced =
        static_cast<int>(producedCache->at(monstNum));
    const int expected =
        SurviveMulDivSigned32(elapsedSeconds, target, periodLength);
    const int difference = SubtractSigned32Wrap(expected, alreadyProduced);
    int error = SubtractSigned32Wrap(difference, result);
    if (error < 0)
        error = SubtractSigned32Wrap(0, error);

    const int correctionLimit = SurviveMulDivSigned32(target, 5, 100);
    if (error > correctionLimit)
    {
        result = SubtractSigned32Wrap(expected, producedCache->at(monstNum));
        if (result < 0)
            result = 0;
    }

    const bool bossPeriod = static_cast<iSurvive*>(this)->IsBossPeriod(period);
    if (bossPeriod && AddSigned32Wrap(producedCache->at(monstNum), result) < target)
        result = AddSigned32Wrap(result, 1);
    else if (target == 1 && AddSigned32Wrap(producedCache->at(monstNum), result) < target &&
             elapsedSeconds > (periodLength / 4))
        result = AddSigned32Wrap(result, 1);

    if (AddSigned32Wrap(producedCache->at(monstNum), result) < target &&
        elapsedSeconds >= SubtractSigned32Wrap(periodLength, 1))
        result = SubtractSigned32Wrap(target, producedCache->at(monstNum));

    if (AddSigned32Wrap(producedCache->at(monstNum), result) > target)
        result = SubtractSigned32Wrap(target, producedCache->at(monstNum));
    if (result < 0)
        result = 0;

    int& produced = producedCache->at(monstNum);
    produced = AddSigned32Wrap(produced, result);
    return result;
}


int zSurvive::GetMonst2Create_type2(int period, int unusedTactSeconds, int monstNum)
{
    (void)unusedTactSeconds;

    iSurvive* const interval = static_cast<iSurvive*>(this);
    const int first = interval->GetMonstersCount(period, monstNum);
    const int second = interval->GetMonstersVid(period, monstNum);
    return SubtractSigned32Wrap(first, m_frameWork->GetNoSprites(second, 1u));
}


void zSurvive::BeginArray(int kind)
{
    if (kind != RPGSURVIVE_MONSTERS && kind != RPGSURVIVE_MONSTERSCOUNT)
        return;
    zSurviveVector<zSurviveVector<int> >& outer = kind == RPGSURVIVE_MONSTERS ? m_aiMonstVids : m_aiMonstCnt;
    outer.resize(outer.size() + 1u);
}


int zSurvive::ResolveWeaponIdForClass(
    int period, int _iNum, iRPGHero* _pHero, iRPGFrameWork* frameWork)
{
    if (_pHero == 0)
        DebugAssert(4, "_pHero", SurviveSourceFile(), 356);
    if (_iNum < 0 || _iNum >= 5)
        DebugAssert(4, "_iNum >= 0 && _iNum < 5", SurviveSourceFile(), 357);

    int target = 0;
    const iRPGObj* equipped = _pHero->GetEquip(_iNum + 2);
    if (equipped != 0)
        target = equipped->GetBaseId();

    iSurvive* const initialSecondary = this;
    zSurvive* primary = static_cast<zSurvive*>(initialSecondary);
    int current = primary->GetWpnIdForClass(period, _iNum, _pHero, frameWork);
    if (current == 0)
    {
        if (target != 0)
        {
            iLogConsole* const logger = ::gpLogCons;
            logger->LogStr(LT_ERR,
                "Error in Survive.cfg. prd = %d. wpn class = %d",
                period, _iNum);
            DebugAssert(5, "0", SurviveSourceFile(), 369);
        }
        return 0;
    }

    int previous = 0;
    iSurvive* chain = initialSecondary;

    while (chain != 0)
    {
        zSurvive* chainPrimary = static_cast<zSurvive*>(chain->QueryInterface(53));

        while (period >= 0)
        {
            const int candidate = chainPrimary->GetWpnIdForClass(
                period, _iNum, _pHero, frameWork);
            if (candidate == target)
                return previous;
            --period;
            previous = candidate;
        }

        chain = chain->GetPrevInt();
        if (chain == 0)
            break;

        zSurvive* const previousPrimary = static_cast<zSurvive*>(chain->QueryInterface(53));
        period = SubtractSigned32Wrap(static_cast<iSurvive*>(previousPrimary)->GetPerCnt(), 1);
    }

    return target != 0 ? previous : 0;
}

void zSurvive::DestroyMonsterCountCache()
{
    m_aM2CMonst.~zSurviveVector<int>();
}

void zSurvive::DestroyMonsterTimeCache()
{
    m_aM2CLastTime.~zSurviveVector<int>();
}

unsigned int __cdecl zSurvive::RegisterMonsterCountCache()
{
    return RegisterCrtAtExit(&zSurvive::DestroyMonsterCountCache);
}

unsigned int __cdecl zSurvive::RegisterMonsterTimeCache()
{
    return RegisterCrtAtExit(&zSurvive::DestroyMonsterTimeCache);
}

static zSurviveVector<int>* SurviveAppendEmptyNestedControl(zSurviveVector<zSurviveVector<int> >& outer)
{
    outer.resize(outer.size() + 1u);
    return outer.m_end - 1;
}

bool zSurvive::ParseUnknownCommand(
    const char* name, int count, const char** values, iRPGFrameWork* frameWork)
{
    if (zRPGObj::ParseUnknownCommand(name, count, values, frameWork))
        return true;

    if (std::strcmp(name, "BossSize") == 0)
    {
        m_iBossSize = BitsToSigned32(CrtAtoiText(values[0]));
        return true;
    }
    if (std::strcmp(name, "BossHpCoeff") == 0)
    {
        m_iBossHpCoeff = BitsToSigned32(CrtAtoiText(values[0]));
        return true;
    }
    if (std::strcmp(name, "PauseBoss") == 0)
    {
        m_iPauseBoss = BitsToSigned32(CrtAtoiText(values[0]));

        SurviveAppendEmptyNestedControl(m_aiMonstVids);

        const int skillValue = GetSkillValue(RPGSURVIVE_BOSSVID);
        zSurviveVector<int>* vids =
            static_cast<zSurviveVector<int>*>(m_aiMonstVids.m_begin) +
            m_aiMonstVids.size() - 1;
        vids->push_back(skillValue);

        zSurviveVector<int>* counts = SurviveAppendEmptyNestedControl(m_aiMonstCnt);
        counts->push_back(-1);
        const int pauseBoss = m_iPauseBoss;
        m_aiPeriodsLen.push_back(pauseBoss);
        m_aiPauseLen.push_back(0);
        m_withoutMenu.push_back(1);

        const unsigned int regularPeriods = m_aiPeriodsLen.size() - 1u;
        m_bossPeriodFlags.resize(regularPeriods, false);
        m_bossPeriodFlags.push_back(true);
        return true;
    }
    if (std::strcmp(name, "BossColor") == 0 && count == 3)
    {
        m_iBossColor[0] = BitsToSigned32(CrtAtoiText(values[0]));
        m_iBossColor[1] = BitsToSigned32(CrtAtoiText(values[1]));
        m_iBossColor[2] = BitsToSigned32(CrtAtoiText(values[2]));
        return true;
    }
    if (std::strcmp(name, "ScoresCoeff") == 0)
    {
        m_iScoresCoeff = BitsToSigned32(CrtAtoiText(values[0]));
        return true;
    }

    int mode = 0;
    if (std::strcmp(name, "Bonuses") == 0)
        mode = 1;
    else if (std::strcmp(name, "BossBonuses") == 0)
        mode = 2;
    else if (std::strcmp(name, "AddWeapon") == 0)
        mode = 3;
    else
        return false;

    if (mode == 3)
    {
        SurviveAppendEmptyNestedControl(m_addedWeaponLists);
    }

    for (int i = 0; i < count; ++i)
    {
        int resolvedId = 0;
        iRPGObj* resolved = frameWork->GetObjByNamee(values[i], true);

        if (resolved != 0)
        {
            resolvedId = resolved->GetBaseId();
        }
        else if (std::strcmp(values[i], "Empty") != 0)
        {
            iLogConsole* const logger = ::gpLogCons;
            logger->LogStr(LT_INPTERR, "Survive: Object %s was not found!", values[i]);
        }

        if (mode == 1)
            m_aiBonus.push_back(resolvedId);
        else if (mode == 2)
            m_aiBossBonus.push_back(resolvedId);
        else
        {
            zSurviveVector<int>* addWeaponList =
                &m_addedWeaponLists.at(m_addedWeaponLists.size() - 1);
            addWeaponList->push_back(resolvedId);
        }
    }
    return true;
}


int zSurvive::GetMonsterVidsCnt(int _iPeriod)
{
    const unsigned int monstCnt = m_aiMonstCnt.size();
    const unsigned int monstVids = m_aiMonstVids.size();
    if (monstCnt != monstVids)
        DebugAssert(4, "m_aiMonstCnt.size() == m_aiMonstVids.size()", SurviveSourceFile(), 257);
    const unsigned int periods = m_aiPeriodsLen.size();
    if (m_aiMonstCnt.size() != periods)
        DebugAssert(4, "m_aiMonstCnt.size() == m_aiPeriodsLen.size()", SurviveSourceFile(), 258);
    const unsigned int pauses = m_aiPauseLen.size();
    if (m_aiMonstCnt.size() != pauses)
        DebugAssert(4, "m_aiMonstCnt.size() == m_aiPauseLen.size()", SurviveSourceFile(), 259);
    const unsigned int currentMonstVids = m_aiMonstVids.size();
    if (currentMonstVids != 0 && _iPeriod >= BitsToSigned32(currentMonstVids))
        DebugAssert(4, "!m_aiMonstVids.size() || _iPeriod < (int)m_aiMonstVids.size()", SurviveSourceFile(), 262);
    if (m_aiMonstVids.size() == 0 || _iPeriod == -1)
        return 0;
    const zSurviveVector<int>* nested =
        &m_aiMonstVids.at(_iPeriod);
    return static_cast<int>((reinterpret_cast<const unsigned char*>(nested->m_end)-
                             reinterpret_cast<const unsigned char*>(nested->m_begin))>>2);
}

int zSurvive::GetMonstersCount(int _iPeriod, int _iMonstersNum)
{
    if (_iMonstersNum >= GetMonsterVidsCnt(_iPeriod))
        DebugAssert(4, "_iMonstersNum < GetMonsterVidsCnt( _iPeriod )", SurviveSourceFile(), 272);
    int configuredCount = 0;
    if (_iPeriod != -1)
    {
        const zSurviveVector<int>* nested =
            &m_aiMonstCnt.at(_iPeriod);
        configuredCount = nested->at(_iMonstersNum);
    }
    const int monsterCountScale = ms_MonstCntCoeff;
    return SurviveAbsScaled(configuredCount, monsterCountScale);
}

int zSurvive::GetMonstersVid(int _iPeriod, int _iMonstersNum)
{
    if (_iMonstersNum >= GetMonsterVidsCnt(_iPeriod))
        DebugAssert(4, "_iMonstersNum < GetMonsterVidsCnt( _iPeriod )", SurviveSourceFile(), 449);
    if (_iPeriod == -1)
        return 0;
    const zSurviveVector<int>* nested =
        &m_aiMonstVids.at(_iPeriod);
    return nested->at(_iMonstersNum);
}

int zSurvive::GetPeriodLength(int _iPeriod)
{
    const unsigned int periodCount = m_aiPeriodsLen.size();
    if (_iPeriod >= BitsToSigned32(periodCount))
        DebugAssert(4, "_iPeriod < (int)m_aiPeriodsLen.size()", SurviveSourceFile(), 457);
    return _iPeriod == -1 ? 0 : m_aiPeriodsLen.at(_iPeriod);
}

int zSurvive::GetPauseLength(int _iPeriod)
{
    const unsigned int periodCount = m_aiPeriodsLen.size();
    if (_iPeriod >= BitsToSigned32(periodCount))
        DebugAssert(4, "_iPeriod < (int)m_aiPeriodsLen.size()", SurviveSourceFile(), 465);
    return _iPeriod == -1 ? 0 : m_aiPauseLen.at(_iPeriod);
}

int zSurvive::GetBossToCrt(int period)
{
    (void)period;
    return 0;
}

int zSurvive::GetBossBonusesCnt(int _iPeriod)
{
    const unsigned int periodCount = m_aiPeriodsLen.size();
    if (_iPeriod >= BitsToSigned32(periodCount))
        DebugAssert(4, "_iPeriod < (int)m_aiPeriodsLen.size()", SurviveSourceFile(), 612);
    return static_cast<int>(m_aiBossBonus.size());
}

int zSurvive::GetBossBonusBaseId(int _iPeriod, int _iNum)
{
    const unsigned int periodCount = m_aiPeriodsLen.size();
    if (_iPeriod >= BitsToSigned32(periodCount))
        DebugAssert(4, "_iPeriod < (int)m_aiPeriodsLen.size()", SurviveSourceFile(), 621);
    const unsigned int bonusCount = m_aiBossBonus.size();
    if (_iNum >= BitsToSigned32(bonusCount))
        DebugAssert(4, "_iNum < (int)m_aiBossBonus.size()", SurviveSourceFile(), 622);
    return m_aiBossBonus.at(_iNum);
}

int zSurvive::GetPeriodBonusId(int _iPeriod)
{
    const unsigned int periods = m_aiPeriodsLen.size();
    if (_iPeriod >= BitsToSigned32(periods))
        DebugAssert(4, "_iPeriod < (int)m_aiPeriodsLen.size()", SurviveSourceFile(), 647);
    const unsigned int bonusCount = m_aiBonus.size();
    const unsigned int bonusRandCount = m_aiBonusRand.size();
    if (bonusCount != bonusRandCount)
        DebugAssert(4, "m_aiBonus.size() == m_aiBonusRand.size()", SurviveSourceFile(), 648);
    if (IsBossPeriod(_iPeriod))
    {
        if (m_aiBossBonus.m_begin != 0 && m_aiBossBonus.size() != 0)
            return m_aiBossBonus.at(0);
        return 0;
    }
    int total = 0;
    for (unsigned int i = 0;i<m_aiBonusRand.size();++i)
        total = AddSigned32Wrap(total, GetBonusBaseId(static_cast<int>(i)));
    int randomValue = Random(SubtractSigned32Wrap(total, 1));
    for (unsigned int i = 0;i<m_aiBonusRand.size();++i)
    {
        randomValue = SubtractSigned32Wrap(randomValue, GetBonusBaseId(static_cast<int>(i)));
        if (randomValue<0)
            return m_aiBonus.at(i);
    }
    return 0;
}

int zSurvive::GetFullLen()
{
    const unsigned int periods = m_aiPeriodsLen.size();
    const unsigned int pauses = m_aiPauseLen.size();
    if (periods != pauses)
        DebugAssert(4, "m_aiPeriodsLen.size() == m_aiPauseLen.size()", SurviveSourceFile(), 193);
    int result = 0;
    for (unsigned int i = 0;i<m_aiPeriodsLen.size();++i)
    {
        result = AddSigned32Wrap(result, m_aiPeriodsLen.at(i));
        result = AddSigned32Wrap(result, m_aiPauseLen.at(i));
    }
    return result;
}

int zSurvive::GetAddWpnCnt(int period)
{
    (void)period;
    return 5;
}


int zSurvive::GetPerCnt()
{
    return static_cast<int>(m_aiPeriodsLen.size());
}

bool zSurvive::IsBossPeriod(int period)
{
    return period < BitsToSigned32(m_bossPeriodFlags.m_bitCount)
        ? m_bossPeriodFlags.at(period) : false;
}

int zSurvive::GetBossSize()
{
    return m_iBossSize;
}
int zSurvive::GetBossHpCoeff()
{
    return m_iBossHpCoeff;
}
int zSurvive::GetBossColor(int index)
{
    return m_iBossColor[index];
}
int zSurvive::GetScoresCoeff()
{
    return m_iScoresCoeff;
}


