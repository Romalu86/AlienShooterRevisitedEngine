#pragma once

#include "basetype.h"
#include "exports.h"
#include <cstring>
#include <stdexcept>

namespace VectorOperations
{
template<class T>
T* InsertOne(T*& begin, T*& end, T*& capacityEnd,
    T* position, const T& sourceValue)
{
    int index = 0;
    if (begin && begin != end)
    {
        if (begin > end)
            ::_invalid_parameter_noinfo();
        index = static_cast<int>(position-begin);
    }

    const T value = sourceValue;
    const unsigned int reserved = begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u;
    const unsigned int count = begin
        ? static_cast<unsigned int>(end-begin) : 0u;
    if (0x3fffffffu-count < 1u)
        throw std::length_error("vector<T> too long");
    if (reserved < count+1u)
    {
        const unsigned int half = reserved/2u;
        unsigned int newCapacity = reserved > 0x3fffffffu-half
            ? 0u : reserved+half;
        if (newCapacity < count+1u)
            newCapacity = count+1u;
        T* const storage = static_cast<T*>(CrtNew(newCapacity*sizeof(T)));
        const unsigned int prefix = position == begin ? 0u
            : static_cast<unsigned int>(position-begin);
        if (prefix)
            ::memmove_s(storage, prefix*sizeof(T), begin, prefix*sizeof(T));
        storage[prefix] = value;
        const unsigned int suffix = position == end ? 0u
            : static_cast<unsigned int>(end-position);
        if (suffix)
            ::memmove_s(storage+prefix+1u, suffix*sizeof(T), position, suffix*sizeof(T));
        if (begin)
            CrtFreePointer(begin);
        capacityEnd = storage+newCapacity;
        end = storage+count+1u;
        begin = storage;
    }
    else
    {
        T* const oldEnd = end;
        const unsigned int tail = static_cast<unsigned int>(oldEnd-position);
        if (tail < 1u)
        {
            *oldEnd = value;
            ++end;
        }
        else
        {
            *oldEnd = *(oldEnd-1);
            ++end;
            const int shift = static_cast<int>(tail-1u);
            if (shift > 0)
                ::memmove_s(position+1, shift*sizeof(T), position, shift*sizeof(T));
            *position = value;
        }
    }
    if (begin > end)
        ::_invalid_parameter_noinfo();
    T* const inserted = begin+index;
    if (inserted > end || inserted < begin)
        ::_invalid_parameter_noinfo();
    return inserted;
}

template<class T>
void Append(T*& begin, T*& end, T*& capacityEnd, const T& value)
{
    const unsigned int count = begin
        ? static_cast<unsigned int>(end-begin) : 0u;
    const unsigned int reserved = begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u;
    if (count < reserved)
    {
        *end++ = value;
        return;
    }
    if (begin > end)
        ::_invalid_parameter_noinfo();
    InsertOne(begin, end, capacityEnd, end, value);
}

template<class T>
void Clear(T*& begin, T*& end)
{
    T* const oldEnd = end;
    if (begin > oldEnd)
        ::_invalid_parameter_noinfo();
    if (begin > end)
        ::_invalid_parameter_noinfo();
    if (begin != oldEnd)
        end = begin;
}

template<class T, class Proxy>
void Clear(T*& begin, T*& end, Proxy*& proxy)
{
    T* const last = end;
    if (begin > last)
        ::_invalid_parameter_noinfo();
    T* const first = begin;
    if (first > end)
        ::_invalid_parameter_noinfo();
    if (begin > first || first > end)
        ::_invalid_parameter_noinfo();
    Proxy* const firstOwner = proxy;
    if (begin > last || last > end)
        ::_invalid_parameter_noinfo();
    if (!firstOwner || firstOwner != proxy)
        ::_invalid_parameter_noinfo();
    if (first != last)
    {
        const int remaining = static_cast<int>(end-last);
        if (remaining > 0)
            ::memmove_s(first, remaining*sizeof(T), last, remaining*sizeof(T));
        end = first+remaining;
    }
}

template<class T>
T* Copy(const T* first, const T* last, T* output)
{
    while (first != last)
    {
        if (output) *output = *first;
        ++first;
        ++output;
    }
    return output;
}

template<class T>
void Insert(T*& begin, T*& end, T*& capacityEnd,
    T* position, unsigned int count, const T& value)
{
    const unsigned int available = static_cast<unsigned int>(capacityEnd-end);
    if (available<count)
    {
        T* const oldBegin = begin;
        T* const oldEnd = end;
        const unsigned int oldSize = oldBegin
            ? static_cast<unsigned int>(oldEnd-oldBegin) : 0u;
        const unsigned int newCapacity = oldSize+(count<oldSize ? oldSize : count);
        const unsigned int allocationCount = BitsToSigned32(newCapacity)<0
            ? 0u : newCapacity;
        T* const storage = static_cast<T*>(CrtNew(allocationCount*sizeof(T)));
        T* output = Copy(oldBegin, position, storage);
        for (unsigned int remaining = count; remaining; --remaining, ++output)
            if (output) *output = value;
        Copy(position, oldEnd, output);
        CrtFreePointer(oldBegin);
        capacityEnd = storage+newCapacity;
        begin = storage;
        end = storage+oldSize+count;
        return;
    }

    const unsigned int tailCount = static_cast<unsigned int>(end-position);
    if (tailCount<count)
    {
        Copy(position, end, position+count);
        T* output = end;
        for (unsigned int remaining = count-tailCount; remaining; --remaining, ++output)
            if (output) *output = value;
        for (output = position; output != end; ++output)
            *output = value;
        end += count;
    }
    else if (count)
    {
        Copy(end-count, end, end);
        T* source = end-count;
        T* output = end;
        while (source != position)
            *--output = *--source;
        for (output = position; output != position+count; ++output)
            *output = value;
        end += count;
    }
}

template<class T, class Proxy>
void Assign(T*& begin, T*& end, T*& capacityEnd, Proxy*& proxy,
    T* const& sourceBegin, T* const& sourceEnd)
{
    const unsigned int count = sourceBegin
        ? static_cast<unsigned int>(sourceEnd-sourceBegin) : 0u;
    if (!count)
    {
        Clear(begin, end, proxy);
        return;
    }
    const unsigned int oldSize = begin
        ? static_cast<unsigned int>(end-begin) : 0u;
    if (count <= oldSize)
    {
        if (static_cast<int>(count) > 0)
            ::memmove_s(begin, count*sizeof(T), sourceBegin, count*sizeof(T));
        const unsigned int copied = sourceBegin
            ? static_cast<unsigned int>(sourceEnd-sourceBegin) : 0u;
        end = begin+copied;
    }
    else if (count <= (begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u))
    {
        T* const first = sourceBegin;
        T* const middle = first+oldSize;
        if (static_cast<int>(oldSize) > 0)
            ::memmove_s(begin, oldSize*sizeof(T), first, oldSize*sizeof(T));
        const unsigned int tail = static_cast<unsigned int>(sourceEnd-middle);
        T* const newEnd = end+tail;
        if (tail)
            ::memmove_s(end, tail*sizeof(T), middle, tail*sizeof(T));
        end = newEnd;
    }
    else
    {
        if (begin)
            CrtFreePointer(begin);
        begin = 0;
        end = 0;
        capacityEnd = 0;
        const unsigned int newCount = sourceBegin
            ? static_cast<unsigned int>(sourceEnd-sourceBegin) : 0u;
        if (newCount)
        {
            if (newCount > 0x3fffffffu)
                throw std::length_error("vector<T> too long");
            T* const storage = static_cast<T*>(CrtNew(newCount*sizeof(T)));
            begin = storage;
            end = storage;
            capacityEnd = storage+newCount;
            const unsigned int copied = sourceBegin
                ? static_cast<unsigned int>(sourceEnd-sourceBegin) : 0u;
            T* const newEnd = storage+copied;
            if (copied)
                ::memmove_s(storage, copied*sizeof(T), sourceBegin, copied*sizeof(T));
            end = newEnd;
        }
    }
}
}
