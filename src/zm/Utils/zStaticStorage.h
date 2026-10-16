#pragma once

#include <type_traits>

template<class T>
struct StaticObjectStorage
{
    typename std::aligned_storage<sizeof(T), alignof(T)>::type memory;

    T* Address()
    {
        return reinterpret_cast<T*>(&memory);
    }

    T& Get()
    {
        return *Address();
    }
};
