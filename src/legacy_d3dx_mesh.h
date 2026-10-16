#pragma once
#include "basetype.h"
#pragma push_macro("INPUT")
#undef INPUT
#define INPUT WIN32_INPUT
#include <d3d9.h>
#pragma pop_macro("INPUT")

struct ID3DXMesh;
struct ID3DXBuffer;
struct D3DXATTRIBUTERANGE;

extern "C" HRESULT __stdcall D3DXCreateSphere(
    IDirect3DDevice9* device, float radius, UINT slices, UINT stacks,
    ID3DXMesh** meshOut, ID3DXBuffer** adjacencyOut);

struct ID3DXBaseMesh : IUnknown
{
    virtual HRESULT __stdcall DrawSubset(DWORD) = 0;
    virtual DWORD __stdcall GetNumFaces() = 0;
    virtual DWORD __stdcall GetNumVertices() = 0;
    virtual DWORD __stdcall GetFVF() = 0;
    virtual HRESULT __stdcall GetDeclaration(D3DVERTEXELEMENT9*) = 0;
    virtual DWORD __stdcall GetNumBytesPerVertex() = 0;
    virtual DWORD __stdcall GetOptions() = 0;
    virtual HRESULT __stdcall GetDevice(IDirect3DDevice9**) = 0;
    virtual HRESULT __stdcall CloneMeshFVF(DWORD, DWORD, IDirect3DDevice9*, ID3DXMesh**) = 0;
    virtual HRESULT __stdcall CloneMesh(DWORD, const D3DVERTEXELEMENT9*, IDirect3DDevice9*, ID3DXMesh**) = 0;
    virtual HRESULT __stdcall GetVertexBuffer(IDirect3DVertexBuffer9**) = 0;
    virtual HRESULT __stdcall GetIndexBuffer(IDirect3DIndexBuffer9**) = 0;
    virtual HRESULT __stdcall LockVertexBuffer(DWORD, void**) = 0;
    virtual HRESULT __stdcall UnlockVertexBuffer() = 0;
    virtual HRESULT __stdcall LockIndexBuffer(DWORD, void**) = 0;
    virtual HRESULT __stdcall UnlockIndexBuffer() = 0;
    virtual HRESULT __stdcall GetAttributeTable(D3DXATTRIBUTERANGE*, DWORD*) = 0;
    virtual HRESULT __stdcall ConvertPointRepsToAdjacency(const DWORD*, DWORD*) = 0;
    virtual HRESULT __stdcall ConvertAdjacencyToPointReps(const DWORD*, DWORD*) = 0;
    virtual HRESULT __stdcall GenerateAdjacency(float, DWORD*) = 0;
    virtual HRESULT __stdcall UpdateSemantics(D3DVERTEXELEMENT9*) = 0;
};

struct ID3DXMesh : ID3DXBaseMesh
{
};
