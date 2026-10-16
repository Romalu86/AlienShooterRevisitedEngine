#pragma once
#include <stddef.h>
#include <memory>
#include <string>
#include "zm/Utils/zContainerProxy.h"

enum { REC_TYPES_CNT = 4 };

struct AS_RECORD_DWORD_VECTOR
{
    zContainerProxy<AS_RECORD_DWORD_VECTOR>* proxy;
    std::allocator<int> allocator;
    int* begin;
    int* end;
    int* capacityEnd;
    AS_RECORD_DWORD_VECTOR();
    AS_RECORD_DWORD_VECTOR(const AS_RECORD_DWORD_VECTOR& other);
    ~AS_RECORD_DWORD_VECTOR() noexcept(false);

    const int& At(unsigned int index) const;
    AS_RECORD_DWORD_VECTOR& operator=(const AS_RECORD_DWORD_VECTOR& other);
    void insert(int* position, unsigned int count, const int& value);
    int* insert(int* position, const int& value);
};

struct AS_RECORD_TEXT
{
    std::allocator<char> allocator;
    union
    {
        char localText[16];
        char* allocatedText;
    };
    unsigned int size;
    unsigned int capacity;

    AS_RECORD_TEXT() noexcept;
    AS_RECORD_TEXT(const AS_RECORD_TEXT& other);
    ~AS_RECORD_TEXT() noexcept;
    AS_RECORD_TEXT& operator=(const AS_RECORD_TEXT& other);
    AS_RECORD_TEXT& operator=(const char* text);
    AS_RECORD_TEXT& assign(const char* text, unsigned int length);
    AS_RECORD_TEXT& append(const char* text, unsigned int length);
    AS_RECORD_TEXT& append(const AS_RECORD_TEXT& other, unsigned int offset, unsigned int length);
    AS_RECORD_TEXT& append(char value);
    const char* c_str() const noexcept;
    char* Data() noexcept;
    operator std::string() const;

private:
    void Grow(unsigned int length);
};

struct AS_RECORD
{
    AS_RECORD_TEXT m_playerName;
    int m_totalScore;
    int m_monsterKills;
    int m_elapsedSeconds;
    unsigned int m_checksum;
    AS_RECORD_TEXT m_recordTimestamp;
    AS_RECORD_DWORD_VECTOR m_weaponScores;
    AS_RECORD_DWORD_VECTOR m_weaponKills;
    unsigned char m_cheater;

    AS_RECORD();
    AS_RECORD(const AS_RECORD& other);
    ~AS_RECORD() noexcept(false);
    AS_RECORD& operator=(const AS_RECORD& other);


    int GetHash(int mode);
};

struct AS_RECORD_VECTOR
{
    zContainerProxy<AS_RECORD_VECTOR>* proxy;
    std::allocator<AS_RECORD> allocator;
    AS_RECORD* begin;
    AS_RECORD* end;
    AS_RECORD* capacityEnd;
    AS_RECORD_VECTOR();
    ~AS_RECORD_VECTOR() noexcept(false);

    AS_RECORD& At(unsigned int index) const;
    void push_back(const AS_RECORD& record);

private:
    AS_RECORD* insert(AS_RECORD* position,
        zContainerProxy<AS_RECORD_VECTOR>* owner, const AS_RECORD& record);
    void InsertOne(AS_RECORD* position, const AS_RECORD& record);
};

struct AS_RECORD_PTR_VECTOR
{
    zContainerProxy<AS_RECORD_PTR_VECTOR>* proxy;
    std::allocator<AS_RECORD*> allocator;
    AS_RECORD** begin;
    AS_RECORD** end;
    AS_RECORD** capacityEnd;
    AS_RECORD_PTR_VECTOR();
    ~AS_RECORD_PTR_VECTOR() noexcept(false);

    AS_RECORD*& At(unsigned int index);
    AS_RECORD* const& At(unsigned int index) const;
};

class zRecordDB
{
public:


    unsigned char m_DataFlags[REC_TYPES_CNT];
    AS_RECORD_VECTOR m_Data[REC_TYPES_CNT];
    unsigned char m_OutFlags[REC_TYPES_CNT];
    AS_RECORD_PTR_VECTOR m_OutData[REC_TYPES_CNT];


    virtual void* QueryInterface(int interfaceId) const;
    virtual int iRelease();
    zRecordDB();
    virtual ~zRecordDB() noexcept(false);
    static void operator delete(void* memory);
    void AddRecord(int iType, const AS_RECORD& record);
    int GetRecCnt(int iType);
    const AS_RECORD* GetRecData(int iType, int index);
    void ClearTable(int iType);

private:
    bool LoadIfNeed(int iType);
    bool LoadFromFile(int iType);
    bool SaveToFile(int iType);
    void BuildDataIfNeed(int iType);
};

extern zRecordDB* RecordDatabase;


