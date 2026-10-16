#include "myerror.h"
#include <cstdio>
#pragma once
#include "basetype.h"
#include <stddef.h>
#include <float.h>
#include <cstring>

class MYERROR;
extern MYERROR* Error;
class STREAM;

struct HKEY__;
struct HWND__;
struct _FILETIME;

class STRING {
public:
    STRING() noexcept : value(StringGlobals::EmptySentinel())
{
}
    STRING(const char* source);
    STRING(const STRING& source);
    ~STRING();

    union {
        char* m_value;
        char* value;
    };

    const STRING& operator=(const STRING& source);
    const STRING& operator=(const char* source);
    const STRING& operator+=(const STRING& suffix);
    const STRING& operator+=(const char* suffix);
    const STRING operator+(const STRING& right) const;
    const STRING operator+(const char* right) const;
    void Read(std::FILE* file);
    void Read(STREAM* stream);
    int LoadFile(const STRING& fileName);
    void ToWideChar(unsigned short* output,int outputChars) const;
    const STRING SplitRegPath(HKEY__** root) const;
    int Replace(const char* oldText,const char* newText);
    void RemoveEndChars(const char* chars);
    void RemoveBeginChars(const char* chars);
    int CountChar(int needle) const;
    int Length() const;
    const char* CharPtr() const;
    const STRING Before(const char* delimiter) const;
    const STRING After(const char* delimiter) const;
    const STRING BeforeLast(const char* delimiter) const;
    const STRING BeforeLast(const STRING& delimiter) const;
    STRING ToLower() const;
    STRING ToUpper() const;
    const STRING AfterLast(const char* delimiter) const;
    const STRING Add(int delta) const;
    const STRING SubString(unsigned int begin, unsigned int end) const;

private:
    static char EMPTY[1];
    friend char* StringGlobals::EmptySentinel();
public:
    STRING ToBase64(int addCode) const;

int IsEmpty() const;
    int Int() const;
    void Write(STREAM* stream) const;
    void Write(std::FILE* file) const;
    const STRING Before(const STRING& delimiter) const;
    const STRING After(const STRING& delimiter) const;
    char operator[](int index) const;

private:
    friend const STRING operator+(const char* left,const STRING& right);
    STRING(const char* left,const char* right);

};

class PROFILE {
public:
    STRING fileName;

    int Load(const STRING& fileName);
    int GetInt(const STRING& section,const STRING& key,int defaultValue);
    STRING GetString(const STRING& section,const STRING& key,const STRING& defaultValue);
};

class CRC32 {
public:
    unsigned int value;
    CRC32()
    {
    }
    CRC32(const void* data,unsigned int size);
    unsigned int Add(const void* data,unsigned int size);
    void AddFile(STRING fileName);
    int CheckListFromFile(STRING fileName);
};

const STRING Printf(const char* format,...);
const STRING GetDateString();
const STRING GetTimeString();
STRING FFirstFile(int* findHandle,const STRING& fileMask,_FILETIME* lastWriteTime);
STRING FNextFile(int* findHandle,_FILETIME* lastWriteTime);
int FFileLength(const STRING& fileName);
void* FFileMapping(const STRING& fileName);
int FGetFileTime(STRING fileName,_FILETIME* creationTime,_FILETIME* accessTime,_FILETIME* writeTime);
int FSetFileTime(STRING fileName,_FILETIME* creationTime,_FILETIME* accessTime,_FILETIME* writeTime);
const STRING FCurrentDirectory();
int Random(int maximum);
float Random(float scale);
int Random2(int maximum);
float Random2(float scale);
std::FILE* FOpen(const STRING& fileName,const STRING& mode);
STRING FGets(std::FILE* file);
int Sqrt(int value);


#include <stddef.h>

char* StringEmptySentinel();

const STRING operator+(const char* left,const STRING& right);
const STRING Int2Str(int value);

void DestroyTextureStageDescription();


class STRING;


namespace zutl {
    bool FileExists(const char* fileName);
    void MoveToTrash(const char* fileName);
    const char* GetTimeStr();
    const char* GetTimeStrFull();
    int GetCurTime();
    unsigned int GetHashPJW(const char* text);
    int GetHashNb(const char* text);
}


static inline bool CStringEqualUnchecked(const char* a, const char* b)
{
    while (*a == *b)
    {
        if (*a == '\0') return true;
        ++a;
        ++b;
    }
    return false;
}


#include <math.h>
static inline int TruncateDoubleToInt32(double value);
static inline int TruncateDoubleViaCrtToInt32(double value);
static inline int TruncatedPlanarDistance(float x, float y)
{
    const double dx = static_cast<double>(x);
    const double dy = static_cast<double>(y);
    const float squaredDistance = static_cast<float>(dx*dx+dy*dy);
    const float distance = static_cast<float>(sqrt(static_cast<double>(squaredDistance)));
    return TruncateDoubleViaCrtToInt32(static_cast<double>(distance));
}

bool CrtSse2ConversionEnabled();
void SetCrtSse2ConversionEnabled(bool enabled);

#if defined(_MSC_VER) && defined(_M_IX86)
#include <emmintrin.h>
#pragma fenv_access(on)
#endif
static inline int TruncateDoubleToInt32(double value)
{
    volatile double number = value;
    if (!(number >= -9223372036854775808.0 && number < 9223372036854775808.0))
        return 0;
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned int originalControl = 0u;
    unsigned int conversionControl = 0u;
    (void)__control87_2(0u, 0u, &originalControl, 0);
    (void)__control87_2(_RC_CHOP, _MCW_RC, &conversionControl, 0);
#endif
    const long long converted = static_cast<long long>(number);
#if defined(_MSC_VER) && defined(_M_IX86)
    (void)__control87_2(originalControl, _MCW_RC, &conversionControl, 0);
#endif
    return BitsToSigned32(static_cast<unsigned int>(converted));
}
static inline int TruncateDoubleViaCrtToInt32(double value)
{
    if (CrtSse2ConversionEnabled())
    {
        const volatile double number = value;
#if defined(_MSC_VER) && defined(_M_IX86)
        return _mm_cvttsd_si32(_mm_set_sd(number));
#else
        if (number >= -2147483648.0 && number < 2147483648.0)
            return static_cast<int>(number);
        return BitsToSigned32(0x80000000u);
#endif
    }
    return TruncateDoubleToInt32(value);
}

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma fenv_access(off)
#endif

static inline int TruncateFloatViaCrtToInt32(float value)
{
    return TruncateDoubleViaCrtToInt32(static_cast<double>(value));
}

static inline int TruncateFloatToInt32(float value)
{
    return TruncateDoubleToInt32(static_cast<double>(value));
}

void ReportLiveSpriteReference(void* object);

static inline void* AdjustThisPointer(void* self, int delta)
{
    return static_cast<unsigned char*>(self)+delta;
}

typedef unsigned long long CrtUInt64;

static inline double CartesianHeadingRadians(float a, float b)
{
    const float angle = static_cast<float>(atan2(static_cast<double>(a), -static_cast<double>(b)));
    return static_cast<double>(angle);
}

#include <math.h>

static inline unsigned char SaturatingAddByte(unsigned char a, unsigned char b)
{
    unsigned int sum = static_cast<unsigned int>(a) + static_cast<unsigned int>(b);
    return static_cast<unsigned char>(sum > 255u ? 255u : sum);
}


#if defined(_MSC_VER) && defined(_M_IX86)
#define CRT_FLOAT_INLINE __forceinline
#else
#define CRT_FLOAT_INLINE inline
#endif

static CRT_FLOAT_INLINE unsigned int FloatingPointBits(const float& value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    return __builtin_bit_cast(unsigned int, value);
#else
    unsigned int bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
#endif
}

static CRT_FLOAT_INLINE unsigned long long FloatingPointBits(const double& value)
{
#if defined(_MSC_VER) && defined(_M_IX86)
    return __builtin_bit_cast(unsigned long long, value);
#else
    unsigned long long bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
#endif
}

static CRT_FLOAT_INLINE unsigned long long FloatingPointComparisonBits(unsigned int bits)
{
    const unsigned long long sign = static_cast<unsigned long long>(bits & 0x80000000u) << 32;
    unsigned int exponent = (bits >> 23) & 0xFFu;
    unsigned int fraction = bits & 0x007FFFFFu;
    if (exponent == 0u) {
        if (fraction == 0u)
            return sign;
        exponent = 897u;
        while ((fraction & 0x00800000u) == 0u) {
            fraction <<= 1;
            --exponent;
        }
    } else if (exponent == 0xFFu) {
        exponent = 0x7FFu;
    } else {
        exponent += 896u;
    }
    return sign | (static_cast<unsigned long long>(exponent) << 52) |
        (static_cast<unsigned long long>(fraction & 0x007FFFFFu) << 29);
}

static CRT_FLOAT_INLINE unsigned long long FloatingPointComparisonBits(unsigned long long bits)
{
    return bits;
}

static inline bool FloatIsNaN(const float& value)
{
    return (FloatingPointBits(value) & 0x7FFFFFFFu) > 0x7F800000u;
}

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma float_control(precise, off, push)
#endif

template<class Left, class Right>
static CRT_FLOAT_INLINE bool FloatEqualOrUnordered(const Left& left, const Right& right)
{
    const auto leftBits = FloatingPointBits(left);
    const auto rightBits = FloatingPointBits(right);
    const unsigned long long leftComparison = FloatingPointComparisonBits(leftBits);
    const unsigned long long rightComparison = FloatingPointComparisonBits(rightBits);
    const unsigned long long leftMagnitude = leftComparison & 0x7FFFFFFFFFFFFFFFull;
    const unsigned long long rightMagnitude = rightComparison & 0x7FFFFFFFFFFFFFFFull;
    const bool sameValue = leftComparison == rightComparison || (leftMagnitude | rightMagnitude) == 0u;
    const bool unordered = leftMagnitude > 0x7FF0000000000000ull || rightMagnitude > 0x7FF0000000000000ull;
#if defined(_MSC_VER) && defined(_M_IX86)
    const Left leftCopy = __builtin_bit_cast(Left, leftBits);
    const Right rightCopy = __builtin_bit_cast(Right, rightBits);
#else
    Left leftCopy;
    Right rightCopy;
    std::memcpy(&leftCopy, &leftBits, sizeof(leftCopy));
    std::memcpy(&rightCopy, &rightBits, sizeof(rightCopy));
#endif
    const volatile bool orderedGreater = rightCopy < leftCopy;
    return !orderedGreater && (sameValue || unordered);
}

template<class Left, class Right>
static CRT_FLOAT_INLINE bool FloatLessOrUnordered(const Left& left, const Right& right)
{
    return !(right <= left);
}

template<class Left, class Right>
static CRT_FLOAT_INLINE bool FloatGreaterOrdered(const Left& left, const Right& right)
{
    return right < left;
}

#if defined(_MSC_VER) && defined(_M_IX86)
#pragma float_control(pop)
#endif

#undef CRT_FLOAT_INLINE

static inline unsigned int LowMul100000(unsigned int value)
{
    return value*100000u;
}


static inline bool StrEq(const char* left, const char* right)
{
    if (!left || !right)return left == right;
    while (*left == *right)
    {
        if (!*left)return true;
        ++left;
        ++right;
    }
    return false;
}


static inline int CompareCStringsUnchecked(const char* left, const char* right)
{
    if (!left || !right)return left == right?0:(left?1:-1);
    while (*left && *left == *right)
    {
        ++left;
        ++right;
    }
    return (unsigned char)*left-(unsigned char)*right;
}

static inline void ReportAllocationFailure(unsigned int requestedBytes)
{
    const STRING format("!!!ERROR!!!::LIST: Not enough memory %i");
    ::Error->LogExit(format.m_value, requestedBytes);
}
static inline unsigned int ScaleSignedFixedPoint8(int value, int previousCount, int targetCount)
{
    if (previousCount <= 1)previousCount = 1;
    const int shiftedValue = BitsToSigned32(Signed32ToBits(value)<<8u);
    const int scaledValue = shiftedValue/previousCount;
    int scaledProduct = MultiplySigned32Wrap(scaledValue, targetCount);
    if (scaledProduct<0)scaledProduct = AddSigned32Wrap(scaledProduct, 255);
    return static_cast<unsigned int>(scaledProduct>>8);
}

#include <math.h>

#include <stddef.h>

static inline int CompareCStrings(const char* left, const char* right)
{
    if (!left || !right)return left == right;
    while (*left && *left == *right)
    {
        ++left;
        ++right;
    }
    return static_cast<unsigned char>(*left)-static_cast<unsigned char>(*right);
}


static inline int SignedHalf(int value)
{
    return (value-(value>>31))>>1;
}


unsigned int  ResourceObjectIdInTable(const void* self,const void* object);
unsigned int  ResourceReplaceReferenceBC(void* self,void* value);
