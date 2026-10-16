#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <limits>
#include "zm/Game/RecordDB/zRecordDB.h"
#include "myerror.h"
#include "mylib.h"
#include "map.h"
#include "basetype.h"
#include "exports.h"
#include "zm/Utils/zVectorOperations.h"

zRecordDB* RecordDatabase = nullptr;

AS_RECORD_TEXT::AS_RECORD_TEXT() noexcept : size(0u), capacity(15u)
{
    localText[0] = '\0';
}

AS_RECORD_TEXT::AS_RECORD_TEXT(const AS_RECORD_TEXT& other)
    : allocator(other.allocator), size(0u), capacity(15u)
{
    localText[0] = '\0';
    assign(other.c_str(), other.size);
}

AS_RECORD_TEXT::~AS_RECORD_TEXT() noexcept
{
    if (capacity >= 16u)
        allocator.deallocate(allocatedText, static_cast<size_t>(capacity) + 1u);
    capacity = 15u;
    size = 0u;
    localText[0] = '\0';
}

const char* AS_RECORD_TEXT::c_str() const noexcept
{
    return capacity >= 16u ? allocatedText : localText;
}

char* AS_RECORD_TEXT::Data() noexcept
{
    return capacity >= 16u ? allocatedText : localText;
}

AS_RECORD_TEXT& AS_RECORD_TEXT::assign(const char* text, unsigned int length)
{
    const char* const previous = c_str();
    const std::uintptr_t sourceAddress = reinterpret_cast<std::uintptr_t>(text);
    const std::uintptr_t previousAddress = reinterpret_cast<std::uintptr_t>(previous);
    if (text && sourceAddress >= previousAddress && sourceAddress < previousAddress + size)
    {
        const unsigned int offset = static_cast<unsigned int>(sourceAddress - previousAddress);
        const unsigned int remaining = size - offset;
        if (length > remaining)
            length = remaining;
        char* const destination = Data();
        const unsigned int suffix = remaining - length;
        if (suffix != 0u)
        {
            ::memmove_s(destination + offset + length, capacity - offset - length,
                destination + size, 0u);
            size -= suffix;
            destination[size] = '\0';
        }
        if (offset != 0u)
        {
            ::memmove_s(destination, capacity, destination + offset, size - offset);
            size -= offset;
            destination[size] = '\0';
        }
        return *this;
    }
    if (length > 0xFFFFFFFEu)
        throw std::length_error("string too long");
    if (capacity < length)
        Grow(length);
    char* destination = Data();
    if (length != 0u)
        ::memcpy_s(destination, capacity, text, length);
    size = length;
    destination[length] = '\0';
    return *this;
}

void AS_RECORD_TEXT::Grow(unsigned int length)
{
    unsigned int newCapacity = length | 15u;
    if (newCapacity > 0xFFFFFFFEu)
        newCapacity = length;
    else if (capacity / 2u > newCapacity / 3u)
    {
        const unsigned int halfCapacity = capacity / 2u;
        if (capacity <= 0xFFFFFFFEu - halfCapacity)
            newCapacity = capacity + halfCapacity;
    }

    char* replacement;
    try
    {
        replacement = allocator.allocate(static_cast<size_t>(newCapacity) + 1u);
    }
    catch (...)
    {
        newCapacity = length;
        try
        {
            replacement = allocator.allocate(static_cast<size_t>(newCapacity) + 1u);
        }
        catch (...)
        {
            if (capacity >= 16u)
                allocator.deallocate(allocatedText, static_cast<size_t>(capacity) + 1u);
            capacity = 15u;
            size = 0u;
            localText[0] = '\0';
            throw;
        }
    }
    if (size != 0u)
        ::memcpy_s(replacement, static_cast<size_t>(newCapacity) + 1u, c_str(), size);
    if (capacity >= 16u)
        allocator.deallocate(allocatedText, static_cast<size_t>(capacity) + 1u);
    allocatedText = replacement;
    capacity = newCapacity;
    replacement[size] = '\0';
}

AS_RECORD_TEXT& AS_RECORD_TEXT::append(const char* text, unsigned int length)
{
    const char* const previous = c_str();
    const std::uintptr_t sourceAddress = reinterpret_cast<std::uintptr_t>(text);
    const std::uintptr_t previousAddress = reinterpret_cast<std::uintptr_t>(previous);
    if (text && sourceAddress >= previousAddress && sourceAddress < previousAddress + size)
        return append(*this, static_cast<unsigned int>(sourceAddress - previousAddress), length);
    if (0xFFFFFFFFu - size <= length || size + length < size)
        throw std::length_error("string too long");
    if (length != 0u)
    {
        const unsigned int newSize = size + length;
        if (newSize > 0xFFFFFFFEu)
            throw std::length_error("string too long");
        if (capacity < newSize)
            Grow(newSize);
        char* const destination = Data();
        ::memcpy_s(destination + size, capacity - size, text, length);
        size = newSize;
        destination[newSize] = '\0';
    }
    return *this;
}

AS_RECORD_TEXT& AS_RECORD_TEXT::append(const AS_RECORD_TEXT& other,
    unsigned int offset, unsigned int length)
{
    if (offset > other.size)
        throw std::out_of_range("invalid string position");
    const unsigned int remaining = other.size - offset;
    if (length > remaining)
        length = remaining;
    if (0xFFFFFFFFu - size <= length || size + length < size)
        throw std::length_error("string too long");
    if (length != 0u)
    {
        const unsigned int newSize = size + length;
        if (newSize > 0xFFFFFFFEu)
            throw std::length_error("string too long");
        if (capacity < newSize)
            Grow(newSize);
        char* const destination = Data();
        ::memcpy_s(destination + size, capacity - size, other.c_str() + offset, length);
        size = newSize;
        destination[newSize] = '\0';
    }
    return *this;
}

AS_RECORD_TEXT& AS_RECORD_TEXT::append(char value)
{
    if (0xFFFFFFFFu - size <= 1u)
        throw std::length_error("string too long");
    const unsigned int newSize = size + 1u;
    if (newSize > 0xFFFFFFFEu)
        throw std::length_error("string too long");
    if (capacity < newSize)
        Grow(newSize);
    char* const destination = Data();
    destination[size] = value;
    size = newSize;
    destination[newSize] = '\0';
    return *this;
}

AS_RECORD_TEXT& AS_RECORD_TEXT::operator=(const AS_RECORD_TEXT& other)
{
    return assign(other.c_str(), other.size);
}

AS_RECORD_TEXT& AS_RECORD_TEXT::operator=(const char* text)
{
    return assign(text, static_cast<unsigned int>(std::strlen(text)));
}

AS_RECORD_TEXT::operator std::string() const
{
    return std::string(c_str(), size);
}

void* zRecordDB::QueryInterface(int interfaceId) const
{
    if (interfaceId == 57)
        return const_cast<zRecordDB*>(this);
    DebugAssert( 5u, "0", "..\\src\\zm\\Game\\RecordDB\\zRecordDB.cpp", 18u);
    return 0;
}

int zRecordDB::iRelease()
{
    if (this != nullptr)
        delete this;
    return 0;
}

void zRecordDB::operator delete(void* memory)
{
    CrtFreePointer(memory);
}

const int& AS_RECORD_DWORD_VECTOR::At(unsigned int index) const
{
    if (index >= static_cast<unsigned int>(end-begin))
        ::_invalid_parameter_noinfo();
    return begin[index];
}

AS_RECORD& AS_RECORD_VECTOR::At(unsigned int index) const
{
    if (index >= static_cast<unsigned int>(end-begin))
        ::_invalid_parameter_noinfo();
    return begin[index];
}

AS_RECORD*& AS_RECORD_PTR_VECTOR::At(unsigned int index)
{
    if (index >= static_cast<unsigned int>(end-begin))
        ::_invalid_parameter_noinfo();
    return begin[index];
}

AS_RECORD* const& AS_RECORD_PTR_VECTOR::At(unsigned int index) const
{
    if (index >= static_cast<unsigned int>(end-begin))
        ::_invalid_parameter_noinfo();
    return begin[index];
}

AS_RECORD_PTR_VECTOR::AS_RECORD_PTR_VECTOR()
{
    proxy = static_cast<zContainerProxy<AS_RECORD_PTR_VECTOR>*>(CrtNew(sizeof(*proxy)));
    if (proxy)
        proxy->m_owner = this;
    begin = end = capacityEnd = nullptr;
}

AS_RECORD_PTR_VECTOR::~AS_RECORD_PTR_VECTOR() noexcept(false)
{
    if (begin)
        CrtFreePointer(begin);
    begin = end = capacityEnd = nullptr;
    CrtFreePointer(proxy);
}

AS_RECORD_VECTOR::AS_RECORD_VECTOR()
{
    proxy = static_cast<zContainerProxy<AS_RECORD_VECTOR>*>(CrtNew(sizeof(*proxy)));
    if (proxy)
        proxy->m_owner = this;
    begin = end = capacityEnd = nullptr;
}

AS_RECORD_VECTOR::~AS_RECORD_VECTOR() noexcept(false)
{
    if (begin)
    {
        AS_RECORD* const last = end;
        for (AS_RECORD* current = begin; current != last; ++current)
            current->~AS_RECORD();
        CrtFreePointer(begin);
    }
    begin = end = capacityEnd = nullptr;
    CrtFreePointer(proxy);
}

bool zRecordDB::LoadIfNeed(int iType)
{
    if (iType < 0 || iType >= REC_TYPES_CNT)
        DebugAssert(5u, "iType >= 0 && iType <REC_TYPES_CNT", "..\\src\\zm\\Game\\RecordDB\\zRecordDB.cpp", 119u);
    if (m_DataFlags[iType])
        return true;
    m_DataFlags[iType] = 1;
    return LoadFromFile(iType);
}

zRecordDB::~zRecordDB() noexcept(false)
{
}

zRecordDB::zRecordDB()
{
    for (int tableIndex = 0; tableIndex < REC_TYPES_CNT; ++tableIndex)
    {
        m_DataFlags[tableIndex] = 0;
        m_OutFlags[tableIndex] = 1;
    }
}

int zRecordDB::GetRecCnt(int iType)
{
    (void)LoadIfNeed(iType);
    BuildDataIfNeed(iType);
    const AS_RECORD_VECTOR& data = m_Data[iType];
    const AS_RECORD_PTR_VECTOR& out = m_OutData[iType];
    const int dataCount = data.begin ? static_cast<int>(data.end-data.begin) : 0;
    const int outCount = out.begin ? static_cast<int>(out.end-out.begin) : 0;
    if (dataCount != outCount)
        DebugAssert(5u, "m_Data[iType].size() == m_OutData[iType].size()", "..\\src\\zm\\Game\\RecordDB\\zRecordDB.cpp", 91u);
    return static_cast<int>(out.end-out.begin);
}

const AS_RECORD* zRecordDB::GetRecData(int iType, int index)
{
    if (iType < 0 || iType >= REC_TYPES_CNT || index < 0)
        return 0;
    const AS_RECORD_PTR_VECTOR& out = m_OutData[iType];
    const int count = static_cast<int>(out.end-out.begin);
    return index<count ? out.At(static_cast<unsigned int>(index)) : 0;
}

AS_RECORD_DWORD_VECTOR::AS_RECORD_DWORD_VECTOR()
{
    proxy = static_cast<zContainerProxy<AS_RECORD_DWORD_VECTOR>*>(CrtNew(sizeof(*proxy)));
    if (proxy)
        proxy->m_owner = this;
    begin = end = capacityEnd = nullptr;
}

AS_RECORD_DWORD_VECTOR::~AS_RECORD_DWORD_VECTOR() noexcept(false)
{
    if (begin)
        CrtFreePointer(begin);
    begin = end = capacityEnd = nullptr;
    CrtFreePointer(proxy);
}

AS_RECORD_DWORD_VECTOR::AS_RECORD_DWORD_VECTOR(const AS_RECORD_DWORD_VECTOR& input)
{
    proxy = static_cast<zContainerProxy<AS_RECORD_DWORD_VECTOR>*>(CrtNew(sizeof(*proxy)));
    if (proxy)
        proxy->m_owner = this;
    begin = end = capacityEnd = nullptr;
    try
    {
        const unsigned int count = static_cast<unsigned int>(input.end-input.begin);
        if (count != 0u)
        {
            if (count > 0x3fffffffu)
                throw std::length_error("vector<T> too long");
            begin = static_cast<int*>(CrtNew(count*sizeof(int)));
            end = begin;
            capacityEnd = begin+count;
            const int* const last = input.end;
            if (input.begin > last) ::_invalid_parameter_noinfo();
            const int* const first = input.begin;
            if (first > input.end) ::_invalid_parameter_noinfo();
            const unsigned int copied = static_cast<unsigned int>(last-first);
            if (copied)
                ::memmove_s(begin, copied*sizeof(int), first, copied*sizeof(int));
            end = begin+copied;
        }
    }
    catch (...)
    {
        CrtFreePointer(proxy);
        throw;
    }
}

AS_RECORD_DWORD_VECTOR& AS_RECORD_DWORD_VECTOR::operator=(const AS_RECORD_DWORD_VECTOR& other)
{
    if (this != &other)
        VectorOperations::Assign(begin, end, capacityEnd, proxy, other.begin, other.end);
    return *this;
}

void AS_RECORD_DWORD_VECTOR::insert(int* position, unsigned int count, const int& value)
{
    if (count == 0u) return;
    const unsigned int oldSize = begin
        ? static_cast<unsigned int>(end - begin) : 0u;
    if (0x3FFFFFFFu - oldSize < count)
        throw std::length_error("vector<T> too long");
    const unsigned int reserved = begin
        ? static_cast<unsigned int>(capacityEnd - begin) : 0u;
    const int valueCopy = value;
    if (reserved >= oldSize + count)
    {
        VectorOperations::Insert(begin, end, capacityEnd, position, count, valueCopy);
        return;
    }

    const unsigned int half = reserved / 2u;
    unsigned int newCapacity = reserved > 0x3FFFFFFFu - half
        ? 0u : reserved + half;
    if (newCapacity < oldSize + count)
        newCapacity = oldSize + count;
    int* const storage = static_cast<int*>(CrtNew(newCapacity * sizeof(int)));
    int* output = VectorOperations::Copy(begin, position, storage);
    for (unsigned int i = 0u; i < count; ++i)
        *output++ = valueCopy;
    VectorOperations::Copy(position, end, output);
    CrtFreePointer(begin);
    begin = storage;
    end = storage + oldSize + count;
    capacityEnd = storage + newCapacity;
}


template<class T>
static void RecordDBInsertVectorValue(T*& begin, T*& end, T*& capacityEnd,
    T* position, const T& sourceValue)
{
    const unsigned int reserved = begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u;
    T* const previousEnd = end;
    const unsigned int count = begin
        ? static_cast<unsigned int>(previousEnd-begin) : 0u;
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
        T* const inserted = storage+prefix;
        if (prefix)
            ::memmove_s(storage, prefix*sizeof(T), begin, prefix*sizeof(T));
        *inserted = sourceValue;
        const unsigned int suffix = position == end ? 0u
            : static_cast<unsigned int>(end-position);
        if (suffix)
            ::memmove_s(inserted+1, suffix*sizeof(T), position, suffix*sizeof(T));
        const unsigned int newSize = begin
            ? static_cast<unsigned int>(end-begin)+1u : 1u;
        if (begin)
            CrtFreePointer(begin);
        capacityEnd = storage+newCapacity;
        end = storage+newSize;
        begin = storage;
        return;
    }

    const unsigned int tail = static_cast<unsigned int>(previousEnd-position);
    const T value = sourceValue;
    if (tail < 1u)
    {
        if (tail)
            ::memmove_s(position+1, tail*sizeof(T), position, tail*sizeof(T));
        T* const fillBegin = end;
        const unsigned int fillCount = 1u-static_cast<unsigned int>(end-position);
        for (unsigned int i = 0u; i < fillCount; ++i)
            fillBegin[i] = value;
        ++end;
        for (T* current = position; current != end-1; ++current)
            *current = value;
    }
    else
    {
        T* const previousLast = previousEnd-1;
        ::memmove_s(previousEnd, sizeof(T), previousLast, sizeof(T));
        end = previousEnd+1;
        const int shift = static_cast<int>(previousLast-position);
        if (shift > 0)
            ::memmove_s(position+1, shift*sizeof(T), position, shift*sizeof(T));
        *position = value;
    }
}

template<class T, class Proxy>
static T* RecordDBInsertVectorElement(T*& begin, T*& end, T*& capacityEnd,
    Proxy*& proxy, T* position, Proxy* inputOwner, const T& value)
{
    T* const previousEnd = end;
    T* const previousBegin = begin;
    int index = 0;
    if (previousBegin != previousEnd)
    {
        if (previousBegin > previousEnd)
            ::_invalid_parameter_noinfo();
        if (!inputOwner || inputOwner != proxy)
            ::_invalid_parameter_noinfo();
        index = static_cast<int>(position-previousBegin);
    }
    RecordDBInsertVectorValue(begin, end, capacityEnd, position, value);
    T* const currentBegin = begin;
    if (currentBegin > end)
        ::_invalid_parameter_noinfo();
    Proxy* const owner = proxy;
    if (!owner)
        ::_invalid_parameter_noinfo();
    T* const inserted = currentBegin+index;
    if (inserted > owner->m_owner->end || inserted < owner->m_owner->begin)
        ::_invalid_parameter_noinfo();
    return inserted;
}

template<class T, class Proxy>
static void RecordDBAppendVectorElement(T*& begin, T*& end, T*& capacityEnd,
    Proxy*& proxy, const T& value)
{
    const unsigned int reserved = begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u;
    T* const position = end;
    const unsigned int count = begin
        ? static_cast<unsigned int>(position-begin) : 0u;
    if (count < reserved)
    {
        *position = value;
        end = position+1;
        return;
    }
    if (begin > position)
        ::_invalid_parameter_noinfo();
    RecordDBInsertVectorElement(begin, end, capacityEnd, proxy, position, proxy, value);
}

int* AS_RECORD_DWORD_VECTOR::insert(int* position, const int& value)
{
    return RecordDBInsertVectorElement(begin, end, capacityEnd, proxy, position, proxy, value);
}

void*  RecordDBVectorRecordErase(void* self, void* first, void* last)
{
    AS_RECORD_VECTOR& storage = *static_cast<AS_RECORD_VECTOR*>(self);
    AS_RECORD* const firstRecord = static_cast<AS_RECORD*>(first);
    if (!self || storage.begin > firstRecord || firstRecord > storage.end)
        ::_invalid_parameter_noinfo();
    zContainerProxy<AS_RECORD_VECTOR>* const firstOwner = storage.proxy;
    AS_RECORD* const lastRecord = static_cast<AS_RECORD*>(last);
    if (storage.begin > lastRecord || lastRecord > storage.end)
        ::_invalid_parameter_noinfo();
    if (!firstOwner || firstOwner != storage.proxy)
        ::_invalid_parameter_noinfo();
    if (firstRecord != lastRecord)
    {
        AS_RECORD* destination = firstRecord;
        AS_RECORD* source = lastRecord;
        AS_RECORD* const oldEnd = storage.end;
        while (source != oldEnd)
        {
            *destination = *source;
            ++destination;
            ++source;
        }
        AS_RECORD* const tailEnd = storage.end;
        for (AS_RECORD* record = destination; record != tailEnd; ++record)
            record->~AS_RECORD();
        storage.end = destination;
    }
    return first;
}

void zRecordDB::ClearTable(int iType)
{
    (void)LoadIfNeed(iType);
    AS_RECORD_VECTOR& slot = m_Data[iType];
    AS_RECORD* const last = slot.end;
    if (slot.begin > last)
        ::_invalid_parameter_noinfo();
    AS_RECORD* const first = slot.begin;
    if (first > slot.end)
        ::_invalid_parameter_noinfo();
    (void)RecordDBVectorRecordErase(&slot, first, last);
    m_OutFlags[iType] = 1u;
    (void)SaveToFile(iType);
}

void zRecordDB::BuildDataIfNeed(int iType)
{
    const unsigned int index = static_cast<unsigned int>(iType);
    if (m_OutFlags[index] == 0u)
        return;
    m_OutFlags[index] = 0u;

    AS_RECORD_VECTOR& data = m_Data[index];
    AS_RECORD_PTR_VECTOR& out = m_OutData[index];
    VectorOperations::Clear(out.begin, out.end, out.proxy);
    const int count = static_cast<int>(data.end-data.begin);

    for (int recordIndex = 0;recordIndex<count;++recordIndex)
    {
        AS_RECORD* record = &data.At(static_cast<unsigned int>(recordIndex));
        RecordDBAppendVectorElement(out.begin, out.end, out.capacityEnd, out.proxy, record);
    }

    if (count <= 1)
        return;

    int heap = count-1;
    while (heap>0)
    {
        int best = heap;
        for (int recordIndex = 0;recordIndex<heap;++recordIndex)
        {
            const int score = out.At(static_cast<unsigned int>(recordIndex))->m_totalScore;
            const int bestScore = out.At(static_cast<unsigned int>(best))->m_totalScore;
            if (score<bestScore)
                best = recordIndex;
            else if (score == bestScore)
            {
                const int kills = out.At(static_cast<unsigned int>(recordIndex))->m_monsterKills;
                const int bestKills = out.At(static_cast<unsigned int>(best))->m_monsterKills;
                if (kills<bestKills)
                    best = recordIndex;
            }
        }
        AS_RECORD* swappedRecord = out.At(static_cast<unsigned int>(heap));
        AS_RECORD** const selected = &out.At(static_cast<unsigned int>(best));
        AS_RECORD*& last = out.At(static_cast<unsigned int>(heap));
        last = *selected;
        out.At(static_cast<unsigned int>(best)) = swappedRecord;
        --heap;
    }
}

AS_RECORD::AS_RECORD()
{
    m_cheater = 0;
    m_totalScore = 0;
    m_monsterKills = 0;
    m_elapsedSeconds = 0;
    m_checksum = 0;
}

AS_RECORD::~AS_RECORD() noexcept(false)
{
}

AS_RECORD::AS_RECORD(const AS_RECORD& other)
    : m_playerName(other.m_playerName),
      m_totalScore(other.m_totalScore), m_monsterKills(other.m_monsterKills),
      m_elapsedSeconds(other.m_elapsedSeconds), m_checksum(other.m_checksum),
      m_recordTimestamp(other.m_recordTimestamp),
      m_weaponScores(other.m_weaponScores), m_weaponKills(other.m_weaponKills),
      m_cheater(other.m_cheater)
{
}

static void RecordDBDestroyRecords(AS_RECORD* first, AS_RECORD* last)
{
    for (; first != last; ++first)
        first->~AS_RECORD();
}

static AS_RECORD* RecordDBCopyConstructRecords(const AS_RECORD* first,
    const AS_RECORD* last, AS_RECORD* destination)
{
    AS_RECORD* current = destination;
    try
    {
        for (; first != last; ++first, ++current)
            if (current)
                new (current) AS_RECORD(*first);
    }
    catch (...)
    {
        RecordDBDestroyRecords(destination, current);
        throw;
    }
    return current;
}

static AS_RECORD* RecordDBFillConstructRecords(AS_RECORD* destination,
    unsigned int count, const AS_RECORD& record)
{
    AS_RECORD* current = destination;
    try
    {
        for (; count != 0u; --count, ++current)
            if (current)
                new (current) AS_RECORD(record);
    }
    catch (...)
    {
        RecordDBDestroyRecords(destination, current);
        throw;
    }
    return current;
}

void AS_RECORD_VECTOR::push_back(const AS_RECORD& record)
{
    const unsigned int count = begin
        ? static_cast<unsigned int>(end-begin) : 0u;
    const unsigned int reserved = begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u;
    if (count < reserved)
    {
        AS_RECORD* const destination = end;
        RecordDBFillConstructRecords(destination, 1u, record);
        end = destination+1;
        return;
    }
    AS_RECORD* const position = end;
    if (begin > position)
        ::_invalid_parameter_noinfo();
    (void)insert(position, proxy, record);
}

AS_RECORD* AS_RECORD_VECTOR::insert(AS_RECORD* position,
    zContainerProxy<AS_RECORD_VECTOR>* inputOwner, const AS_RECORD& record)
{
    AS_RECORD* const previousEnd = end;
    AS_RECORD* const previousBegin = begin;
    unsigned int index = 0u;
    if (previousBegin != previousEnd)
    {
        if (previousBegin > previousEnd)
            ::_invalid_parameter_noinfo();
        if (!inputOwner || inputOwner != proxy)
            ::_invalid_parameter_noinfo();
        index = static_cast<unsigned int>(position-previousBegin);
    }
    InsertOne(position, record);
    AS_RECORD* const currentBegin = begin;
    if (currentBegin > end)
        ::_invalid_parameter_noinfo();
    zContainerProxy<AS_RECORD_VECTOR>* const owner = proxy;
    if (!owner)
        ::_invalid_parameter_noinfo();
    AS_RECORD* const inserted = currentBegin+index;
    if (inserted > owner->m_owner->end || inserted < owner->m_owner->begin)
        ::_invalid_parameter_noinfo();
    return inserted;
}

void AS_RECORD_VECTOR::InsertOne(AS_RECORD* position, const AS_RECORD& record)
{
    const unsigned int oldCapacity = begin
        ? static_cast<unsigned int>(capacityEnd-begin) : 0u;
    const unsigned int oldSize = begin
        ? static_cast<unsigned int>(end-begin) : 0u;
    const unsigned int maxCount = std::numeric_limits<unsigned int>::max()/sizeof(AS_RECORD);
    if (maxCount-oldSize < 1u)
        throw std::length_error("vector<T> too long");
    if (oldCapacity < oldSize+1u)
    {
        const unsigned int half = oldCapacity/2u;
        unsigned int newCapacity = oldCapacity > maxCount-half
            ? 0u : oldCapacity+half;
        if (newCapacity < oldSize+1u)
            newCapacity = oldSize+1u;
        AS_RECORD* const allocation = static_cast<AS_RECORD*>(CrtNew(newCapacity*sizeof(AS_RECORD)));
        AS_RECORD* completeEnd = allocation;
        try
        {
            completeEnd = RecordDBCopyConstructRecords(begin, position, allocation);
            completeEnd = RecordDBFillConstructRecords(completeEnd, 1u, record);
            completeEnd = RecordDBCopyConstructRecords(position, end, completeEnd);
            AS_RECORD* const previousEnd = end;
            AS_RECORD* const previousBegin = begin;
            const unsigned int newSize = previousBegin
                ? static_cast<unsigned int>(previousEnd-previousBegin)+1u : 1u;
            if (previousBegin)
            {
                RecordDBDestroyRecords(previousBegin, previousEnd);
                CrtFreePointer(begin);
            }
            capacityEnd = allocation+newCapacity;
            end = allocation+newSize;
            begin = allocation;
        }
        catch (...)
        {
            RecordDBDestroyRecords(allocation, completeEnd);
            CrtFreePointer(allocation);
            throw;
        }
        return;
    }

    const unsigned int tailCount = static_cast<unsigned int>(end-position);
    AS_RECORD temporary(record);
    if (tailCount < 1u)
    {
        AS_RECORD* const previousEnd = end;
        RecordDBCopyConstructRecords(position, previousEnd, position+1);
        try
        {
            RecordDBFillConstructRecords(end, 1u-static_cast<unsigned int>(end-position), temporary);
        }
        catch (...)
        {
            RecordDBDestroyRecords(position+1, end+1);
            throw;
        }
        ++end;
        for (AS_RECORD* current = position; current != end-1; ++current)
            *current = temporary;
    }
    else
    {
        AS_RECORD* const previousEnd = end;
        AS_RECORD* const previousLast = previousEnd-1;
        end = RecordDBCopyConstructRecords(previousLast, previousEnd, previousEnd);
        AS_RECORD* source = previousLast;
        AS_RECORD* destination = previousEnd;
        while (source != position)
            *--destination = *--source;
        *position = temporary;
    }
}

AS_RECORD& AS_RECORD::operator=(const AS_RECORD& other)
{
    m_playerName = other.m_playerName;
    m_totalScore = other.m_totalScore;
    m_monsterKills = other.m_monsterKills;
    m_elapsedSeconds = other.m_elapsedSeconds;
    m_checksum = other.m_checksum;
    m_recordTimestamp = other.m_recordTimestamp;
    m_weaponScores = other.m_weaponScores;
    m_weaponKills = other.m_weaponKills;
    m_cheater = other.m_cheater;
    return *this;
}

void zRecordDB::AddRecord(int iType, const AS_RECORD& source)
{
    m_DataFlags[iType] = 0;
    (void)LoadIfNeed(iType);

    AS_RECORD record(source);
    {
        STRING timeText = GetTimeString();
        STRING dateText = GetDateString();
        STRING withDash = dateText+" - ";
        STRING timestamp = withDash+timeText;
        const char* const stamp = timestamp.value;
        const unsigned int stampLength = CStringLength(stamp);
        record.m_recordTimestamp.assign(stamp, stampLength);
    }

    record.m_checksum = static_cast<unsigned int>(record.GetHash(2));
    AS_RECORD_VECTOR& slot = m_Data[iType];
    slot.push_back(record);
    m_OutFlags[iType] = 1;
    (void)SaveToFile(iType);

}

static void RecordDBBuildHallOfFamePath(AS_RECORD_TEXT& output, int iType)
{
    {
        STRING basePath = Map->WindowsUserPath();
        const char* const base = basePath.value;
        output.assign(base, CStringLength(base));
    }

    const char* suffix = 0;
    if (iType == 0)
        suffix = "Saves\\HallOfFame1.txt";
    else if (iType == 1)
        suffix = "Saves\\HallOfFame2.txt";
    else if (iType == 2)
        suffix = "Saves\\HallOfFame3.txt";
    else if (iType == 3)
        suffix = "Saves\\HallOfFame4.txt";
    else
        DebugAssert(5u, "0", "..\\src\\zm\\Game\\RecordDB\\zRecordDB.cpp", 59u);

    if (suffix)
        output.append(suffix, 21u);
}

int AS_RECORD::GetHash(int mode)
{
    AS_RECORD_TEXT serialized(m_playerName);
    serialized.append(' ');
    serialized.append(m_recordTimestamp, 0u, 0xFFFFFFFFu);

    const int* const scalarValues[3] = {&m_totalScore, &m_monsterKills, &m_elapsedSeconds};
    for (unsigned int scalar = 0;scalar<3u;++scalar)
    {
        serialized.append(' ');
        STRING number = Int2Str(*scalarValues[scalar]);
        const char* const text = number.value;
        serialized.append(text, CStringLength(text));
    }

    const AS_RECORD_DWORD_VECTOR* vectors[2] = {&m_weaponScores, &m_weaponKills};
    for (unsigned int vectorIndex = 0;vectorIndex<2u;++vectorIndex)
    {
        const AS_RECORD_DWORD_VECTOR& values = *vectors[vectorIndex];
        for (int i = 0;i<(values.begin ? static_cast<int>(values.end-values.begin) : 0);++i)
        {
            serialized.append(' ');
            STRING number = Int2Str(values.At(static_cast<unsigned int>(i)));
            const char* const text = number.value;
            serialized.append(text, CStringLength(text));
        }
    }

    const char* const data = serialized.c_str();
    const unsigned int length = serialized.size;
    unsigned int result = 0u;
    if (mode == 2)
        result = static_cast<unsigned int>(zutl::GetHashNb(data));
    else if (mode == 1)
    {
        CRC32 crc;
        crc.value = 0u;
        result = crc.Add(data, length);
    }
    else if (mode == 0)
        result = zutl::GetHashPJW(data);
    else
        DebugAssert(5u, "0", "..\\src\\zm\\Game\\RecordDB\\zRecordDB.cpp", 325u);

    return static_cast<int>(result);
}

bool zRecordDB::SaveToFile(int iType)
{
    {
        AS_RECORD_TEXT path;
        RecordDBBuildHallOfFamePath(path, iType);
        zutl::MoveToTrash(path.c_str());
    }

    std::FILE* output;
    {
        AS_RECORD_TEXT path;
        RecordDBBuildHallOfFamePath(path, iType);
        output = std::fopen(path.c_str(), "w");
    }
    if (!output)
        return false;

    (void)std::fprintf(output, "%s\n", "FileVersion");
    (void)std::fprintf(output, "%d\n", 105);

    const AS_RECORD_VECTOR& records = m_Data[iType];
    for (unsigned int recordIndex = 0;records.begin &&
        recordIndex<static_cast<unsigned int>(records.end-records.begin);++recordIndex)
    {
        const AS_RECORD& record = records.At(recordIndex);
        (void)std::fprintf(output, "%s\n", record.m_playerName.c_str());
        (void)std::fprintf(output, "%s\n", record.m_recordTimestamp.c_str());
        (void)std::fprintf(output, "%d\n", record.m_totalScore);
        (void)std::fprintf(output, "%d\n", record.m_monsterKills);
        (void)std::fprintf(output, "%d\n", record.m_elapsedSeconds);
        (void)std::fprintf(output, "%d\n", static_cast<int>(record.m_checksum));
        (void)std::fprintf(output, "%d\n", static_cast<int>(record.m_cheater));

        const AS_RECORD_DWORD_VECTOR* const vectors[2] = {&record.m_weaponScores, &record.m_weaponKills};
        for (unsigned int vectorIndex = 0;vectorIndex<2u;++vectorIndex)
        {
            const AS_RECORD_DWORD_VECTOR& values = *vectors[vectorIndex];
            const int count = values.begin ? static_cast<int>(values.end - values.begin) : 0;
            (void)std::fprintf(output, "%d\n", count);
            for (int element = 0; element < (values.begin ? static_cast<int>(values.end - values.begin) : 0); ++element)
                (void)std::fprintf(output, "%d\n", values.At(static_cast<unsigned int>(element)));
        }
    }

    (void)std::fclose(output);
    return true;
}

static bool RecordDBReadLine(std::FILE* input, AS_RECORD_TEXT& line)
{
    static char buffer[2048];
    if (!std::fgets(buffer, sizeof(buffer), input))
        return false;

    unsigned int length = static_cast<unsigned int>(std::strlen(buffer));
    while (length != 0u && (buffer[length - 1] == '\r' || buffer[length - 1] == '\n'))
        buffer[--length] = '\0';
    line.assign(buffer, length);
    return true;
}

bool zRecordDB::LoadFromFile(int iType)
{
    std::FILE* input;
    {
        AS_RECORD_TEXT path;
        RecordDBBuildHallOfFamePath(path, iType);
        input = std::fopen(path.c_str(), "rt");
    }
    if (!input)
        return false;

    AS_RECORD_VECTOR& records = m_Data[iType];
    AS_RECORD* const last = records.end;
    if (records.begin > last)
        ::_invalid_parameter_noinfo();
    AS_RECORD* const first = records.begin;
    if (first > records.end)
        ::_invalid_parameter_noinfo();
    (void)RecordDBVectorRecordErase(&records, first, last);
    AS_RECORD_TEXT line;
    AS_RECORD record;
    const auto readLine = [&input, &line]() -> bool
    {
        return RecordDBReadLine(input, line);
    };

    (void)readLine();
    int version = 100;
    if (std::strcmp(line.c_str(), "FileVersion") == 0)
    {
        if (!readLine())
            return false;
        version = BitsToSigned32(CrtAtoiText(line.c_str()));
        if (!readLine())
            return false;
    }

    while (!std::feof(input))
    {
        record.m_playerName.assign(line.c_str(), line.size);
        if (!readLine())
            return false;
        record.m_recordTimestamp.assign(line.c_str(), line.size);

        int* const scalarFields[3] = {&record.m_totalScore, &record.m_monsterKills, &record.m_elapsedSeconds};
        for (unsigned int scalar = 0;scalar<3u;++scalar)
        {
            if (!readLine())
                return false;
            *scalarFields[scalar] = BitsToSigned32(CrtAtoiText(line.c_str()));
        }
        if (!readLine())
            return false;
        record.m_checksum = CrtAtoiText(line.c_str());

        if (version >= 105)
        {
            if (!readLine())
                return false;
            record.m_cheater = CrtAtoiText(line.c_str()) != 0u;
        }

        const unsigned int storedChecksum = record.m_checksum;
        AS_RECORD_DWORD_VECTOR* const vectors[2] = {&record.m_weaponScores, &record.m_weaponKills};
        for (int vectorIndex = 0;vectorIndex<2;++vectorIndex)
        {
            if (version<101+vectorIndex)
                continue;
            AS_RECORD_DWORD_VECTOR& values = *vectors[vectorIndex];
            VectorOperations::Clear(values.begin, values.end, values.proxy);
            if (!readLine())
                return false;
            const int count = BitsToSigned32(CrtAtoiText(line.c_str()));
            for (int element = 0;element<count;++element)
            {
                if (!readLine())
                    return false;
                const int value = BitsToSigned32(CrtAtoiText(line.c_str()));
                RecordDBAppendVectorElement(values.begin, values.end, values.capacityEnd, values.proxy, value);
            }
        }

        bool hashMatches = storedChecksum == static_cast<unsigned int>(-666);
        if (!hashMatches)
            hashMatches = storedChecksum == static_cast<unsigned int>(record.GetHash(2));
        if (!hashMatches && version<103)
        {
            hashMatches = storedChecksum == static_cast<unsigned int>(record.GetHash(0));
            if (hashMatches)
                record.m_checksum = static_cast<unsigned int>(record.GetHash(2));
        }
        if (!hashMatches && version<104)
        {
            hashMatches = storedChecksum == static_cast<unsigned int>(record.GetHash(1));
            if (hashMatches)
                record.m_checksum = static_cast<unsigned int>(record.GetHash(2));
        }
        if (hashMatches)
            records.push_back(record);
        if (!readLine())
            break;
    }
    (void)std::fclose(input);
    return true;
}
