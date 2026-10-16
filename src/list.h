#pragma once

#include "mylib.h"
#include "exports.h"

template<class T>
class LIST
{
public:
    virtual ~LIST();

    int No() const;
    void Push(const T& value);
    void Insert(T value);
    void InsertFirst(T value);
    void InsertBefore(int index,T value);
    int DeleteNumber(int index);
    int Location(const T& value) const;
    void Release();
    void Expand(int requestedCapacity);
    void ExpandForInsert();
    T NextIterateNon0(int* index) const;

    T* Data()
    {
        return m_data;
    }
    const T* Data() const
    {
        return m_data;
    }
    int Capacity() const
    {
        return m_capacity;
    }

public:

int m_count;
    int m_capacity;
    T*  m_data;
};

template<class T>
struct NAMED_LIST_STRUCT
{
    int m_hash;
    STRING m_name;
    T m_value;

    NAMED_LIST_STRUCT();
    NAMED_LIST_STRUCT& operator=(const NAMED_LIST_STRUCT& source);
    ~NAMED_LIST_STRUCT();
};

template<class T>
class NAMED_LIST : public LIST<NAMED_LIST_STRUCT<T> >
{
public:
    int Location(const STRING& name) const;
    void Insert(STRING name,T value);
    STRING Name(int index) const;
    static int GetHash(const char* name);

    T& Value(int index)
    {
        return this->m_data[index].m_value;
    }
    const T& Value(int index) const
    {
        return this->m_data[index].m_value;
    }
    NAMED_LIST_STRUCT<T>& Record(int index)
    {
        return this->m_data[index];
    }
    const NAMED_LIST_STRUCT<T>& Record(int index) const
    {
        return this->m_data[index];
    }
};

struct ACT;
template<> LIST<ACT>::~LIST();

template<> LIST<int>::~LIST();
template<> void LIST<int>::Expand(int requestedCapacity);

class SPRITE;

template<> LIST<SPRITE*>::~LIST();
template<> void LIST<SPRITE*>::Expand(int requestedCapacity);

template<> SPRITE* LIST<SPRITE*>::NextIterateNon0(int* index) const;


#include "basetype.h"

