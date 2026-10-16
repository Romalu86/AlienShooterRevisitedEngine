#pragma once

#include <cstring>
#include <cstdlib>
#include <stdexcept>
#include "basetype.h"
#include "zm/Utils/zContainerProxy.h"

template<class T>
struct zQuestVector
{
    zContainerProxy<zQuestVector<T>>* m_owner;
    T* m_begin;
    T* m_end;
    T* m_capEnd;

    struct Iterator
    {
        zContainerProxy<zQuestVector<T>>* owner;
        T* position;
    };

    zQuestVector()
        : m_owner(static_cast<zContainerProxy<zQuestVector<T>>*>(
              CrtNew(sizeof(zContainerProxy<zQuestVector<T>>)))),
          m_begin(0), m_end(0), m_capEnd(0)
    {
        if (m_owner)
            m_owner->m_owner = this;
    }
    ~zQuestVector() noexcept(false)
    {
        CrtFreePointer(m_begin);
        m_begin = 0;
        m_end = 0;
        m_capEnd = 0;
        CrtFreePointer(m_owner);
    }

    unsigned int size() const
    {
        return static_cast<unsigned int>(m_end-m_begin);
    }

    Iterator Begin()
    {
        if (m_begin > m_end)
            ::_invalid_parameter_noinfo();
        Iterator result = { m_owner, m_begin };
        return result;
    }

    Iterator End()
    {
        if (m_begin > m_end)
            ::_invalid_parameter_noinfo();
        Iterator result = { m_owner, m_end };
        return result;
    }

    static bool Less(Iterator first, Iterator second)
    {
        if (!first.owner || first.owner != second.owner)
            ::_invalid_parameter_noinfo();
        return first.position < second.position;
    }

    static T& Value(Iterator position)
    {
        if (!position.owner)
            ::_invalid_parameter_noinfo();
        zQuestVector<T>* const container = position.owner ? position.owner->m_owner : 0;
        if (position.position >= container->m_end)
            ::_invalid_parameter_noinfo();
        return *position.position;
    }

    static void Increment(Iterator& position)
    {
        if (!position.owner)
            ::_invalid_parameter_noinfo();
        zQuestVector<T>* const container = position.owner ? position.owner->m_owner : 0;
        if (position.position >= container->m_end)
            ::_invalid_parameter_noinfo();
        ++position.position;
    }

    Iterator Erase(Iterator position)
    {
        for (T* next = position.position+1; next != m_end; ++next)
            *(next-1) = *next;
        --m_end;
        if (position.position < m_begin || position.position > m_end)
            ::_invalid_parameter_noinfo();
        Iterator result = { m_owner, position.position };
        return result;
    }

    Iterator Erase(Iterator first, Iterator last)
    {
        if (PtrToU32(this) == 0u || first.position < m_begin || first.position > m_end)
            ::_invalid_parameter_noinfo();
        Iterator result = { m_owner, first.position };
        if (last.position < m_begin || last.position > m_end)
            ::_invalid_parameter_noinfo();
        if (!result.owner || result.owner != m_owner)
            ::_invalid_parameter_noinfo();
        if (first.position != last.position)
        {
            const int remaining = static_cast<int>(m_end-last.position);
            if (remaining > 0)
                ::memmove_s(first.position, remaining*sizeof(T), last.position, remaining*sizeof(T));
            m_end = first.position+remaining;
        }
        return result;
    }

    T& at(unsigned int index)
    {
        if (index >= size())
            ::_invalid_parameter_noinfo();
        return m_begin[index];
    }
    const T& at(unsigned int index) const
    {
        if (index >= size())
            ::_invalid_parameter_noinfo();
        return m_begin[index];
    }

    void Append(const T& value)
    {
        const unsigned int reserved = m_begin ?
            static_cast<unsigned int>(m_capEnd-m_begin) : 0u;
        if (size() < reserved)
        {
            *m_end++ = value;
            return;
        }
        if (m_begin > m_end)
            ::_invalid_parameter_noinfo();
        Iterator position = { m_owner, m_end };
        Insert(position, value);
    }

    Iterator Insert(Iterator position, const T& value)
    {
        unsigned int index = 0u;
        if (size() != 0u)
        {
            if (m_begin > m_end)
                ::_invalid_parameter_noinfo();
            if (!position.owner || position.owner != m_owner)
                ::_invalid_parameter_noinfo();
            index = static_cast<unsigned int>(position.position-m_begin);
        }
        InsertValue(position.position, value);
        if (m_begin > m_end)
            ::_invalid_parameter_noinfo();
        if (!m_owner)
            ::_invalid_parameter_noinfo();
        zQuestVector<T>* const container = m_owner ? m_owner->m_owner : 0;
        T* const inserted = m_begin+index;
        if (inserted > container->m_end || inserted < container->m_begin)
            ::_invalid_parameter_noinfo();
        Iterator result = { m_owner, inserted };
        return result;
    }

private:
    void InsertValue(T* position, const T& value)
    {
        const unsigned int count = 1u;
        const T insertedValue = value;
        const unsigned int oldSize = size();
        const unsigned int reserved = m_begin ?
            static_cast<unsigned int>(m_capEnd-m_begin) : 0u;
        const unsigned int maximum = 0xffffffffu / static_cast<unsigned int>(sizeof(T));
        if (maximum-oldSize<count)
            throw std::length_error("vector<T> too long");
        if (reserved<oldSize+count)
        {
            const unsigned int half = reserved/2u;
            unsigned int capacity = reserved>maximum-half ? 0u : reserved+half;
            if (capacity<oldSize+count)
                capacity = oldSize+count;
            T* const storage = static_cast<T*>(CrtNew(capacity*sizeof(T)));
            const unsigned int prefix = m_begin ?
                static_cast<unsigned int>(position-m_begin) : 0u;
            if (prefix)
                ::memmove_s(storage, prefix*sizeof(T), m_begin, prefix*sizeof(T));
            for (unsigned int index = 0; index<count; ++index)
                storage[prefix+index] = insertedValue;
            const unsigned int suffix = oldSize-prefix;
            if (suffix)
                ::memmove_s(storage+prefix+count, suffix*sizeof(T), position, suffix*sizeof(T));
            if (m_begin)
                CrtFreePointer(m_begin);
            m_capEnd = storage+capacity;
            m_end = storage+oldSize+count;
            m_begin = storage;
            return;
        }

        const unsigned int tailSize = m_begin ?
            static_cast<unsigned int>(m_end-position) : 0u;
        if (tailSize<count)
        {
            if (tailSize)
                ::memmove_s(position+count, tailSize*sizeof(T), position, tailSize*sizeof(T));
            const unsigned int fillCount = count-tailSize;
            for (unsigned int index = 0; index<fillCount; ++index)
                m_end[index] = insertedValue;
            T* const oldEnd = m_end;
            m_end+=count;
            for (T* destination = position; destination != oldEnd; ++destination)
                *destination = insertedValue;
        }
        else if (count>0)
        {
            T* const oldEnd = m_end;
            ::memmove_s(oldEnd, count*sizeof(T), oldEnd-count, count*sizeof(T));
            m_end+=count;
            const unsigned int remaining = tailSize-count;
            if (remaining)
                ::memmove_s(position+count, remaining*sizeof(T), position, remaining*sizeof(T));
            for (unsigned int index = 0; index<count; ++index)
                position[index] = insertedValue;
        }
    }

};

