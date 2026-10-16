#include "surface.h"
#include "basetype.h"
#include "graph.h"
#include "vid.h"
#include "myerror.h"
#include "legacy_d3dx_effect.h"
#include "legacy_d3dx_mesh.h"
#include <cstring>

unsigned int SURFACE::noVertex = 0u;
unsigned int SURFACE::noTri = 0u;

namespace
{
IDirect3DVertexBuffer9* sharedQuadVertexBuffer = 0;
IUnknown* sharedGeometryBuffer = 0;

void SurfaceError(TYPE_ERROR type, const char* subject, HRESULT result)
{
    if (::Error)
        ::Error->Error(STRING("SURFACE"), type, STRING(subject), static_cast<unsigned long>(result));
}

template<class T> void ReleaseSurfaceBuffer(T*& buffer)
{
    if (buffer)
    {
        buffer->Release();
        buffer = 0;
    }
}
}

SURFACE::SURFACE()
    : referenceCount(1u), vertexBuffer(0), indexBuffer(0), primitiveType(D3DPT_TRIANGLELIST),
      vertexFormat(D3DFVF_XYZ), vertexStride(sizeof(VECTOR))
{
}

SURFACE::~SURFACE()
{
    InvalidateDeviceObjects();
}

void SURFACE::InvalidateDeviceObjects()
{
    ReleaseSurfaceBuffer(sharedGeometryBuffer);
    ReleaseSurfaceBuffer(sharedQuadVertexBuffer);
    ReleaseBuffers();
}

void SURFACE::ReleaseBuffers()
{
    ReleaseSurfaceBuffer(vertexBuffer);
    ReleaseSurfaceBuffer(indexBuffer);
}

void SURFACE::AddRef()
{
    ++referenceCount;
}

void SURFACE::Release()
{
    if (--referenceCount == 0u) delete this;
}

bool SURFACE::CreateVertexBuffer(unsigned int vertexCount)
{
    vertexFormat = D3DFVF_XYZ;
    vertexStride = sizeof(VECTOR);
    const D3DPOOL pool = (Graph->capabilityFlags&0x10u) != 0u ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT;
    unsigned int retries = 0u;
    HRESULT result;
    for (;;)
    {
        result = Graph->device->CreateVertexBuffer(vertexCount*vertexStride,
            D3DUSAGE_WRITEONLY, vertexFormat, pool, &vertexBuffer, 0);
        if (result != D3DERR_OUTOFVIDEOMEMORY || !VID::lastUse || ++retries >= 5u)
            break;
        VID::lastUse->lruPrevious->ReleaseVideoMemory();
    }
    if (FAILED(result))
    {
        SurfaceError(E_CREATE, "vertex buffer in default memory", result);
        result = Graph->device->CreateVertexBuffer(vertexCount*vertexStride,
            D3DUSAGE_WRITEONLY, vertexFormat, D3DPOOL_MANAGED, &vertexBuffer, 0);
        if (FAILED(result))
            SurfaceError(E_CREATE, "Vertex buffer", result);
    }
    return SUCCEEDED(result);
}

bool SURFACE::CreateIndexBuffer(unsigned int indexCount)
{
    const D3DPOOL pool = (Graph->capabilityFlags&0x10u) != 0u ? D3DPOOL_SYSTEMMEM : D3DPOOL_DEFAULT;
    const HRESULT result = Graph->device->CreateIndexBuffer(indexCount*sizeof(unsigned short),
        D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, pool, &indexBuffer, 0);
    if (FAILED(result))
        SurfaceError(E_CREATE, "index buffer", result);
    return SUCCEEDED(result);
}

bool SURFACE::Create(const VECTOR* vertices, unsigned int vertexCount,
                     const unsigned short* indices, unsigned int indexCount,
                     unsigned int requestedPrimitiveType)
{
    ReleaseSurfaceBuffer(indexBuffer);
    ReleaseSurfaceBuffer(vertexBuffer);
    primitiveType = requestedPrimitiveType;
    (void)CreateVertexBuffer(vertexCount);
    if (!vertexBuffer) return false;
    void* data = 0;
    HRESULT result = vertexBuffer->Lock(0u, vertexCount*vertexStride, &data, 0u);
    if (FAILED(result))
    {
        SurfaceError(E_LOCK, "for CreateFromVid vertex buffer", result);
        return false;
    }
    std::memcpy(data, vertices, vertexCount*vertexStride);
    vertexBuffer->Unlock();
    if (indexCount == 0u)
        return true;
    (void)CreateIndexBuffer(indexCount);
    if (!indexBuffer) return false;
    result = indexBuffer->Lock(0u, indexCount*sizeof(unsigned short), &data, 0u);
    if (FAILED(result))
    {
        SurfaceError(E_LOCK, "for CreateFromVid index buffer", result);
        return false;
    }
    std::memcpy(data, indices, indexCount*sizeof(unsigned short));
    indexBuffer->Unlock();
    return true;
}

bool SURFACE::CreateFromVid(const VID_SURFACE& vid)
{
    if (!vid.frames) return false;
    ReleaseSurfaceBuffer(indexBuffer);
    ReleaseSurfaceBuffer(vertexBuffer);
    const CADR_DATA& last = vid.frames[vid.noCadr-1];
    const unsigned int vertexCount = last.baseVertex+last.vertexCount;
    const unsigned int indexCount = last.baseIndex+last.indexCount;
    if (vertexCount == 0u && indexCount == 0u) return false;
    primitiveType = D3DPT_TRIANGLELIST;
    (void)CreateVertexBuffer(vertexCount);
    if (!vertexBuffer) return false;
    VECTOR* vertices = 0;
    HRESULT result = vertexBuffer->Lock(0u, vertexCount*vertexStride,
        reinterpret_cast<void**>(&vertices), 0u);
    if (FAILED(result))
    {
        SurfaceError(E_LOCK, "for CreateFromVid vertex buffer", result);
        return false;
    }
    unsigned short* indices = 0;
    if (indexCount != 0u)
    {
        (void)CreateIndexBuffer(indexCount);
        if (!indexBuffer) return false;
        result = indexBuffer->Lock(0u, indexCount*sizeof(unsigned short),
            reinterpret_cast<void**>(&indices), 0u);
        if (FAILED(result))
        {
            SurfaceError(E_LOCK, "for CreateFromVid index buffer", result);
            vertexBuffer->Unlock();
            return false;
        }
    }
    const float originX = -static_cast<float>(vid.vidSizeX/2);
    const float originY = -static_cast<float>(vid.vidSizeY/2);
    for (int i = 0; i < vid.noCadr; ++i)
    {
        const CADR_DATA& frame = vid.frames[i];
        const unsigned char* source = vid.surfaceData+frame.frameDataOffset;
        if (indices)
            std::memcpy(indices+frame.baseIndex, source, frame.indexCount*sizeof(unsigned short));
        source += frame.indexCount*sizeof(unsigned short);
        for (unsigned int j = 0; j < frame.vertexCount; ++j)
        {
            unsigned int packed;
            std::memcpy(&packed, source+j*sizeof(packed), sizeof(packed));
            VECTOR& vertex = vertices[frame.baseVertex+j];
            const float z = static_cast<float>((packed>>20)&0x3FFu)/
                static_cast<float>(1u<<(packed>>30))-32.0f;
            vertex.x = static_cast<float>(packed&0x3FFu)+originX;
            vertex.z = z;
            vertex.y = static_cast<float>((static_cast<double>((packed>>10)&0x3FFu)+originY+z)/0.7070602178573608);
        }
    }
    if (indices) indexBuffer->Unlock();
    vertexBuffer->Unlock();
    return true;
}

namespace
{
void DrawSurfaceQuad(const VECTOR (&vertices)[4])
{
    Graph->device->SetFVF(D3DFVF_XYZ);
    unsigned int passes = 0u;
    Graph->pseudo3DEffect->Begin(&passes, 0u);
    for (unsigned int pass = 0u; pass < passes; ++pass)
    {
        Graph->pseudo3DEffect->BeginPass(pass);
        Graph->device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2u, vertices, sizeof(VECTOR));
        Graph->pseudo3DEffect->EndPass();
    }
    Graph->pseudo3DEffect->End();
    SURFACE::noTri += 2u;
    SURFACE::noVertex += 4u;
}
}

void SURFACE::DrawFlatQuad(int width, int height, int offsetX, int offsetY)
{
    const float x0 = static_cast<float>(offsetX);
    const float x1 = static_cast<float>(AddSigned32Wrap(offsetX, width));
    const float y0 = static_cast<float>(static_cast<double>(offsetY)/0.7070602178573608);
    const float y1 = static_cast<float>(static_cast<double>(AddSigned32Wrap(offsetY, height))/0.7070602178573608);
    const VECTOR vertices[4] = {
        VECTOR(x0, y0, 0.0f), VECTOR(x1, y0, 0.0f),
        VECTOR(x0, y1, 0.0f), VECTOR(x1, y1, 0.0f)
    };
    DrawSurfaceQuad(vertices);
}

void SURFACE::DrawRaisedQuad(int width, int height, int offsetX, int offsetY)
{
    const float x0 = static_cast<float>(offsetX);
    const float x1 = static_cast<float>(AddSigned32Wrap(offsetX, width));
    const float y0 = static_cast<float>(static_cast<double>(offsetY)/0.7070602178573608);
    const float y1 = static_cast<float>(static_cast<double>(AddSigned32Wrap(offsetY, height))/0.7070602178573608);
    const float z = y1-y0;
    const VECTOR vertices[4] = {
        VECTOR(x0, y1, z), VECTOR(x1, y1, z),
        VECTOR(x0, y1, 0.0f), VECTOR(x1, y1, 0.0f)
    };
    DrawSurfaceQuad(vertices);
}

void SURFACE::CreateQuad()
{
    if (!sharedQuadVertexBuffer)
    {
        const VECTOR vertices[4] = {
            VECTOR(0.0f, 0.0f, 0.0f), VECTOR(1.0f, 0.0f, 0.0f),
            VECTOR(0.0f, 1.0f, 0.0f), VECTOR(1.0f, 1.0f, 0.0f)
        };
        (void)Create(vertices, 4u, 0, 0u, D3DPT_TRIANGLESTRIP);
        sharedQuadVertexBuffer = vertexBuffer;
    }
    else
    {
        vertexFormat = D3DFVF_XYZ;
        vertexStride = sizeof(VECTOR);
        primitiveType = D3DPT_TRIANGLESTRIP;
        vertexBuffer = sharedQuadVertexBuffer;
    }
    vertexBuffer->AddRef();
}

void SURFACE::Draw(int firstVertex, int vertexCount, int firstIndex, int indexCount) const
{
    if (!vertexBuffer)
    {
        SurfaceError(E_INVALID, "dxVertex Draw()", firstVertex);
        return;
    }
    if (indexBuffer)
    {
        const HRESULT result = Graph->device->SetIndices(indexBuffer);
        if (FAILED(result)) SurfaceError(E_SET, "index", result);
    }
    const HRESULT result = Graph->device->SetStreamSource(0u, vertexBuffer, 0u, vertexStride);
    if (FAILED(result)) SurfaceError(E_SET, "Vertex", result);
    Graph->device->SetFVF(vertexFormat);
    if (primitiveType == D3DPT_TRIANGLELIST)
    {
        Graph->device->DrawIndexedPrimitive(D3DPT_TRIANGLELIST, firstVertex, 0u,
            vertexCount, firstIndex, indexCount/3);
        SURFACE::noTri += indexCount/3;
    }
    else
    {
        Graph->device->DrawPrimitive(static_cast<D3DPRIMITIVETYPE>(primitiveType),
            firstVertex, SubSigned32Wrap(vertexCount, 2));
        SURFACE::noTri += SubSigned32Wrap(vertexCount, 2);
    }
    SURFACE::noVertex += vertexCount;
}

void SURFACE::DrawWithEffect(int firstVertex, int vertexCount, int firstIndex, int indexCount) const
{
    unsigned int passes = 0u;
    Graph->pseudo3DEffect->Begin(&passes, 0u);
    for (unsigned int pass = 0; pass < passes; ++pass)
    {
        Graph->pseudo3DEffect->BeginPass(pass);
        Draw(firstVertex, vertexCount, firstIndex, indexCount);
        Graph->pseudo3DEffect->EndPass();
    }
    Graph->pseudo3DEffect->End();
}

int SPHERE_SURFACE::RestoreDeviceObjects()
{
    ID3DXMesh* mesh = 0;
    (void)D3DXCreateSphere(Graph->device, 1.0f, 12u, 10u, &mesh, 0);
    VECTOR* const vertices = new VECTOR[521];
    void* vertexData = 0;
    void* indexData = 0;
    (void)mesh->LockVertexBuffer(D3DLOCK_READONLY, &vertexData);
    (void)mesh->LockIndexBuffer(D3DLOCK_READONLY, &indexData);
    const unsigned int sourceStride = mesh->GetNumBytesPerVertex();
    const int vertexCount = static_cast<int>(mesh->GetNumVertices());
    const unsigned int indexCount = mesh->GetNumFaces()*3u;
    for (int i = 0; i < vertexCount; ++i)
        std::memcpy(&vertices[i], static_cast<const unsigned char*>(vertexData)+i*sourceStride, sizeof(VECTOR));
    vertices[vertexCount] = VECTOR(0.0f, 0.0f, 0.0f);
    const unsigned int drawVertexCount = static_cast<unsigned int>(vertexCount)+1u;
    if (drawVertexCount != 0u)
        (void)Create(vertices, drawVertexCount, static_cast<const unsigned short*>(indexData),
            indexCount, D3DPT_TRIANGLELIST);
    (void)mesh->UnlockIndexBuffer();
    (void)mesh->UnlockVertexBuffer();
    (void)mesh->Release();
    delete[] vertices;
    return 0;
}
