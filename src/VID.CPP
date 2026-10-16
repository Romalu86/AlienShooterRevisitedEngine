#if defined(_WIN32)
#define INPUT WIN32_INPUT
#include <windows.h>
#include "engine_globals.h"
#include <d3d9.h>
#undef INPUT
#endif
#include "myerror.h"
#include "mylib.h"
#include "vid.h"

unsigned int VID_SURFACE::noObj = 0;
unsigned int VID_SURFACE::noVertex = 0;
unsigned int VID_SURFACE::noTri = 0;
unsigned int VID_SURFACE::noDrawReal[16] = {};
unsigned int VID_SURFACE::noDrawCall[16] = {};
unsigned int VID_SURFACE::noDrawShadow = 0;
#include "list.h"
#include "texture.h"
#include "basetype.h"
#include "region.h"
#include "message.h"
#include "sprite.h"
#include "map.h"
#include "resread.h"
#include "sprite_collector.h"
#include "graph.h"
#include "system_font.h"
#include "legacy_d3dx_effect.h"
#include "constant.h"
#include "sound.h"
#include "net.h"
#include <new>
#include <cstdlib>
#include <intrin.h>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>


VECTOR2 VID::viewMin;
VECTOR2 VID::viewMax;
int VID::MemoryInUse = 0;
unsigned int VID::useTextureMemory = 0;
unsigned int VID::useVertexMemory = 0;
int VID::optDrawFlagmanBehindWalls = 0;
int VID::optBlackWhite = 0;
CADR_DATA* VID::lastUse = 0;


namespace
{
enum VidHardwareRuntimeFlags
{
    kVidRuntimeHidden       = 0x00000800u,
    kVidAnimateScale        = 0x00000002u,
    kVidAnimateOffset       = 0x00000004u
};
enum VidOwnerDrawFlags
{
    kVidApplyPixelCenterOffset = 0x00008000u,
    kVidApplyDirectionOffset   = 0x00010000u
};
}

int VID::GetMaxAmmo() const
{
    if (linkVid && linkVid->nWeapon && linkVid->childVid[8])
        return linkVid->weapon->MaxAmmo;
    return weapon->MaxAmmo;
}





const GAMMA& GAMMA::operator = (const GAMMA& source)
{
    modulate = source.modulate;
    add = source.add;
    return *this;
}

void __cdecl VidStaticRangeAInit()
{
    VID::viewMax.x = 0.0f;
    VID::viewMax.y = 1024.0f;
}

void __cdecl VidStaticRangeBInit()
{
    VID::viewMin.x = 0.0f;
    VID::viewMin.y = 1024.0f;
}


unsigned int __cdecl Vid_StaticCleanupRegister01()
{
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}


void VID::SetPropHideWithoutLink(int hide)
{
    runtimeFlags = (runtimeFlags&~0x00000800u)|(hide?0x00000800u:0u);
}


VECTOR VECTOR::operator-(const VECTOR& value) const
{
    return VECTOR(x-value.x, y-value.y, z-value.z);
}


VECTOR VECTOR::operator*(float scale) const
{
    return VECTOR(scale*x, scale*y, scale*z);
}


void VID::SetPropHide(int hide)
{
    const unsigned int hideBit = hide?0x00000800u:0u;
    for (VID* current = this;current;current = current->linkVid)
        current->runtimeFlags = (current->runtimeFlags&~0x00000800u)|hideBit;
}


static unsigned int VidArrayAllocationBytes(unsigned int count, unsigned int itemBytes)
{
    return count>0xFFFFFFFFu/itemBytes ? 0xFFFFFFFFu : count*itemBytes;
}

static int ScaleHpRatioFpu(int currentHp, int newMaxHp, int oldMaxHp)
{

    const double product = static_cast<double>(currentHp)*static_cast<double>(newMaxHp);
    return TruncateDoubleViaCrtToInt32(product/static_cast<double>(oldMaxHp));
}

static int TruncateFixedPointTowardZero(int value, unsigned int fractionalBits)
{
    if (value >= 0) return value>>fractionalBits;
    return -static_cast<int>((0u-static_cast<unsigned int>(value))>>fractionalBits);
}

static int ScaleHpRatioFixedShift4(int currentHp, int newMaxHp, int oldMaxHp)
{
    unsigned int productBits = static_cast<unsigned int>(currentHp)*static_cast<unsigned int>(newMaxHp);
    productBits <<= 4;
    const int quotient = BitsToSigned32(productBits)/oldMaxHp;
    return TruncateFixedPointTowardZero(quotient, 4u);
}

static int ScaleHpRatioFixedShift8(int currentHp, int newMaxHp, int oldMaxHp)
{
    unsigned int productBits = static_cast<unsigned int>(currentHp)*static_cast<unsigned int>(newMaxHp);
    productBits <<= 8;
    const int quotient = BitsToSigned32(productBits)/oldMaxHp;
    return TruncateFixedPointTowardZero(quotient, 8u);
}

static SPRITE* FirstAllSpriteForVid()
{
    SPRITE_COLLECTOR* collector = ::Sprites;
    return static_cast<SPRITE*>(collector->FirstAllSprite());
}

static SPRITE* NextAllSpriteForVid()
{
    SPRITE_COLLECTOR* collector = ::Sprites;
    return static_cast<SPRITE*>(collector->NextAllSprite());
}

void VID::SetHpCoeff(unsigned int direction, int hpCoeff)
{
    const unsigned int dir = static_cast<unsigned int>(direction)&7u;
    VID* vid = this;
    while (vid)
    {
        int oldMaxHp = vid->maxHpByPlayer[dir];
        if (oldMaxHp <= 1) oldMaxHp = 1;

        if (hpCoeff >= 0)
        {

            const int baseHp = vid->maxHp;
            int newMaxHp = BitsToSigned32(static_cast<unsigned int>(baseHp)*static_cast<unsigned int>(hpCoeff));
            newMaxHp /= 100;
            if (newMaxHp <= 1) newMaxHp = 1;
            vid->maxHpByPlayer[dir] = newMaxHp;
        }

        if (vid->maxHp != 0)
        {
            SPRITE* sprite = FirstAllSpriteForVid();
            while (sprite)
            {
                if (sprite->Vid() == vid && ((sprite->RuntimeFlags()>>SPRITE::ArmyShift)&7u) == dir)
                {
                    const int newMaxHp = vid->maxHpByPlayer[dir];
                    const int currentHp = sprite->m_hp;
                    int scaledHp;
                    if (newMaxHp>5000)
                        scaledHp = ScaleHpRatioFpu(currentHp, newMaxHp, oldMaxHp);
                    else
                        scaledHp = ScaleHpRatioFixedShift4(currentHp, newMaxHp, oldMaxHp);
                    if (scaledHp <= 1) scaledHp = 1;
                    sprite->ChangeHp(scaledHp);
                }
                sprite = NextAllSpriteForVid();
            }
        }

        VID* const linkedVid = vid->linkVid;
        if (linkedVid == 0 || linkedVid->maxHp == 0)
            break;
        vid = linkedVid;
    }
}

void VID::SetMaxHp(unsigned int direction, int hp)
{
    if (hp < 0)
        return;
    const unsigned int dir = static_cast<unsigned int>(direction)&7u;
    VID* vid = this;
    while (vid)
    {
        int oldMaxHp = vid->maxHpByPlayer[dir];
        if (oldMaxHp <= 1) oldMaxHp = 1;
        vid->maxHpByPlayer[dir] = hp;

        if (vid->maxHp != 0)
        {
            SPRITE* sprite = FirstAllSpriteForVid();
            while (sprite)
            {
                if (sprite->Vid() == vid && ((sprite->RuntimeFlags()>>SPRITE::ArmyShift)&7u) == dir)
                {
                    const int newMaxHp = vid->maxHpByPlayer[dir];
                    const int currentHp = sprite->m_hp;
                    int scaledHp = (newMaxHp>5000)
                        ? ScaleHpRatioFpu(currentHp, newMaxHp, oldMaxHp)
                        : ScaleHpRatioFixedShift8(currentHp, newMaxHp, oldMaxHp);
                    if (scaledHp <= 1) scaledHp = 1;
                    sprite->ChangeHp(scaledHp);
                }
                sprite = NextAllSpriteForVid();
            }
        }

        VID* const linkedVid = vid->linkVid;
        if (linkedVid == 0 || linkedVid->maxHp == 0)
            break;
        vid = linkedVid;
    }
}
void VID::SetMaxHp(int hp)
{
    if (hp < 0)
        return;

    int oldMaxHp[PlayerSlotCount];
    for (int player = 0; player < PlayerSlotCount; ++player)
    {
        oldMaxHp[player] = maxHpByPlayer[player];
        if (oldMaxHp[player] <= 1) oldMaxHp[player] = 1;
        maxHpByPlayer[player] = hp;
    }
    maxHp = hp;
    if (hp == 0)
        return;

    SPRITE* sprite = FirstAllSpriteForVid();
    while (sprite)
    {
        if (sprite->Vid() == this)
        {
            const unsigned int player = (sprite->RuntimeFlags() >> SPRITE::ArmyShift) & 7u;
            const int currentHp = sprite->m_hp;
            int scaledHp = (maxHp > 5000)
                ? ScaleHpRatioFpu(currentHp, hp, oldMaxHp[player])
                : ScaleHpRatioFixedShift8(currentHp, hp, oldMaxHp[player]);
            if (scaledHp <= 1) scaledHp = 1;
            sprite->ChangeHp(scaledHp);
        }
        sprite = NextAllSpriteForVid();
    }
}


static bool DirectionQuantizationOverrideEnabled()
{

    NET* const state = ::Net;
    return state != 0 && (state->m_flags&0x10u) != 0u;
}

int VID::RealDirectionWithGraphDetail(unsigned char direction)
{
    if ((property&0x20000000u) != 0u)
        direction = static_cast<unsigned char>(direction+Graph->viewDirection.value);
    const unsigned int wrapped = (static_cast<unsigned int>(directionQuantizationOffset)+static_cast<unsigned int>(direction))&0xFFu;
    const int directionCount = noDir;

    if ((runtimeFlags&0x00000200u) != 0u)
    {
        GRAPH* const camera = Graph;
        if (static_cast<int>(camera->graphDetail)<5 || DirectionQuantizationOverrideEnabled())
        {
            if (directionCount == 32)
                return static_cast<int>(((wrapped<<5)>>8)&0xFFFCu);
            return static_cast<int>(((wrapped*static_cast<unsigned int>(directionCount))>>8)&0xFFFEu);
        }
    }

    return static_cast<int>((wrapped*static_cast<unsigned int>(directionCount))>>8);
}

void VID::ResetSprites()
{
    for (int i = 0;i<ScriptFunctionCount;++i)
        scriptFunction[i] = -1;

    for (int player = 0;player<PlayerSlotCount;++player)
    {
        spriteLimitByPlayer[player] = -1;
        spriteCountsByPlayer[player] = 0u;
        killedByPlayer[player] = 0;
        reCapturedByPlayer[player] = 0;
        maxHpByPlayer[player] = maxHp;
    }

    spriteLimit = -1;
    runtimeFlags &= ~0x20u;
}


int VID::GetBuildTime() const
{
    const VID* source = (linkVid && linkVid->nWeapon != 0)?linkVid:this;
    return BitsToSigned32(source->weapon->BuildTime)/1000;
}

void VID_SURFACE::ReleaseVideoMemory()
{
    surfaceGeometry->ReleaseBuffers();
    if (surfaceTexture) surfaceTexture->ReleaseVideoMemory();
    surfaceTexture = 0;
    if (!frames || (surfaceFlags&1u) != 0u) return;
    for (int i = 0;i<static_cast<int>(noCadr);++i)
        frames[i].ReleaseVideoMemory();
}

void VID_SURFACE::SetFramesToDevice()
{
    EnsureGeometry();
    for (int i = 0; i < noCadr; ++i)
        CADR_DATA::SetTextureToDevice(frames+i, this);
}

CADR_DATA::CADR_DATA()
{
    lastUseTick = 0u;
    lruPrevious = 0u;
    lruNext = 0u;
    indexCount = 0u;
    additionalDataCount = 0u;
    baseIndex = 0u;
    baseVertex = 0u;
    vertexCount = 0u;
    width = 0u;
    height = 0u;
    offsetX = 0u;
    offsetY = 0u;
    maxZ = 10.0f;
    minZ = -999999.0f;
    textureObject = 0u;
    vertexBuffer = 0u;
    indexBuffer = 0u;
}

CADR_DATA::~CADR_DATA() noexcept(false)
{
    if (textureObject)
    {
        TEXTURE* const object = textureObject;
        delete object;
        textureObject = 0u;
    }
    if (indexBuffer)
    {
        IUnknown* const object = indexBuffer;
        (void)object->Release();
        indexBuffer = 0u;
    }
    if (vertexBuffer)
    {
        IUnknown* const object = vertexBuffer;
        (void)object->Release();
        vertexBuffer = 0u;
    }
}


void __cdecl InitializeVidFirstRangeAtStartup()
{
    VidStaticRangeAInit();
}

void __cdecl InitializeVidSecondRangeAtStartup()
{
    VidStaticRangeBInit();
}


void* __cdecl InitializeVidRootAtStartup()
{
    return VidInitializeGlobalRoot();
}


int VID::GetFireDamageMax() const
{
    const int linkedDamage = linkVid?linkVid->GetFireDamageMax():0;
    const int child15Damage = childVid[15]
        ? MultiplySigned32Wrap(childVid[15]->GetFireDamageMax(), noChild[15]):0;
    const int child14Damage = childVid[14]
        ? MultiplySigned32Wrap(childVid[14]->GetFireDamageMax(), noChild[14]):0;
    const int child8Damage = childVid[8]
        ? MultiplySigned32Wrap(childVid[8]->GetFireDamageMax(), noChild[8]):0;
    int result = AddSigned32Wrap(deathDamageMax, child8Damage);
    result = AddSigned32Wrap(result, child14Damage);
    result = AddSigned32Wrap(result, child15Damage);
    return AddSigned32Wrap(result, linkedDamage);
}


void* __cdecl VidInitializeGlobalRoot()
{
    VID* object = static_cast<VID*>(CrtNew(sizeof(VID)));
    if (object) new(object) VID;
    static_cast<void>(::EmptyVid = object);
    return object;
}

unsigned int VID::CalcImportantForSync()
{
    if ((runtimeFlags&0x1000u) != 0u) return 1u;

    if ((spriteType&0x20u) != 0u || spriteClass == 19u)
    {
        runtimeFlags &= ~0x1000u;
        return 0u;
    }

    if (spriteClass == 2u || spriteClass == 7u || spriteClass == 3u ||
       (spriteType&0x18u) != 0u || deathDamageMin != 0 || deathDamageMax != 0 ||
       (property&0x68u) != 0u)
    {
        runtimeFlags |= 0x1000u;
        return 1u;
    }

    if ((runtimeFlags&0x2000u) != 0u) return 0u;
    runtimeFlags |= 0x2000u;

    if (linkVid && linkVid->CalcImportantForSync() != 0u)
    {
        runtimeFlags = (runtimeFlags&~0x2000u)|0x1000u;
        return 1u;
    }

    for (int i = 0;i<AnimationSlotCount;++i)
    {
        if (childVid[i] && childVid[i]->CalcImportantForSync() != 0u)
        {
            runtimeFlags = (runtimeFlags&~0x2000u)|0x1000u;
            return 1u;
        }
    }

    runtimeFlags &= ~0x2000u;
    return (runtimeFlags>>12)&1u;
}

void CADR_DATA::CalcMaxMinZ(const VID_SURFACE* surface)
{
    if (FloatBitsU32(minZ) != 0xC97423F0u) return;
    const unsigned int vertexCount = this->vertexCount;
    if (vertexCount == 0u)
    {
        minZ = 0.0012f;
        return;
    }

    const unsigned int frameBlockOffset = frameDataOffset;
    const unsigned int indexWordCount = indexCount;
    const unsigned char* const data = surface->surfaceData;
    maxZ = 0.0f;
    minZ = 999.0f;
    const unsigned char* const vertexData = data+indexWordCount*2u+frameBlockOffset;
    const float bias = 32.0f;
    for (unsigned int i = 0;i<vertexCount;++i)
    {
        const unsigned int packedVertex = *reinterpret_cast<const unsigned int*>(vertexData+i*4u);
        const unsigned int mantissa = (packedVertex>>20)&0x3FFu;
        const unsigned int divisor = 1u<<(packedVertex>>30);
        const float value = static_cast<float>(mantissa)/static_cast<float>(divisor)-bias;
        if (value>maxZ) maxZ = value;
        if (!(value >= minZ)) minZ = value;
    }
}

static int ClampByteToRange(int value)
{
    if (value<0)return 0;
    if (value>255)return 255;
    return value;
}

COLOR::COLOR(const GAMMA& gamma, const COLOR& baseColor)
{
    if (gamma.modulate == 0u && gamma.add == 0u)
    {
        value = baseColor.value;
        return;
    }

    const unsigned int inverse = ~gamma.modulate;
    const unsigned int add = gamma.add;
    const unsigned int base = baseColor.value;
    int blue = ((static_cast<int>(inverse&0xFFu)+1)*static_cast<int>(base&0xFFu)>>8)+static_cast<int>(add&0xFFu);
    int green = ((static_cast<int>((inverse>>8)&0xFFu)+1)*static_cast<int>((base>>8)&0xFFu)>>8)+static_cast<int>((add>>8)&0xFFu);
    int red = ((static_cast<int>((inverse>>16)&0xFFu)+1)*static_cast<int>((base>>16)&0xFFu)>>8)+static_cast<int>((add>>16)&0xFFu);
    blue = ClampByteToRange(blue);
    green = ClampByteToRange(green);
    red = ClampByteToRange(red);
    value = 0xFF000000u|(static_cast<unsigned int>(red)<<16)|
        (static_cast<unsigned int>(green)<<8)|static_cast<unsigned int>(blue);
}





float VID::CalculateZSpeed(float numerator, float denominator) const
{
    const float scale = maxSpeed;
    float result;

    if (scale == 999999.0f)
        result = 0.0f;
    else if ((property&2u) != 0u || (property&4u) != 0u)
    {
        const float graphScale = (property&2u) != 0u ? Const->Gravitation : Const->Gravitation2;
        result = static_cast<float>((static_cast<double>(denominator)*graphScale/scale*0.5)+
                                    (static_cast<double>(numerator)*scale/denominator));
        result *= result>0.0f ? 1.1f : 0.9f;
    }
    else if ((property&0x08000000u) != 0u)
        result = maxZSpeed;
    else if (topZ != 0.0f && spriteClass != 5u)
        result = 0.0f;
    else if (acceleration == 999999.0f)
        result = static_cast<float>(static_cast<double>(numerator)*scale/denominator);
    else
        result = static_cast<float>((static_cast<double>(numerator)*scale)/
                                    ((static_cast<double>(scale)*scale*0.5/acceleration)+denominator));

    if (result>maxZSpeed) return maxZSpeed;
    if (result < -maxZSpeed) result = -maxZSpeed;
    return result;
}

void VID::SetGamma(const GAMMA& gamma, int slot)
{
    GAMMA value;
    value.modulate = gamma.modulate;
    value.add = gamma.add;
    if (slot<0)
    {
        gammaRaw = value;
        for (int i = 0;i<PlayerSlotCount;++i) gammaByPlayer[i] = value;
        return;
    }

    if (slot<PlayerSlotCount)
    {
        gammaByPlayer[slot] = value;
        return;
    }

    const STRING message("n_gamma in VID::SetGamma");
    Error(E_INVALID, message, static_cast<unsigned int>(slot));
}


void CADR_DATA::ReleaseVideoMemory()
{
    if (textureObject)
    {
        TEXTURE* const texture = textureObject;
        const TEXTURE& info = *texture;
        const unsigned int pixelCount = info.m_width*info.m_height;
        unsigned int bytes = 0u;
        if (info.m_format == D3DFMT_DXT1)
            bytes = static_cast<unsigned int>(BitsToSigned32(pixelCount)/2);
        else
        {
            unsigned int bytesPerPixel = 2u;
            if (info.m_format == D3DFMT_P8 || info.m_format == D3DFMT_L8) bytesPerPixel = 1u;
            else if (info.m_format == D3DFMT_A8R8G8B8) bytesPerPixel = 4u;
            else if (info.m_format == D3DFMT_R8G8B8) bytesPerPixel = 3u;
            bytes = pixelCount*bytesPerPixel;
        }
        static_cast<void>(VID::useTextureMemory = VID::useTextureMemory-bytes);
        if (textureObject)
        {
            delete textureObject;
            textureObject = 0u;
        }

        if (lruPrevious)
        {
            if (VID::lastUse == this)
                static_cast<void>(VID::lastUse = lruNext);
            lruPrevious->lruNext = lruNext;
            lruNext->lruPrevious = lruPrevious;
            lruNext = 0u;
            lruPrevious = 0u;
            if (VID::lastUse == this)
                static_cast<void>(VID::lastUse = 0);
        }
    }

    if (indexBuffer)
    {
        IUnknown* const object = indexBuffer;
        (void)object->Release();
        indexBuffer = 0u;
    }

    if (vertexBuffer)
    {
        const unsigned int displayValue = Graph->vertexShaderVersion;
        const unsigned int bytesPerEntry = displayValue<256u ? 20u : 12u;
        static_cast<void>(VID::useVertexMemory = VID::useVertexMemory-static_cast<unsigned int>(vertexCount)*bytesPerEntry);
        if (vertexBuffer)
        {
            IUnknown* const object = vertexBuffer;
            (void)object->Release();
            vertexBuffer = 0u;
        }
    }
}



static int DrawRichTextRun(
    const VID_FONT* font, int x, int y, const char* text, int length, unsigned int color)
{
    ID3DXFont* const renderer = font->fontBackend;
    RECT rect = {x, y, AddSigned32Wrap(x, 1), AddSigned32Wrap(y, 1)};
    const char* drawText = text;
    int drawLength = length;
    char prefixed[4096] = {};

    if (text[0] == 'W')
    {
        const int cellWidth = static_cast<int>(font->vidSizeX);
        rect.left = BitsToSigned32(Signed32ToBits(x)-Signed32ToBits(cellWidth/2));
        prefixed[0] = ' ';
        unsigned int i = 0u;
        do
        {
            prefixed[i+1u] = text[i];
        }
        while (text[i++] != '\0');
        drawText = prefixed;
        drawLength = AddSigned32Wrap(length, 1);
    }

    return renderer->DrawTextA(
        0, drawText, drawLength, &rect, (DT_NOCLIP|DT_SINGLELINE), color);
}

void VID::Error(TYPE_ERROR type, const STRING& subject, unsigned long code) const
{
    const STRING objectFormat("VID [%i-%s]");
    ::Error->Error(objectFormat, type, subject, code, nVid, name.CharPtr());
}

VID::~VID() noexcept(false)
{

    unsigned int totalSprites = 0u;
    for (int i = 0;i<PlayerSlotCount;++i) totalSprites += spriteCountsByPlayer[i];
    if (totalSprites != 0u)
    {
        const STRING message("Not all sprites with this VID deleted");
        Error(E_ERROR, message, totalSprites);
    }

    if (nextMirror != this)
    {
        VID* predecessor = nextMirror;
        while (predecessor->nextMirror != this) predecessor = predecessor->nextMirror;
        predecessor->nextMirror = nextMirror;
    }
    else
    {
        if ((storageFlags&8u) == 0u && cadrLinkXYZ)
            CrtFreePointer(cadrLinkXYZ);
        cadrLinkXYZ = 0;

        if ((storageFlags&8u) == 0u || (property&8u) != 0u)
        {
            if (gridZ)
            {
                CrtFreePointer(gridZ);
                gridZ = 0;
            }
            if (gridCadrShift)
            {
                CrtFreePointer(gridCadrShift);
                gridCadrShift = 0;
            }
        }
    }

}
static void VidReportMissingGridTransform(const VID* vid)
{
    const STRING text("not exData for SetGrid");
    vid->Error(E_ERROR, text, 0u);
}

void VID::ResetGridZ(const SPRITE* sprite)
{
    const SPRITE* const mouse =
        Mouse;
    if (sprite == mouse || noGridZ == 0) return;

    EX_SPRITE_DATA* transform = sprite->ExtraData();
    if (!transform)
    {
        VidReportMissingGridTransform(this);
        transform = sprite->ExtraData();
    }

    const int frame = BitsToSigned32(transform->reserved38);
    if (frame<0) return;

    if (sprite->m_vid->spriteClass == 8u) return;

    int begin = 0;
    int end = noGridZ;
    if (gridCadrShift)
    {
        const int frameCount = static_cast<int>(noCadr);
        begin = (frame<frameCount)?gridCadrShift[frame]:0;
        end = (frame<frameCount-1)?gridCadrShift[frame+1]:noGridZ;
    }
    if (begin >= end) return;

    const VECTOR scale = transform->scale;
    for (int i = begin;i<end;++i)
    {
        const VECTOR& dot = gridZ[i];
        const float scaledX = dot.x*scale.x;
        const float scaledY = dot.y*scale.y;
        const float scaledZ = dot.z*scale.z;
        VECTOR point(
            sprite->m_xyz.x+scaledX,
            sprite->m_xyz.y+scaledY,
            sprite->m_xyz.z+scaledZ);
        Map->ClearTempGroundZ(point);
    }
}

void VID::SetGridZ(const SPRITE* sprite)
{
    const SPRITE* const mouse =
        Mouse;
    if (sprite == mouse || noGridZ == 0) return;

    EX_SPRITE_DATA* transform = sprite->ExtraData();
    if (!transform)
    {
        VidReportMissingGridTransform(this);
        transform = sprite->ExtraData();
    }

    transform->reserved38 = sprite->m_currentFrame;
    const int frame = sprite->m_currentFrame;

    int begin = 0;
    int end = noGridZ;
    if (gridCadrShift)
    {
        const int frameCount = static_cast<int>(noCadr);
        begin = (frame<frameCount)?gridCadrShift[frame]:0;
        end = (frame<frameCount-1)?gridCadrShift[frame+1]:noGridZ;
    }
    if (begin >= end) return;

    const VECTOR scale = transform->scale;
    const bool permanent = sprite->m_vid->spriteClass == 8u;
    for (int i = begin;i<end;++i)
    {
        const VECTOR& dot = gridZ[i];
        const float scaledX = dot.x*scale.x;
        const float scaledY = dot.y*scale.y;
        const float scaledZ = dot.z*scale.z;
        VECTOR point(
            sprite->m_xyz.x+scaledX,
            sprite->m_xyz.y+scaledY,
            sprite->m_xyz.z+scaledZ);
        if (permanent) Map->SetGroundZ(point);
        else Map->SetTempGroundZ(point);
    }
}

VID_FONT::VID_FONT(VID* source, const STRING& fontName) : VID()
{
    frameSpeedDefault = 71u;
    noCadr = 256;
    vidSizeX = static_cast<short>(TruncateFloatViaCrtToInt32(source->sizeXYZ.x));
    vidSizeY = static_cast<short>(TruncateFloatViaCrtToInt32(source->sizeXYZ.y));
    buildSizeX = source->sizeXYZ.x;
    buildSizeY = source->sizeXYZ.y;
    type = 0x4000u;
    vidName = fontName;

    const unsigned int weight = (property&0x00200000u)?700u:400u;
    const BOOL italic = (property&0x00400000u)?TRUE:FALSE;
    fontBackend = 0;
    HDC const dc = ::GetDC(0);
    const int dpiY = ::GetDeviceCaps(dc, LOGPIXELSY);
    const int scaledDpiY = TruncateDoubleViaCrtToInt32(static_cast<double>(dpiY)*source->scaleXYZ.y);
    const int fontHeight = BitsToSigned32(0u-Signed32ToBits(::MulDiv(static_cast<int>(vidSizeY), scaledDpiY, 72)));
    const int dpiX = ::GetDeviceCaps(dc, LOGPIXELSX);
    const int scaledDpiX = TruncateDoubleViaCrtToInt32(static_cast<double>(dpiX)*source->scaleXYZ.x);
    const int fontWidth = BitsToSigned32(0u-Signed32ToBits(::MulDiv(static_cast<int>(vidSizeX), scaledDpiX, 72)));
    (void)::ReleaseDC(0, dc);

    GRAPH* const graph = Graph;
    const HRESULT hr = D3DXCreateFontA(graph->device, fontHeight,
        static_cast<UINT>(fontWidth), weight, 0u, italic, 1u, 0u, 0u, 1u,
        fontName.value, &fontBackend);
    if (hr<0)
    {
        STRING subject = "font"+fontName;
        Error(E_CREATE, subject, static_cast<unsigned long>(hr));
        return;
    }

    RECT rect = {0, 0, 1, 1};
    static const char measureText[] = "X";
    (void)fontBackend->DrawTextA(0, measureText, 1, &rect, 0x520u, 0xFFFFFFFFu);
    vidSizeX = static_cast<short>(rect.right);
    vidSizeY = static_cast<short>(rect.bottom);
}

VID_SURFACE::VID_SURFACE(VID* source, RESOURCE* resource, const STRING& name)
{
    unsigned int flags = surfaceFlags;
    flags = (flags&0xFFFFFF00u)|(((flags&0xFFu)&0xFEu)|2u);
    this->surfaceGeometry = new SURFACE;
    this->surfaceGeometry->CreateQuad();
    this->surfaceTexture = 0;
    this->surfaceBackend = 0;
    this->frames = 0;
    this->surfaceData = 0;
    this->palette = 0;
    this->surfaceFlags = flags;
    this->recolorPalette = 0;
    (void)(this->vidName = name);
    this->type = 0x41u;
    this->noCadr = (source && source->spriteClass == 19u)?256:1;

    if (this->noCadr == 1)
    {

        const STRING stem = name.BeforeLast(".");
        const int stemLength = stem.Length();
        const int stemTail = (stemLength != 0)?static_cast<signed char>(stem[stemLength-1]):0;
        if (isdigit(stemTail) != 0)
        {
            const STRING suffix = name.AfterLast("_");
            if (isdigit(static_cast<signed char>(suffix[0])) != 0)
            {
                for (;;)
                {
                    bool found;
                    {
                        const STRING candidate = name.Add(this->noCadr);
                        std::FILE* file = 0;
                        if (!candidate.IsEmpty())
                            file = std::fopen(candidate.CharPtr(), "rb");
                        if (file) (void)std::fclose(file);
                        found = file != 0;
                    }
                    if (!found) break;
                    ++this->noCadr;
                }
            }
        }
    }

    if (resource) LoadParameters(resource);

    this->surfaceMode = (!(this->sizeXYZ.z>this->sizeXYZ.y))?1:0;
    this->renderMode = 0u;

    D3DXImageInfo imageInfo;
    (void)D3DXGetImageInfoFromFileA(name.CharPtr(), &imageInfo);
    this->pixelFormat = imageInfo.Format;

    unsigned int imageWidth = imageInfo.Width;
    unsigned int imageHeight = imageInfo.Height;
    if (this->spriteClass == 19u)
    {
        imageWidth = (imageWidth>>4u)-1u;
        imageHeight = (imageHeight>>4u)-1u;
    }
    this->vidSizeX = static_cast<short>(imageWidth);
    this->vidSizeY = static_cast<short>(imageHeight);
    this->buildSizeX = static_cast<float>(this->vidSizeX);
    this->buildSizeY = static_cast<float>(this->vidSizeY);

    if (this->pixelFormat == D3DFMT_A8R8G8B8 || this->pixelFormat == D3DFMT_A4R4G4B4 ||
       this->pixelFormat == D3DFMT_A1R5G5B5 || this->pixelFormat == D3DFMT_DXT3 ||
       this->pixelFormat == D3DFMT_DXT5)
        this->type = static_cast<unsigned short>(this->type|2u);

    const int frameCount = static_cast<int>(this->noCadr);
    this->frames = new(std::nothrow) CADR_DATA[frameCount];
    for (int frameIndex = 0;frameIndex<frameCount;++frameIndex)
        this->frames[frameIndex].height = 1u;

}

static bool ResolveVidLinkTarget(int index, VID*& target)
{
    MAP* const application = Map;
    VID* const source = application->StoredVid(index);
    if (!source) return false;
    target = source->exchangedVid;
    return true;
}

static void ReportVidReferenceError(const VID* owner, const char* subjectText, int value)
{
    if (g_iZmDebug != 0u) return;
    STRING subject;
    subject = subjectText;
    owner->Error(E_INVALID, subject, static_cast<unsigned long>(value));
}


void VID::SetChildAndLink()
{
    if (nLinkVid != 0)
    {
        if (nLinkVid == nVid)
            ReportVidReferenceError(this, "LinkVid == nVid", nLinkVid);
        else
        {
            VID* resolved = 0;
            if (ResolveVidLinkTarget(nLinkVid, resolved)) linkVid = resolved;
            else ReportVidReferenceError(this, "LinkVid", nLinkVid);
        }
    }

    const WEAPON_RUNTIME* const weaponState = weapon;
    if (weaponState->maxColumn >= 0)
    {
        for (int lane = 0; lane <= weaponState->maxColumn; ++lane)
        {
            if (weaponState->GammaR[lane] != 0 ||
               weaponState->GammaG[lane] != 0 ||
               weaponState->GammaB[lane] != 0 ||
               weaponState->GammaA[lane] != 0)
                runtimeFlags |= 0x01u;

            const VECTOR& lane0 = weaponState->Scale[lane];
            if (lane0.x != 1.0f || lane0.y != 1.0f || lane0.z != 1.0f)
                runtimeFlags |= 0x02u;

            const VECTOR& lane1 = weaponState->Shift[lane];
            if (!FloatEqualOrUnordered(lane1.x, 0.0f) ||
               !FloatEqualOrUnordered(lane1.y, 0.0f) ||
               !FloatEqualOrUnordered(lane1.z, 0.0f))
                runtimeFlags |= 0x04u;

            if (weaponState->DirectionX[lane] != 0 ||
               weaponState->DirectionY[lane] != 0 ||
               weaponState->DirectionZ[lane] != 0)
                runtimeFlags |= 0x08u;

            if (!FloatEqualOrUnordered(weaponState->Speed[lane], 0.0f))
                runtimeFlags |= 0x10u;
        }
    }

    if (lifeTime != 999999 ||
       (property&0x00200000u) != 0u ||
       !FloatEqualOrUnordered(maxSpeed, randomSpeed) ||
       noGridZ != 0 ||
       (property&0x00182000u) != 0u ||
       (runtimeFlags&0x1Fu) != 0u ||
       (nWeapon != 0 && childVid[8] != 0) ||
       (property&0x28u) != 0u)
        actionAuxStateRequired = 1;

    if ((spriteClass == 2u || spriteClass == 7u) && (noDir == 16 || noDir == 32))
    {
        runtimeFlags |= 0x200u;
        if (linkVid && (linkVid->noDir == 16 || linkVid->noDir == 32))
            linkVid->runtimeFlags |= 0x200u;

        for (int slot = 15; slot <= 16; ++slot)
            if (childVid[slot] && (childVid[slot]->noDir == 16 || childVid[slot]->noDir == 32))
                childVid[slot]->runtimeFlags |= 0x200u;
    }

    if (!FloatEqualOrUnordered(weaponState->Weight, 999999.0f))
        movementTactEnabled |= 1;

    for (int slot = 0; slot<AnimationSlotCount; ++slot)
    {
        const int sourceChild = nChildVid[slot];
        if (sourceChild == 0) continue;

        const unsigned int childIndexBits = static_cast<unsigned int>(sourceChild);
        const unsigned int signMask = 0u-(childIndexBits>>31u);
        const int childIndex = static_cast<int>((childIndexBits^signMask)-signMask);
        VID* resolved = 0;
        if (ResolveVidLinkTarget(childIndex, resolved))
        {
            childVid[slot] = resolved;
            if ((resolved->property&0x80u) != 0u || (resolved->property&0x00080000u) != 0u)
                actionAuxStateRequired = 1;
        }
        else
            ReportVidReferenceError(this, "child", sourceChild);
    }

    runtimeFlags = (runtimeFlags&~0x100u)|((nWeapon != 0 && childVid[8] != 0)?0x100u:0u);
}

static void VidReportResourceError(const VID* owner,TYPE_ERROR type,const char* text,int value);
static VID_FONT* SelectRichTextFontVid(VID_FONT* currentFont,int index,bool requireValid);
static bool RichTextTagStartsWith(const char* p,const char* literal);

static void MeasureRichTextRun(const VID_FONT* selectedFont, const char* begin, int length, unsigned int color, RECT* rect)
{
    (void)selectedFont->fontBackend->DrawTextA(0, begin, length, rect, 0x520u, color);
}
static void AppendRichTextLineOffset(LIST<int>* lines, int width)
{
    if (lines->m_count >= lines->m_capacity)
    {
        const int newCapacity = lines->m_capacity*2+4;
        lines->Expand(newCapacity);
    }
    lines->m_data[lines->m_count++] = width;
}
void VID_FONT::CalcTextSize(const STRING& textValue, int* outMaxWidth, int* outHeight,
    int* outLineCount, LIST<int>* lineOffsets, unsigned long alignFlags) const
{
    VID_FONT* const owner = const_cast<VID_FONT*>(this);
    if (!owner->fontBackend)
    {
        VidReportResourceError(owner, E_ERROR, "not font for CalcText", 0);
        return;
    }

    *outMaxWidth = 0;
    *outHeight = 0;
    *outLineCount = 0;
    lineOffsets->m_count = 0;
    lineOffsets->m_capacity = 0;
    if (lineOffsets->m_data) ::operator delete(lineOffsets->m_data);
    lineOffsets->m_data = 0;

    const STRING* const textObject = &textValue;
    const char* const text = textObject->m_value;
    VID_FONT* selected = owner;
    int lineWidth = 0;
    int runLength = 0;
    RECT measureRect = {0, 0, 1, 1};

    for (int index = 0;;++index)
    {
        const char ch = text[index];
        if (ch == '\0')
        {
            if (runLength>0)
            {
                MeasureRichTextRun(selected, text+index-runLength, runLength, 0xFFFFFFFFu, &measureRect);
                lineWidth += measureRect.right;
                *outHeight += measureRect.bottom;
            }
            ++*outLineCount;
            AppendRichTextLineOffset(lineOffsets, lineWidth);
            if (lineWidth>*outMaxWidth) *outMaxWidth = lineWidth;
            break;
        }

        const bool lineBreak = (ch == '\n' || ch == '\r');
        const bool alignTag = ch == '<' && RichTextTagStartsWith(text+index, "<Align=");
        const bool fontTag = ch == '<' && RichTextTagStartsWith(text+index, "<Font=");
        const bool colorTag = ch == '<' && RichTextTagStartsWith(text+index, "<Color=");

        if (alignTag || fontTag || colorTag || lineBreak)
        {
            if (runLength>0)
            {
                MeasureRichTextRun(selected, text+index-runLength, runLength, 0xFFFFFFFFu, &measureRect);
                if (!alignTag) lineWidth += measureRect.right;
            }

            if (alignTag)
            {
                lineWidth = static_cast<int>(selected->vidSizeX)*BitsToSigned32(CrtAtoiText(text+index+7));
            }
            else if (fontTag)
            {
                VID_FONT* const candidate = SelectRichTextFontVid(selected, BitsToSigned32(CrtAtoiText(text+index+6)), false);
                if (candidate) selected = candidate;
            }
            else if (lineBreak)
            {
                *outHeight += static_cast<int>(selected->vidSizeY);
                ++*outLineCount;
                AppendRichTextLineOffset(lineOffsets, lineWidth);
                if (lineWidth>*outMaxWidth) *outMaxWidth = lineWidth;
                lineWidth = 0;
                ++index;
            }

            runLength = -1;
            if (alignTag || fontTag || colorTag)
            {
                while (text[index] && text[index] != '>') ++index;
            }
        }
        ++runLength;
    }

    for (int i = 0;i<lineOffsets->m_count;++i)
    {
        if ((alignFlags&1u) != 0u) lineOffsets->m_data[i] = -(lineOffsets->m_data[i]/2);
        else if ((alignFlags&2u) != 0u) lineOffsets->m_data[i] = -lineOffsets->m_data[i];
        else lineOffsets->m_data[i] = 0;
    }
}

namespace {
static VECTOR EvaluateVidHardwareCurve(
    const WEAPON_RUNTIME* owner,const VECTOR* keys,float phase);
static int VidHardwareHalfTowardZero(int value);
}

namespace {
struct VidShadowVertex
{
    float x;
    float y;
    float z;
    float rhw;
    float u;
    float v;
};

static bool VidShadowOutsideRuntimeBounds(const VID* owner, float x, float y, const VECTOR& scale)
{
    const float extentX = owner->buildSizeX*(0.5f)*scale.x;
    const float extentY = owner->buildSizeY*(0.5f)*scale.y;
    if (x-extentX>VID::viewMax.x &&
       y-extentY>VID::viewMax.y)
        return true;
    if (!(x+extentX >= VID::viewMin.x) &&
       !(y+extentY >= VID::viewMin.y))
        return true;
    return false;
}



static bool VidShadowUsesDirectionalQuad(const CADR_DATA* frame)
{
    const int width = static_cast<int>(static_cast<short>(frame->width));
    const int height = static_cast<int>(static_cast<short>(frame->height));
    const int maximum = width>height?width:height;
    const double threshold = static_cast<double>(frame->maxZ)*1.7999999523162842;
    return threshold<maximum;
}
}

void VID_SURFACE::DrawShadow(const SPRITE* spriteValue, const VECTOR& offset)
{
    VID_SURFACE* const self = this;

    CADR_DATA* const frameBase = self->frames;
    if (!frameBase || (self->runtimeFlags&kVidRuntimeHidden) != 0u ||
        self->renderMode != 0u || (self->type&2u) != 0u) return;

    const SPRITE* const sprite = spriteValue;
    const VECTOR* const shadowOrigin = &offset;

    const float directionX = shadowOrigin->x-sprite->Coor().x;
    const float directionY = shadowOrigin->y-sprite->Coor().y;
    const unsigned char shadowDirection = static_cast<unsigned char>(
        Decart2Polar(directionX, directionY).value+128u);
    unsigned char drawDirection = shadowDirection;

    CADR_DATA* frame = 0;
    bool directionalQuad = false;
    const unsigned int ownerProperty = self->property;
    if ((ownerProperty&0x20000000u) != 0u)
    {
        const int relativeFrame = static_cast<int>(sprite->CurrentFrame())-sprite->m_currentFrameBegin;
        const unsigned char relativeDirection = static_cast<unsigned char>(
            sprite->Direction().value-shadowDirection);
        const int realDirection = self->RealDirectionWithGraphDetail(relativeDirection);
        const int animation = sprite->Animation();
        const int frameIndex = relativeFrame+self->animationBaseFrame[animation]+
            realDirection*self->animationFrameCount[animation];
        frame = frameBase+frameIndex;

        directionalQuad = VidShadowUsesDirectionalQuad(frameBase);
    }
    else
    {
        frame = frameBase+static_cast<int>(sprite->CurrentFrame());
        directionalQuad = VidShadowUsesDirectionalQuad(frameBase+sprite->m_currentFrameBegin);
    }

    if (frame->height == 0u) return;

    VECTOR scale;
    const EX_SPRITE_DATA* const transformOwner =
        sprite->ExtraData();
    if (transformOwner) scale = transformOwner->scale;
    else
    {
        scale.x = 1.0f;
        scale.y = 1.0f;
        scale.z = 1.0f;
    }
    scale.x *= self->scaleXYZ.x;
    scale.y *= self->scaleXYZ.y;
    scale.z *= self->scaleXYZ.z;

    float positionX = sprite->Coor().x;
    float positionY = sprite->Coor().y-sprite->Coor().z;
    if ((ownerProperty&0x00010000u) != 0u)
    {
        const unsigned int waveIndex = (::CurrentTime>>3u)&0xFFu;
        positionY -= FSin[waveIndex]*self->topZ;
    }

    const unsigned int runtimeFlags = self->runtimeFlags;
    if ((runtimeFlags&kVidAnimateScale) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            self->weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveScale = EvaluateVidHardwareCurve(curveOwner, curveOwner->Scale, phase);
        scale.x *= curveScale.x;
        scale.y *= curveScale.y;
        scale.z *= curveScale.z;
    }
    if ((runtimeFlags&kVidAnimateOffset) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            self->weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveOffset = EvaluateVidHardwareCurve(curveOwner, curveOwner->Shift, phase);
        positionX += curveOffset.x;
        positionY += curveOffset.y;
    }

    if (VidShadowOutsideRuntimeBounds(self, positionX, positionY, scale)) return;

    if (directionalQuad && (ownerProperty&0x20000000u) != 0u)
    {
        if ((ownerProperty&0x00002000u) != 0u) drawDirection = sprite->Direction().value;
        else
        {
            const unsigned char relativeDirection = static_cast<unsigned char>(
                sprite->Direction().value-shadowDirection);
            const int realDirection = self->RealDirectionWithGraphDetail(relativeDirection);
            const int correction = (realDirection<<8)/self->noDir;
            drawDirection = static_cast<unsigned char>(sprite->Direction().value-static_cast<unsigned char>(correction));
        }
    }

    self->lastRenderUseTick = ::RealCurrentTime;
    CADR_DATA::SetTextureToDevice(frame, self);
    if (FloatBitsU32(frame->minZ) == 0xC97423F0u)
        frame->CalcMaxMinZ(self);
    if (frame->maxZ <= 0.0f) return;

    const VECTOR2 groundPoint(sprite->Coor().x, sprite->Coor().y);
    float groundDelta = sprite->Coor().z-Map->GetGroundZ(groundPoint);
    if (groundDelta<0.0f) groundDelta = 0.0f;

    const int width = static_cast<int>(static_cast<short>(frame->width));
    const int height = static_cast<int>(static_cast<short>(frame->height));
    const int halfOwnerX = VidHardwareHalfTowardZero(self->vidSizeX);
    const int halfOwnerY = VidHardwareHalfTowardZero(self->vidSizeY);
    const int frameX = static_cast<int>(static_cast<short>(frame->offsetX));
    const int frameY = static_cast<int>(static_cast<short>(frame->offsetY));
    const int xSource[4] = {0, width, 0, width};
    const int ySource[4] = {0, 0, height, height};
    const TEXTURE* const texture = frame->textureObject;
    const float textureWidth = static_cast<float>(texture->m_width);
    const float textureHeight = static_cast<float>(texture->m_height);
    VidShadowVertex vertices[4];
    for (int i = 0;i<4;++i)
    {
        vertices[i].x = static_cast<float>(xSource[i]-halfOwnerX+frameX);
        vertices[i].y = static_cast<float>(ySource[i]-halfOwnerY+frameY)-groundDelta;
        vertices[i].z = i<2 ? static_cast<float>(static_cast<double>(frame->maxZ)*0.30000001192092896) : frame->minZ;
        vertices[i].rhw = 1.0f;
        vertices[i].u = static_cast<float>(xSource[i])/textureWidth;
        vertices[i].v = static_cast<float>(ySource[i])/textureHeight;
    }

    const ANGLE rotation(drawDirection);
    const float rotateXX = rotation.Cos();
    const float rotateXY = rotation.Sin();
    MAP* const world = Map;
    for (int i = 0;i<4;++i)
    {
        VidShadowVertex& vertex = vertices[i];
        vertex.x *= scale.x;
        vertex.y *= scale.y;
        vertex.z *= scale.z;
        vertex.z = static_cast<float>((static_cast<double>(sprite->Coor().z)+128.0+vertex.z)/8192.0);

        if (!directionalQuad || (ownerProperty&0x20000000u) != 0u)
        {
            const float x = vertex.x*rotateXX-vertex.y*rotateXY;
            const float y = vertex.y*rotateXX+vertex.x*rotateXY;
            vertex.x = x;
            vertex.y = y;
        }
        if (directionalQuad)
        {
            const float radius = static_cast<float>(-static_cast<double>(frame->maxZ)*0.6000000238418579);
            const ANGLE shadowAngle(shadowDirection);
            vertex.x -= radius*shadowAngle.Sin();
            vertex.y += radius*shadowAngle.CosY();
        }

        const float worldX = positionX+vertex.x;
        const float worldY = positionY+vertex.y;
        if (self->layer != 11)
        {
            const VECTOR2 projected = world->ToScreenScaled(VECTOR2(worldX, worldY));
            vertex.x = projected.x;
            vertex.y = projected.y;
        }
        else
        {
            vertex.x = worldX-world->ShiftXY().x;
            vertex.y = worldY-world->ShiftXY().y;
        }
        vertex.y += groundDelta;
    }

    GRAPH* const graph = Graph;
    IDirect3DDevice9* const device = graph->device;
    (void)device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2u, vertices, sizeof(VidShadowVertex));
    static_cast<void>(VID_SURFACE::noDrawShadow = VID_SURFACE::noDrawShadow+1u);
}

static VID_FONT* SelectRichTextFontVid(VID_FONT* currentFont, int index, bool requireValid)
{
    VID* const sentinel = ::EmptyVid;
    VID* const stored = index >= 0 ? Map->StoredVid(index) : 0;
    if (!stored || stored == sentinel || (stored->type&0x4000u) == 0u)
        return requireValid ? 0 : currentFont;

    VID_FONT* const font = static_cast<VID_FONT*>(stored);
    return font->fontBackend ? font : (requireValid ? 0 : currentFont);
}
static bool RichTextTagStartsWith(const char* textCursor, const char* literal)
{
    while (*literal)
    {
        if (*textCursor++ != *literal++) return false;
    }
    return true;
}
static unsigned int NormalizeRichTextColor(unsigned int color)
{
    if ((color & 0xFF000000u) != 0u)
        return color;
    if ((color & 0x00FFFFFFu) != 0u)
        return color | 0xFF000000u;
    return 0xFFFFFFFFu;
}

static int DrawRichTextRunClipped(
    VID_FONT* owner, VID_FONT* selected, int x, int y, const char* text, int length, unsigned int color, bool measureRun, RECT* measureRect)
{
    int start = x;
    const char* begin = text;
    int count = length;
    while (count>0 && start<0)
    {
        MeasureRichTextRun(selected, begin, 1, color, measureRect);
        start += measureRect->right;
        ++begin;
        --count;
    }
    if (measureRun) MeasureRichTextRun(selected, begin, count, color, measureRect);
    (void)DrawRichTextRun(selected, start, y, begin, count, color);
    return start+measureRect->right;
}

void VID_FONT::DrawTextA(int startX, int startY, const STRING& textValue, int textLength,
    int firstLine, int lastLine, COLOR colorValue, const LIST<int>& lineOffsetValues) const
{
    const LIST<int>* const lineOffsets = &lineOffsetValues;
    VID_FONT* const owner = const_cast<VID_FONT*>(this);
    if (!owner->fontBackend || textLength == 0) return;
    const STRING* const textObject = &textValue;
    const char* const text = textObject->m_value;
    if (!*text) return;

    const unsigned int originalColor = colorValue.value;
    unsigned int currentColor = originalColor;
    VID_FONT* selected = owner;
    int line = 0;
    int y = startY;
    int x = startX+lineOffsets->m_data[0];
    int runLength = 0;
    RECT measureRect = {0, 0, 1, 1};
    const int endIndex = textLength;
    const int lastVisibleLine = lastLine+1;

    (void)Graph->device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);

    int index = 0;
    for (;index<endIndex && line <= lastVisibleLine;++index)
    {
        const char ch = text[index];
        const bool lineBreak = (ch == '\n' || ch == '\r');
        const bool alignTag = ch == '<' && RichTextTagStartsWith(text+index, "<Align=");
        const bool fontTag = ch == '<' && RichTextTagStartsWith(text+index, "<Font=");
        const bool colorTag = ch == '<' && RichTextTagStartsWith(text+index, "<Color=");

        if (alignTag || fontTag || colorTag || lineBreak)
        {
            if (runLength>0 && line >= firstLine)
            {
                const char* const begin = text+index-runLength;
                const int advanced = DrawRichTextRunClipped(owner, selected, x, y, begin, runLength, currentColor, !lineBreak, &measureRect);
                if (!alignTag && !lineBreak) x = advanced;
            }

            if (alignTag)
            {
                x = startX+static_cast<int>(selected->vidSizeX)*BitsToSigned32(CrtAtoiText(text+index+7));
            }
            else if (fontTag)
            {
                const int fontIndex = BitsToSigned32(CrtAtoiText(text+index+6));
                VID_FONT* const candidate = SelectRichTextFontVid(selected, fontIndex, true);
                if (candidate)
                {
                    selected = candidate;
                    currentColor = NormalizeRichTextColor(selected->gammaRaw.add);
                }
                else
                {
                    VidReportResourceError(owner, E_INVALID, "font in DrawText()", fontIndex);
                }
            }
            else if (colorTag)
            {
                long parsed = static_cast<long>(currentColor);
                (void)sscanf(text+index+7, "%li", &parsed);
                if (parsed == 0)
                    currentColor = originalColor;
                else
                {
                    currentColor = static_cast<unsigned int>(parsed);
                    if ((currentColor&0xFF000000u) == 0u)
                        currentColor |= 0xFF000000u;
                }
            }
            else if (lineBreak)
            {
                if (text[index+1] == '\n' || text[index+1] == '\r') ++index;
                ++line;
                y += static_cast<int>(selected->vidSizeY);
                x = startX+lineOffsets->m_data[line];
            }

            runLength = -1;
            if (alignTag || fontTag || colorTag)
            {
                while (index<endIndex && text[index] != '>') ++index;
            }
        }
        ++runLength;
    }

    if (runLength>0 && line >= firstLine && line <= lastVisibleLine)
    {
        const char* begin = text+index-runLength;
        (void)DrawRichTextRunClipped(owner, selected, x, y, begin, runLength, currentColor, false, &measureRect);
    }
}


static void VidReportResourceError(const VID* owner, TYPE_ERROR type, const char* text, int value)
{
    STRING subject;
    subject = text;
    owner->Error(type, subject, static_cast<unsigned long>(value));
}

static int VidClampGammaDelta(int value)
{
    if (value>255) return 255;
    if (value<-255) return -255;
    return value;
}

static void VidSetGammaDelta(GAMMA& gamma, int component, int value)
{
    unsigned char* const bytes = gamma.value;
    value = VidClampGammaDelta(value);
    bytes[component] = 0;
    bytes[component+4] = 0;
    if (value<0) bytes[component] = static_cast<unsigned char>(-value);
    else bytes[component+4] = static_cast<unsigned char>(value);
}



static long VidResourceTell(RESOURCE* resource)
{
    return static_cast<long>(BitsToSigned32(static_cast<unsigned int>(std::ftell(resource->FileHandle()))));
}

static unsigned int VidResourceSectionSize(const RESOURCE* resource)
{
    return resource->RecordSize();
}

static int VidResourceBytesToEndOfSubrecord(RESOURCE* resource)
{
    const long position = VidResourceTell(resource);
    const int currentPosition = resource->DataPosition();
    const int currentSize = resource->SubRecordSize();
    return currentPosition-static_cast<int>(position)+currentSize+4;
}

static void VidSkipResourceBytes(RESOURCE* resource, long bytes)
{
    resource->SetIoState(2u);
    (void)std::fseek(resource->FileHandle(), BitsToSigned32(Signed32ToBits(static_cast<int>(bytes))), SEEK_CUR);
}

void VID::LoadParameters(RESOURCE* resource)
{
    (void)resource->Read(&spriteType, 4);
    (void)resource->Read(&spriteClass, 4);
    (void)resource->Read(&property, 4);
    (void)resource->Read(&moveMask, 4);
    if (nVid == 0) property |= 0x01000000u;

    (void)resource->Read(&sizeXYZ.x, 4);
    (void)resource->Read(&sizeXYZ.y, 4);
    (void)resource->Read(&sizeXYZ.z, 4);
    (void)resource->Read(&maxHp, 4);
    (void)resource->Read(&maxSpeed, 4);
    (void)resource->Read(&randomSpeed, 4);
    (void)resource->Read(&maxZSpeed, 4);
    (void)resource->Read(&randomZSpeed, 4);
    (void)resource->Read(&acceleration, 4);
    (void)resource->Read(&slow, 4);
    (void)resource->Read(&rotationSpeed, 4);
    (void)resource->Read(&nWeapon, 4);
    (void)resource->Read(&deathRange, 4);
    (void)resource->Read(&deathDamageMin, 4);
    (void)resource->Read(&deathDamageMax, 4);
    if (deathDamageMax<deathDamageMin)
    {
        deathDamageMax = deathDamageMin;
        VidReportResourceError(this, E_INVALID, "deathDamageMax < deathDamageMin", 0);
    }
    (void)resource->Read(&deathPush, 4);
    deathPush = static_cast<float>(static_cast<double>(deathPush)/50.0);

    (void)resource->Read(&linkXYZ.x, 4);
    (void)resource->Read(&linkXYZ.y, 4);
    (void)resource->Read(&linkXYZ.z, 4);
    linkXYZ.y = -linkXYZ.y;
    (void)resource->Read(&nLinkVid, 4);
    (void)resource->Read(&topZ, 4);
    (void)resource->Read(&forMoveUpZ, 4);
    (void)resource->Read(&forMoveDownZ, 4);
    (void)resource->Read(&lifeTime, 4);

    VidSkipResourceBytes(resource, 16);
    (void)resource->Read(&noDir, 4);
    (void)resource->Read(noAnimCadr, sizeof(noAnimCadr));
    (void)resource->Read(sfx, sizeof(sfx));
    (void)resource->Read(frameSpeed, sizeof(frameSpeed));
    (void)resource->Read(childX, sizeof(childX));
    (void)resource->Read(childY, sizeof(childY));
    (void)resource->Read(childZ, sizeof(childZ));
    (void)resource->Read(nChildVid, sizeof(nChildVid));
    (void)resource->Read(noChild, sizeof(noChild));

    int gammaRed;
    int gammaGreen;
    int gammaBlue;
    int gammaAlpha;
    (void)resource->Read(&gammaRed, 4);
    (void)resource->Read(&gammaGreen, 4);
    (void)resource->Read(&gammaBlue, 4);
    (void)resource->Read(&gammaAlpha, 4);
    (void)resource->Read(&blackWhite, sizeof(blackWhite));
    if (optBlackWhite == 0) blackWhite = 0;

    gammaRaw.modulate = 0;
    gammaRaw.add = 0;
    VidSetGammaDelta(gammaRaw, 3, gammaAlpha);
    VidSetGammaDelta(gammaRaw, 2, gammaRed);
    VidSetGammaDelta(gammaRaw, 1, gammaGreen);
    VidSetGammaDelta(gammaRaw, 0, gammaBlue);
    for (int player = 0;player<PlayerSlotCount;++player) gammaByPlayer[player] = gammaRaw;

    (void)resource->Read(&scaleXYZ.x, 4);
    (void)resource->Read(&scaleXYZ.y, 4);
    (void)resource->Read(&scaleXYZ.z, 4);

    const float unlimited = 999999.0f;
    if (rotationSpeed == unlimited) rotationSpeed = 0.0f;
    else if (rotationSpeed == 0.0f) rotationSpeed = unlimited;
    else rotationSpeed = static_cast<float>(256.0/static_cast<double>(rotationSpeed));

    if (maxSpeed != unlimited) maxSpeed = static_cast<float>(static_cast<double>(maxSpeed)/1000.0);
    if (randomSpeed != unlimited) randomSpeed = static_cast<float>(static_cast<double>(randomSpeed)/1000.0);
    if (maxZSpeed != unlimited) maxZSpeed = static_cast<float>(static_cast<double>(maxZSpeed)/1000.0);
    if (randomZSpeed != unlimited) randomZSpeed = static_cast<float>(static_cast<double>(randomZSpeed)/1000.0);
    if (acceleration != unlimited) acceleration = static_cast<float>(static_cast<double>(acceleration)/1000000.0);
    if (slow != unlimited) slow = static_cast<float>(static_cast<double>(slow)/1000000.0);

    movementTactEnabled = (maxSpeed != 0.0f ||
                         randomSpeed != 0.0f ||
                         maxZSpeed != 0.0f ||
                         randomZSpeed != 0.0f ||
                         (property&0x06u) != 0u) ? 1 : 0;

    for (int slot = 0;slot<AnimationSlotCount;++slot)
        if (frameSpeed[slot] == 0) frameSpeed[slot] = static_cast<int>(frameSpeedDefault);

    realSizeXYZ = sizeXYZ;

    if ((property&0x100u) != 0u)
    {
        const double diagonalScale = 0.70706021785736083984375;
        const double yAsX = static_cast<double>(sizeXYZ.y)/diagonalScale;
        if (static_cast<double>(sizeXYZ.x)>yAsX)
            sizeXYZ.y = static_cast<float>(static_cast<double>(sizeXYZ.x)*diagonalScale);
        else sizeXYZ.x = static_cast<float>(yAsX);
    }
    halfSizeXY.x = sizeXYZ.x*0.5f;
    halfSizeXY.y = sizeXYZ.y*0.5f;

    if (noDir == 0)
    {
        VidReportResourceError(this, E_INVALID, "NoDir == 0", 0);
        exit(1);
    }

    if (noCadr == 0)
        VidReportResourceError(this, E_INVALID, "noCadr == 0", 0);
    else if (static_cast<int>(noCadr)<noDir && spriteClass != 19u)
    {
        VidReportResourceError(this, E_INVALID, "noCadr < noDir", 0);
        noDir = static_cast<int>(noCadr);
    }

    int requestedFrames = 0;
    for (int slot = 0;slot<AnimationSlotCount;++slot)
        requestedFrames = BitsToSigned32(static_cast<unsigned int>(requestedFrames)+
            static_cast<unsigned int>(noAnimCadr[slot])*static_cast<unsigned int>(noDir));

    if (requestedFrames>static_cast<int>(noCadr))
    {
        VidReportResourceError(this, E_MISSING, "noCadr for noAnimCadr and noDir", 0);
        for (int slot = AnimationSlotCount-1;slot >= 0;--slot)
        {
            const int withoutCurrent = BitsToSigned32(static_cast<unsigned int>(requestedFrames)-
                static_cast<unsigned int>(noAnimCadr[slot])*static_cast<unsigned int>(noDir));
            if (withoutCurrent>static_cast<int>(noCadr))
            {
                noAnimCadr[slot] = 0;
                requestedFrames = withoutCurrent;
                continue;
            }
            const int excess = BitsToSigned32(static_cast<unsigned int>(requestedFrames)-
                static_cast<unsigned int>(static_cast<int>(noCadr)));
            noAnimCadr[slot] = BitsToSigned32(static_cast<unsigned int>(noAnimCadr[slot])-
                static_cast<unsigned int>(excess/noDir));
            break;
        }
    }

    directionQuantizationOffset = 128/noDir;
    if ((spriteType == 32u || spriteClass == 10u) && moveMask == 0u && nWeapon == 0)
        runtimeFlags &= ~0x80u;

    int firstRealAnimation = -1;
    int totalFrames = 0;
    for (int slot = 0;slot<AnimationSlotCount;++slot)
    {
        const SOUND* const soundOwner =
            ::Sound;
        const int soundIndex = sfx[slot];
        const SFX* const soundTable = soundOwner->m_sfx;
        if (soundTable)
        {
            bool soundLoaded = false;
            if (soundIndex >= 0 && soundIndex <= soundOwner->m_sfxCount)
            {
                soundLoaded = soundTable[soundIndex].m_bufferCount != 0u;
            }
            if (!soundLoaded && nVid != -1)
            {
                {
                    STRING subject("sfx");
                    Error(E_INVALID, subject, static_cast<unsigned long>(sfx[slot]));
                }
                sfx[slot] = 0;
            }
        }

        const int animationFrames = noAnimCadr[slot];
        if (animationFrames != 0)
        {
            if (animationFrames != 1) runtimeFlags |= 0x80u;
            animationBaseFrame[slot] = totalFrames;
            animationFrameCount[slot] = animationFrames;
            if (firstRealAnimation<0)
            {
                firstRealAnimation = slot;
                for (int previous = 0;previous<slot;++previous)
                    if (animationFrameCount[previous] == 0)
                        animationFrameCount[previous] = animationFrames;
            }
        }
        else
        {
            bool copiedClass10Fallback = false;
            if (spriteClass == 10u && (slot&1) != 0 && slot <= 13 &&
               noAnimCadr[firstRealAnimation+1] != 0)
            {
                animationBaseFrame[slot] = animationBaseFrame[firstRealAnimation+1];
                animationFrameCount[slot] = animationFrameCount[firstRealAnimation+1];
                copiedClass10Fallback = true;
            }
            if (!copiedClass10Fallback)
            {
                animationBaseFrame[slot] = 0;
                animationFrameCount[slot] = *reinterpret_cast<const int*>(
                    reinterpret_cast<const unsigned char*>(this)+static_cast<int>(offsetof(VID, animationFrameCount))+firstRealAnimation*4);
            }
        }

        totalFrames = BitsToSigned32(static_cast<unsigned int>(totalFrames)+
            static_cast<unsigned int>(noAnimCadr[slot])*static_cast<unsigned int>(noDir));
        if (totalFrames>static_cast<int>(noCadr))
        {
            VidReportResourceError(this, E_ERROR, "noCadr and noAnimCadr and noDir", slot);
            animationBaseFrame[slot] = 0;
            animationFrameCount[slot] = *reinterpret_cast<const int*>(
                reinterpret_cast<const unsigned char*>(this)+static_cast<int>(offsetof(VID, animationFrameCount))+firstRealAnimation*4);
        }
    }

    if ((property&0x08u) != 0u && noGridZ == 0)
    {
        minXGridZ = maxXGridZ = minYGridZ = maxYGridZ = 0.0f;

        const int widthExtent = TruncateFloatViaCrtToInt32(sizeXYZ.x)+17;
        const int rowStride = widthExtent/8;
        const int heightExtent = TruncateFloatViaCrtToInt32(sizeXYZ.y)+17;
        const int scratchCount = (heightExtent*rowStride)/8+1;
        VECTOR* const scratch = static_cast<VECTOR*>(::operator new(VidArrayAllocationBytes(static_cast<unsigned int>(scratchCount), sizeof(VECTOR))));

        noGridZ = 0;
        for (int y = 0;static_cast<float>(y)<sizeXYZ.y;y += 8)
        {
            for (int x = 0;static_cast<float>(x)<sizeXYZ.x;x += 8)
            {
                VECTOR& point = scratch[noGridZ++];
                point.x = static_cast<float>(x)-halfSizeXY.x;
                point.y = static_cast<float>(y)-halfSizeXY.y;
                point.z = sizeXYZ.z;
            }
            VECTOR& right = scratch[noGridZ++];
            right.x = halfSizeXY.x;
            right.y = static_cast<float>(y)-halfSizeXY.y;
            right.z = sizeXYZ.z;
        }
        for (int x = 0;static_cast<float>(x)<sizeXYZ.x;x += 8)
        {
            VECTOR& bottom = scratch[noGridZ++];
            bottom.x = static_cast<float>(x)-halfSizeXY.x;
            bottom.y = halfSizeXY.y;
            bottom.z = sizeXYZ.z;
        }
        VECTOR& corner = scratch[noGridZ++];
        corner.x = halfSizeXY.x;
        corner.y = halfSizeXY.y;
        corner.z = sizeXYZ.z;

        if (gridZ) ::operator delete(gridZ);
        gridZ = static_cast<VECTOR*>(::operator new(VidArrayAllocationBytes(static_cast<unsigned int>(noGridZ), sizeof(VECTOR))));
        for (int index = 0;index<noGridZ;++index)
        {
            gridZ[index] = scratch[index];
            if (gridZ[index].x<minXGridZ) minXGridZ = gridZ[index].x;
            if (gridZ[index].x>maxXGridZ) maxXGridZ = gridZ[index].x;
            if (gridZ[index].y<minYGridZ) minYGridZ = gridZ[index].y;
            if (gridZ[index].y>maxYGridZ) maxYGridZ = gridZ[index].y;
        }
        ::operator delete(scratch);
    }
}

namespace {
static void VidReleaseFrameArray(VID_SURFACE& surface)
{
    if (surface.nextMirror != &surface) return;
    if (surface.frames)
    {
        delete[] surface.frames;
        surface.frames = 0;
    }
    surface.surfaceData = 0;
}
}

namespace
{
static unsigned int VidTextureBitsPerPixel(D3DFORMAT format)
{
    switch (format)
    {
    case D3DFMT_R8G8B8: return 24u;
    case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: return 32u;
    case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5:
    case D3DFMT_A4R4G4B4: case D3DFMT_A8L8: case D3DFMT_V8U8: return 16u;
    case D3DFMT_A8: case D3DFMT_P8: case D3DFMT_L8: return 8u;
    case D3DFMT_DXT1: return 4u;
    case D3DFMT_DXT3: case D3DFMT_DXT5: return 8u;
    default: return 0u;
    }
}

template<class T> T VidReadPixel(const unsigned char* data)
{
    T value;
    std::memcpy(&value, data, sizeof(value));
    return value;
}

template<class T> void VidWritePixel(unsigned char* data, T value)
{
    std::memcpy(data, &value, sizeof(value));
}
}

static unsigned int VidBlendBlackWhite(unsigned int color, int amount)
{
    if (amount == 0) return color;
    const int blue = static_cast<int>(color & 0xFFu);
    const int green = static_cast<int>((color >> 8) & 0xFFu);
    const int red = static_cast<int>((color >> 16) & 0xFFu);
    const int gray = (red + green + blue) / 3;
    int outputRed = red + (gray - red) * amount / 255;
    int outputGreen = green + (gray - green) * amount / 255;
    int outputBlue = blue + (gray - blue) * amount / 255;
    if (outputRed < 0) outputRed = 0;
    if (outputRed > 255) outputRed = 255;
    if (outputGreen < 0) outputGreen = 0;
    if (outputGreen > 255) outputGreen = 255;
    if (outputBlue < 0) outputBlue = 0;
    if (outputBlue > 255) outputBlue = 255;
    return (color & 0xFF000000u) |
        (static_cast<unsigned int>(outputRed) << 16) |
        (static_cast<unsigned int>(outputGreen) << 8) |
        static_cast<unsigned int>(outputBlue);
}

static unsigned short VidBlendBlackWhite4444(unsigned short pixel, int amount)
{
    if (amount == 0) return pixel;
    const unsigned int color =
        (static_cast<unsigned int>(pixel & 0xF000u) << 16) |
        (static_cast<unsigned int>(pixel & 0x0F00u) << 12) |
        (static_cast<unsigned int>(pixel & 0x00F0u) << 8) |
        (static_cast<unsigned int>(pixel & 0x000Fu) << 4);
    const unsigned int mixed = VidBlendBlackWhite(color, amount);
    return static_cast<unsigned short>(
        ((mixed >> 16) & 0xF000u) |
        ((mixed >> 12) & 0x0F00u) |
        ((mixed >> 8) & 0x00F0u) |
        ((mixed >> 4) & 0x000Fu));
}

void CADR_DATA::UploadTextureData(TEXTURE* texture, VID_SURFACE* surface)
{
    const unsigned short flags = surface->type;
    const int blackWhite = surface->blackWhite;
    const int frameHeight = static_cast<short>(height);
    RECT rect = {0, 0, static_cast<short>(width), (flags&0x2000u) != 0u ? frameHeight*2 : frameHeight};
    if ((flags&0x10u) != 0u) rect.left = 999999;
    int pitch = 0;
    unsigned char* destination = texture->Lock(&pitch, &rect);
    if (!destination)
    {
        surface->Error(E_LOCK, STRING("texture"), static_cast<unsigned long>(this-surface->frames));
        return;
    }
    const unsigned char* source = surface->surfaceData+frameDataOffset+
        indexCount*2u+(vertexCount+additionalDataCount)*4u;
    const D3DFORMAT sourceFormat = surface->pixelFormat;
    const unsigned int sourceBits = VidTextureBitsPerPixel(sourceFormat);
    const unsigned int sourcePixelBytes = sourceBits/8u+((flags&0x8000u) != 0u ? 1u : 0u)+
        ((flags&0x2000u) != 0u ? 1u : 0u)+((flags&4u) != 0u ? 2u : 0u);
    if (sourceFormat == D3DFMT_DXT1 || sourceFormat == D3DFMT_DXT3 || sourceFormat == D3DFMT_DXT5)
    {
        std::memcpy(destination, source, static_cast<unsigned int>(static_cast<short>(width))*frameHeight*sourceBits/8u);
        texture->UnLock();
        return;
    }
    const unsigned int* palette = surface->recolorPalette ? surface->recolorPalette : surface->palette;
    const bool paletteImage = (flags&8u) != 0u;
    const bool paletteExtras = (flags&0xA004u) != 0u;
    const bool palette32 = texture->m_format == D3DFMT_A8R8G8B8;
    const bool expand4444 = sourceFormat == D3DFMT_A4R4G4B4 && texture->m_format == D3DFMT_A8R8G8B8;
    const unsigned int outputBytes = paletteImage ? (palette32 ? 4u : 2u) :
        (sourceFormat == D3DFMT_A8R8G8B8 || expand4444 ? 4u : sourceFormat == D3DFMT_R8G8B8 ? 3u : 2u);
    const int secondaryPlaneOffset = frameHeight*pitch;
    while (source[0] != 0u || source[1] != 0u)
    {
        const unsigned int skip = *source++;
        if (sourceFormat != D3DFMT_R8G8B8 || paletteImage)
        {
            if (expand4444)
            {
                for (unsigned int i = 0; i < skip; ++i)
                    VidWritePixel<unsigned short>(destination+i*4u, 0u);
            }
            else
                std::memset(destination, 0, skip*outputBytes);
        }
        destination += skip*outputBytes;
        unsigned int count = *source++;
        while (count-- != 0u)
        {
            if (paletteImage)
            {
                const unsigned int color = VidBlendBlackWhite(palette[*source++], blackWhite);
                if (palette32)
                    VidWritePixel(destination, color);
                else if (!paletteExtras && texture->m_format == D3DFMT_A4R4G4B4)
                    VidWritePixel<unsigned short>(destination, static_cast<unsigned short>(
                        ((color>>16)&0xF000u)|((color>>12)&0xF00u)|((color>>8)&0xF0u)|((color>>4)&0xFu)));
                else
                    VidWritePixel<unsigned short>(destination, static_cast<unsigned short>(
                        ((color>>9)&0xFC00u)|((color>>6)&0x3E0u)|((color>>3)&0x1Fu)|0x8000u));
                if ((flags&0xA000u) == 0xA000u)
                {
                    if (palette32)
                    {
                        const unsigned int red = source[1]&0xF8u;
                        unsigned int green = (source[1]<<5u)+((source[0]>>3u)&0x18u);
                        if (green > 255u) green = 255u;
                        const unsigned int blue = (source[0]&0x3Fu)*4u;
                        VidWritePixel(destination+secondaryPlaneOffset, 0xFF000000u|(red<<16u)|(green<<8u)|blue);
                    }
                    else
                        VidWritePixel<unsigned short>(destination+secondaryPlaneOffset,
                            static_cast<unsigned short>((VidReadPixel<unsigned short>(source)>>1u)|0x8000u));
                    source += 2u;
                }
                else
                {
                    if ((flags&0x8000u) != 0u) ++source;
                    if ((flags&0x2000u) != 0u)
                    {
                        const unsigned int value = *source++;
                        if (palette32)
                        {
                            unsigned int green = value<<5u;
                            if (green > 255u) green = 255u;
                            VidWritePixel(destination+secondaryPlaneOffset,
                                0xFF000000u|((value&0xF8u)<<16u)|(green<<8u));
                        }
                        else
                            VidWritePixel<unsigned short>(destination+secondaryPlaneOffset,
                                static_cast<unsigned short>((value|0xFF00u)<<7u));
                    }
                }
                if ((flags&4u) != 0u) source += 2u;
            }
            else
            {
                if (sourceFormat == D3DFMT_A8R8G8B8)
                    VidWritePixel(destination, VidReadPixel<unsigned int>(source));
                else if (sourceFormat == D3DFMT_R8G8B8)
                    VidWritePixel(destination, VidReadPixel<unsigned int>(destination)|
                        (VidReadPixel<unsigned int>(source)&0x00FFFFFFu));
                else if (expand4444)
                {
                    const unsigned int color = VidReadPixel<unsigned short>(source);
                    const unsigned int expanded = ((color&0xF000u)<<16u)|((color&0xF00u)<<12u)|
                        ((color&0xF0u)<<8u)|((color&0xFu)<<4u);
                    VidWritePixel(destination, VidBlendBlackWhite(expanded, blackWhite));
                }
                else if (sourceFormat == D3DFMT_A4R4G4B4)
                    VidWritePixel(destination, VidBlendBlackWhite4444(
                        VidReadPixel<unsigned short>(source), blackWhite));
                else
                    VidWritePixel(destination, VidReadPixel<unsigned short>(source));
                source += sourcePixelBytes;
            }
            destination += outputBytes;
        }
    }
    texture->UnLock();
}

void __stdcall CADR_DATA::SetTextureToDevice(CADR_DATA* frame, VID_SURFACE* surface)
{
    if (frame->textureObject == 0u && frame->height != 0u)
    {
        D3DFORMAT desiredFormat = surface->pixelFormat;
        if (desiredFormat == D3DFMT_P8)
        {
            desiredFormat = ((surface->type&2u) != 0u || (Graph->capabilityFlags&0x8000u) != 0u)?D3DFMT_A8R8G8B8:D3DFMT_A1R5G5B5;
        }
        const unsigned int memoryLimit = Graph->textureMemoryMiB >= 1024u ? 256000000u :
            Graph->textureMemoryMiB >= 384u ? 192000000u : Graph->textureMemoryMiB >= 256u ? 128000000u : 90000000u;
        CADR_DATA* root = VID::lastUse;
        while (root)
        {
            CADR_DATA* const oldest = root->lruPrevious;
            const unsigned int age = ::RealCurrentTime-oldest->lastUseTick;
            if (age <= 5000u) break;
            if (VID::useTextureMemory <= memoryLimit && age <= 20000u) break;
            oldest->ReleaseVideoMemory();
            root = VID::lastUse;
        }

        D3DXImageInfo imageInfo;
        const bool fileTexture = (surface->surfaceFlags&2u) != 0u;
        void* const textureMemory = CrtNew(sizeof(TEXTURE));
        if (fileTexture)
        {
            if (textureMemory)
            {
                bool constructed = false;
                try
                {
                    const int frameIndex = static_cast<int>(frame-surface->frames);
                    STRING frameName = surface->vidName.Add(frameIndex);
                    const unsigned long colorKey = (surface->spriteClass == 19u)?0xFF000000u:0u;
                    frame->textureObject = new(textureMemory) TEXTURE(frameName, colorKey, &imageInfo);
                    constructed = true;
                }
                catch (...)
                {
                    if (!constructed) CrtFreePointer(textureMemory);
                    throw;
                }
            }
            else frame->textureObject = 0u;
            TEXTURE* const texture = frame->textureObject;
            if (texture->m_texture != 0 || texture->m_cpuData != 0)
            {
                frame->width = static_cast<unsigned short>(surface->spriteClass == 19u?
                    (imageInfo.Width>>4u)-1u:imageInfo.Width);
                frame->height = static_cast<unsigned short>(surface->spriteClass == 19u?
                    (imageInfo.Height>>4u)-1u:imageInfo.Height);
            }
        }
        else
        {
            if (textureMemory)
            {
                const int rows = static_cast<short>(frame->height)*((surface->type&0x2000u) != 0u ? 2 : 1);
                const int textureHeight = (rows+3)&~3;
                const int textureWidth = (static_cast<int>(static_cast<short>(frame->width))+3)&~3;
                try
                {
                    frame->textureObject = new(textureMemory) TEXTURE(textureWidth, textureHeight,
                        desiredFormat, 3u);
                }
                catch (...)
                {
                    CrtFreePointer(textureMemory);
                    throw;
                }
            }
            else frame->textureObject = 0u;
        }

        TEXTURE* const texture =
            frame->textureObject;
        const D3DFORMAT format = texture->m_format;
        const unsigned int pixels = texture->m_height*texture->m_width;
        unsigned int textureBytes;
        if (format == D3DFMT_DXT1) textureBytes = static_cast<unsigned int>(BitsToSigned32(pixels)/2);
        else if (format == D3DFMT_P8 || format == D3DFMT_L8) textureBytes = pixels;
        else if (format == D3DFMT_A8R8G8B8) textureBytes = pixels*4u;
        else if (format == D3DFMT_R8G8B8) textureBytes = pixels*3u;
        else textureBytes = pixels*2u;
        static_cast<void>(VID::useTextureMemory = VID::useTextureMemory+textureBytes);

        TEXTURE* const liveTexture = frame->textureObject;
        if (liveTexture->m_texture == 0u && liveTexture->m_cpuData == 0u)
        {
            STRING subject("texture");
            const int frameIndex = static_cast<int>(frame-surface->frames);
            surface->Error(E_CREATE, subject, static_cast<unsigned long>(frameIndex));
            return;
        }

        if ((surface->surfaceFlags&2u) == 0u)
        {
            frame->UploadTextureData(liveTexture, surface);
        }
    }

    if (frame->textureObject != 0u)
    {
        frame->textureObject->SetTextureToDevice();
        frame->lastUseTick = ::RealCurrentTime;
        if ((surface->surfaceFlags&1u) == 0u)
        {
            const bool linked = frame->lruPrevious != 0u;
            CADR_DATA* root = VID::lastUse;
            if (linked)
            {
                if (root == frame) static_cast<void>(VID::lastUse = frame->lruNext);
                CADR_DATA* const previous = frame->lruPrevious;
                CADR_DATA* const nextAddress = frame->lruNext;
                previous->lruNext = nextAddress;
                CADR_DATA* const next = frame->lruNext;
                CADR_DATA* const previousAddress = frame->lruPrevious;
                next->lruPrevious = previousAddress;
                frame->lruNext = 0u;
                frame->lruPrevious = 0u;
                root = VID::lastUse;
                if (root == frame) goto frameLruSelf;
            }
            if (root != 0u)
            {
                frame->lruNext = root;
                frame->lruPrevious = VID::lastUse->lruPrevious;
                frame->lruPrevious->lruNext = frame;
                frame->lruNext->lruPrevious = frame;
            }
            else
            {
frameLruSelf:
                frame->lruNext = frame;
                frame->lruPrevious = frame;
            }
            static_cast<void>(VID::lastUse = frame);
        }
    }
}



namespace {
struct VidHardwareDrawTransform
{
    VECTOR scale;
    VECTOR position;
};
static VECTOR EvaluateVidHardwareCurve(
    const WEAPON_RUNTIME* owner, const VECTOR* keys, float phase)
{

    const int index = TruncateFloatViaCrtToInt32(phase);
    if (index >= owner->maxColumn)
        return keys[owner->maxColumn];

    const float fraction = phase-static_cast<float>(index);
    const VECTOR first = keys[index];
    const VECTOR next = keys[index+1];
    const VECTOR delta = next-first;
    const VECTOR scaled = delta*fraction;
    return first+scaled;
}
static bool VidHardwareDrawOutsideRuntimeBounds(
    const VID_SURFACE* owner, const VidHardwareDrawTransform& transform)
{

    const float extentY = owner->buildSizeY*(0.5f)
        *transform.scale.y*(1.29999995f);
    const float extentX = owner->buildSizeX*(0.5f)
        *transform.scale.x*(1.29999995f);
    const float diagonal = transform.position.y-transform.position.z;

    if (transform.position.x-extentX>VID::viewMax.x &&
       diagonal-extentY>VID::viewMax.y)
        return true;

    if (!(transform.position.x+extentX >= VID::viewMin.x) &&
       !(diagonal+extentY >= VID::viewMin.y))
        return true;
    return false;
}
template<class Left, class Right>
static inline bool VidX87EqualOrUnordered(const Left& left, const Right& right)
{
    return FloatEqualOrUnordered(left, right);
}
static unsigned char SelectVidHardwareDrawDirection(
    VID_SURFACE* owner, const SPRITE* request, const float& speed, const float& zSpeed)
{

    const unsigned int ownerFlags = owner->property;
    unsigned char result = 0u;
    if ((ownerFlags&0x20000000u) == 0u) return result;

    if ((ownerFlags&0x00080000u) != 0u)
    {
        const EX_SPRITE_DATA* source =
            request->ExtraData();
        const float requestIso = request->Coor().y-request->Coor().z;
        const double sourceIso = static_cast<double>(source->cachedPosition.y)-source->cachedPosition.z;
        if (!VidX87EqualOrUnordered(source->cachedPosition.x, request->Coor().x) ||
           !VidX87EqualOrUnordered(requestIso, sourceIso))
        {
            const float deltaX = request->Coor().x-source->cachedPosition.x;
            const float deltaIso = static_cast<float>(((static_cast<double>(request->Coor().y)-request->Coor().z)-sourceIso)*1.41430664f);
            return Decart2Polar(deltaX, deltaIso).value;
        }

        if (VidX87EqualOrUnordered(zSpeed, 0.0f))
            return request->Direction().value;
        const ANGLE directionAngle(request->Direction().value);
        const float first = directionAngle.Sin()*speed;
        const float second = static_cast<float>((static_cast<double>(directionAngle.Cos())*speed
            +zSpeed)*(-1.41430664f));
        return Decart2Polar(first, second).value;
    }

    if ((ownerFlags&0x00002000u) != 0u) return request->Direction().value;

    const unsigned char stored = request->Direction().value;
    VID* directionVid = request->Vid();
    const int realDirection = directionVid->RealDirectionWithGraphDetail(stored);
    const int directionCount = owner->noDir;
    const int correction = (realDirection<<8)/directionCount;
    return static_cast<unsigned char>(stored-static_cast<unsigned char>(correction));
}

static int VidHardwareHalfTowardZero(int value)
{
    return value/2;
}
}

VID::VID()
{
    gammaRaw.modulate = 0u;
    gammaRaw.add = 0u;

    for (int i = 0;i<PlayerSlotCount;++i)
    {
        gammaByPlayer[i].modulate = 0u;
        gammaByPlayer[i].add = 0u;
    }

    sizeXYZ.x = 24.0f;
    sizeXYZ.y = 16.0f;
    sizeXYZ.z = 20.0f;
    spriteClass = 6u;
    spriteType = 0u;
    property = 0x01000000u;
    halfSizeXY.x = sizeXYZ.x*0.5f;
    halfSizeXY.y = sizeXYZ.y*0.5f;
    scaleXYZ.x = 1.0f;
    scaleXYZ.y = 1.0f;
    scaleXYZ.z = 1.0f;
    nextMirror = this;
    exchangedVid = this;
    type = 0u;
    layer = 13;
    const unsigned int originalRuntimeFlags = *reinterpret_cast<volatile unsigned int*>(&runtimeFlags);
    const unsigned int originalStorageFlags = *reinterpret_cast<volatile unsigned int*>(&storageFlags);
    runtimeFlags = (originalRuntimeFlags&0xFFFFC080u)|0x80u;
    storageFlags = (originalStorageFlags&0xFFFFFFE6u)|0x24u;
    nVid = -1;
    maxHp = 0;
    noDir = 1;
    resourceFrameParameter = 1;
    rotationSpeed = 0.02559999935328960418701171875f;
    editorTreeItem = 0u;
    directionQuantizationOffset = 0;
    frameSpeedDefault = 71u;
    nLinkVid = 0;
    linkVid = 0;
    nWeapon = 0;
    weapon = 0;
    noGridZ = 0;
    gridZ = 0;
    gridCadrShift = 0;
    cadrLinkXYZ = 0;
    sourceVid = 0;
    lastRenderUseTick = 0u;
    minXGridZ = 0.0f;
    maxXGridZ = 0.0f;
    minYGridZ = 0.0f;
    maxYGridZ = 0.0f;
    actionAuxStateRequired = 0;
    movementTactEnabled = 0;
    notCreateAsChild = 0;

    for (int i = 0;i<AnimationSlotCount;++i)
    {
        sfx[i] = 0;
        nChildVid[i] = 0;
        childVid[i] = 0;
        noAnimCadr[i] = 0;
        animationBaseFrame[i] = 0;
        animationFrameCount[i] = 0;
        frameSpeed[i] = 71;
    }

    ResetSprites();
}

VID_SOFTWARE::VID_SOFTWARE()
{
    frameOffsets = 0;
    allocatedBytes = 0u;
    mappedFileBase = 0;
}

VID_SOFTWARE::VID_SOFTWARE(VID_SOFTWARE& source) : VID()
{
    nextMirror = source.nextMirror;
    source.nextMirror = this;
    layer = source.layer;
    type = source.type;
    noCadr = source.noCadr;
    frameSpeedDefault = source.frameSpeedDefault;
    vidSizeX = source.vidSizeX;
    vidSizeY = source.vidSizeY;
    buildSizeX = source.buildSizeX;
    buildSizeY = source.buildSizeY;
    frameOffsets = source.frameOffsets;
    mappedFileBase = source.mappedFileBase;
    allocatedBytes = source.allocatedBytes;
    noGridZ = source.noGridZ;
    cadrLinkXYZ = source.cadrLinkXYZ;
    vidName = source.vidName;
    CopyBytes(paletteTables, source.paletteTables, sizeof(paletteTables));
    runtimeFlags = (runtimeFlags&~0x800u)|(source.runtimeFlags&kVidRuntimeHidden);
    storageFlags &= ~4u;

    if (noGridZ != 0)
    {
        const unsigned int bytes = static_cast<unsigned int>(noGridZ)*sizeof(VECTOR);
        gridZ = static_cast<VECTOR*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(noGridZ), sizeof(VECTOR))));
        if (noGridZ>0)
            CopyBytes(gridZ, source.gridZ, bytes);
        if (source.gridCadrShift)
        {
            const int count = static_cast<int>(noCadr);
            gridCadrShift = static_cast<int*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(count), sizeof(int))));
            if (count>0)
                CopyBytes(gridCadrShift, source.gridCadrShift,
                                 static_cast<unsigned int>(count)*sizeof(int));
        }
        else gridCadrShift = 0;
    }
}

VID_SOFTWARE::~VID_SOFTWARE()
{
    if (nextMirror == this)
    {
        if (mappedFileBase)
            (void)UnmapViewOfFile(mappedFileBase);
        mappedFileBase = 0;
        if (frameOffsets) CrtFreePointer(frameOffsets);
        frameOffsets = 0;
        static_cast<void>(VID::MemoryInUse = VID::MemoryInUse-allocatedBytes);
        allocatedBytes = 0u;
    }
}

VID_LIGHT::VID_LIGHT() : VID()
{
    lightDataBytes = 0u;
    lightData = 0;
}

VID_LIGHT::VID_LIGHT(VID_LIGHT& source) : VID()
{
    nextMirror = source.nextMirror;
    source.nextMirror = this;
    layer = source.layer;
    type = source.type;
    noCadr = source.noCadr;
    frameSpeedDefault = source.frameSpeedDefault;
    vidSizeX = source.vidSizeX;
    vidSizeY = source.vidSizeY;
    buildSizeX = source.buildSizeX;
    buildSizeY = source.buildSizeY;
    lightData = source.lightData;
    lightDataBytes = source.lightDataBytes;
    cadrLinkXYZ = source.cadrLinkXYZ;
    resourceFrameParameter = source.resourceFrameParameter;
    lastRenderUseTick = source.lastRenderUseTick;
    storageFlags &= ~4u;
}

VID_LIGHT::~VID_LIGHT()
{
    if (nextMirror == this)
    {
        if (lightData) (void)(CrtFreePointer(lightData), 0u);
        lightData = 0;
        static_cast<void>(VID::MemoryInUse = VID::MemoryInUse-lightDataBytes);
        lightDataBytes = 0u;
    }
}

VID_FONT::VID_FONT(VID_FONT& source) : VID()
{
    nextMirror = source.nextMirror;
    source.nextMirror = this;
    layer = source.layer;
    type = source.type;
    noCadr = source.noCadr;
    frameSpeedDefault = source.frameSpeedDefault;
    vidSizeX = source.vidSizeX;
    vidSizeY = source.vidSizeY;
    buildSizeX = source.buildSizeX;
    buildSizeY = source.buildSizeY;
    cadrLinkXYZ = source.cadrLinkXYZ;
    fontBackend = source.fontBackend;
    lastRenderUseTick = source.lastRenderUseTick;
    storageFlags &= ~4u;
}

VID_FONT::~VID_FONT()
{
    if (nextMirror == this && fontBackend)
    {
        (void)fontBackend->Release();
        fontBackend = 0;
    }
}

VID_SURFACE::VID_SURFACE()
{
    const unsigned int flags = surfaceFlags&~3u;
    surfaceGeometry = new SURFACE;
    surfaceTexture = 0;
    surfaceBackend = 0;
    frames = 0;
    surfaceData = 0;
    palette = 0;
    renderMode = 0u;
    recolorPalette = 0;
    surfaceMode = -1;
    surfaceFlags = flags;
}

VID_SURFACE::~VID_SURFACE()
{
    if (surfaceBackend)
    {
        (void)surfaceBackend->Release();
        surfaceBackend = 0;
    }
    if (recolorPalette)
    {
        CrtFreePointer(recolorPalette);
        recolorPalette = 0;
    }
    if (surfaceGeometry)
    {
        surfaceGeometry->Release();
        surfaceGeometry = 0;
    }
    if (nextMirror == this)
    {
        delete surfaceTexture;
        surfaceTexture = 0;
        VidReleaseFrameArray(*this);
    }
}

namespace
{
    const short kEmptySoftwareGridCell=static_cast<short>(-32000);

    static int SoftwareGridCellIndex(int x, int y)
    {

        return (y>>3)*256+(x>>3);
    }

    static void SoftwareRasterizeDepthFrame(const unsigned char* frame, short* cells)
    {
        const int headerCount = *reinterpret_cast<const short*>(frame);
        const unsigned char* runs = frame+2+headerCount*6;
        int y = *reinterpret_cast<const short*>(runs);
        const int endY = y+*reinterpret_cast<const short*>(runs+2);
        runs += 4;

        while (y<endY)
        {
            int x = 0;
            while (*reinterpret_cast<const unsigned short*>(runs) != 0u)
            {
                const unsigned int skip = runs[0];
                const unsigned int count = runs[1];
                runs += 2;
                x += static_cast<int>(skip);

                const unsigned char* pixels = runs;
                for (unsigned int i = 0;i<count;++i, ++x, pixels += 2)
                {
                    const int z = (static_cast<int>(*reinterpret_cast<const unsigned short*>(pixels))>>3)-128;
                    const int projectedY = y+z;
                    if (x >= 0 && x<2048 && projectedY >= 0 && projectedY<2048)
                    {
                        short& cell = cells[SoftwareGridCellIndex(x, projectedY)];
                        if (z>cell) cell = static_cast<short>(z);
                    }
                }

                runs += count*3u;
            }
            runs += 2;
            ++y;
        }
    }

    static void SoftwareRasterizeMaskFrame(const unsigned char* frame, short* cells)
    {
        const int headerCount = *reinterpret_cast<const short*>(frame);
        const unsigned char* runs = frame+2+headerCount*6;
        int y = *reinterpret_cast<const short*>(runs);
        const int endY = y+*reinterpret_cast<const short*>(runs+2);
        runs += 4;

        while (y<endY)
        {
            int x = 0;
            while (*reinterpret_cast<const unsigned short*>(runs) != 0u)
            {
                const unsigned int skip = *runs++;
                const unsigned int count = *runs++;
                x += static_cast<int>(skip);
                for (unsigned int i = 0;i<count;++i, ++x)
                {
                    if (x >= 0 && x<2048 && y >= 0 && y<2048)
                        cells[SoftwareGridCellIndex(x, y)] = 0;
                }
                runs += count;
            }
            runs += 2;
            ++y;
        }
    }
}


void VID_SOFTWARE::Load(RESOURCE* resource)
{

    if ((type&0x0100u) != 0u)
    {
        SetPropHide(1);
        return;
    }

    {
        STRING fileName = resource->GetFileName();
        mappedFileBase = static_cast<unsigned char*>(FFileMapping(fileName));
    }

    if (!mappedFileBase)
    {
        VidReportResourceError(this, E_CREATE, "file mapping", 0);
        SetPropHide(1);
        return;
    }

    if ((type&0x0008u) != 0u)
    {
        if (resource->GoNext(ResourceType::Palette) == 0)
        {
            const long paletteOffset = VidResourceTell(resource);
            mappedPalette = mappedFileBase+paletteOffset;
        }
        else
        {
            VidReportResourceError(this, E_LOAD, "PAL ", 0);
        }
    }

    if (resource->GoNext(ResourceType::Data) != 0)
        VidReportResourceError(this, E_LOAD, "DATA", 0);

    allocatedBytes = VidResourceSectionSize(resource);
    if ((type&0x0008u) != 0u) allocatedBytes += 2048u;

    frameOffsets = static_cast<unsigned int*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(static_cast<int>(noCadr)), sizeof(unsigned int))));
    if (!frameOffsets)
    {
        {
            STRING subject("cadrShift");
            Error(E_MEMORY, subject, static_cast<unsigned long>(static_cast<int>(noCadr)));
        }
        return;
    }

    if ((type&0x0008u) != 0u)
    {
        for (unsigned int offset = 0;offset<1024u;offset += 4u)
            *reinterpret_cast<unsigned int*>(paletteTables+offset) =
                *reinterpret_cast<const unsigned int*>(mappedPalette+offset);
        __movsd(reinterpret_cast<unsigned long*>(paletteTables+8192u),
                reinterpret_cast<const unsigned long*>(paletteTables), 256u);
    }

    unsigned int frameBytes;
    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        (void)resource->Read(&frameBytes, sizeof(frameBytes));

        if (frameBytes != 2u)
        {
            frameOffsets[frameIndex] = static_cast<unsigned int>(VidResourceTell(resource));
        }
        else
        {
            const long frameOffset = VidResourceTell(resource);
            const short linkedFrame = *reinterpret_cast<const short*>(mappedFileBase+frameOffset);
            frameOffsets[frameIndex] = frameOffsets[linkedFrame];
        }

        VidSkipResourceBytes(resource, static_cast<long>(frameBytes));

        if (VidResourceBytesToEndOfSubrecord(resource) >= 12)
        {
            if (!cadrLinkXYZ)
                cadrLinkXYZ = static_cast<VECTOR*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(static_cast<int>(noCadr)), sizeof(VECTOR))));
            VECTOR* const frameLink = cadrLinkXYZ+frameIndex;
            (void)resource->Read(&frameLink->x, 4u);
            (void)resource->Read(&frameLink->y, 4u);
            (void)resource->Read(&frameLink->z, 4u);
        }

        (void)resource->GoNextSub(ResourceType::Data);
    }

    static_cast<void>(VID::MemoryInUse = VID::MemoryInUse+allocatedBytes);

    SetLayer();

    if ((property&0x20u) == 0u || (type&0x0008u) == 0u || (type&0x0001u) == 0u)
        return;

    minXGridZ = maxXGridZ = minYGridZ = maxYGridZ = 0.0f;

    VECTOR* const scratch = static_cast<VECTOR*>(CrtNew(50331648u));

    gridCadrShift = static_cast<int*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(static_cast<int>(noCadr)), sizeof(int))));

    short cells[256*256];
    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        for (int i = 0;i<256*256;++i) cells[i] = kEmptySoftwareGridCell;
        gridCadrShift[frameIndex] = noGridZ;

        const unsigned char* const frame = mappedFileBase+frameOffsets[frameIndex];
        if ((type&0x0004u) != 0u) SoftwareRasterizeDepthFrame(frame, cells);
        else SoftwareRasterizeMaskFrame(frame, cells);

        const int rows = static_cast<int>(vidSizeY)/8;
        const int columns = static_cast<int>(vidSizeX)/8;
        for (int row = rows-1;row >= 0;--row)
        {
            for (int column = 0;column<columns;++column)
            {
                const short z = cells[row*256+column];
                if (z == kEmptySoftwareGridCell) continue;

                VECTOR& point = scratch[noGridZ++];
                point.x = static_cast<float>(column*8-static_cast<int>(vidSizeX)/2);
                point.y = static_cast<float>(row*8-static_cast<int>(vidSizeY)/2);
                point.z = static_cast<float>(z);
            }
        }
    }

    gridZ = static_cast<VECTOR*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(noGridZ), sizeof(VECTOR))));
    for (int i = 0;i<noGridZ;++i)
    {
        gridZ[i] = scratch[i];
        if (gridZ[i].x<minXGridZ) minXGridZ = gridZ[i].x;
        if (gridZ[i].x>maxXGridZ) maxXGridZ = gridZ[i].x;
        if (gridZ[i].y<minYGridZ) minYGridZ = gridZ[i].y;
        if (gridZ[i].y>maxYGridZ) maxYGridZ = gridZ[i].y;
    }

    CrtFreePointer(scratch);
}


void VID_LIGHT::Load(RESOURCE* resource)
{
    if (resource->GoNext(ResourceType::Data) != 0)
    {
        const STRING text("DATA");
        Error(E_LOAD, text, 0u);
    }

    vidSizeX = static_cast<short>(TruncateFloatViaCrtToInt32(sizeXYZ.x));
    vidSizeY = static_cast<short>(TruncateFloatViaCrtToInt32(sizeXYZ.y));
    buildSizeX = sizeXYZ.x;
    buildSizeY = sizeXYZ.y;

    lightDataBytes = static_cast<unsigned int>(resource->SubLoad(&lightData));
    if (lightDataBytes == 0u)
    {
        const STRING text("cadr");
        Error(E_LOAD, text, 0u);
    }
    static_cast<void>(VID::MemoryInUse = VID::MemoryInUse+lightDataBytes);
}


void VID_LIGHT::SetLayer()
{
    layer = 7;
}

void VID_LIGHT::Draw(SPRITE* sprite)
{

    const GAMMA spriteGamma = sprite->GetGamma();
    GAMMA vidGamma;
    vidGamma.modulate = gammaRaw.modulate;
    vidGamma.add = gammaRaw.add;
    GAMMA finalGamma(vidGamma, spriteGamma);
    if ((property&0x800u) == 0u)
    {
        GAMMA graphGamma;
        graphGamma.modulate = Graph->gamma.modulate;
        graphGamma.add = Graph->gamma.add;
        finalGamma = GAMMA(graphGamma, finalGamma);
    }

    const int frameIndex = sprite->CurrentFrame();
    const unsigned int frameColor = reinterpret_cast<const unsigned int*>(lightData)[frameIndex];
    if ((runtimeFlags&kVidRuntimeHidden) != 0u || (frameColor&0x00FFFFFFu) == 0u)
        return;

    VECTOR drawScale;
    EX_SPRITE_DATA* const transformOwner = sprite->ExtraData();
    if (transformOwner)
        drawScale = transformOwner->scale;
    else
    {
        drawScale.x = 1.0f;
        drawScale.y = 1.0f;
        drawScale.z = 1.0f;
    }

    VECTOR2 halfSize;
    halfSize.x = sizeXYZ.x*0.5f*scaleXYZ.x*drawScale.x;
    halfSize.y = sizeXYZ.y*0.5f*scaleXYZ.y*drawScale.y;

    VECTOR position;
    position = sprite->Coor();

    if ((runtimeFlags&kVidAnimateScale) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveScale = EvaluateVidHardwareCurve(curveOwner, curveOwner->Scale, phase);
        halfSize.x *= curveScale.x;
        halfSize.y *= curveScale.y;
    }

    if ((runtimeFlags&kVidAnimateOffset) != 0u)
    {
        const WEAPON_RUNTIME* const curveOwner =
            weapon;
        const float phase = transformOwner->effectCurvePosition;
        const VECTOR curveOffset = EvaluateVidHardwareCurve(curveOwner, curveOwner->Shift, phase);
        position.x += curveOffset.x;
        position.y += curveOffset.y;
        position.z += curveOffset.z;
    }

    if ((property&kVidApplyDirectionOffset) != 0u)
    {
        const unsigned int waveIndex = (::CurrentTime>>3u)&0xFFu;
        position.z += FSin[waveIndex]*topZ;
    }

    COLOR sourceColor;
    sourceColor.value = frameColor;
    const COLOR drawColor(finalGamma, sourceColor);
    Graph->DrawLight(position, halfSize, drawColor, sprite->Direction(), property&0x1000u);
}


void VID_FONT::InvalidateDeviceObjects()
{
    if (fontBackend)
        (void)fontBackend->OnLostDevice();
}


void VID_FONT::RestoreDeviceObjects()
{
    if (fontBackend)
        (void)fontBackend->OnResetDevice();
}


void VID_FONT::SetLayer()
{
    layer = 11;
}


void VID_FONT::Draw(SPRITE* sprite)
{
    if (!fontBackend) return;

    const GAMMA gamma = sprite->GetGamma();
    const unsigned int gammaAdd = gamma.add;
    unsigned int color;
    if ((gammaAdd&0xFF000000u) != 0u)
        color = gammaAdd;
    else if ((gammaAdd&0x00FFFFFFu) != 0u)
        color = gammaAdd|0xFF000000u;
    else
        color = 0xFFFFFFFFu;

    VECTOR2 projected(sprite->Coor().x, sprite->Coor().y-sprite->Coor().z);
    VECTOR2 shifted = projected-Map->ShiftXY();
    const int x = TruncateFloatViaCrtToInt32(shifted.x);
    projected.x = sprite->Coor().x;
    projected.y = sprite->Coor().y-sprite->Coor().z;
    shifted = projected-Map->ShiftXY();
    int y = TruncateFloatViaCrtToInt32(shifted.y);

    if ((property&kVidApplyDirectionOffset) != 0u)
    {
        const unsigned int index = (::CurrentTime>>3)&0xFFu;
        const float wave = FSin[index];
        y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(TruncateDoubleViaCrtToInt32(static_cast<double>(wave)*topZ)));
    }

    if (sprite == Mouse)
        y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(static_cast<int>(vidSizeY)/2));
    else
    {
        const unsigned int behavior = sprite->Behave();
        if ((behavior&0x08u) != 0u)
            y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(static_cast<int>(vidSizeY)/2));
        else if ((behavior&0x04u) != 0u)
            y = BitsToSigned32(Signed32ToBits(y)-Signed32ToBits(static_cast<int>(vidSizeY)));
    }

    RECT rect = {x, y, BitsToSigned32(Signed32ToBits(x)+1u), BitsToSigned32(Signed32ToBits(y)+1u)};
    GRAPH* const video = Graph;
    if (!(video->screenWidth >= static_cast<float>(x)) || !(video->screenHeight >= static_cast<float>(y))) return;
    if (BitsToSigned32(Signed32ToBits(x)+6u*Signed32ToBits(static_cast<int>(vidSizeX))+10u)<0 ||
       BitsToSigned32(Signed32ToBits(y)+Signed32ToBits(static_cast<int>(vidSizeY))+10u)<0) return;

    IDirect3DDevice9* const device = video->device;
    (void)device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
    (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);
    (void)fontBackend->DrawTextA(
        0, "abcABC", 6, &rect, (DT_NOCLIP|DT_SINGLELINE), color);
}

void VID_SURFACE::Load(RESOURCE* resource)
{


    if (nextMirror == this)
    {
        VidReleaseFrameArray(*this);
        surfaceData = 0;
    }

    (void)resource->Read(&pixelFormat, sizeof(pixelFormat));
    (void)resource->Read(&surfaceMode, sizeof(surfaceMode));
    if ((surfaceMode == 0 || surfaceMode == -1) &&
        std::strstr(vidName.value, "\\101.") == 0) surfaceMode = 1;
    if (VidResourceBytesToEndOfSubrecord(resource) != 0)
        (void)resource->Read(&renderMode, sizeof(renderMode));

    storageFlags |= 2u;
    if (pixelFormat == D3DFMT_UNKNOWN) return;

    const int frameCount = static_cast<int>(noCadr);
    frames = new(std::nothrow) CADR_DATA[frameCount];

    unsigned int cachedOffset = frames?PtrToU32(frames)-4u:0u;

    struct CachedSurfaceHeader
    {
        unsigned int key0;
        unsigned int key1;
    };
    if (VidResourceBytesToEndOfSubrecord(resource) != 0)
    {
        CachedSurfaceHeader requested;
        (void)resource->Read(&cachedOffset, sizeof(cachedOffset));
        (void)resource->Read(&requested, sizeof(requested));
        if (VidResourceBytesToEndOfSubrecord(resource) != 0)
        {
            int sourceDirections = 0;
            (void)resource->Read(&sourceDirections, 4u);
            if (sourceDirections > 0 && noDir == 0) noDir = sourceDirections;
            (void)resource->Read(&resourceFrameParameter, 4u);
            VidSkipResourceBytes(resource, 128);
            VidSkipResourceBytes(resource, 128);
            VidSkipResourceBytes(resource, 128);
            storageFlags &= ~2u;
        }
        const CachedSurfaceHeader* const available =
            reinterpret_cast<const CachedSurfaceHeader*>(Map->m_fastLoadIndex);

        if (available && requested.key0 == available->key0 && requested.key1 == available->key1)
        {
            type = static_cast<unsigned short>(type&0x5FFBu);
            storageFlags|=8u;
            surfaceData = Map->m_fastLoadData;

            const unsigned char* cursor =
                Map->m_fastLoadIndex+cachedOffset;
            for (int i = 0;i<static_cast<int>(noCadr);++i)
            {
                CADR_DATA& frame = frames[i];
                const auto readField = [&cursor](void* field, unsigned int size)
                {
                    std::memcpy(field, cursor, size);
                    cursor += size;
                };
                readField(&frame.indexCount, 2u);
                if ((type&0x0800u) != 0u) readField(&frame.additionalDataCount, 2u);
                readField(&frame.vertexCount, 2u);
                readField(&frame.width, 2u);
                readField(&frame.height, 2u);
                readField(&frame.offsetX, 2u);
                readField(&frame.offsetY, 2u);
                readField(&frame.frameDataOffset, 4u);
                readField(&frame.minZ, 4u);
                readField(&frame.maxZ, 4u);
                if (i != 0)
                {
                    frame.baseIndex = frames[i-1].baseIndex+frames[i-1].indexCount;
                    frame.baseVertex = frames[i-1].baseVertex+frames[i-1].vertexCount;
                }
            }

            minXGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;
            CADR_DATA* const firstFrameRecord = frames;
            maxXGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;
            minYGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;
            maxYGridZ = *reinterpret_cast<const float*>(cursor);
            cursor+=4;

            unsigned char* const firstFrame = surfaceData+firstFrameRecord[0].frameDataOffset;
            const unsigned int packedGridCount =
                *reinterpret_cast<const unsigned int*>(firstFrame-sizeof(unsigned int));
            unsigned int prefixBytes = sizeof(unsigned int);
            if (packedGridCount != 0u)
                prefixBytes = (packedGridCount*3u+static_cast<unsigned int>(static_cast<int>(noCadr)))*4u+4u;

            const unsigned short cachedType = type;
            if ((cachedType&0x0008u) != 0u)
            {
                prefixBytes+=1024u;
                palette = reinterpret_cast<unsigned int*>(firstFrame-prefixBytes);
            }

            if ((cachedType&0x0200u) != 0u)
            {
                unsigned char* linkBase = firstFrame-prefixBytes;
                linkBase-=static_cast<unsigned int>(static_cast<int>(noCadr))*sizeof(VECTOR);
                cadrLinkXYZ = reinterpret_cast<VECTOR*>(linkBase);
            }
            return;
        }
    }
    else if (VidResourceBytesToEndOfSubrecord(resource) == 0)
    {
        VidReportResourceError(this, E_ERROR,
            "old format vid. Can't create fastLoad", 0);
    }

    {
        STRING fileName = resource->GetFileName();
        surfaceData = static_cast<unsigned char*>(FFileMapping(fileName));
    }
    if (!surfaceData)
    {
        VidReportResourceError(this, E_CREATE, "file mapping", 0);
        return;
    }

    if ((type&0x0008u) != 0u)
    {
        if (resource->GoNext(ResourceType::Palette) == 0)
        {
            const long paletteOffset = VidResourceTell(resource);
            palette = reinterpret_cast<unsigned int*>(surfaceData+paletteOffset);
        }
        else
        {
            VidReportResourceError(this, E_LOAD, "PAL ", 0);
            return;
        }
    }

    (void)resource->GoNext(ResourceType::Any);

    if ((type&0x0200u) != 0u)
    {
        const int linkCount = static_cast<int>(noCadr);
        cadrLinkXYZ = static_cast<VECTOR*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(linkCount), sizeof(VECTOR))));
    }

    if (resource->RecordType() != ResourceType::Frame)
        (void)resource->GoNext(ResourceType::Frame);
    if (resource->RecordType() != ResourceType::Frame)
    {
        VidReportResourceError(this, E_LOAD, "CADR", 0);
        goto surfaceFrameBounds;
    }

    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        if (VidResourceBytesToEndOfSubrecord(resource)>4)
        {
            if ((type&0x0200u) != 0u)
            {
                VECTOR* const frameLink = cadrLinkXYZ+frameIndex;
                (void)resource->Read(&frameLink->x, 4u);
                (void)resource->Read(&frameLink->y, 4u);
                (void)resource->Read(&frameLink->z, 4u);
            }

            (void)resource->Read(&frames[frameIndex].indexCount, 2u);
            if ((type&0x0800u) != 0u)
                (void)resource->Read(&frames[frameIndex].additionalDataCount, 2u);
            (void)resource->Read(&frames[frameIndex].vertexCount, 2u);
            (void)resource->Read(&frames[frameIndex].width, 2u);
            (void)resource->Read(&frames[frameIndex].height, 2u);
            (void)resource->Read(&frames[frameIndex].offsetX, 2u);
            (void)resource->Read(&frames[frameIndex].offsetY, 2u);
            const unsigned int streamPosition = static_cast<unsigned int>(VidResourceTell(resource));
            frames[frameIndex].frameDataOffset = streamPosition;
            if (frameIndex != 0)
            {
                frames[frameIndex].baseIndex = frames[frameIndex-1].baseIndex+frames[frameIndex-1].indexCount;
                frames[frameIndex].baseVertex = frames[frameIndex-1].baseVertex+frames[frameIndex-1].vertexCount;
            }
        }
        else
        {
            const long linkOffset = VidResourceTell(resource);
            const int linkedIndex = *reinterpret_cast<const int*>(surfaceData+linkOffset);
            CADR_DATA& frame = frames[frameIndex];
            CADR_DATA& linked = frames[linkedIndex];
            if (&linked != &frame)
            {
                frame.lruNext = 0u;
                frame.lruPrevious = 0u;
                frame.lastUseTick = linked.lastUseTick;
                frame.indexCount = linked.indexCount;
                frame.additionalDataCount = linked.additionalDataCount;
                frame.baseIndex = linked.baseIndex;
                frame.baseVertex = linked.baseVertex;
                frame.geometryParameters[0] = linked.geometryParameters[0];
                frame.geometryParameters[1] = linked.geometryParameters[1];
                frame.vertexCount = linked.vertexCount;
                frame.width = linked.width;
                frame.height = linked.height;
                frame.offsetX = linked.offsetX;
                frame.offsetY = linked.offsetY;
                frame.frameDataOffset = linked.frameDataOffset;
                std::memcpy(&frame.maxZ, &linked.maxZ, sizeof(float));
                frame.textureObject = linked.textureObject;
                frame.vertexBuffer = linked.vertexBuffer;
                frame.indexBuffer = linked.indexBuffer;
            }
        }

        if (frameIndex<static_cast<int>(noCadr)-1)
            (void)resource->GoNextSub(ResourceType::Frame);
    }

surfaceFrameBounds:
    if ((property&0x1000u) != 0u && frames)
        frames[0].CalcMaxMinZ(this);
}


void VID_SURFACE::SetReColorForArmy(COLOR color)
{
    if ((type&8u) == 0u)
    {
        const STRING text("SetReColorForArmy for non paletted vid");
        Error(E_ERROR, text, 0u);
        return;
    }

    if (!recolorPalette)
        recolorPalette = static_cast<unsigned int*>(CrtNew(1024u));

    for (unsigned int i = 0;i<256u;++i)
    {
        const unsigned int sourceColor = palette[i];
        const int red = static_cast<int>((sourceColor>>16u)&0xFFu);
        const int green = static_cast<int>((sourceColor>>8u)&0xFFu);
        const int blue = static_cast<int>(sourceColor&0xFFu);
        const volatile float ramp = static_cast<float>(static_cast<double>(red)/148.0);

        if (red>20 &&
           green <= TruncateDoubleToInt32(static_cast<double>(ramp)*80.0) &&
           (red >= blue?red-blue:blue-red) <= TruncateDoubleViaCrtToInt32(static_cast<double>(ramp)*30.0))
        {
            const float recolorScale = static_cast<float>(red)*(1.0f/128.0f);
            const int scaledBlue = TruncateDoubleViaCrtToInt32(static_cast<double>(color.value&0xFFu)*recolorScale);
            const int scaledGreen = TruncateDoubleViaCrtToInt32(static_cast<double>((color.value>>8u)&0xFFu)*recolorScale);
            const int scaledRed = TruncateDoubleViaCrtToInt32(static_cast<double>((color.value>>16u)&0xFFu)*recolorScale);
            const unsigned int tint = 0xFF000000u|
                (static_cast<unsigned int>(ClampByteToRange(scaledRed))<<16u)|
                (static_cast<unsigned int>(ClampByteToRange(scaledGreen))<<8u)|
                static_cast<unsigned int>(ClampByteToRange(scaledBlue));

            const unsigned int gray = static_cast<unsigned int>(green);
            const unsigned int addBlue = (gray+(tint&0xFFu)>0xFFu)?0xFFu:gray+(tint&0xFFu);
            const unsigned int addGreen = ((gray<<8u)+(tint&0xFF00u)>0xFF00u)?0xFF00u:(gray<<8u)+(tint&0xFF00u);
            const unsigned int addRed = ((gray<<16u)+(tint&0xFF0000u)>0xFF0000u)?0xFF0000u:(gray<<16u)+(tint&0xFF0000u);
            recolorPalette[i] = 0xFF000000u|addRed|addGreen|addBlue;
        }
        else
        {
            recolorPalette[i] = sourceColor;
        }
    }

    ReleaseVideoMemory();

    if (nextMirror != this)
    {
        VID* predecessor = nextMirror;
        while (predecessor->nextMirror != this) predecessor = predecessor->nextMirror;
        predecessor->nextMirror = nextMirror;

        CADR_DATA* const oldFrames = frames;
        const int frameCount = static_cast<int>(noCadr);
        nextMirror = this;
        frames = new(std::nothrow) CADR_DATA[frameCount];
        for (int i = 0;i<frameCount;++i)
        {
            CADR_DATA& dst = frames[i];
            const CADR_DATA& src = oldFrames[i];
            dst.lastUseTick = src.lastUseTick;
            dst.lruPrevious = 0u;
            dst.lruNext = 0u;
            dst.indexCount = src.indexCount;
            dst.additionalDataCount = src.additionalDataCount;
            dst.baseIndex = src.baseIndex;
            dst.baseVertex = src.baseVertex;
            dst.geometryParameters[0] = src.geometryParameters[0];
            dst.geometryParameters[1] = src.geometryParameters[1];
            dst.vertexCount = src.vertexCount;
            dst.width = src.width;
            dst.height = src.height;
            dst.offsetX = src.offsetX;
            dst.offsetY = src.offsetY;
            dst.frameDataOffset = src.frameDataOffset;
            std::memcpy(&dst.maxZ, &src.maxZ, sizeof(float));
            dst.textureObject = src.textureObject;
            dst.vertexBuffer = src.vertexBuffer;
            dst.indexBuffer = src.indexBuffer;
        }

        if (cadrLinkXYZ && (storageFlags&8u) == 0u)
        {
            VECTOR* const oldLinks = cadrLinkXYZ;
            cadrLinkXYZ = static_cast<VECTOR*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(frameCount), sizeof(VECTOR))));
            for (int i = 0;i<frameCount;++i) cadrLinkXYZ[i] = oldLinks[i];
        }
        return;
    }

}


void VID_SURFACE::InvalidateDeviceObjects()
{
    if ((storageFlags&4u) != 0u && surfaceBackend)
    {
        (void)surfaceBackend->Release();
        surfaceBackend = 0;
    }
    if (surfaceTexture) surfaceTexture->ReleaseVideoMemory();
    surfaceTexture = 0;
    ReleaseVideoMemory();
}

void VID::DrawAuxiliaryPass(const SPRITE*)
{
}

void VID::DrawFlagmanBehindWalls(const SPRITE*)
{
}

void VID::DrawMesh(const SPRITE*)
{
}

namespace
{
static GAMMA VidSurfaceDrawGamma(const VID_SURFACE& vid, const SPRITE& sprite)
{
    GAMMA color;
    color.modulate = vid.gammaRaw.modulate;
    color.add = vid.gammaRaw.add;
    GAMMA result(color, sprite.GetGamma());
    if (vid.layer >= 7 && (vid.property&0x800u) == 0u)
    {
        GAMMA graphColor;
        graphColor.modulate = Graph->gamma.modulate;
        graphColor.add = Graph->gamma.add;
        result = GAMMA(graphColor, result);
    }
    return result;
}

static void VidSurfaceSetEffectGamma(const GAMMA& gamma)
{
    const unsigned int diffuse = ~gamma.modulate;
    const unsigned int specular = gamma.add&0x00FFFFFFu;
    const double unit = 0.0039215688593685626983642578125;
    const float diffuseValue[4] = {
        static_cast<float>(((diffuse>>16)&255u)*unit),
        static_cast<float>(((diffuse>>8)&255u)*unit),
        static_cast<float>((diffuse&255u)*unit),
        static_cast<float>((diffuse>>24)*unit)
    };
    const float specularValue[4] = {
        static_cast<float>(((specular>>16)&255u)*unit),
        static_cast<float>(((specular>>8)&255u)*unit),
        static_cast<float>((specular&255u)*unit), 0.0f
    };
    (void)Graph->pseudo3DEffect->SetVector(Graph->diffuseParameter,
        reinterpret_cast<const D3DXVECTOR4*>(diffuseValue));
    (void)Graph->pseudo3DEffect->SetVector(Graph->specularParameter,
        reinterpret_cast<const D3DXVECTOR4*>(specularValue));
}

static void VidSurfaceSetMaterial(const GAMMA& gamma)
{
    if (Graph->vertexShaderVersion < 0x100u)
    {
        (void)Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectWithoutShaderTechnique);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, D3DTA_TFACTOR);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
        (void)Graph->device->SetRenderState(D3DRS_TEXTUREFACTOR, ~gamma.modulate);
        (void)Graph->device->SetRenderState(D3DRS_SPECULARENABLE, 0u);
    }
    else
    {
        (void)Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectTechnique);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        (void)Graph->device->SetRenderState(D3DRS_SPECULARENABLE, gamma.add != 0u ? 1u : 0u);
        VidSurfaceSetEffectGamma(gamma);
    }
}
}

void VID_SURFACE::EnsureGeometry()
{
    if (!frames || (surfaceGeometry && surfaceGeometry->vertexBuffer)) return;
    if (!surfaceGeometry) surfaceGeometry = new SURFACE;
    if (frames[0].indexCount != 0u && surfaceMode == 3)
        (void)surfaceGeometry->CreateFromVid(*this);
    else
        surfaceGeometry->CreateQuad();
}

CADR_DATA* VID_SURFACE::PrepareFrame(const SPRITE* request, RECT* screenRect)
{
    if (request->CurrentFrame() >= noCadr)
    {
        request->Error(E_INVALID, STRING("cadr for Draw"), request->CurrentFrame());
        return 0;
    }
    int frameIndex = request->CurrentFrame();
    if ((property&0x20000000u) != 0u)
    {
        const int animation = request->Animation();
        frameIndex = animationBaseFrame[animation]+
            request->Vid()->RealDirectionWithGraphDetail(request->Direction().value)*animationFrameCount[animation]
            +request->CurrentFrame()-request->m_currentFrameBegin;
    }
    CADR_DATA* const frame = frames+frameIndex;
    if (frame->height == 0u) return 0;
    VidHardwareDrawTransform transform;
    if (request->ExtraData())
        transform.scale = request->ExtraData()->scale;
    else
    {
        transform.scale.x = 1.0f;
        transform.scale.y = 1.0f;
        transform.scale.z = 1.0f;
    }
    transform.scale.x *= scaleXYZ.x;
    transform.scale.y *= scaleXYZ.y;
    transform.scale.z *= scaleXYZ.z;
    transform.position.x = request->Coor().x;
    transform.position.y = request->Coor().y;
    transform.position.z = request->Coor().z;

    if ((property&0x00200000u) != 0u)
    {
        const EX_SPRITE_DATA* source = request->ExtraData();
        const volatile float cachedIso = source->cachedPosition.y-source->cachedPosition.z;
        const volatile float requestIso = request->Coor().y-request->Coor().z;
        const VECTOR2 delta(request->Coor().x-source->cachedPosition.x, requestIso-cachedIso);
        const float distance = delta.NearLength();
        const int sourceFrame = static_cast<int>(request->CurrentFrame())-static_cast<int>(request->m_currentFrameBegin);
        const volatile float ratio = static_cast<float>(static_cast<double>(distance)/static_cast<short>(frames[sourceFrame].height));
        if (ratio>(1.0f))
        {
            transform.scale.y = ratio;
            const volatile float sumX = request->Coor().x+source->cachedPosition.x;
            const volatile float sumY = request->Coor().y+source->cachedPosition.y;
            const volatile float sumZ = request->Coor().z+source->cachedPosition.z;
            transform.position.x = static_cast<float>(static_cast<double>(sumX)*0.5);
            transform.position.y = static_cast<float>(static_cast<double>(sumY)*0.5);
            transform.position.z = static_cast<float>(static_cast<double>(sumZ)*0.5);
        }
    }
    if ((property&kVidApplyPixelCenterOffset) != 0u)
    {
        const float pixelCenterOffset = (1000.0f);
        transform.position.y += pixelCenterOffset;
        transform.position.z += pixelCenterOffset;
    }
    if ((property&kVidApplyDirectionOffset) != 0u)
    {
        const unsigned int i = (::CurrentTime>>3u)&0xFFu;
        transform.position.z += FSin[i]*topZ;
    }
    if ((runtimeFlags&(kVidAnimateScale|kVidAnimateOffset)) != 0u)
    {
        const EX_SPRITE_DATA* source = request->ExtraData();
        const WEAPON_RUNTIME* curves = weapon;
        if ((runtimeFlags&kVidAnimateScale) != 0u)
        {
            const VECTOR v = EvaluateVidHardwareCurve(curves, curves->Scale, source->effectCurvePosition);
            transform.scale.x *= v.x;
            transform.scale.y *= v.y;
            transform.scale.z *= v.z;
        }
        if ((runtimeFlags&kVidAnimateOffset) != 0u)
        {
            const VECTOR v = EvaluateVidHardwareCurve(curves, curves->Shift, source->effectCurvePosition);
            transform.position.x += v.x;
            transform.position.y += v.y;
            transform.position.z += v.z;
        }
    }

    const float extentY = buildSizeY*(0.5f)*transform.scale.y*(1.29999995f);
    const float extentX = buildSizeX*(0.5f)*transform.scale.x*(1.29999995f);
    if (VidHardwareDrawOutsideRuntimeBounds(this, transform)) return 0;

    if ((property&0x1000u) != 0u && renderMode == 4u)
        Graph->DrawShadowToStencil(request->Coor(), VECTOR2(extentX, extentY));
    unsigned char direction = SelectVidHardwareDrawDirection(this, request, request->m_speed, request->m_zSpeed);
    EnsureGeometry();
    CADR_DATA::SetTextureToDevice(frame, this);
    if (screenRect && surfaceMode == 6)
    {
        const float scaledWidth = static_cast<float>(static_cast<short>(frame->width))*transform.scale.x;
        const float scaledHeight = static_cast<float>(static_cast<short>(frame->height))*transform.scale.y;
        const float expandedWidth = static_cast<float>(static_cast<double>(scaledWidth)*1.2999999523162842);
        const float expandedHeight = static_cast<float>(static_cast<double>(scaledHeight)*1.2999999523162842);
        float halfWidth = static_cast<float>(static_cast<double>(expandedWidth)*0.5);
        float halfHeight = static_cast<float>(static_cast<double>(expandedHeight)*0.5);
        if (halfWidth > 128.0f) halfWidth = 128.0f;
        if (halfHeight > 128.0f) halfHeight = 128.0f;
        const float projectedY = transform.position.y-transform.position.z;
        const float x = transform.position.x-Map->ShiftXY().x;
        const float y = projectedY-Map->ShiftXY().y;
        screenRect->left = TruncateFloatViaCrtToInt32(x-halfWidth);
        screenRect->top = TruncateFloatViaCrtToInt32(y-halfHeight);
        screenRect->right = TruncateFloatViaCrtToInt32(x+halfWidth);
        screenRect->bottom = TruncateFloatViaCrtToInt32(y+halfHeight);
    }
    transform.position.x -= Map->ShiftXY().x;
    transform.position.y -= Map->ShiftXY().y;
    if (surfaceMode == 0 || (surfaceMode == -1 && sizeXYZ.y >= sizeXYZ.z)) direction = 0u;
    transform.position.y = static_cast<float>(static_cast<double>(transform.position.y)/0.7070602178573608);
    if (renderMode == 4u &&
        (static_cast<float>(Graph->lightRenderTexture->m_width) != Graph->screenWidth ||
         static_cast<float>(Graph->lightRenderTexture->m_height) != Graph->screenHeight))
    {
        const float ratioX = static_cast<float>(Graph->lightRenderTexture->m_width)/Graph->screenWidth;
        const float ratioY = static_cast<float>(Graph->lightRenderTexture->m_height)/Graph->screenHeight;
        transform.scale.x *= ratioX;
        transform.scale.y *= ratioY;
        transform.position.x *= ratioX;
        transform.position.y *= ratioY;
        transform.position.z *= ratioY;
    }
    transform.position.x -= (Graph->viewLeft+Graph->viewRight)*0.5f;
    transform.position.y -= static_cast<float>(static_cast<double>(Graph->viewTop+Graph->viewBottom)/1.4141204357147217);
    const ANGLE rotation(direction);
    const float matrix[16] = {
        rotation.Cos()*transform.scale.x, rotation.Sin()*transform.scale.x, 0.0f, 0.0f,
        -rotation.Sin()*transform.scale.y, rotation.Cos()*transform.scale.y, 0.0f, 0.0f,
        0.0f, 0.0f, transform.scale.z, 0.0f,
        transform.position.x, transform.position.y, transform.position.z, 1.0f
    };
    Graph->SetTransformMatrix(D3DTS_WORLD, reinterpret_cast<const D3DMATRIX*>(matrix), 0);
    const bool scaled = transform.scale.x != 1.0f || transform.scale.y != 1.0f || transform.scale.z != 1.0f;
    const unsigned int filter = ((type&2u) != 0u || (renderMode != 0u && renderMode != 6u)) && scaled
        ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    (void)Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, filter);
    (void)Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, filter);
    return frame;
}


void VID_SURFACE::SetFrameParameters(const SPRITE* sprite, const CADR_DATA& frame,
    const GAMMA* gamma) const
{
    const int halfX = VidHardwareHalfTowardZero(vidSizeX);
    const int halfY = VidHardwareHalfTowardZero(vidSizeY);
    float shift[2] = {
        static_cast<float>(halfX-static_cast<short>(frame.offsetX))+0.5f,
        static_cast<float>(halfY-static_cast<short>(frame.offsetY))+0.5f
    };
    const float size[2] = {
        static_cast<float>(frame.textureObject->m_width),
        static_cast<float>(frame.textureObject->m_height)
    };
    if (spriteClass == 19u)
    {
        shift[0] += (sprite->CurrentFrame()%16)*(static_cast<float>(static_cast<short>(frame.width))+1.0f);
        shift[1] += (sprite->CurrentFrame()/16)*(static_cast<float>(static_cast<short>(frame.height))+1.0f);
    }
    (void)Graph->pseudo3DEffect->SetValue(Graph->texShiftParameter, shift, sizeof(shift));
    (void)Graph->pseudo3DEffect->SetValue(Graph->texSizeParameter, size, sizeof(size));
    if (gamma) VidSurfaceSetEffectGamma(*gamma);
}

void VID_SURFACE::DrawFrame(const CADR_DATA& frame) const
{
    if (surfaceMode == 3)
    {
        surfaceGeometry->DrawWithEffect(frame.baseVertex, frame.vertexCount,
            frame.baseIndex, frame.indexCount);
        return;
    }
    const int x = static_cast<short>(frame.offsetX)-VidHardwareHalfTowardZero(vidSizeX);
    const int y = static_cast<short>(frame.offsetY)-VidHardwareHalfTowardZero(vidSizeY);
    if (surfaceMode == 0)
        SURFACE::DrawRaisedQuad(static_cast<short>(frame.width), static_cast<short>(frame.height), x, y);
    else
        SURFACE::DrawFlatQuad(static_cast<short>(frame.width), static_cast<short>(frame.height), x, y);
}

void VID_SURFACE::Draw(SPRITE* sprite)
{
    ++noDrawCall[static_cast<unsigned int>(layer)];
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u) return;
    RECT screenRect;
    CADR_DATA* const frame = PrepareFrame(sprite, &screenRect);
    if (!frame) return;
    const GAMMA gamma = VidSurfaceDrawGamma(*this, *sprite);
    (void)Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 1u);
    (void)Graph->device->SetRenderState(D3DRS_ALPHAREF, 0u);
    (void)Graph->device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    (void)Graph->device->SetRenderState(D3DRS_ZFUNC,
        renderMode == 7u || renderMode == 4u || surfaceMode == 6 ? D3DCMP_ALWAYS : D3DCMP_GREATEREQUAL);
    if ((type&2u) == 0u && (renderMode == 0u || renderMode == 6u))
    {
        (void)Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
        (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, renderMode != 6u ? 1u : 0u);
        (void)Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, Graph->graphDetail >= 8u ? 15u : 7u);
    }
    else
    {
        if (renderMode == 1u) (void)Graph->SetAlphaBlend(D3DBLEND_DESTCOLOR, D3DBLEND_SRCCOLOR);
        else if (renderMode == 2u || renderMode == 4u) (void)Graph->SetAlphaBlend(D3DBLEND_ONE, D3DBLEND_ONE);
        else if (renderMode == 5u) (void)Graph->SetAlphaBlend(D3DBLEND_DESTCOLOR, D3DBLEND_ONE);
        else if ((renderMode == 0u || renderMode == 6u || renderMode == 7u) &&
                 pixelFormat != D3DFMT_A1R5G5B5 && (type&2u) != 0u)
            (void)Graph->SetAlphaBlend(D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
        else if (renderMode == 0u || renderMode == 6u || renderMode == 7u)
            (void)Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
        (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, renderMode == 7u ? 1u : 0u);
        (void)Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u);
    }
    VidSurfaceSetMaterial(gamma);
    if (Graph->vertexShaderVersion >= 0x100u) SetFrameParameters(sprite, *frame, 0);
    if (surfaceMode == 6)
    {
        Graph->SetRenderTargetCopy();
        (void)Graph->pseudo3DEffect->SetTechnique("DrawDistortion");
        const VECTOR2 screenShift((Graph->viewLeft+Graph->viewRight+1.0f)*0.5f,
                                  (Graph->viewTop+Graph->viewBottom+1.0f)*0.5f);
        const VECTOR2 screenSize(Graph->screenWidth, Graph->screenHeight);
        (void)Graph->pseudo3DEffect->SetValue("ScreenShiftXY", &screenShift, sizeof(screenShift));
        (void)Graph->pseudo3DEffect->SetValue("ScreenXY", &screenSize, sizeof(screenSize));
        (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);
        (void)Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 0u);
        (void)Graph->device->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        (void)Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        (void)Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        (void)Graph->device->SetSamplerState(1u, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        (void)Graph->device->SetSamplerState(1u, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_BUMPENVMAT00,
            FloatBitsU32(static_cast<float>(static_cast<short>(frame->width))/Graph->screenWidth*0.5f));
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_BUMPENVMAT11,
            FloatBitsU32(static_cast<float>(static_cast<short>(frame->height))/Graph->screenHeight*0.5f));
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLOROP, D3DTOP_BUMPENVMAP);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, D3DTA_CURRENT);
        (void)Graph->device->SetTextureStageState(1u, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        (void)Graph->device->SetTextureStageState(1u, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    }
    DrawFrame(*frame);
    if (surfaceMode == 6)
    {
        (void)Graph->device->SetTexture(1u, 0);
        (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLOROP, D3DTOP_MODULATE);
    }
    if ((property&0x1000u) != 0u && renderMode == 4u)
        (void)Graph->device->SetRenderState(D3DRS_STENCILENABLE, 0u);
    lastRenderUseTick = ::RealCurrentTime;
    if (spriteClass == 8u) ++::NoBuildAlpha;
    ++noObj;
    ++noDrawReal[static_cast<unsigned int>(layer)];
}

void VID_SURFACE::DrawAuxiliaryPass(const SPRITE* sprite)
{
    const bool secondaryPlane = (type&0x2000u) != 0u;
    const bool flatSurface = surfaceMode == 1 ||
        (surfaceMode == -1 && sizeXYZ.y >= sizeXYZ.z);
    if (!secondaryPlane && sprite->Coor().z == 0.0f && flatSurface)
        return;
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u) return;

    CADR_DATA* const frame = PrepareFrame(sprite, 0);
    if (!frame) return;

    const unsigned int color = frame->indexCount == 0u && (type&0x2000u) == 0u &&
        (surfaceMode == 1 || (surfaceMode == -1 && sizeXYZ.y >= sizeXYZ.z)) ?
        0xFF78C0FFu : 0xFFFFFFFFu;
    float shift[2] = {
        static_cast<float>(VidHardwareHalfTowardZero(vidSizeX)-static_cast<short>(frame->offsetX)+0.5),
        static_cast<float>(VidHardwareHalfTowardZero(vidSizeY)-static_cast<short>(frame->offsetY)+0.5)
    };
    if ((type&0x2000u) != 0u) shift[1] += static_cast<short>(frame->height);
    const float size[2] = {
        static_cast<float>(BitsToSigned32(frame->textureObject->m_width)),
        static_cast<float>(BitsToSigned32(frame->textureObject->m_height))
    };
    const double channelScale = 0.0039215688593685626983642578125;
    const float diffuse[4] = {
        static_cast<float>(((color>>16u)&255u)*channelScale),
        static_cast<float>(((color>>8u)&255u)*channelScale),
        static_cast<float>((color&255u)*channelScale),
        static_cast<float>((color>>24u)*channelScale)
    };
    (void)Graph->pseudo3DEffect->SetValue(Graph->texShiftParameter, shift, sizeof(shift));
    (void)Graph->pseudo3DEffect->SetValue(Graph->texSizeParameter, size, sizeof(size));
    (void)Graph->pseudo3DEffect->SetVector(Graph->diffuseParameter,
        reinterpret_cast<const D3DXVECTOR4*>(diffuse));
    (void)Graph->device->SetSamplerState(0u, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    (void)Graph->device->SetSamplerState(0u, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    DrawFrame(*frame);
}

void VID_SURFACE::DrawOnlyZ(const SPRITE* sprite)
{
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u || (type&2u) != 0u ||
        layer > 3 || renderMode != 0u) return;
    CADR_DATA* const frame = PrepareFrame(sprite, 0);
    if (!frame) return;
    if (Graph->vertexShaderVersion < 0x100u)
        (void)Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectWithoutShaderTechnique);
    else
    {
        const D3DXHANDLE technique = Graph->graphDetail >= 8u && (Map->Flags()&MAP::FlagBuildingTerrain) == 0u
            ? Graph->drawObjectWithZTechnique : Graph->drawObjectTechnique;
        (void)Graph->pseudo3DEffect->SetTechnique(technique);
        SetFrameParameters(sprite, *frame, 0);
    }
    DrawFrame(*frame);
    lastRenderUseTick = ::RealCurrentTime;
    ++noObj;
}

void VID_SURFACE::DrawFlagmanBehindWalls(const SPRITE* sprite)
{
    ++noDrawCall[static_cast<unsigned int>(layer)];
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u ||
        (renderMode != 0u && renderMode != 6u) || (type&2u) != 0u) return;
    CADR_DATA* const frame = PrepareFrame(sprite, 0);
    if (!frame) return;
    const GAMMA gamma = VidSurfaceDrawGamma(*this, *sprite);
    if (Graph->vertexShaderVersion < 0x100u)
    {
        (void)Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectWithoutShaderTechnique);
        (void)Graph->device->SetRenderState(D3DRS_SPECULARENABLE, 0u);
    }
    else
    {
        (void)Graph->pseudo3DEffect->SetTechnique(Graph->drawObjectTechnique);
        (void)Graph->device->SetRenderState(D3DRS_SPECULARENABLE, gamma.add != 0u ? 1u : 0u);
        SetFrameParameters(sprite, *frame, &gamma);
    }
    (void)Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 1u);
    (void)Graph->device->SetRenderState(D3DRS_ALPHAREF, 0u);
    (void)Graph->device->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    (void)Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
    (void)Graph->device->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 0u);
    (void)Graph->SetAlphaBlend(D3DBLEND_SRCALPHA, D3DBLEND_INVSRCALPHA);
    (void)Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u);
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, D3DTA_TFACTOR);
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, D3DTA_TFACTOR);
    (void)Graph->device->SetRenderState(D3DRS_TEXTUREFACTOR, ((~gamma.modulate)&0x00FFFFFFu)|0x40000000u);
    surfaceGeometry->DrawWithEffect(frame->baseVertex, frame->vertexCount,
        frame->baseIndex, frame->indexCount);
    lastRenderUseTick = ::RealCurrentTime;
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
}

void VID_SURFACE::DrawMesh(const SPRITE* sprite)
{
    ++noDrawCall[static_cast<unsigned int>(layer)];
    if (!frames || (runtimeFlags&kVidRuntimeHidden) != 0u || (renderMode != 0u && renderMode != 6u)) return;
    CADR_DATA* const frame = PrepareFrame(sprite, 0);
    if (!frame) return;
    VidSurfaceSetMaterial(VidSurfaceDrawGamma(*this, *sprite));
    (void)Graph->device->SetRenderState(D3DRS_COLORWRITEENABLE, 7u);
    (void)Graph->device->SetRenderState(D3DRS_ZFUNC, renderMode == 7u ? D3DCMP_ALWAYS : D3DCMP_GREATEREQUAL);
    (void)Graph->device->SetRenderState(D3DRS_ZWRITEENABLE, 1u);
    (void)Graph->device->SetRenderState(D3DRS_ALPHABLENDENABLE, 0u);
    (void)Graph->device->SetRenderState(D3DRS_ALPHATESTENABLE, 0u);
    (void)Graph->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
    (void)Graph->device->SetRenderState(D3DRS_TEXTUREFACTOR, 0xFFFFFFFFu);
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    DrawFrame(*frame);
    lastRenderUseTick = ::RealCurrentTime;
    (void)Graph->device->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
    (void)Graph->device->SetTextureStageState(0u, D3DTSS_COLORARG1, D3DTA_TEXTURE);
}

namespace
{
    struct VidGridVertex
    {
        float x;
        float y;
        float z;
    };

    static int VidSurfacePixelBytes(unsigned int format)
    {
        return static_cast<int>(VidTextureBitsPerPixel(static_cast<D3DFORMAT>(format))/8u);
    }

    static int VidSurfaceRoundedTextureExtent(int value)
    {
        int rounded = 4;
        while (value>rounded) rounded += rounded;
        return rounded;
    }

    static VidGridVertex VidDecodeGridVertex(unsigned int packed)
    {
        VidGridVertex decodedVertex;
        decodedVertex.x = static_cast<float>(packed&0x3FFu);
        decodedVertex.y = static_cast<float>((packed>>10)&0x3FFu);
        const unsigned int z = (packed>>20)&0x3FFu;
        const unsigned int divisor = 1u<<(packed>>30);
        decodedVertex.z = static_cast<float>(z)/static_cast<float>(divisor)-32.0f;
        return decodedVertex;
    }

    static void VidSwapGridVertex(VidGridVertex& a, VidGridVertex& b)
    {
        const VidGridVertex t = a;
        a = b;
        b = t;
    }

    static void VidGridUpdateCell(short* cells, int x, int projectedY, int z)
    {

        short& cell = cells[(projectedY/8)*256+(x/8)];
        if (z>static_cast<int>(cell)) cell = static_cast<short>(z);
    }

    static void VidRasterizeGridTriangle(const CADR_DATA& frame,
                                         const unsigned char* pixelMask,
                                         int textureWidth,
                                         short* cells,
                                         VidGridVertex a,
                                         VidGridVertex b,
                                         VidGridVertex c)
    {

        if (a.y>b.y) VidSwapGridVertex(a, b);
        if (a.y>c.y) VidSwapGridVertex(a, c);
        if (b.y>c.y) VidSwapGridVertex(b, c);

        const int firstY = TruncateFloatViaCrtToInt32(static_cast<float>(ceil(static_cast<double>(a.y))));
        const int floorLastY = static_cast<int>(floor(static_cast<double>(c.y)));
        if (floorLastY <= firstY) return;

        const double dyAC = static_cast<double>(c.y)-a.y;
        const float splitRatio = static_cast<float>((static_cast<double>(b.y)-a.y)/dyAC);
        const float splitX = static_cast<float>((static_cast<double>(c.x)-a.x)*splitRatio+a.x);
        const float splitZ = static_cast<float>((static_cast<double>(c.z)-a.z)*splitRatio+a.z);
        const float dzdx = static_cast<float>((static_cast<double>(splitZ)-b.z)/(static_cast<double>(splitX)-b.x));
        const float longDx = static_cast<float>((static_cast<double>(c.x)-a.x)/dyAC);
        const float longDz = static_cast<float>((static_cast<double>(c.z)-a.z)/dyAC);

        const int middleY = static_cast<int>(ceil(static_cast<double>(b.y)));
        float shortDx;
        float shortDz;
        float shortX;
        float shortZ;
        if (middleY>firstY)
        {
            const float dy = b.y-a.y;
            shortDx = (b.x-a.x)/dy;
            shortDz = (b.z-a.z)/dy;
            shortX = a.x;
            shortZ = a.z;
        }
        else
        {
            const float dy = c.y-b.y;
            shortDx = (c.x-b.x)/dy;
            shortDz = (c.z-b.z)/dy;
            shortX = b.x;
            shortZ = b.z;
        }

        float longX = a.x;
        float longZ = a.z;
        int y = firstY;
        if (static_cast<int>(ceil(static_cast<double>(c.y))) <= y) return;

        const int frameX = static_cast<int>(static_cast<short>(frame.offsetX));
        const int frameY = static_cast<int>(static_cast<short>(frame.offsetY));

        while (y<static_cast<int>(ceil(static_cast<double>(c.y))))
        {

            if (y == middleY)
            {
                const float dy = c.y-b.y;
                shortDx = (c.x-b.x)/dy;
                shortDz = (c.z-b.z)/dy;
                shortX = b.x;
                shortZ = b.z;
            }

            int xBegin;
            int xEnd;
            float z;
            if (longX>shortX)
            {
                xBegin = TruncateFloatViaCrtToInt32(static_cast<float>(ceil(static_cast<double>(shortX))));
                xEnd = TruncateFloatViaCrtToInt32(static_cast<float>(ceil(static_cast<double>(longX))));
                z = shortZ;
            }
            else
            {
                xBegin = TruncateFloatViaCrtToInt32(static_cast<float>(ceil(static_cast<double>(longX))));
                xEnd = TruncateFloatViaCrtToInt32(static_cast<float>(ceil(static_cast<double>(shortX))));
                z = longZ;
            }

            for (int x = xBegin;x<xEnd;++x, z += dzdx)
            {

                const int iz = TruncateFloatViaCrtToInt32(z);
                const int projectedY = y+iz;
                if (projectedY<0 || projectedY >= 2048 || x<0 || x >= 2048) continue;

                const int localX = x-frameX;
                if (localX<0 || localX >= 1024) continue;

                const int localY = y-frameY;
                if (localY<0 || localY >= 1024) continue;
                if (pixelMask[localY*textureWidth+localX] == 0u) continue;

                VidGridUpdateCell(cells, x, projectedY, iz);
            }

            longX += longDx;
            longZ += longDz;
            shortX += shortDx;
            shortZ += shortDz;
            ++y;
        }
    }
}


void VID_SURFACE::SetLayer()
{
    if (surfaceMode != 3) storageFlags |= 0x10u;
    if (spriteClass == 8u) runtimeFlags &= ~0x80u;
    if (surfaceMode == 6)
        layer = 10;
    else if (spriteClass == 8u)
    {
        if ((type&2u) == 0u && renderMode == 0u) layer = 0;
        else
        {
            layer = 2;
            runtimeFlags &= ~0x80u;
        }
    }
    else if (spriteClass == 23u) layer = 5;
    else if (spriteClass == 10u || spriteClass == 19u) layer = 11;
    else if (renderMode == 4u || renderMode == 5u)
    {
        type |= 0x80u;
        layer = 7;
    }
    else if ((type&2u) == 0u && (renderMode == 0u || renderMode == 6u)) layer = 3;
    else if ((property&0x40000000u) == 0u && renderMode != 7u) layer = 9;
    else layer = 4;

    if ((property&0x20u) == 0u || !frames) return;

    if ((storageFlags&8u) != 0u)
    {
        unsigned char* const frameData = surfaceData+frames[0].frameDataOffset;
        noGridZ = *reinterpret_cast<int*>(frameData-sizeof(int));
        if (noGridZ != 0)
        {
            const int frameCount = static_cast<int>(noCadr);
            gridCadrShift = reinterpret_cast<int*>(
                frameData-(static_cast<unsigned int>(frameCount)*sizeof(int)+sizeof(int)));
            const unsigned int vectorDwords = static_cast<unsigned int>(frameCount)+
                3u*static_cast<unsigned int>(noGridZ)+1u;
            gridZ = reinterpret_cast<VECTOR*>(
                frameData-vectorDwords*sizeof(unsigned int));
        }
    }
    if (gridZ) return;

    maxYGridZ = 0.0f;
    minYGridZ = 0.0f;
    maxXGridZ = 0.0f;
    minXGridZ = 0.0f;

    VECTOR* const scratch = static_cast<VECTOR*>(CrtNew(50331648u));
    unsigned char* const pixelMask = static_cast<unsigned char*>(CrtNew(1048576u));
    gridCadrShift = static_cast<int*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(static_cast<int>(noCadr)), sizeof(int))));
    noGridZ = 0;

    const int bytesPerPixel = VidSurfacePixelBytes(pixelFormat);
    short depthGrid[256*256];

    for (int frameIndex = 0;frameIndex<static_cast<int>(noCadr);++frameIndex)
    {
        CADR_DATA& frame = frames[frameIndex];
        int textureWidth = static_cast<int>(static_cast<short>(frame.width));
        if ((type&0x10u) != 0u)
        {
            textureWidth = VidSurfaceRoundedTextureExtent(textureWidth);

            (void)VidSurfaceRoundedTextureExtent(static_cast<int>(static_cast<short>(frame.height)));
        }

        for (int i = 0;i<256*256;++i) depthGrid[i] = kEmptySoftwareGridCell;
        memset(pixelMask, 0, 1048576u);
        gridCadrShift[frameIndex] = noGridZ;

        if (frame.height == 0u) continue;

        const bool compressed = pixelFormat == D3DFMT_DXT1 ||
                              pixelFormat == D3DFMT_DXT3 ||
                              pixelFormat == D3DFMT_DXT5;
        if (!compressed)
        {
            unsigned char* stream = surfaceData+frame.frameDataOffset+
                static_cast<unsigned int>(frame.indexCount)*2u+
                (static_cast<unsigned int>(frame.vertexCount)+frame.additionalDataCount)*4u;

            if (*reinterpret_cast<unsigned short*>(stream) != 0u)
            {
                unsigned char* maskCursor = pixelMask;
                while (*reinterpret_cast<unsigned short*>(stream) != 0u)
                {
                    maskCursor += *stream++;
                    int runCount = *stream++;
                    while (runCount-- > 0)
                    {
                        *maskCursor = 1u;
                        ++maskCursor;

                        stream += bytesPerPixel;
                        if ((type&0x8000u) != 0u) ++stream;
                        if ((type&0x2000u) != 0u) ++stream;

                        const int maskOffset = static_cast<int>(maskCursor-pixelMask);
                        int localX = maskOffset%textureWidth;
                        int localY = maskOffset/textureWidth;
                        const int x = localX+static_cast<int>(static_cast<short>(frame.offsetX));
                        const int y = localY+static_cast<int>(static_cast<short>(frame.offsetY));

                        if ((type&4u) != 0u)
                        {
                            const int rawDepth = static_cast<int>(*reinterpret_cast<unsigned short*>(stream));
                            stream += 2;
                            const int z = (rawDepth-1024)/8;
                            if (nVid == 632 && surfaceMode == 3)
                            {
                                const int projectedY = y+z;
                                if (projectedY >= 0 && projectedY<2048 && x >= 0 && x<2048)
                                    VidGridUpdateCell(depthGrid, x, projectedY, z);
                            }
                        }

                        if (frame.vertexCount == 0u)
                            depthGrid[(y/8)*256+(x/8)] = 0;
                    }
                }
            }
        }

        if (frame.vertexCount != 0u &&
           !(nVid == 632 && (type&4u) != 0u && surfaceMode == 3))
        {
            const unsigned short* const indices =
                reinterpret_cast<const unsigned short*>(surfaceData+frame.frameDataOffset);
            const unsigned int* const vertices = reinterpret_cast<const unsigned int*>(
                surfaceData+frame.frameDataOffset+static_cast<unsigned int>(frame.indexCount)*2u);

            for (unsigned int i = 0;i<static_cast<unsigned int>(frame.indexCount);i += 3u)
            {
                VidGridVertex a = VidDecodeGridVertex(vertices[indices[i+0u]]);
                VidGridVertex b = VidDecodeGridVertex(vertices[indices[i+1u]]);
                VidGridVertex c = VidDecodeGridVertex(vertices[indices[i+2u]]);
                VidRasterizeGridTriangle(frame, pixelMask, textureWidth, depthGrid, a, b, c);
            }
        }

        const int rows = static_cast<int>(vidSizeY)/8;
        for (int row = rows-1;row >= 0;--row)
        {
            for (int column = 0;column<static_cast<int>(vidSizeX)/8;++column)
            {
                const short z = depthGrid[row*256+column];
                if (z == kEmptySoftwareGridCell) continue;

                scratch[noGridZ].x = static_cast<float>(column*8-static_cast<int>(vidSizeX)/2);
                scratch[noGridZ].y = static_cast<float>(row*8-static_cast<int>(vidSizeY)/2);
                scratch[noGridZ].z = static_cast<float>(z);
                noGridZ = BitsToSigned32(static_cast<unsigned int>(noGridZ)+1u);
            }
        }
    }

    gridZ = static_cast<VECTOR*>(CrtNew(VidArrayAllocationBytes(static_cast<unsigned int>(noGridZ), sizeof(VECTOR))));
    for (int i = 0;i<noGridZ;++i)
    {
        gridZ[i] = scratch[i];
        if (gridZ[i].x<minXGridZ) minXGridZ = gridZ[i].x;
        if (gridZ[i].x>maxXGridZ) maxXGridZ = gridZ[i].x;
        if (gridZ[i].y<minYGridZ) minYGridZ = gridZ[i].y;
        if (gridZ[i].y>maxYGridZ) maxYGridZ = gridZ[i].y;
    }

    CrtFreePointer(scratch);
    CrtFreePointer(pixelMask);
}


void VID_SURFACE::CopyGraphFromVid(VID_SURFACE* source)
{
    ReleaseVideoMemory();
    if (surfaceGeometry) surfaceGeometry->Release();
    surfaceGeometry = 0;

    if (nextMirror == this)
    {
        if ((storageFlags&8u) == 0u && cadrLinkXYZ)
        {
            CrtFreePointer(cadrLinkXYZ);
            cadrLinkXYZ = 0;
        }
        cadrLinkXYZ = 0;

        VidReleaseFrameArray(*this);
        surfaceData = 0;
    }
    else
    {
        VID* predecessor = nextMirror;
        while (predecessor->nextMirror != this) predecessor = predecessor->nextMirror;
        predecessor->nextMirror = nextMirror;
    }

    nextMirror = source->nextMirror;
    source->nextMirror = this;

    layer = source->layer;
    type = source->type;
    noCadr = source->noCadr;
    frameSpeedDefault = source->frameSpeedDefault;
    vidSizeX = source->vidSizeX;
    vidSizeY = source->vidSizeY;
    buildSizeX = source->buildSizeX;
    buildSizeY = source->buildSizeY;
    directionQuantizationOffset = source->directionQuantizationOffset;
    vidName = source->vidName;

    surfaceData = source->surfaceData;
    cadrLinkXYZ = source->cadrLinkXYZ;
    frames = source->frames;
    surfaceGeometry = source->surfaceGeometry;
    surfaceGeometry->AddRef();
    palette = source->palette;
    pixelFormat = source->pixelFormat;
    surfaceMode = source->surfaceMode;
    renderMode = source->renderMode;
    resourceFrameParameter = source->resourceFrameParameter;
    lastRenderUseTick = source->lastRenderUseTick;

    surfaceFlags = (surfaceFlags&~1u)|(source->surfaceFlags&1u);
    surfaceFlags = (surfaceFlags&~2u)|(source->surfaceFlags&2u);
    storageFlags = ((storageFlags&~8u)|(source->storageFlags&8u))&~4u;

    if (source->recolorPalette)
    {
        if (!recolorPalette)
            recolorPalette = static_cast<unsigned int*>(CrtNew(1024u));
        for (unsigned int i = 0;i<256u;++i) recolorPalette[i] = source->recolorPalette[i];
    }
    else if (recolorPalette)
    {
        CrtFreePointer(recolorPalette);
        recolorPalette = 0;
    }

    if ((storageFlags&8u) == 0u || (property&8u) != 0u)
    {
        if (gridZ)
        {
            CrtFreePointer(gridZ);
            gridZ = 0;
        }
        if (gridCadrShift)
        {
            CrtFreePointer(gridCadrShift);
            gridCadrShift = 0;
        }
    }

    noGridZ = source->noGridZ;
    gridZ = source->gridZ;
    gridCadrShift = source->gridCadrShift;
    minXGridZ = source->minXGridZ;
    maxXGridZ = source->maxXGridZ;
    minYGridZ = source->minYGridZ;
    maxYGridZ = source->maxYGridZ;
}


void VID_SURFACE::CopyFromVid(VID_SURFACE* source)
{
    sourceVid = source;
    CopyGraphFromVid(source);

    spriteType = source->spriteType;
    spriteClass = source->spriteClass;
    property = source->property;
    moveMask = source->moveMask;
    sizeXYZ = source->sizeXYZ;
    maxHp = source->maxHp;
    maxSpeed = source->maxSpeed;
    randomSpeed = source->randomSpeed;
    maxZSpeed = source->maxZSpeed;
    randomZSpeed = source->randomZSpeed;
    acceleration = source->acceleration;
    slow = source->slow;
    rotationSpeed = source->rotationSpeed;
    deathRange = source->deathRange;
    deathDamageMin = source->deathDamageMin;
    deathDamageMax = source->deathDamageMax;
    deathPush = source->deathPush;
    linkXYZ = source->linkXYZ;
    nLinkVid = source->nLinkVid;
    linkVid = source->linkVid;
    topZ = source->topZ;
    forMoveUpZ = source->forMoveUpZ;
    forMoveDownZ = source->forMoveDownZ;
    lifeTime = source->lifeTime;
    noDir = source->noDir;
    resourceFrameParameter = source->resourceFrameParameter;
    gammaRaw = source->gammaRaw;
    scaleXYZ = source->scaleXYZ;
    halfSizeXY = source->halfSizeXY;
    realSizeXYZ = source->realSizeXYZ;

    for (int i = 0;i<AnimationSlotCount;++i)
    {
        noAnimCadr[i] = source->noAnimCadr[i];
        sfx[i] = source->sfx[i];
        frameSpeed[i] = source->frameSpeed[i];
        childX[i] = source->childX[i];
        childY[i] = source->childY[i];
        childZ[i] = source->childZ[i];
        nChildVid[i] = source->nChildVid[i];
        childVid[i] = source->childVid[i];
        noChild[i] = source->noChild[i];
        animationBaseFrame[i] = source->animationBaseFrame[i];
        animationFrameCount[i] = source->animationFrameCount[i];
    }

    for (int i = 0;i<PlayerSlotCount;++i)
    {
        maxHpByPlayer[i] = source->maxHpByPlayer[i];
        gammaByPlayer[i] = source->gammaByPlayer[i];
    }

    actionAuxStateRequired = source->actionAuxStateRequired;
    movementTactEnabled = source->movementTactEnabled;
    runtimeFlags = (runtimeFlags&~0x80u)|(source->runtimeFlags&0x80u);
    runtimeFlags = (runtimeFlags&~0x100u)|(source->runtimeFlags&0x100u);
    runtimeFlags = (runtimeFlags&~0x1000u)|(source->runtimeFlags&0x1000u);
    runtimeFlags = (runtimeFlags&~0x200u)|(source->runtimeFlags&0x200u);
}

VID* VID::CreateMirror()
{
    void* const memory = CrtNew(sizeof(VID));
    return memory ? new(memory) VID() : 0;
}


void VID::SetLayer()
{
    layer = 0;
}


template<>
void LIST<int>::Expand(int requestedCapacity)
{
    if (requestedCapacity <= m_capacity) return;
    int* const oldData = m_data;
    int* const newData = reinterpret_cast<int*>(
        CrtNew(static_cast<unsigned int>(requestedCapacity)*sizeof(int)));
    m_data = newData;
    if (!newData)
    {
        STRING format("!!!ERROR!!!::LIST: Not enough memory %i");
        ::Error->LogExit(format, requestedCapacity);
    }
    if (oldData)
    {
        for (int index = 0;index<m_capacity;++index) m_data[index] = oldData[index];
        CrtFreePointer(oldData);
    }
    m_capacity = requestedCapacity;
}

VID* VID_SOFTWARE::CreateMirror()
{
    void* const memory = CrtNew(sizeof(VID_SOFTWARE));
    if (!memory) return 0;
    try
    {
        return new(memory) VID_SOFTWARE(*this);
    }
    catch (...)
    {
        CrtFreePointer(memory);
        throw;
    }
}

VID* VID_FONT::CreateMirror()
{
    void* const memory = CrtNew(sizeof(VID_FONT));
    return memory ? new(memory) VID_FONT(*this) : 0;
}


VID* VID_SURFACE::CreateMirror()
{
    void* const memory = CrtNew(sizeof(VID_SURFACE));
    if (!memory) return 0;
    VID_SURFACE* object;
    try
    {
        object = new(memory) VID_SURFACE();
    }
    catch (...)
    {
        CrtFreePointer(memory);
        throw;
    }
    object->CopyGraphFromVid(this);
    return object;
}

void VID_SURFACE::RestoreDeviceObjects()
{
}



