

#include "system_font.h"
#include "texture.h"
#include <windows.h>
#include <d3d9.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <cstring>
#include <new>
#include <cmath>
#include <cfloat>
#include <malloc.h>
#include "legacy_d3dx_font.h"


const IID IID_ID3DXSprite =
    {0xB07EC84A, 0x8D35, 0x4E86, {0xA9, 0xA0, 0x8D, 0xFF, 0x21, 0xD7, 0x10, 0x75}};

namespace D3DXCore {

class CSprite final : public ID3DXSprite
{
    ULONG references_;
    IDirect3DDevice9* device_;
    BOOL active_;
    IDirect3DStateBlock9* originalState_;
    IDirect3DStateBlock9* drawState_;
    BOOL alphaBlend_;
    BOOL transformed_;

    struct Vertex { float x, y, z; DWORD color; float u, v; };
    struct TLVertex { float x, y, z, rhw; DWORD color; float u, v; };

    static void TransformCoordinate(float& x, float& y, float& z, const D3DXMATRIX& matrix)
    {
        const double sourceX = x, sourceY = y, sourceZ = z;
        x = static_cast<float>(((sourceX * matrix._11 + sourceY * matrix._21)
            + sourceZ * matrix._31) + matrix._41);
        y = static_cast<float>(((sourceX * matrix._12 + sourceY * matrix._22)
            + sourceZ * matrix._32) + matrix._42);
        z = static_cast<float>(((sourceX * matrix._13 + sourceY * matrix._23)
            + sourceZ * matrix._33) + matrix._43);
        const float homogeneous = static_cast<float>(((sourceX * matrix._14
            + sourceY * matrix._24) + sourceZ * matrix._34) + matrix._44);
        const double difference = static_cast<double>(homogeneous) - 1.0;
        if (!(difference > -FLT_EPSILON && difference < FLT_EPSILON))
        {
            const double inverse = 1.0 / homogeneous;
            x = static_cast<float>(inverse * x);
            y = static_cast<float>(inverse * y);
            z = static_cast<float>(inverse * z);
        }
    }

    void RecordState(unsigned int pass)
    {
        device_->BeginStateBlock();
        device_->SetFVF(transformed_ ? (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1) : (D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1));
        device_->SetVertexShader(0);
        if (!transformed_)
        {
            D3DMATRIX identity = {};
            identity._11 = identity._22 = identity._33 = identity._44 = 1.0f;
            device_->SetTransform(D3DTS_VIEW, &identity);
            device_->SetTransform(D3DTS_PROJECTION, &identity);
            if (pass == 0) device_->SetTransform(D3DTS_WORLD, &identity);
        }
        device_->SetRenderState(D3DRS_ZENABLE, FALSE);
        device_->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        device_->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device_->SetRenderState(D3DRS_WRAP0, 0);
        device_->SetRenderState(D3DRS_CLIPPING, TRUE);
        device_->SetRenderState(D3DRS_VERTEXBLEND, 0);
        device_->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
        device_->SetNPatchMode(0.0f);
        device_->SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
        device_->SetRenderState(D3DRS_ENABLEADAPTIVETESSELLATION, FALSE);
        device_->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
        device_->SetRenderState(D3DRS_COLORWRITEENABLE, 0xfu);
        if (!transformed_)
        {
            device_->SetRenderState(D3DRS_FOGENABLE, FALSE);
            device_->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
            device_->SetRenderState(D3DRS_STENCILENABLE, FALSE);
            device_->SetRenderState(D3DRS_LIGHTING, FALSE);
            device_->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_COLOR1);
        }
        device_->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
        device_->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device_->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
        device_->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
        device_->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device_->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
        device_->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device_->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        device_->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
        device_->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
        device_->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device_->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device_->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device_->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device_->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
        if (alphaBlend_)
        {
            device_->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
            device_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
            device_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
            device_->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
        }
        else
        {
            device_->SetRenderState(D3DRS_ALPHATESTENABLE, TRUE);
            device_->SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
            device_->SetRenderState(D3DRS_ALPHAREF, 127u);
        }
        if (pass == 0)
        {
            device_->SetTexture(0, 0);
            device_->SetStreamSource(0, 0, 0, 0);
            device_->EndStateBlock(&originalState_);
        }
        else device_->EndStateBlock(&drawState_);
    }

public:
CSprite() : references_(1), device_(0), active_(FALSE), originalState_(0), drawState_(0)
{
}

    ~CSprite()
    {
        if (device_)
        {
            if (drawState_)
            {
                drawState_->Release();
                drawState_ = 0;
            }
            if (originalState_)
            {
                originalState_->Release();
                originalState_ = 0;
            }
            device_->Release();
        }
    }

    HRESULT __stdcall QueryInterface(REFIID iid, void** output) override
    {
        if (!IsEqualIID(iid, IID_ID3DXSprite) && !IsEqualIID(iid, __uuidof(IUnknown)))
            return E_NOINTERFACE;
        *output = this;
        AddRef();
        return S_OK;
    }

    ULONG __stdcall AddRef() override
    {
        return ++references_;
    }

    ULONG __stdcall Release() override
    {
        const ULONG remaining = --references_;
        if (!remaining)
        {
            this->~CSprite();
            CrtFreePointer(this);
        }
        return remaining;
    }

    HRESULT __stdcall GetDevice(IDirect3DDevice9** output) override
    {
        if (!output) return D3DERR_INVALIDCALL;
        *output = device_;
        device_->AddRef();
        return S_OK;
    }

    HRESULT Initialize(IDirect3DDevice9* device)
    {
        if (!device) return D3DERR_INVALIDCALL;
        device_ = device;
        device_->AddRef();
        D3DCAPS9 caps;
        D3DDEVICE_CREATION_PARAMETERS creation;
        device_->GetDeviceCaps(&caps);
        device_->GetCreationParameters(&creation);
        transformed_ = ((caps.PrimitiveMiscCaps & D3DPMISCCAPS_CLIPTLVERTS) != 0 ||
            (creation.BehaviorFlags & D3DCREATE_PUREDEVICE) != 0);
        alphaBlend_ = ((caps.SrcBlendCaps & D3DPBLENDCAPS_SRCALPHA) != 0 &&
            (caps.DestBlendCaps & D3DPBLENDCAPS_INVSRCALPHA) != 0);
        return S_OK;
    }

    HRESULT __stdcall Begin() override
    {
        if (active_) return D3DERR_INVALIDCALL;
        if (!originalState_)
        {
            RecordState(0);
            RecordState(1);
        }
        originalState_->Capture();
        drawState_->Apply();
        if (!transformed_)
        {
            D3DVIEWPORT9 viewport;
            device_->GetViewport(&viewport);

            const double inverseWidth = 1.0/static_cast<double>(viewport.Width);
            const float inverseHeight = static_cast<float>(1.0/static_cast<double>(viewport.Height));
            D3DMATRIX world = {};
            world._11 = static_cast<float>(inverseWidth+inverseWidth);
            world._22 = -2.0f*inverseHeight;
            world._41 = static_cast<float>(-2.0*static_cast<double>(viewport.X)*inverseWidth-inverseWidth-1.0);
            world._42 = static_cast<float>((2.0*static_cast<double>(viewport.Y)+1.0)*inverseHeight+1.0);
            world._44 = 1.0f;
            device_->SetTransform(D3DTS_WORLD, &world);
        }
        active_ = TRUE;
        return S_OK;
    }

    HRESULT __stdcall End() override
    {
        if (!active_) return D3DERR_INVALIDCALL;
        originalState_->Apply();
        active_ = FALSE;
        return S_OK;
    }

    HRESULT __stdcall OnLostDevice() override
    {
        if (device_)
        {
            if (drawState_)
            {
                drawState_->Release();
                drawState_ = 0;
            }
            if (originalState_)
            {
                originalState_->Release();
                originalState_ = 0;
            }
        }
        return S_OK;
    }


    HRESULT __stdcall OnResetDevice() override
    {
        return S_OK;
    }

    HRESULT __stdcall Draw(IDirect3DTexture9* texture, const RECT* source,
        const D3DXVECTOR2* scaling, const D3DXVECTOR2* rotationCenter,
        float rotation, const D3DXVECTOR2* translation, D3DCOLOR color) override
    {
        D3DXMATRIX transform = {};
        transform._11 = scaling ? scaling->x : 1.0f;
        transform._22 = scaling ? scaling->y : 1.0f;
        transform._33 = transform._44 = 1.0f;
        if (rotation != 0.0f)
        {
            const double halfAngle = -0.5 * static_cast<double>(rotation);
            const float z = static_cast<float>(std::sin(halfAngle));
            const float w = static_cast<float>(std::cos(halfAngle));
            const double twiceZ = 2.0 * z;
            const float xz = static_cast<float>(twiceZ * 0.0);
            const float yz = static_cast<float>(twiceZ * 0.0);
            const double xw = 0.0 * w;
            const double yw = 0.0 * w;
            const double zw = twiceZ * w;
            const double zz = twiceZ * z;
            D3DXMATRIX rotationMatrix = {};
            rotationMatrix._11 = rotationMatrix._22 = static_cast<float>(1.0 - zz);
            rotationMatrix._12 = static_cast<float>(zw);
            rotationMatrix._21 = static_cast<float>(-zw);
            rotationMatrix._13 = static_cast<float>(xz - yw);
            rotationMatrix._31 = static_cast<float>(xz + yw);
            rotationMatrix._23 = static_cast<float>(yz + xw);
            rotationMatrix._32 = static_cast<float>(yz - xw);
            rotationMatrix._33 = rotationMatrix._44 = 1.0f;
            if (rotationCenter)
            {
                transform._41 -= rotationCenter->x;
                transform._42 -= rotationCenter->y;
            }
            const D3DXMATRIX beforeRotation = transform;
            for (unsigned int row = 0; row < 4; ++row)
                for (unsigned int column = 0; column < 4; ++column)
                    transform.m[row][column] = static_cast<float>(
                        ((static_cast<double>(beforeRotation.m[row][3]) * rotationMatrix.m[3][column]
                        + static_cast<double>(beforeRotation.m[row][0]) * rotationMatrix.m[0][column])
                        + static_cast<double>(beforeRotation.m[row][2]) * rotationMatrix.m[2][column])
                        + static_cast<double>(beforeRotation.m[row][1]) * rotationMatrix.m[1][column]);
            if (rotationCenter)
            {
                transform._41 += rotationCenter->x;
                transform._42 += rotationCenter->y;
            }
        }
        if (translation)
        {
            transform._41 += translation->x;
            transform._42 += translation->y;
        }
        return DrawTransform(texture, source, &transform, color);
    }

    HRESULT __stdcall DrawTransform(IDirect3DTexture9* texture, const RECT* source,
        const D3DXMATRIX* transform, D3DCOLOR color) override
    {
        if (!texture) return D3DERR_INVALIDCALL;
        D3DSURFACE_DESC textureDescription;
        texture->GetLevelDesc(0, &textureDescription);
        RECT sampleRect;
        if (source)
        {
            sampleRect = *source;

            if (sampleRect.left<0 || sampleRect.left>sampleRect.right ||
                sampleRect.left>static_cast<LONG>(textureDescription.Width) || sampleRect.top<0 ||
                sampleRect.top>sampleRect.bottom || sampleRect.top>static_cast<LONG>(textureDescription.Height))
            return D3DERR_INVALIDCALL;
        }
        else
        {
            sampleRect.left = sampleRect.top = 0;
            sampleRect.right = static_cast<LONG>(textureDescription.Width);
            sampleRect.bottom = static_cast<LONG>(textureDescription.Height);
        }
        BOOL hasAlpha = ((color & 0xff000000u) != 0xff000000u);
        switch (textureDescription.Format)
        {
        case D3DFMT_A8R8G8B8:
        case D3DFMT_A1R5G5B5:
        case D3DFMT_A4R4G4B4:
        case D3DFMT_A8:
        case D3DFMT_A8R3G3B2:
        case D3DFMT_A8P8:
        case D3DFMT_P8:
        case D3DFMT_A8L8:
        case D3DFMT_A4L4:
        case D3DFMT_DXT2:
        case D3DFMT_DXT3:
        case D3DFMT_DXT4:
        case D3DFMT_DXT5:
            hasAlpha = TRUE;
            break;
        default:
            break;
        }
        const BOOL wasActive = active_;
        if (!wasActive)
        {
            const HRESULT begin = Begin();
            if (FAILED(begin)) return begin;
        }
        HRESULT result = device_->SetTexture(0, texture);
        if (SUCCEEDED(result))
            result = device_->SetRenderState(alphaBlend_ ? D3DRS_ALPHABLENDENABLE :
            D3DRS_ALPHATESTENABLE, hasAlpha);
        if (SUCCEEDED(result))
        {
            const float width = static_cast<float>(sampleRect.right-sampleRect.left);
            const float height = static_cast<float>(sampleRect.bottom-sampleRect.top);
            const float inverseWidth = static_cast<float>(1.0/static_cast<double>(textureDescription.Width));
            const float inverseHeight = static_cast<float>(1.0/static_cast<double>(textureDescription.Height));
            const float u0 = static_cast<float>(static_cast<double>(sampleRect.left)*inverseWidth);
            const float u1 = static_cast<float>(static_cast<double>(sampleRect.right)*inverseWidth);
            const float v0 = static_cast<float>(static_cast<double>(sampleRect.top)*inverseHeight);
            const float v1 = static_cast<float>(static_cast<double>(sampleRect.bottom)*inverseHeight);
            if (transformed_)
            {
                TLVertex vertices[4] = {
                    {0.0f, 0.0f, 0.0f, 1.0f, color, u0, v0},
                    {0.0f, height, 0.0f, 1.0f, color, u0, v1},
                    {width, height, 0.0f, 1.0f, color, u1, v1},
                    {width, 0.0f, 0.0f, 1.0f, color, u1, v0}
                };
                for (unsigned int index = 0; index < 4; ++index)
                {
                    if (transform) TransformCoordinate(vertices[index].x,
                        vertices[index].y, vertices[index].z, *transform);
                    vertices[index].x -= 0.5f;
                    vertices[index].y -= 0.5f;
                    vertices[index].z = 0.0f;
                    vertices[index].rhw = 1.0f;
                }
                result = device_->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(TLVertex));
            }
            else
            {
                Vertex vertices[4] = {
                    {0.0f, 0.0f, 0.0f, color, u0, v0},
                    {0.0f, height, 0.0f, color, u0, v1},
                    {width, height, 0.0f, color, u1, v1},
                    {width, 0.0f, 0.0f, color, u1, v0}
                };
                if (transform)
                    for (unsigned int index = 0; index < 4; ++index)
                        TransformCoordinate(vertices[index].x, vertices[index].y,
                            vertices[index].z, *transform);
                result = device_->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, vertices, sizeof(Vertex));
            }
            if (SUCCEEDED(result)) result = S_OK;
        }
        if (!wasActive)
        {
            const HRESULT end = End();
            if (FAILED(end) && SUCCEEDED(result)) result = end;
        }
        return result;
    }
};


}

HRESULT __stdcall D3DXCreateSprite(IDirect3DDevice9* device, ID3DXSprite** output)
{
    if (!device || !output) return D3DERR_INVALIDCALL;
    void* const storage = CrtNew(sizeof(D3DXCore::CSprite));
    if (!storage) return E_OUTOFMEMORY;
    D3DXCore::CSprite* sprite = new(storage) D3DXCore::CSprite;
    const HRESULT result = sprite->Initialize(device);
    if (FAILED(result))
    {
        sprite->~CSprite();
        CrtFreePointer(sprite);
        return result;
    }
    *output = sprite;
    return S_OK;
}
