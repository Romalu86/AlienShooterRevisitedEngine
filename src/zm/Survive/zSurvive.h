#pragma once

#include <stddef.h>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include "../RPGHero/zRPGBaseObject.h"

class iRPGFrameWork;
class iRPGHero;
class iSurvive
{
public:
    virtual void* QueryInterface(int interfaceId) const = 0;
    virtual int iRelease() = 0;
    virtual int GetIntrval(int deltaTime, iSurvive*& outInterval, int& outIndex, int* outDepth) = 0;
    virtual int GetMonsterVidsCnt(int _iPeriod) = 0;
    virtual int GetMonstersCount(int _iPeriod, int _iMonstersNum) = 0;
    virtual int GetMonstersVid(int _iPeriod, int _iMonstersNum) = 0;
    virtual int GetPeriodLength(int _iPeriod) = 0;
    virtual int GetPauseLength(int _iPeriod) = 0;
    virtual int GetBossToCrt(int period) = 0;
    virtual int GetBossBonusesCnt(int _iPeriod) = 0;
    virtual int GetBossBonusBaseId(int _iPeriod, int _iNum) = 0;
    virtual int GetPeriodBonusId(int _iPeriod) = 0;
    virtual int GetFullLen() = 0;
    virtual int GetAddWpnCnt(int period) = 0;
    virtual int ResolveWeaponIdForClass(int period, int monsterClass, iRPGHero* hero, iRPGFrameWork* frameWork) = 0;
    virtual int GetAddWpnId(int period, int num, iRPGHero* hero, iRPGFrameWork* frameWork) = 0;
    virtual iSurvive* GetPrevInt() = 0;
    virtual int GetPerCnt() = 0;
    virtual bool IsBossPeriod(int period) = 0;
    virtual int GetBossSize() = 0;
    virtual int GetBossHpCoeff() = 0;
    virtual int GetBossColor(int index) = 0;
    virtual int GetScoresCoeff() = 0;
};

template <class T>
class zSurviveVector
{
public:
    T* m_begin;
    T* m_end;
    T* m_capacity;
    unsigned char m_allocatorState;

    zSurviveVector() : m_begin(0), m_end(0), m_capacity(0) {}
    zSurviveVector(zSurviveVector&& other) noexcept
        : m_begin(other.m_begin), m_end(other.m_end), m_capacity(other.m_capacity)
    {
        other.m_begin = 0;
        other.m_end = 0;
        other.m_capacity = 0;
    }
    ~zSurviveVector() noexcept(false)
    {
        clear();
        if (m_begin)
            CrtFreePointer(m_begin);
        m_begin = 0;
        m_end = 0;
        m_capacity = 0;
    }
    unsigned int size() const
    {
        return static_cast<unsigned int>(m_end - m_begin);
    }
    T& at(unsigned int index) { return m_begin[index]; }
    const T& at(unsigned int index) const { return m_begin[index]; }

    void clear()
    {
        for (T* item = m_begin; item != m_end; ++item)
            item->~T();
        m_end = m_begin;
    }
    void push_back(const T& value)
    {
        const T* source = &value;
        const bool alias = source >= m_begin && source < m_end;
        const unsigned int index = alias ? static_cast<unsigned int>(source - m_begin) : 0u;
        reserveFor(1u);
        if (alias)
            source = m_begin + index;
        ::new(m_end) T(*source);
        ++m_end;
    }
    void resize(unsigned int count)
    {
        const unsigned int current = size();
        if (count < current)
        {
            T* const end = m_begin + count;
            for (T* item = end; item != m_end; ++item)
                item->~T();
            m_end = end;
        }
        else if (current < count)
        {
            reserveFor(count - current);
            while (size() < count)
                ::new(m_end++) T();
        }
    }

private:
    void reserveFor(unsigned int count)
    {
        const unsigned int maximum = 0xffffffffu / sizeof(T);
        const unsigned int current = size();
        if (maximum - current < count)
            throw std::length_error("vector<T> too long");
        const unsigned int capacity = static_cast<unsigned int>(m_capacity - m_begin);
        if (current + count <= capacity)
            return;
        const unsigned int half = capacity / 2u;
        unsigned int nextCapacity = capacity > maximum - half ? 0u : capacity + half;
        if (nextCapacity < current + count)
            nextCapacity = current + count;
        T* const storage = static_cast<T*>(CrtNew(nextCapacity * sizeof(T)));
        if (!storage)
            throw std::bad_alloc();
        for (unsigned int index = 0; index < current; ++index)
            ::new(storage + index) T(static_cast<T&&>(m_begin[index]));
        clear();
        if (m_begin)
            CrtFreePointer(m_begin);
        m_begin = storage;
        m_end = storage + current;
        m_capacity = storage + nextCapacity;
    }
};

class zSurviveBitVector
{
public:
    zSurviveVector<unsigned int> m_words;
    unsigned int m_bitCount;

    zSurviveBitVector() : m_bitCount(0) {}
    ~zSurviveBitVector() noexcept(false) { m_bitCount = 0; }
    bool at(int index) const
    {
        return (m_words.m_begin[index >> 5] & (1u << (index & 31))) != 0;
    }
    void resize(unsigned int count, bool value)
    {
        const unsigned int oldCount = m_bitCount;
        if (count == oldCount)
            return;
        m_words.resize((count + 31u) >> 5);
        if (oldCount < count)
        {
            for (unsigned int index = oldCount; index < count; ++index)
            {
                unsigned int& word = m_words.m_begin[index >> 5];
                const unsigned int mask = 1u << (index & 31u);
                if (value) word |= mask;
                else word &= ~mask;
            }
        }
        m_bitCount = count;
        if (count < oldCount && (count & 31u) != 0u)
            m_words.m_end[-1] &= (1u << (count & 31u)) - 1u;
    }
    void push_back(bool value)
    {
        if (m_bitCount == 0xffffffffu)
            throw std::length_error("vector<bool> too long");
        resize(m_bitCount + 1u, value);
    }
};


class zSurvive : public zRPGObj, public iSurvive
{
public:
    zSurviveVector<zSurviveVector<int> > m_aiMonstVids;
    zSurviveVector<zSurviveVector<int> > m_aiMonstCnt;
    zSurviveVector<int> m_aiPeriodsLen;
    zSurviveVector<int> m_aiPauseLen;
    zSurviveVector<int> m_withoutMenu;
    zSurviveBitVector m_bossPeriodFlags;
    zSurviveVector<int> m_aiBonus;
    zSurviveVector<int> m_aiBonusRand;
    zSurviveVector<zSurviveVector<int> > m_addedWeaponLists;

    iRPGFrameWork* m_frameWork;
    int m_iBossSize;
    int m_iBossHpCoeff;
    int m_iPauseBoss;
    int m_iBossColor[3];
    zSurviveVector<int> m_aiBossBonus;
    int m_iScoresCoeff;

    zSurvive(OBJ_TYPE _eType,iRPGFrameWork* frameWork);
    ~zSurvive() override;

    void* QueryInterface(int interfaceId) const override;
    int iRelease() override;
    void SetDataArrays() override;
    void InitParams() override;
    bool GetParams(const char* name, int nameLength, int& outCount, int* outKind) const override;
    bool ParseUnknownCommand(const char* name, int count, const char** values, iRPGFrameWork* frameWork) override;
    void SetCharValue(int stringParam, const char* textValue) override;
    void OnSkillChange(int skillSelector, int oldValue, int newValue) override;
    void BeginArray(int kind) override;
    int GetIntrval(int deltaTime, iSurvive*& outInterval, int& outIndex, int* outDepth) override;
    int GetMonsterVidsCnt(int _iPeriod) override;
    int GetMonstersCount(int _iPeriod, int _iMonstersNum) override;
    int GetMonstersVid(int _iPeriod, int _iMonstersNum) override;
    int GetPeriodLength(int _iPeriod) override;
    int GetPauseLength(int _iPeriod) override;
    int GetBossToCrt(int period) override;
    int GetBossBonusesCnt(int _iPeriod) override;
    int GetBossBonusBaseId(int _iPeriod, int _iNum) override;
    int GetPeriodBonusId(int _iPeriod) override;
    int GetFullLen() override;
    int GetAddWpnCnt(int period) override;
    int ResolveWeaponIdForClass(int period, int monsterClass, iRPGHero* hero, iRPGFrameWork* frameWork) override;
    int GetAddWpnId(int period, int num, iRPGHero* hero, iRPGFrameWork* frameWork) override;
    iSurvive* GetPrevInt() override;
    int GetPerCnt() override;
    bool IsBossPeriod(int period) override;
    int GetBossSize() override;
    int GetBossHpCoeff() override;
    int GetBossColor(int index) override;
    int GetScoresCoeff() override;
    zSurvive* GetNextInt();
    int GetWpnIdForClass(int period, int num, iRPGHero* hero, iRPGFrameWork* frameWork);
    int GetAbsPeriodNum(int value);
    int GetBonusBaseId(int _iNum);
    int GetWithoutMenu(int index);
    static void ResetMonst2Create();
    static int ms_MonstCntCoeff;
    static int ms_EmptyBonusWeightCoeff;
    static unsigned int __cdecl RegisterMonsterCountCache();
    static unsigned int __cdecl RegisterMonsterTimeCache();
    int GetMonst2Create(int period, int tactTime, int monstNum);

private:
    static bool m__abChangeableParams[MAX_RPGPARAMSCOUNT];
    static bool m__abBaseParams[MAX_RPGPARAMSCOUNT];
    static bool m__abArrayParams[MAX_RPGPARAMSCOUNT];
    static zSurvive* m_pM2CInterval;
    static int m_iM2CPeriod;
    static zSurviveVector<int>& m_aM2CMonst;
    static zSurviveVector<int>& m_aM2CLastTime;
    static void DestroyMonsterCountCache();
    static void DestroyMonsterTimeCache();

    int GetMonst2Create_type1(int period, int tactTime, int monstNum);
    int GetMonst2Create_type2(int period, int tactTime, int monstNum);
};


