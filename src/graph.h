#pragma once
#if defined(_WIN32)
#define INPUT WIN32_INPUT
#include <d3d9.h>
#undef INPUT
#else
enum _D3DFORMAT : int;
enum _D3DTRANSFORMSTATETYPE : int;
enum _D3DPRIMITIVETYPE : int;
#endif
#include "basetype.h"
#include "mylib.h"
#include "surface.h"
struct VECTOR;
struct VECTOR2;
struct _D3DMATRIX;
class COLOR;
class ANGLE;
class DIALOG_COMBO_BOX;
class DIALOG_BUTTON;
class STREAM;
class RESOURCE;
class VID;
class TEXTURE;
struct IDirect3DSurface9;
struct IDirect3D9;
struct IDirect3DDevice9;
struct ID3DXEffect;
struct ID3DXFont;
typedef const char* D3DXHANDLE;
struct HWND__;
struct IGraphBuilder;
struct IMediaControl;
struct IMediaEvent;
struct IVideoWindow;

struct GRAPH_INIT
{
    char title[256];
    unsigned int allowedWidth[32];
    unsigned int allowedHeight[32];
    unsigned int allowedColorBits[8];
    unsigned int flags;
    unsigned int adapter;
    unsigned int width;
    unsigned int height;
    unsigned int colorBits;
    unsigned int fullScreen;
};

struct DD_DRIVER
{
    int GetMode(int width, int height, int bitsPerPixel) const;
    STRING GetModeDesctription(int mode) const;
    void FillParameters(int adapter, IDirect3D9* d3d, const GRAPH_INIT* startup);

    char description[80];
    unsigned char reserved50[20];
    int textureMemory;
    int modeCount;
    unsigned int width[16];
    unsigned int height[16];
    unsigned int format[16];
    unsigned int depthFormat[16];
    unsigned int desktopFormat;
    unsigned int pixelShaderVersion;
    unsigned int flags;
};

struct GRAPH_PRESENT_PARAMETERS
{
    unsigned int backBufferWidth;
    unsigned int backBufferHeight;
    unsigned int backBufferFormat;
    unsigned int backBufferCount;
    unsigned int multiSampleType;
    unsigned int multiSampleQuality;
    unsigned int swapEffect;
    HWND__* deviceWindow;
    int windowed;
    int enableAutoDepthStencil;
    unsigned int autoDepthStencilFormat;
    unsigned int flags;
    unsigned int fullScreenRefreshRate;
    unsigned int presentationInterval;
};

class MOVIE
{
public:
    MOVIE()
        : graphBuilder(0),
          mediaControl(0),
          mediaEvent(0),
          videoWindow(0)
    {
}
    ~MOVIE();
    void Open(const STRING& fileName, int x, int y);
    void Release();
    void Pause();
    void Resume();
    int Update();

    IGraphBuilder* graphBuilder;
    IMediaControl* mediaControl;
    IMediaEvent* mediaEvent;
    IVideoWindow* videoWindow;
};

class GRAPH
{
public:
    static COLOR YELLOW;
    static COLOR LIGHTRED;
    static COLOR RED;
    static COLOR LIGHTGREEN;
    static COLOR GREEN;
    static COLOR LIGHTBLUE;
    static COLOR BLUE;
    static COLOR GRAY;
    static COLOR WHITE;
    static COLOR BROWN;
    static COLOR MAGENTA;
    static COLOR CYAN;
    static COLOR BLACK;

    explicit GRAPH(const GRAPH_INIT* startup);
    virtual ~GRAPH();

    int Init(HWND__* window);
    int SetGraphDetail(unsigned int detail);
    void RestoreDeviceObjects();
    void InvalidateDeviceObjects();
    int PreTact();
    void PostTact(int mode);
    void SetDefaultState() const;
    void SetScreenScale(const VECTOR& scale);
    void SetViewPort(const VECTOR2& leftTop, const VECTOR2& rightBottom);
    void SetViewTransform(ANGLE direction, const VECTOR2& scale) const;
    void SetTransformMatrix(_D3DTRANSFORMSTATETYPE mode, const _D3DMATRIX* matrix, const GAMMA* gamma) const;
    int GetGraphScreenAvailable(int width, int height) const;
    void SetScreenSize(int width, int height);
    void SetDrawShadow(int enabled);
    void DrawPrimitive(_D3DPRIMITIVETYPE primitiveType, unsigned long vertexShader,
                       const void* vertexData,unsigned int vertexStride,int vertexCount) const;
    void ClearScreen(COLOR color, float depth = 0.0f);
    int DrawLoadBar(STRING text, VID* vid, ANGLE direction, int mode);
    void DrawDebugText(const STRING& text, ...);
    void Effect(int mode, int width, int height, int flags);
    void SetEnvironment(unsigned int mask);
    void SetWind(int magnitude, ANGLE direction);
    void SetFont(const STRING& fontName, int fontWidth, int fontHeight);
    void PlayMovie(const STRING& fileName);
    void PutsXY(float x, float y, const char* text, COLOR color);
    void PutsXY(float x, float y, const STRING& text, COLOR color);
    void PutsXY(const VECTOR2& position, const char* text, COLOR color);
    void PutsXY(const VECTOR2& position, const STRING& text, COLOR color);
    void PrintfXY(const VECTOR2& position, const char* format, ...);
    void PrintfXY(float x, float y, const char* format, ...);
    void UpdateStartupDialog(DIALOG_COMBO_BOX* adapterCombo, DIALOG_COMBO_BOX* resolutionCombo, DIALOG_BUTTON* capabilityOptionButton);
    void FlipToGDISurface();
    void SetGamma(const GAMMA& gamma);
    long SetAlphaBlend(unsigned long sourceBlend, unsigned long destinationBlend) const;
    long SetRenderTarget(IDirect3DSurface9* surface, const VECTOR2& leftTop, const VECTOR2& rightBottom, IDirect3DSurface9* depthSurface);
    void Tact(int mode);
    void SetRenderTargetCopy();
    void DrawShadowToStencil(const VECTOR& position, const VECTOR2& halfExtents);
    void DrawLight(const VECTOR& position, const VECTOR2& size, COLOR color, ANGLE direction, int drawShadow);
    void SetTime(unsigned long oldTime, unsigned long newTime);
    int GetEffectState(int effect) const;
    void Box(const VECTOR2& leftTop, const VECTOR2& rightBottom, COLOR color) const;
    void Line(const VECTOR2& first, const VECTOR2& second, COLOR color) const;
    void Line(float x0, float y0, float x1, float y1, COLOR color) const;
    float ViewXMin() const;
    float ViewYMin() const;
    float ViewXMax() const;
    const VECTOR2& ViewMin() const;
    const VECTOR2& ViewMax() const;
    float SizeX() const;
    float SizeY() const;
    const VECTOR2& SizeXY() const;
    void Bar(const VECTOR2& leftTop, const VECTOR2& rightBottom, COLOR color);
    void PutBigPixel(const VECTOR2& point, COLOR color) const;
    void LightBar(const VECTOR2& leftTop, const VECTOR2& rightBottom, unsigned int color);
    void ShadowBar(const VECTOR2& leftTop, const VECTOR2& rightBottom, int level);
    void SaveScreenShot(const STRING& fileName, int left, int top, int right, int bottom) const;
    void DrawFog(const VECTOR2& leftTop, const VECTOR2& rightBottom, int start, int end, COLOR color, unsigned short* ramp, int phase, int blend);
    void DrawRain();
    void DrawSnow();
    bool DrawLightsEnabled() const;
    void SetDrawLights(bool enabled);
    bool DrawMeshEnabled() const;
    void SetDrawMesh(unsigned int enabled);
    void BeginPause();
    void EndPause();
    void OldLoadParameters(STREAM* stream, const VECTOR2& viewport);
    void LoadParameters(RESOURCE* resource, const VECTOR2& viewport);
    void SaveParameters(STREAM* stream) const;

    unsigned int drawFlags;
    unsigned int terrainClearColor;
    unsigned int lightPixelCount;
    unsigned int capabilityFlags;
    unsigned int pixelShaderVersion;
    unsigned int vertexShaderVersion;
    unsigned int maxStreams;
    unsigned int maxLights;
    unsigned int textureMemoryMiB;
    unsigned int deviceState;
    unsigned int graphDetail;
    unsigned int sceneState;
    float screenWidth;
    float screenHeight;
    float viewLeft;
    float viewTop;
    float viewRight;
    float viewBottom;
    float screenScaleX;
    float screenScaleY;
    float screenScaleZ;
    float viewportOffsetX;
    float viewportOffsetY;
    int adapterCount;
    DD_DRIVER adapters[8];
    unsigned int selectedAdapter;

    unsigned int effectWidth[16];
    unsigned int effectHeight[16];
    unsigned int effectStart[16];
    unsigned int effectDuration[16];
    float effectCenterX;
    float effectCenterY;
    float effectScreenScale[3];
    GAMMA effectGamma;
    GAMMA gamma;
    ANGLE viewDirection;
    unsigned int environmentFlags;
    unsigned int graphParameterValue;
    unsigned char graphParameterBytes[2];
    unsigned char windDirection;
    float windMagnitude;
    float windX;
    float windY;
    ID3DXFont* fontBackend;
    STRING fontName;
    float fontWidth;
    float fontHeight;
    STRING statusText;
    float projectionMatrix[16];
    MOVIE movie;
    HWND__* window;
    unsigned int depthFormat;
    unsigned int backBufferFormat;
    unsigned int desktopFormat;
    unsigned int builtPixelFormat;
    SPHERE_SURFACE lightSphere;
    TEXTURE* renderTextures[5];
    TEXTURE* lightSampleTexture;
    TEXTURE* hiBuffer;
    TEXTURE* renderTargetCopy;
    TEXTURE* lightRenderTexture;
    IDirect3DSurface9* lightDepthSurface;
    IDirect3DSurface9* savedRenderTarget;
    IDirect3DSurface9* backBufferDepthSurface;
    IDirect3DSurface9* renderTarget;
    IDirect3DSurface9* depthSurface;
    IDirect3DDevice9* device;
    IDirect3D9* direct3D;
    ID3DXEffect* pseudo3DEffect;
    GRAPH_PRESENT_PARAMETERS present;
    D3DXHANDLE drawLightTechnique;
    D3DXHANDLE lightColorParameter;
    D3DXHANDLE zCoeffParameter;
    D3DXHANDLE diffuseParameter;
    D3DXHANDLE specularParameter;
    D3DXHANDLE worldViewMatrixParameter;
    D3DXHANDLE viewMatrixParameter;
    D3DXHANDLE projectionMatrixParameter;
    D3DXHANDLE renderTargetTextureParameter;
    D3DXHANDLE drawObjectTechnique;
    D3DXHANDLE drawObjectWithZTechnique;
    D3DXHANDLE drawObjectWithoutShaderTechnique;
    D3DXHANDLE drawBackWithZTechnique;
    D3DXHANDLE drawVolumeFogTechnique;
    D3DXHANDLE texShiftParameter;
    D3DXHANDLE texSizeParameter;
    unsigned char stencilReference;

private:
    void DrawEffect(int effect);
    void DrawSquall();
};


extern GRAPH* Graph;

STRING GetPixelFormat(_D3DFORMAT format);
const STRING& GetTextureState();
void __cdecl SetTextArg(STRING* text, unsigned int flags);





#include <stddef.h>
struct GraphCompilerPair00
{
    int m_dword00;
    int m_dword04;
    void* StorePairAt00(int first, int second);
};

