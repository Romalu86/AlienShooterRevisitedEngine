#pragma once

struct IDirect3DVertexBuffer9;
struct IDirect3DIndexBuffer9;
struct VECTOR;
class VID_SURFACE;

class SURFACE
{
public:
    SURFACE();
    virtual ~SURFACE();

    void InvalidateDeviceObjects();
    void ReleaseBuffers();
    void AddRef();
    void Release();
    bool CreateFromVid(const VID_SURFACE& vid);
    static void DrawFlatQuad(int width, int height, int offsetX, int offsetY);
    static void DrawRaisedQuad(int width, int height, int offsetX, int offsetY);
    static unsigned int noVertex;
    static unsigned int noTri;
    bool Create(const VECTOR* vertices, unsigned int vertexCount,
                const unsigned short* indices, unsigned int indexCount,
                unsigned int primitiveType);
    void CreateQuad();
    void Draw(int firstVertex, int vertexCount, int firstIndex, int indexCount) const;
    void DrawWithEffect(int firstVertex, int vertexCount, int firstIndex, int indexCount) const;

    unsigned int referenceCount;
    IDirect3DVertexBuffer9* vertexBuffer;
    IDirect3DIndexBuffer9* indexBuffer;
    unsigned int primitiveType;
    unsigned int vertexFormat;
    unsigned int vertexStride;

private:
    bool CreateVertexBuffer(unsigned int vertexCount);
    bool CreateIndexBuffer(unsigned int indexCount);
    SURFACE(const SURFACE&);
    SURFACE& operator=(const SURFACE&);
};

class SPHERE_SURFACE : public SURFACE
{
public:
    int RestoreDeviceObjects();
};
