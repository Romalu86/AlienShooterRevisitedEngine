#pragma once
#include "basetype.h"

#pragma push_macro("INPUT")
#undef INPUT
#define INPUT WIN32_INPUT
#include <d3d9.h>
#pragma pop_macro("INPUT")

struct D3DXVECTOR2
{
    float x, y;
};

struct D3DXMATRIX : D3DMATRIX
{
};

extern const IID IID_ID3DXSprite;

struct ID3DXSprite;

struct ID3DXFont : IUnknown
{
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** device) = 0;
    virtual HRESULT __stdcall GetDescA(LOGFONTA* description) = 0;
    virtual HRESULT __stdcall GetDescW(LOGFONTW* description) = 0;
    virtual BOOL __stdcall GetTextMetricsA(TEXTMETRICA* metrics) = 0;
    virtual BOOL __stdcall GetTextMetricsW(TEXTMETRICW* metrics) = 0;
    virtual HDC __stdcall GetDC() = 0;
    virtual HRESULT __stdcall GetGlyphData(UINT glyph, IDirect3DTexture9** texture,
        RECT* blackBox, POINT* cellIncrement) = 0;
    virtual HRESULT __stdcall PreloadCharacters(UINT first, UINT last) = 0;
    virtual HRESULT __stdcall PreloadGlyphs(UINT first, UINT last) = 0;
    virtual HRESULT __stdcall PreloadTextA(const char* text, INT length) = 0;
    virtual HRESULT __stdcall PreloadTextW(const wchar_t* text, INT length) = 0;
    virtual INT __stdcall DrawTextA(ID3DXSprite* sprite, const char* text, INT length,
        RECT* bounds, DWORD format, D3DCOLOR color) = 0;
    virtual INT __stdcall DrawTextW(ID3DXSprite* sprite, const wchar_t* text, INT length,
        RECT* bounds, DWORD format, D3DCOLOR color) = 0;
    virtual HRESULT __stdcall OnLostDevice() = 0;
    virtual HRESULT __stdcall OnResetDevice() = 0;
};

struct ID3DXSprite : IUnknown
{
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9** output) = 0;
    virtual HRESULT __stdcall Begin() = 0;
    virtual HRESULT __stdcall Draw(IDirect3DTexture9* texture, const RECT* source,
        const D3DXVECTOR2* scaling, const D3DXVECTOR2* rotationCenter,
        float rotation, const D3DXVECTOR2* translation, D3DCOLOR color) = 0;
    virtual HRESULT __stdcall DrawTransform(IDirect3DTexture9* texture,
        const RECT* source, const D3DXMATRIX* transform, D3DCOLOR color) = 0;
    virtual HRESULT __stdcall End() = 0;
    virtual HRESULT __stdcall OnLostDevice() = 0;
    virtual HRESULT __stdcall OnResetDevice() = 0;
};

extern "C" __declspec(dllimport) HRESULT WINAPI D3DXCreateFontA(
    IDirect3DDevice9* device, INT height, UINT width, UINT weight,
    UINT mipLevels, BOOL italic, DWORD charSet, DWORD outputPrecision,
    DWORD quality, DWORD pitchAndFamily, const char* faceName, ID3DXFont** output);
