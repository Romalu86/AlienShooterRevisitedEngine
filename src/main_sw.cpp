
#include <new>
#include <cstdlib>
#include <limits>
#include <cmath>
#include "engine_globals.h"
#include <clocale>
#include "zm/Survive/zSurvive.h"
#include "zm/Game/ScriptExec/zScriptExec.h"
#include "zm/Utils/zStaticStorage.h"
#include "mylib.h"
#include "basetype.h"
#include "input.h"
#include "zm/IniParser/zIniParser.h"
#if defined(_WIN32)
#  define INPUT WIN32_INPUT
#endif
#include <windows.h>
#include <d3d9.h>
#include <dinput.h>
#include <mmsystem.h>
#if defined(_WIN32)
#  undef INPUT
#endif
#include "runtime/win32_callbacks.h"
#include <stdarg.h>
#include <float.h>
#if defined(_MSC_VER) && defined(_M_IX86)
#include <xmmintrin.h>
#include <isa_availability.h>
extern "C" int __isa_available;
#endif
#include <malloc.h>
#include <io.h>
#include <errno.h>
#include "map.h"
#include "zm/Game/zItemTaker.h"
#include "player_arcade.h"
#include "rail.h"
#include "engine.h"
#include "sprite.h"
#include "sprite_collector.h"
#include "sprite_list.h"
#include "exports.h"
#include "graph.h"
#include "texture.h"
#include "system_font.h"
#include "legacy_d3dx_effect.h"
#include "net.h"
#include "constant.h"
#include "zm/Input/zInputMap.h"
void __cdecl DialogServiceGlobalStringCleanup();

namespace {
GRAPH_INIT graphStartup = {
    "",
    {800u, 1024u, 1024u, 1024u, 960u, 1024u, 1280u},
    {600u, 768u, 576u, 640u, 600u, 600u, 720u},
    {32u},
    0u, 0u, 1024u, 768u, 32u, 1u
};
char startupResourceName[256] = {};
char startupResourceLength[256] = "0";
}


unsigned char CrtStaticInitializationGuards[4] = {};
char* crtProgramPath = 0;

VID* EmptyVid = 0;
SOUND* Sound = 0;
PROFILE* Strings = 0;
int NoBuildAlpha = 0;
ADD_CREATE_SPRITE* AddCreateSprite = 0;
zProfLdr* g_pProfLdr = 0;
zDlgSrv* g_pDlgSrv = 0;
iLogConsole* gpLogCons = 0;
iIniParser* g_pParser = 0;
bool* g_abMonsterVid = 0;
int* g_aiNamedMonstersLast = 0;
void (__cdecl *DrawLoadScreenTact)(int) = &MylibNoOp;

void __cdecl SFINIT0_MonsterVidAllocate(void)
{
    g_abMonsterVid = static_cast<bool*>(
        VirtualAlloc(0, 4096u, MEM_COMMIT, PAGE_READWRITE));
}


void __cdecl SFINIT1_Proc(void)
{
    g_aiNamedMonstersLast = static_cast<int*>(
        VirtualAlloc(0, 10u*sizeof(int), MEM_COMMIT, PAGE_READWRITE));
}


void __cdecl SFINIT2_Proc(void)
{
    IniLineBuffer = static_cast<char*>(VirtualAlloc(0, 1000u, MEM_COMMIT, PAGE_READWRITE));
}

class DIALOG_ITEM
{
public:
    HWND window;
    unsigned int controlId;
    STRING GetText() const;
};

class DIALOG_TEXT : public DIALOG_ITEM
{
public:
    operator STRING() const;
};

STRING DIALOG_ITEM::GetText() const
{
    char buffer[32000];
    *reinterpret_cast<unsigned int*>(buffer) = 0u;
    GetDlgItemTextA(window, static_cast<int>(controlId), buffer, 32000);
    STRING result = {StringEmptySentinel()};
    result = buffer;
    return result;
}

DIALOG_TEXT::operator STRING() const
{
    return GetText();
}

STRING MAP::OpenDialog(const char* filter, const char* defaultExtension) const
{
    return OpenSaveDialog(0, filter, defaultExtension);
}

namespace {

StaticObjectStorage<STRING> synchroErrorLogFileNameStorage;

struct MainSwDialogItemRef
{
    HWND window;
    unsigned int controlId;
};

static void MainSwSyncStartupDialog(HWND window)
{
    GRAPH* const graph = Graph;
    MainSwDialogItemRef device = {window, 1000u};
    MainSwDialogItemRef mode = {window, 1001u};
    MainSwDialogItemRef fullscreen = {window, 1005u};
    graph->UpdateStartupDialog(
        reinterpret_cast<DIALOG_COMBO_BOX*>(&device),
        reinterpret_cast<DIALOG_COMBO_BOX*>(&mode),
        reinterpret_cast<DIALOG_BUTTON*>(&fullscreen));
}



static void MainSwHideStartControls(HWND window)
{
    if ((graphStartup.flags&4u) != 0u) return;
    static const unsigned int controlIds[] = {1005u, 1006u, 1007u, 1008u, 1009u, 1010u, 1012u};
    for (unsigned int controlIndex = 0u;controlIndex<sizeof(controlIds)/sizeof(controlIds[0]);++controlIndex)
    {
        HWND control = GetDlgItem(window, static_cast<int>(controlIds[controlIndex]));
        ShowWindow(control, SW_HIDE);
    }
}

}

INT_PTR __stdcall AppStart(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)lParam;
    if (message == WM_INITDIALOG)
    {
        SetWindowTextA(hwnd, graphStartup.title);
        MainSwHideStartControls(hwnd);
        SendDlgItemMessageA(hwnd, 1010u, BM_SETCHECK, BST_UNCHECKED, 0);
        MainSwSyncStartupDialog(hwnd);
        {
            const STRING label("Green Blood");
            SendDlgItemMessageA(hwnd, 1017u, CB_ADDSTRING, 0,
                static_cast<LPARAM>(PtrToU32(label.value)));
        }
        {
            const STRING label("Red Blood");
            SendDlgItemMessageA(hwnd, 1017u, CB_ADDSTRING, 0,
                static_cast<LPARAM>(PtrToU32(label.value)));
        }
        return TRUE;
    }
    if (message != WM_COMMAND)
        return FALSE;

    const unsigned int controlId = wParam & 0xFFFFu;
    const unsigned int notification = (wParam >> 16u) & 0xFFFFu;
    if (controlId == 1000u || controlId == 1001u)
    {
        if (notification == CBN_SELENDOK)
            MainSwSyncStartupDialog(hwnd);
        return FALSE;
    }
    if (controlId == IDOK)
    {
        MainSwSyncStartupDialog(hwnd);
        graphStartup.flags =
            SendDlgItemMessageA(hwnd, 1010u, BM_GETCHECK, 0, 0) != 0 ? 8u : 0u;
        EndDialog(hwnd, 1);
    }
    else if (controlId == IDCANCEL)
        EndDialog(hwnd, 0);
    return FALSE;
}

extern "C" void __cdecl MainSwCreateCurScreenShot(void)
{
    STRING timeText = GetTimeString();
    STRING dateText = GetDateString();

    STRING screensAndDate = "\\Screens\\"+dateText;
    STRING withSpace = screensAndDate+" ";
    STRING withTime = withSpace+timeText;
    STRING fileName = withTime+".bmp";

    (void)fileName.Replace(
        ":",
        "h");
    (void)fileName.Replace(
        ":",
        "m");
    (void)fileName.Replace(
        ":",
        "s");

    GRAPH* const graph = Graph;
    MAP* const app = Map;
    STRING appPath = app->WindowsUserPath();
    STRING fullPath = appPath+fileName;

    const int width = TruncateFloatToInt32(graph->screenWidth);
    const int height = TruncateFloatToInt32(graph->screenHeight);
    graph->SaveScreenShot(fullPath, 0, 0, width, height);

}


LRESULT __stdcall AppWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    MAP* app = Map;
    const unsigned int lParam32 = static_cast<unsigned int>(lParam);

    if (app->WorkWndMessage(hwnd, message, wParam, static_cast<unsigned long>(lParam32)) != 0)
        return 1;

    if (message != WM_COMMAND)
    {
        return ::DefWindowProcA(hwnd, message, wParam, lParam);
    }

    const unsigned int command = wParam&0xFFFFu;
    if (command == 0x9C42u)
    {
        CONSTANT* const constants = ::Const;
        if (constants != 0 && constants->DebugMode != 0)
        {
            Map->Save(STRING("current.sav"));
        }
        return 0;
    }

    if (command == 0x9C41u)
    {
        CONSTANT* const constants = ::Const;
        if (constants != 0 && constants->DebugMode != 0)
        {
            NET* const session = ::Net;
            if (session != 0)session->NetworkOff();
            Map->Load(Map->OpenDialog(
                "Map Files\0*.map\0Save Files\0*.sav\0Demo Files\0*.dem\0All Files\0*.*\0",
                "map"));
        }
        return 0;
    }

    return 0;
}

static void RunCrtAtExitCallbacks();

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    if (!AS2RuntimeInitializeStandalone()) return 0;

    SFINIT0_MonsterVidAllocate();
    SFINIT1_Proc();
    SFINIT2_Proc();

    char* const resourceName = startupResourceName;
    char* const resourceLengthText = startupResourceLength;
    ::LoadStringA(hInstance, 1u, resourceName, 256);
    ::LoadStringA(hInstance, 2u, resourceLengthText, 256);

    const int expectedLength = BitsToSigned32(CrtAtoiText(resourceLengthText));
    bool resourceMatches = true;
    if (expectedLength > 0)
    {
        std::FILE* const resource = std::fopen(resourceName, "rb");
        if (resource)
        {
            const unsigned int actualLength = static_cast<unsigned int>(_filelength(_fileno(resource)));
            resourceMatches = (actualLength == static_cast<unsigned int>(expectedLength));
            if (resourceMatches) (void)std::fclose(resource);
        }
        else resourceMatches = false;
    }
    if (!resourceMatches)
    {
        ::MessageBoxA(0, "Invalid game version", "Error", 0u);
        return 0;
    }

    void* const appStorage = CrtNew(sizeof(MAP_STEAM));
    if (appStorage)
    {
        StaticObjectStorage<STRING> commandStorage;
        STRING* const command = ::new (commandStorage.Address()) STRING(lpCmdLine);
        MAP_STEAM* const app = new(appStorage) MAP_STEAM(
            hInstance, hPrevInstance, *command, nShowCmd, &graphStartup);
        Map = static_cast<MAP*>(app);
        command->~STRING();
    }
    else Map = 0;

    MAP* live = Map;
    if (live && (live->Flags() & MAP::FlagStartupComplete) != 0u)
    {
        int done;
        do
        {
            done = live->Tact();
            live = Map;
        }
        while (done == 0);
    }
    delete Map;

    ::VirtualFree(g_abMonsterVid, 0u, MEM_RELEASE);
    ::VirtualFree(g_aiNamedMonstersLast, 0u, MEM_RELEASE);
    ::VirtualFree(IniLineBuffer, 0u, MEM_RELEASE);
    return 0;
}




#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <io.h>
#include <time.h>
#include <stdexcept>
#if defined(_MSC_VER)
#include <eh.h>
#endif

void __cdecl InitializeMapLoadingDirectionAtStartup();
unsigned int __cdecl InitializeMapLoadingTextAtStartup();
void __cdecl RefreshQuarterTickAtStartup();
extern "C" unsigned int __cdecl MylibStaticCleanupGuard();
unsigned int __cdecl NetHostListStaticInit();
void* __cdecl InitializeVidRootAtStartup();
void __cdecl InitializeVidFirstRangeAtStartup();
void __cdecl InitializeVidSecondRangeAtStartup();
void __cdecl DialogSrvRegisterStaticCleanupSet();
void __cdecl InitializeGraphBlackAtStartup();
void __cdecl InitializeGraphCyanAtStartup();
void __cdecl InitializeGraphMagentaAtStartup();
void __cdecl InitializeGraphBrownAtStartup();
void __cdecl InitializeGraphBlueAtStartup();
void __cdecl InitializeGraphGrayAtStartup();
void __cdecl InitializeGraphGreenAtStartup();
void __cdecl InitializeGraphLightBlueAtStartup();
void __cdecl InitializeGraphLightGreenAtStartup();
void __cdecl InitializeGraphLightRedAtStartup();
void __cdecl InitializeGraphRedAtStartup();
void __cdecl InitializeGraphWhiteAtStartup();
void __cdecl InitializeGraphYellowAtStartup();
static unsigned int __cdecl MainSwGlobalStringStaticInit();
unsigned int __cdecl ManStaticInitGlobalColorPalette();
unsigned int __cdecl StaticAtexitGuard();
unsigned int __cdecl BaseTexture_StaticCleanupGuard01();
unsigned int __cdecl BaseTexture_StaticCleanupGuard02();
unsigned int __cdecl BaseTexture_StaticCleanupGuard03();
unsigned int __cdecl BaseTexture_StaticCleanupGuard04();
unsigned int __cdecl BaseTexture_StaticCleanupGuard05();
void __cdecl zIniParser_RegisterSharedCleanupOnce();
void __cdecl zItemInfo_RegisterSharedCleanupOnce();

namespace {

typedef int (__cdecl *CrtNewHandler)(unsigned int);
typedef void (__cdecl *CrtExitFunction)(int);
typedef void (__cdecl *CrtBannerFunction)();
typedef HWND (WINAPI *CrtGetActiveWindowFunction)();
typedef HWND (WINAPI *CrtGetLastActivePopupFunction)(HWND);
typedef int (WINAPI *CrtMessageBoxFunction)(HWND, const char*, const char*, unsigned int);

CrtNewHandler crtNewHandler = 0;
int crtNewMode = 0;
CrtExitFunction crtExit = &_exit;
CrtBannerFunction crtRuntimeBanner = 0;
CrtMessageBoxFunction crtMessageBox = 0;
CrtGetActiveWindowFunction crtGetActiveWindow = 0;
CrtGetLastActivePopupFunction crtGetLastActivePopup = 0;
int crtErrorMode = 0;
int crtApplicationType = 2;
unsigned int crtCharacterMappingApi = 0;
unsigned int crtCharacterTypeApi = 0;
char crtModulePath[MAX_PATH] = {};

struct CrtFileRecord
{
    char* pointer;
    int count;
    char* base;
    int flags;
    int file;
    int characterBuffer;
    int bufferSize;
    char* temporaryName;
};

char crtInputBuffer[4096] = {};
CrtFileRecord crtInitialStreams[20] = {
    {crtInputBuffer, 0, crtInputBuffer, 257, 0, 0, 4096, 0},
    {0, 0, 0, 2, 1, 0, 0, 0},
    {0, 0, 0, 2, 2, 0, 0, 0}
};
int crtStreamCount = 0;
CrtFileRecord** crtStreamTable = 0;

}

#if defined(_MSC_VER) && defined(_M_IX86)


unsigned int __cdecl CrtControlFp(unsigned int newControl, unsigned int mask)
{

    unsigned int x87Control = 0u;
    (void)__control87_2(newControl, mask&0xFFF7FFFFu, &x87Control, 0);
    return x87Control;
}
#else
unsigned int __cdecl CrtControlFp(unsigned int, unsigned int)
{
    return 0u;
}
#endif

static int __cdecl CrtCallNewHandler(unsigned int size)
{
    const CrtNewHandler handler = crtNewHandler;
    if (handler == 0)return 0;
    return handler(size) != 0 ? 1 : 0;
}

static void* __cdecl CrtAllocateWithNewHandler(unsigned int size, unsigned int mode)
{
    if (size>0xFFFFFFE0u)
    {
        (void)CrtCallNewHandler(size);
        errno = ENOMEM;
        return 0;
    }
    for (;;)
    {
        void* const memory = std::malloc(size);
        if (memory != 0) return memory;
        if (mode == 0u || CrtCallNewHandler(size) == 0)
        {
            errno = ENOMEM;
            return 0;
        }
    }
}

void* __cdecl CrtNew(unsigned int size)
{
    for (;;)
    {
        void* const memory = CrtAllocateWithNewHandler(size, crtNewMode);
        if (memory != 0) return memory;
        if (CrtCallNewHandler(size) == 0) throw std::bad_alloc();
    }
}

void* __cdecl CrtAllocate(unsigned int size)
{
    return CrtAllocateWithNewHandler(size, crtNewMode);
}


unsigned int __cdecl CrtFileGetC(unsigned int streamAddress)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    return static_cast<unsigned int>(std::fgetc(stream));
}
unsigned int __cdecl CrtFileClose(unsigned int streamAddress)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    return static_cast<unsigned int>(std::fclose(stream));
}
unsigned int __cdecl CrtFileOpen(unsigned int pathAddress, unsigned int modeAddress)
{
    return PtrToU32(std::fopen(reinterpret_cast<const char*>(U32ToPtr(pathAddress)),
        reinterpret_cast<const char*>(U32ToPtr(modeAddress))));
}


unsigned int __cdecl CrtFileSeek(unsigned int streamAddress, unsigned int byteOffset, unsigned int origin)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    return static_cast<unsigned int>(std::fseek(stream, BitsToSigned32(byteOffset), BitsToSigned32(origin)));
}
unsigned int __cdecl CrtFileFlush(unsigned int streamAddress)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    return static_cast<unsigned int>(std::fflush(stream));
}
unsigned int __cdecl CrtFileWrite(unsigned int inputAddress, unsigned int elementSize, unsigned int elementCount, unsigned int streamAddress)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    const unsigned int requestedBytes = elementSize*elementCount;
    const unsigned int transferredBytes = static_cast<unsigned int>(std::fwrite(U32ToPtr(inputAddress), 1u, requestedBytes, stream));
    if (requestedBytes == 0u) return 0u;
    return transferredBytes == requestedBytes?elementCount:transferredBytes/elementSize;
}
unsigned int __cdecl CrtFilePutC(unsigned int character, unsigned int streamAddress)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    return static_cast<unsigned int>(std::fputc(BitsToSigned32(character), stream));
}

unsigned int __cdecl CrtFileUngetC(unsigned int character, unsigned int streamAddress)
{
    std::FILE* const stream = reinterpret_cast<std::FILE*>(U32ToPtr(streamAddress));
    return static_cast<unsigned int>(std::ungetc(BitsToSigned32(character), stream));
}

static void SetCrtFileError(DWORD systemError)
{
    struct ErrorMapping
    {
        DWORD systemError;
        int error;
    };
    static const ErrorMapping mappings[] =
    {
        {ERROR_INVALID_FUNCTION, EINVAL},
        {ERROR_FILE_NOT_FOUND, ENOENT},
        {ERROR_PATH_NOT_FOUND, ENOENT},
        {ERROR_TOO_MANY_OPEN_FILES, EMFILE},
        {ERROR_ACCESS_DENIED, EACCES},
        {ERROR_INVALID_HANDLE, EBADF},
        {ERROR_ARENA_TRASHED, ENOMEM},
        {ERROR_NOT_ENOUGH_MEMORY, ENOMEM},
        {ERROR_INVALID_BLOCK, ENOMEM},
        {ERROR_BAD_ENVIRONMENT, E2BIG},
        {ERROR_BAD_FORMAT, ENOEXEC},
        {ERROR_INVALID_ACCESS, EINVAL},
        {ERROR_INVALID_DATA, EINVAL},
        {ERROR_INVALID_DRIVE, ENOENT},
        {ERROR_CURRENT_DIRECTORY, EACCES},
        {ERROR_NOT_SAME_DEVICE, EXDEV},
        {ERROR_NO_MORE_FILES, ENOENT},
        {ERROR_LOCK_VIOLATION, EACCES},
        {ERROR_BAD_NETPATH, ENOENT},
        {ERROR_NETWORK_ACCESS_DENIED, EACCES},
        {ERROR_BAD_NET_NAME, ENOENT},
        {ERROR_FILE_EXISTS, EEXIST},
        {ERROR_CANNOT_MAKE, EACCES},
        {ERROR_FAIL_I24, EACCES},
        {ERROR_INVALID_PARAMETER, EINVAL},
        {ERROR_NO_PROC_SLOTS, EAGAIN},
        {ERROR_DRIVE_LOCKED, EACCES},
        {ERROR_BROKEN_PIPE, EPIPE},
        {ERROR_DISK_FULL, ENOSPC},
        {ERROR_INVALID_TARGET_HANDLE, EBADF},
        {ERROR_INVALID_HANDLE, EINVAL},
        {ERROR_WAIT_NO_CHILDREN, ECHILD},
        {ERROR_CHILD_NOT_COMPLETE, ECHILD},
        {ERROR_DIRECT_ACCESS_HANDLE, EBADF},
        {ERROR_NEGATIVE_SEEK, EINVAL},
        {ERROR_SEEK_ON_DEVICE, EACCES},
        {ERROR_DIR_NOT_EMPTY, ENOTEMPTY},
        {ERROR_NOT_LOCKED, EACCES},
        {ERROR_BAD_PATHNAME, ENOENT},
        {ERROR_MAX_THRDS_REACHED, EAGAIN},
        {ERROR_LOCK_FAILED, EACCES},
        {ERROR_ALREADY_EXISTS, EEXIST},
        {ERROR_FILENAME_EXCED_RANGE, ENOENT},
        {ERROR_NESTING_NOT_ALLOWED, EAGAIN},
        {ERROR_NOT_ENOUGH_QUOTA, ENOMEM}
    };

    (void)_set_doserrno(systemError);
    for (const ErrorMapping& mapping : mappings)
    {
        if (mapping.systemError == systemError)
        {
            errno = mapping.error;
            return;
        }
    }
    if (systemError >= ERROR_WRITE_PROTECT && systemError <= ERROR_SHARING_BUFFER_EXCEEDED)
        errno = EACCES;
    else if (systemError >= ERROR_INVALID_STARTING_CODESEG && systemError <= ERROR_INFLOOP_IN_RELOC_CHAIN)
        errno = ENOEXEC;
    else
        errno = EINVAL;
}

int __cdecl CrtRemove(const char* path)
{
    const DWORD error = DeleteFileA(path) ? ERROR_SUCCESS : GetLastError();
    if (error == ERROR_SUCCESS) return 0;
    SetCrtFileError(error);
    return -1;
}

namespace {

enum CrtCharacterMask
{
    CtypeUpper = 1, CtypeLower = 2, CtypeDigit = 4, CtypeSpace = 8,
    CtypePunctuation = 16, CtypeControl = 32, CtypeBlank = 64,
    CtypeHexDigit = 128, CtypeAlphabetic = 256, CtypeLeadByte = 32768
};

unsigned short crtClassicCharacterTypes[257] = {
    0, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl,
    CtypeControl, CtypeControl, CtypeControl | CtypeSpace, CtypeControl | CtypeSpace, CtypeControl | CtypeSpace, CtypeControl | CtypeSpace, CtypeControl | CtypeSpace, CtypeControl,
    CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl,
    CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl, CtypeControl,
    CtypeControl, CtypeBlank | CtypeSpace, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation,
    CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation,
    CtypePunctuation, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit,
    CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypeDigit | CtypeHexDigit, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation,
    CtypePunctuation, CtypePunctuation, CtypeUpper | CtypeHexDigit, CtypeUpper | CtypeHexDigit, CtypeUpper | CtypeHexDigit, CtypeUpper | CtypeHexDigit, CtypeUpper | CtypeHexDigit, CtypeUpper | CtypeHexDigit,
    CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper,
    CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper,
    CtypeUpper, CtypeUpper, CtypeUpper, CtypeUpper, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation,
    CtypePunctuation, CtypePunctuation, CtypeLower | CtypeHexDigit, CtypeLower | CtypeHexDigit, CtypeLower | CtypeHexDigit, CtypeLower | CtypeHexDigit, CtypeLower | CtypeHexDigit, CtypeLower | CtypeHexDigit,
    CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower,
    CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypeLower,
    CtypeLower, CtypeLower, CtypeLower, CtypeLower, CtypePunctuation, CtypePunctuation, CtypePunctuation, CtypePunctuation,
    CtypeControl, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0
};
unsigned short* crtCharacterTypes = crtClassicCharacterTypes + 1;
char crtDecimalPoint = '.';
int crtMaximumMultibyteLength = 1;
unsigned int crtCtypeLocale = 0;
unsigned int crtCollateLocale = 0;
unsigned int crtCtypeCodePage = 0;
unsigned int crtCollateCodePage = 0;
volatile LONG crtLocaleReaders = 0;
unsigned int crtLocaleWriters = 0;

CRITICAL_SECTION crtInitialLockOne = {};
CRITICAL_SECTION crtInitialLockNine = {};
CRITICAL_SECTION crtExitLockStorage = {};
CRITICAL_SECTION crtLockCreationStorage = {};
CRITICAL_SECTION* crtExitLock = &crtExitLockStorage;
CRITICAL_SECTION* crtLockCreationLock = &crtLockCreationStorage;
CRITICAL_SECTION* crtLocaleLock = 0;
CRITICAL_SECTION* crtTypeInfoLock = 0;

CRITICAL_SECTION stlStaticLock = {};
volatile LONG stlStaticLockState = 0;
CrtExitCallback* crtExitCallbacksBegin = 0;
CrtExitCallback* crtExitCallbacksEnd = 0;
unsigned int crtExitProcedureEntered = 0;
unsigned char crtExitReturnToCaller = 0;

}

static void __cdecl CrtAmsgExit(unsigned int code);

static void __cdecl CrtInitLocks()
{
    InitializeCriticalSection(crtLockCreationLock);
    InitializeCriticalSection(crtExitLock);
    InitializeCriticalSection(&crtInitialLockNine);
    InitializeCriticalSection(&crtInitialLockOne);
}

static void __cdecl CrtUnlock(CRITICAL_SECTION* const& section)
{
    LeaveCriticalSection(section);
}

static void __cdecl CrtLock(CRITICAL_SECTION*& slot)
{
    if (slot == 0)
    {
        CRITICAL_SECTION* const section = static_cast<CRITICAL_SECTION*>(
            CrtAllocateWithNewHandler(sizeof(CRITICAL_SECTION), crtNewMode));
        if (section == 0) CrtAmsgExit(17u);
        CrtLock(crtLockCreationLock);
        if (slot == 0)
        {
            InitializeCriticalSection(section);
            slot = section;
        }
        else
            CrtFreePointer(section);
        CrtUnlock(crtLockCreationLock);
    }
    EnterCriticalSection(slot);
}

static unsigned int __cdecl CrtLCMapStringA(
    unsigned int locale, unsigned int flags, unsigned int input, int count,
    unsigned int output, int outputCount, unsigned int codePage, unsigned int strict)
{
    if (crtCharacterMappingApi == 0u)
    {
        if (LCMapStringW(0u, LCMAP_LOWERCASE, L"", 1, 0, 0) != 0)
            crtCharacterMappingApi = 1u;
        else if (LCMapStringA(0u, LCMAP_LOWERCASE, "", 1, 0, 0) != 0)
            crtCharacterMappingApi = 2u;
        else
            return 0u;
    }
    if (count > 0)
    {
        const char* const begin = reinterpret_cast<const char*>(U32ToPtr(input));
        const char* end = begin;
        unsigned int remaining = static_cast<unsigned int>(count);
        while (remaining != 0u && *end != 0)
        {
            ++end;
            --remaining;
        }
        if (*end == 0) count = static_cast<int>(end-begin);
    }
    const unsigned int api = crtCharacterMappingApi;
    if (api == 2u)
        return static_cast<unsigned int>(LCMapStringA(locale, flags,
            reinterpret_cast<const char*>(U32ToPtr(input)), count,
            reinterpret_cast<char*>(U32ToPtr(output)), outputCount));
    if (api != 1u) return 0u;
    if (codePage == 0u) codePage = crtCtypeCodePage;
    const int wideCount = MultiByteToWideChar(codePage, strict != 0u?(MB_PRECOMPOSED|MB_ERR_INVALID_CHARS):MB_PRECOMPOSED,
        reinterpret_cast<const char*>(U32ToPtr(input)), count, 0, 0);
    if (wideCount == 0) return 0u;
    wchar_t* wide;
    __try
    {
        const unsigned int bytes = static_cast<unsigned int>(wideCount)*2u;
        wide = static_cast<wchar_t*>(_alloca((bytes+3u)&~3u));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        wide = 0;
    }
    if (wide == 0) return 0u;
    if (MultiByteToWideChar(codePage, MB_PRECOMPOSED, reinterpret_cast<const char*>(U32ToPtr(input)),
            count, wide, wideCount) == 0)
        return 0u;
    const int mappedCount = LCMapStringW(locale, flags, wide, wideCount, 0, 0);
    if (mappedCount == 0) return 0u;
    if ((flags&LCMAP_SORTKEY) != 0u)
    {
        if (outputCount == 0) return static_cast<unsigned int>(mappedCount);
        if (mappedCount > outputCount) return 0u;
        if (LCMapStringW(locale, flags, wide, wideCount,
                reinterpret_cast<wchar_t*>(U32ToPtr(output)), outputCount) == 0)
            return 0u;
        return static_cast<unsigned int>(mappedCount);
    }
    wchar_t* mapped;
    __try
    {
        const unsigned int bytes = static_cast<unsigned int>(mappedCount)*2u;
        mapped = static_cast<wchar_t*>(_alloca((bytes+3u)&~3u));
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        mapped = 0;
    }
    if (mapped == 0) return 0u;
    if (LCMapStringW(locale, flags, wide, wideCount, mapped, mappedCount) == 0)
        return 0u;
    return static_cast<unsigned int>(WideCharToMultiByte(codePage, WC_COMPOSITECHECK|WC_SEPCHARS,
        mapped, mappedCount, outputCount != 0?reinterpret_cast<char*>(U32ToPtr(output)):0,
        outputCount != 0?outputCount:0, 0, 0));
}

unsigned int __cdecl CrtLowercase(unsigned int textAddress)
{
    char* const text = reinterpret_cast<char*>(U32ToPtr(textAddress));
    if (crtCtypeLocale == 0u)
    {
        for (char* cursor = text; *cursor != 0; ++cursor)
            if (*cursor >= 'A' && *cursor <= 'Z') *cursor = static_cast<char>(*cursor+('a'-'A'));
        return textAddress;
    }
    volatile LONG* const readers = &crtLocaleReaders;
    InterlockedIncrement(readers);
    const bool locked = crtLocaleWriters != 0u;
    if (locked)
    {
        InterlockedDecrement(readers);
        CrtLock(crtLocaleLock);
    }
    const unsigned int locale = crtCtypeLocale;
    if (locale == 0u)
    {
        if (locked) CrtUnlock(crtLocaleLock);
        else InterlockedDecrement(readers);
        for (char* cursor = text; *cursor != 0; ++cursor)
            if (*cursor >= 'A' && *cursor <= 'Z') *cursor = static_cast<char>(*cursor+('a'-'A'));
        return textAddress;
    }
    char* converted = 0;
    const unsigned int count = CrtLCMapStringA(locale, LCMAP_LOWERCASE, textAddress, -1, 0u, 0, 0u, 1u);
    if (count != 0u)
    {
        converted = static_cast<char*>(CrtAllocateWithNewHandler(count, crtNewMode));
        if (converted != 0 && CrtLCMapStringA(crtCtypeLocale,
                LCMAP_LOWERCASE, textAddress, -1, PtrToU32(converted), BitsToSigned32(count), 0u, 1u) != 0u)
            strcpy(text, converted);
    }
    if (locked) CrtUnlock(crtLocaleLock);
    else InterlockedDecrement(readers);
    CrtFreePointer(converted);
    return textAddress;
}

unsigned int __cdecl CrtUppercase(unsigned int textAddress)
{
    char* const text = reinterpret_cast<char*>(U32ToPtr(textAddress));
    if (crtCtypeLocale == 0u)
    {
        for (char* cursor = text; *cursor != 0; ++cursor)
            if (*cursor >= 'a' && *cursor <= 'z') *cursor = static_cast<char>(*cursor-('a'-'A'));
        return textAddress;
    }
    volatile LONG* const readers = &crtLocaleReaders;
    InterlockedIncrement(readers);
    const bool locked = crtLocaleWriters != 0u;
    if (locked)
    {
        InterlockedDecrement(readers);
        CrtLock(crtLocaleLock);
    }
    const unsigned int locale = crtCtypeLocale;
    if (locale == 0u)
    {
        if (locked) CrtUnlock(crtLocaleLock);
        else InterlockedDecrement(readers);
        for (char* cursor = text; *cursor != 0; ++cursor)
            if (*cursor >= 'a' && *cursor <= 'z') *cursor = static_cast<char>(*cursor-('a'-'A'));
        return textAddress;
    }
    char* converted = 0;
    const unsigned int count = CrtLCMapStringA(locale, LCMAP_UPPERCASE, textAddress, -1, 0u, 0, 0u, 1u);
    if (count != 0u)
    {
        converted = static_cast<char*>(CrtAllocateWithNewHandler(count, crtNewMode));
        if (converted != 0 && CrtLCMapStringA(crtCtypeLocale,
                LCMAP_UPPERCASE, textAddress, -1, PtrToU32(converted), BitsToSigned32(count), 0u, 1u) != 0u)
            strcpy(text, converted);
    }
    if (locked) CrtUnlock(crtLocaleLock);
    else InterlockedDecrement(readers);
    CrtFreePointer(converted);
    return textAddress;
}

static int __cdecl CrtMessageBoxA(const char* text, const char* caption, unsigned int type)
{
    if (crtMessageBox == 0)
    {
        HMODULE const userInterface = LoadLibraryA("user32.dll");
        if (userInterface == 0) return 0;
        FARPROC const messageBox = GetProcAddress(userInterface, "MessageBoxA");
        crtMessageBox = reinterpret_cast<CrtMessageBoxFunction>(messageBox);
        if (messageBox == 0) return 0;
        crtGetActiveWindow = reinterpret_cast<CrtGetActiveWindowFunction>(
            GetProcAddress(userInterface, "GetActiveWindow"));
        crtGetLastActivePopup = reinterpret_cast<CrtGetLastActivePopupFunction>(
            GetProcAddress(userInterface, "GetLastActivePopup"));
    }
    HWND parent = 0;
    const CrtGetActiveWindowFunction getActiveWindow = crtGetActiveWindow;
    if (getActiveWindow != 0)
    {
        parent = getActiveWindow();
        if (parent != 0)
        {
            const CrtGetLastActivePopupFunction getLastActivePopup = crtGetLastActivePopup;
            if (getLastActivePopup != 0) parent = getLastActivePopup(parent);
        }
    }
    return crtMessageBox(parent, text, caption, type);
}

static void __cdecl CrtWriteRuntimeMessage(unsigned int code)
{
    struct RuntimeMessage
    {
        unsigned int code;
        const char* text;
    };
    static const RuntimeMessage messages[19] = {
        {2u, "R6002\r\n- floating point not loaded\r\n"},
        {8u, "R6008\r\n- not enough space for arguments\r\n"},
        {9u, "R6009\r\n- not enough space for environment\r\n"},
        {10u, "\r\nabnormal program termination\r\n"},
        {16u, "R6016\r\n- not enough space for thread data\r\n"},
        {17u, "R6017\r\n- unexpected multithread lock error\r\n"},
        {18u, "R6018\r\n- unexpected heap error\r\n"},
        {19u, "R6019\r\n- unable to open console device\r\n"},
        {24u, "R6024\r\n- not enough space for _onexit/atexit table\r\n"},
        {25u, "R6025\r\n- pure virtual function call\r\n"},
        {26u, "R6026\r\n- not enough space for stdio initialization\r\n"},
        {27u, "R6027\r\n- not enough space for lowio initialization\r\n"},
        {28u, "R6028\r\n- unable to initialize heap\r\n"},
        {120u, "DOMAIN error\r\n"},
        {121u, "SING error\r\n"},
        {122u, "TLOSS error\r\n"},
        {252u, "\r\n"},
        {255u, "runtime error "},
        {0u, reinterpret_cast<const char*>(0x80000000u)}
    };
    unsigned int index = 0u;
    while (index < 18u && messages[index].code != code) ++index;
    if (messages[index].code != code) return;

    const unsigned int mode = crtErrorMode;
    if (mode == 1u || (mode == 0u && crtApplicationType == 1u))
    {
        const DWORD length = static_cast<DWORD>(strlen(
            messages[index].text));
        const char* const text = messages[index].text;
        HANDLE const errorOutput = GetStdHandle(STD_ERROR_HANDLE);
        DWORD written = code;
        (void)WriteFile(errorOutput, text, length, &written, 0);
        return;
    }
    if (code == 252u) return;

    char programName[260];
    if (GetModuleFileNameA(0, programName, 260u) == 0u)
        strcpy(programName, "<program name unknown>");
    char* displayedName = programName;
    if (strlen(programName)+1u > 60u)
    {
        displayedName += strlen(programName)-59u;
        memcpy(displayedName, "...", 3u);
    }
    char message[160];
    strcpy(message, "Runtime Error!\n\nProgram: ");
    strcat(message, displayedName);
    strcat(message, "\n\n");
    strcat(message, messages[index].text);
    (void)CrtMessageBoxA(message, "Microsoft Visual C++ Runtime Library",
        MB_ICONERROR | MB_TASKMODAL | MB_SETFOREGROUND);
}

static void __cdecl CrtWriteRuntimeBanner()
{
    const unsigned int mode = crtErrorMode;
    if (mode == 1u || (mode == 0u && crtApplicationType == 1u))
    {
        CrtWriteRuntimeMessage(252u);
        const CrtBannerFunction banner = crtRuntimeBanner;
        if (banner != 0) banner();
        CrtWriteRuntimeMessage(255u);
    }
}

static void __cdecl CrtAmsgExit(unsigned int code)
{
    if (crtErrorMode == 1u) CrtWriteRuntimeBanner();
    CrtWriteRuntimeMessage(code);
    crtExit(255);
}

static unsigned int __cdecl CrtGetStringTypeA(
    unsigned int infoType, unsigned int input, int count, unsigned int output,
    unsigned int codePage, unsigned int locale, unsigned int strict)
{
    unsigned int api = crtCharacterTypeApi;
    if (api == 0u)
    {
        unsigned short result;
        if (GetStringTypeW(1u, L"", 1, &result) != 0)
            api = 1u;
        else if (GetStringTypeA(0u, 1u, "", 1, &result) != 0)
            api = 2u;
        else
            return 0u;
        crtCharacterTypeApi = api;
    }
    if (api == 2u)
    {
        if (locale == 0u) locale = crtCtypeLocale;
        return static_cast<unsigned int>(GetStringTypeA(locale, infoType,
            reinterpret_cast<const char*>(U32ToPtr(input)), count,
            reinterpret_cast<unsigned short*>(U32ToPtr(output))));
    }
    if (api != 1u) return 0u;
    if (codePage == 0u) codePage = crtCtypeCodePage;
    const int size = MultiByteToWideChar(codePage, strict != 0u?9u:1u,
        reinterpret_cast<const char*>(U32ToPtr(input)), count, 0, 0);
    if (size == 0) return 0u;
    wchar_t* wide;
    __try
    {
        const unsigned int bytes = static_cast<unsigned int>(size)*2u;
        wide = static_cast<wchar_t*>(_alloca((bytes+3u)&~3u));
        memset(wide, 0, bytes);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        wide = 0;
    }
    if (wide == 0) return 0u;
    const int converted = MultiByteToWideChar(codePage, 1u,
        reinterpret_cast<const char*>(U32ToPtr(input)), count, wide, size);
    if (converted == 0) return 0u;
    return static_cast<unsigned int>(GetStringTypeW(infoType, wide, converted,
        reinterpret_cast<unsigned short*>(U32ToPtr(output))));
}

static unsigned int __cdecl CrtIsCtype(unsigned int ch, unsigned int mask)
{
    if (ch+1u <= 256u)
        return crtCharacterTypes[BitsToSigned32(ch)]&mask;
    char bytes[4];
    const unsigned int lead = (ch>>8u)&0xFFu;
    int count;
    if ((crtCharacterTypes[lead] & CtypeLeadByte) != 0u)
    {
        bytes[0] = static_cast<char>(lead);
        bytes[1] = static_cast<char>(ch);
        bytes[2] = 0;
        count = 2;
    }
    else
    {
        bytes[0] = static_cast<char>(ch);
        bytes[1] = 0;
        count = 1;
    }
    unsigned short result;
    if (CrtGetStringTypeA(1u, PtrToU32(bytes), count, PtrToU32(&result), 0u, 0u, 1u) == 0u)
        return 0u;
    return result&mask;
}

unsigned int __cdecl CrtIsAlpha(unsigned int ch)
{
    if (crtMaximumMultibyteLength>1) return CrtIsCtype(ch, CtypeAlphabetic | CtypeLower | CtypeUpper);
    return crtCharacterTypes[BitsToSigned32(ch)] & (CtypeAlphabetic | CtypeLower | CtypeUpper);
}

unsigned int __cdecl CrtIsDigit(unsigned int ch)
{
    if (crtMaximumMultibyteLength>1) return CrtIsCtype(ch, CtypeDigit);
    return crtCharacterTypes[BitsToSigned32(ch)] & CtypeDigit;
}

unsigned int __cdecl CrtIsSpace(unsigned int ch)
{
    if (crtMaximumMultibyteLength>1) return CrtIsCtype(ch, CtypeSpace);
    return crtCharacterTypes[BitsToSigned32(ch)] & CtypeSpace;
}

unsigned int __cdecl CrtIsAlnum(unsigned int ch)
{
    if (crtMaximumMultibyteLength>1) return CrtIsCtype(ch, CtypeAlphabetic | CtypeLower | CtypeUpper | CtypeDigit);
    return crtCharacterTypes[BitsToSigned32(ch)] & (CtypeAlphabetic | CtypeLower | CtypeUpper | CtypeDigit);
}

static int RegisterCrtExitDrain()
{
    static const int result = std::atexit(&RunCrtAtExitCallbacks);
    return result;
}

int __cdecl RegisterCrtAtExitCallback(CrtExitCallback callback)
{
    CrtLock(crtExitLock);
    const unsigned int capacity = static_cast<unsigned int>(_msize(crtExitCallbacksBegin));
    CrtExitCallback* const begin = crtExitCallbacksBegin;
    CrtExitCallback* end = crtExitCallbacksEnd;
    const unsigned int usedBytes = PtrToU32(end) - PtrToU32(begin);
    if (capacity < usedBytes + sizeof(CrtExitCallback))
    {
        const unsigned int grownCapacity = static_cast<unsigned int>(_msize(begin)) + 16u;
        CrtExitCallback* const grown = static_cast<CrtExitCallback*>(
            std::realloc(crtExitCallbacksBegin, grownCapacity));
        if (grown == 0)
        {
            CrtUnlock(crtExitLock);
            return -1;
        }
        const unsigned int used = PtrToU32(crtExitCallbacksEnd) - PtrToU32(crtExitCallbacksBegin);
        crtExitCallbacksBegin = grown;
        end = grown + used / sizeof(CrtExitCallback);
        crtExitCallbacksEnd = end;
    }
    *end = callback;
    ++crtExitCallbacksEnd;
    CrtUnlock(crtExitLock);
    return callback ? 0 : -1;
}


static void RunCrtAtExitCallbacks()
{
    CrtLock(crtExitLock);
    crtExitProcedureEntered = 1u;
    crtExitReturnToCaller = 0u;
    CrtExitCallback* const begin = crtExitCallbacksBegin;
    if (begin != 0)
    {
        CrtExitCallback* current = crtExitCallbacksEnd;
        while (PtrToU32(current) > PtrToU32(crtExitCallbacksBegin))
        {
            --current;
            const CrtExitCallback callback = *current;
            if (callback) callback();
        }
    }
    CrtUnlock(crtExitLock);
}

static volatile LONG* CrtStaticLockState()
{
    return &stlStaticLockState;
}
static CRITICAL_SECTION* CrtStaticLockCs()
{
    return &stlStaticLock;
}


void __cdecl CrtStaticLockCleanup()
{
    volatile LONG* const state = CrtStaticLockState();
    const LONG previous = InterlockedExchange(state, 3);
    if (previous == 2)DeleteCriticalSection(CrtStaticLockCs());
}

unsigned int __cdecl CrtStaticLockConstruct(unsigned int self)
{
    volatile LONG* const state = CrtStaticLockState();
    CRITICAL_SECTION* const cs = CrtStaticLockCs();
    const LONG current = *state;
    if (current == 2)
    {
        EnterCriticalSection(cs);
        return self;
    }
    if (current == 0)
    {
        const LONG previous = InterlockedExchange(state, 1);
        if (previous == 0)
        {
            InitializeCriticalSection(cs);
            (void)RegisterCrtAtExitCallback(&CrtStaticLockCleanup);
            *state = 2;
        }
        else if (previous == 2)
        {

            *state = 2;
        }
    }
    while (*state == 1)Sleep(1);
    if (*state == 2)EnterCriticalSection(cs);
    return self;
}


unsigned int __cdecl CrtStaticLockDestruct(unsigned int self)
{
    (void)self;
    if (*CrtStaticLockState() == 2)LeaveCriticalSection(CrtStaticLockCs());
    return 0u;
}
static volatile LONG crtStaticInitializationState=0;


static void SetStaticInitializationGuard(unsigned char& guard)
{
    if ((guard&1u) == 0u) guard = static_cast<unsigned char>(guard|1u);
}

static unsigned int __cdecl SharedNoopStaticInitializer()
{


    SetStaticInitializationGuard(CrtStaticInitializationGuards[0]);
    return static_cast<unsigned int>(RegisterCrtAtExitCallback(&MapStaticCleanupNoOp));
}

static unsigned int __cdecl InitializeInputMappingDefaults()
{
    for (unsigned int action = 0; action < zInputMap::ACTION_COUNT; ++action)
    {
        for (unsigned int slot = 0; slot < zInputMap::MAX_MAPPING_COUNT; ++slot)
        {
            InpAction& mapping = zInputMap::m_aiMappings[action][slot];
            mapping.inputMode = IM_NONE;
            mapping.inputValue = 0;
        }
    }
    return 0;
}

static unsigned int __cdecl InitializeInputClickPosition()
{
    INPUT::lClickMouseCoor = VECTOR2(0.0f, 0.0f);
    return 0;
}

static unsigned int __cdecl InitializeSharedCleanupGuardPair()
{
    SetStaticInitializationGuard(CrtStaticInitializationGuards[0]);
    const int a = RegisterCrtAtExitCallback(&MapStaticCleanupNoOp);
    SetStaticInitializationGuard(CrtStaticInitializationGuards[3]);
    const int b = RegisterCrtAtExitCallback(&MapStaticCleanupNoOp);
    return static_cast<unsigned int>(a?a:b);
}

static unsigned int __cdecl InitializeSharedCleanupGuardGroup()
{
    int rc = 0;
    for (unsigned int i = 0u;i<sizeof(CrtStaticInitializationGuards)/sizeof(CrtStaticInitializationGuards[0]);++i)
    {
        SetStaticInitializationGuard(CrtStaticInitializationGuards[i]);
        const int x = RegisterCrtAtExitCallback(&MapStaticCleanupNoOp);
        if (!rc)rc = x;
    }
    return static_cast<unsigned int>(rc);
}

static void __cdecl _cropzeros(char* text)
{
    const char decimal = crtDecimalPoint;
    char* fractionBegin = text;
    while (*fractionBegin && *fractionBegin != decimal)++fractionBegin;
    if (!*fractionBegin)return;
    ++fractionBegin;
    char* exponentBegin = fractionBegin;
    while (*exponentBegin && *exponentBegin != 'e' && *exponentBegin != 'E')++exponentBegin;
    char* trimEnd = exponentBegin;
    while (trimEnd[-1] == '0')--trimEnd;
    if (trimEnd[-1] == decimal)--trimEnd;
    while ((*trimEnd++ = *exponentBegin++) != 0)
    {
    }
}

static int __cdecl CrtToLowerWithLocale(int ch)
{
    if (crtCtypeLocale == 0u)
    {
        if (ch >= 'A' && ch <= 'Z')return ch + ('a' - 'A');
        return ch;
    }
    if (ch < 256)
    {
        unsigned int upper;
        if (crtMaximumMultibyteLength > 1)
            upper = CrtIsCtype(static_cast<unsigned int>(ch), 1u);
        else
        {
            upper = crtCharacterTypes[ch] & CtypeUpper;
        }
        if (upper == 0u)return ch;
    }
    char input[3];
    const unsigned int lead = (static_cast<unsigned int>(ch) >> 8u) & 0xFFu;
    int count;
    if ((crtCharacterTypes[lead] & CtypeLeadByte) != 0u)
    {
        input[0] = static_cast<char>(lead);
        input[1] = static_cast<char>(ch);
        input[2] = 0;
        count = 2;
    }
    else
    {
        input[0] = static_cast<char>(ch);
        input[1] = 0;
        count = 1;
    }
    unsigned char output[3];
    const unsigned int size = CrtLCMapStringA(crtCtypeLocale,
        LCMAP_LOWERCASE, PtrToU32(input), count, PtrToU32(output), 3, 0u, 1u);
    if (size == 0u)return ch;
    if (size == 1u)return output[0];
    return (static_cast<int>(output[1]) << 8) | output[0];
}

static int __cdecl CrtToLower(int ch)
{
    if (crtCtypeLocale == 0u)
    {
        if (ch >= 'A' && ch <= 'Z')return ch + ('a' - 'A');
        return ch;
    }
    volatile LONG* const localeGate =
        &crtLocaleReaders;
    InterlockedIncrement(localeGate);
    const bool separateLocaleLock = crtLocaleWriters != 0u;
    if (separateLocaleLock)
    {
        InterlockedDecrement(localeGate);
        CrtLock(crtLocaleLock);
    }
    const int result = CrtToLowerWithLocale(ch);
    if (separateLocaleLock)
        CrtUnlock(crtLocaleLock);
    else
        InterlockedDecrement(localeGate);
    return result;
}

static void __cdecl _forcdecpt(char* text)
{
    char* insertion = text;
    if (CrtToLower(static_cast<int>(static_cast<signed char>(*insertion))) != 'e')
    {
        do
        {
            ++insertion;
        }
        while (CrtIsDigit(static_cast<unsigned int>(
            static_cast<int>(static_cast<signed char>(*insertion)))) != 0u);
    }
    const char decimal = crtDecimalPoint;
    char displaced = *insertion;
    *insertion++ = decimal;
    char copied;
    do
    {
        const char next = *insertion;
        copied = displaced;
        *insertion++ = copied;
        displaced = next;
    }
    while (copied != 0);
}

static int __cdecl _positive(const double* value)
{
    volatile double number = *value;
    return number >= 0.0 ? 1 : 0;
}

union CrtLongDouble12
{
    unsigned int limbs[3];
    unsigned short words[6];
};

static const CrtLongDouble12 _pow10pos[29] = {
    {{0x00000000u, 0x00000000u, 0x4002A000u}},
    {{0x00000000u, 0x00000000u, 0x4005C800u}},
    {{0x00000000u, 0x00000000u, 0x4008FA00u}},
    {{0x00000000u, 0x00000000u, 0x400C9C40u}},
    {{0x00000000u, 0x00000000u, 0x400FC350u}},
    {{0x00000000u, 0x00000000u, 0x4012F424u}},
    {{0x00000000u, 0x80000000u, 0x40169896u}},
    {{0x00000000u, 0x20000000u, 0x4019BEBCu}},
    {{0x00000000u, 0xC9BF0400u, 0x40348E1Bu}},
    {{0xA1000000u, 0x1BCECCEDu, 0x404ED3C2u}},
    {{0xB59EF020u, 0xADA82B70u, 0x40699DC5u}},
    {{0x25FD5DD0u, 0x4F8E1AE5u, 0x4083EB19u}},
    {{0x95D79671u, 0x8D050E43u, 0x409EAF29u}},
    {{0x44A0BFF9u, 0x8F1281EDu, 0x40B98281u}},
    {{0xA6D53CBFu, 0x1F49FFCFu, 0x40D3C278u}},
    {{0x8CE0C66Fu, 0x47C980E9u, 0x41A893BAu}},
    {{0x556B85BCu, 0xF78D3927u, 0x427CE070u}},
    {{0xDE8EDDBCu, 0xEBFB9DF9u, 0x4351AA7Eu}},
    {{0xE376E6A1u, 0x2F29F2CCu, 0x44268184u}},
    {{0xAA171028u, 0xE310AEF8u, 0x44FAC4C5u}},
    {{0xF3D4A7EBu, 0x4AE1EBF7u, 0x45CF957Au}},
    {{0x91C7CC65u, 0xA0AEA60Eu, 0x46A3E319u}},
    {{0x0C17650Du, 0x75868175u, 0x4D48C976u}},
    {{0xA7E44258u, 0x353B3993u, 0x53EDB2B8u}},
    {{0x5DE5A74Du, 0x3B5DC53Du, 0x5A929E8Bu}},
    {{0xF0A65DFFu, 0x54C020A1u, 0x61378CA5u}},
    {{0x5A8BFDD1u, 0x5D25D88Bu, 0x67DBF989u}},
    {{0xF3F895AAu, 0xC8A2BF27u, 0x6E80DD5Du}},
    {{0x979BC94Cu, 0x52028A20u, 0x7525C460u}},
};

static const CrtLongDouble12 _pow10neg[29] = {
    {{0xCCCDCCCDu, 0xCCCCCCCCu, 0x3FFBCCCCu}},
    {{0xD70A3D71u, 0x0A3D70A3u, 0x3FF8A3D7u}},
    {{0xDF3B645Au, 0x6E978D4Fu, 0x3FF58312u}},
    {{0x652CD3C3u, 0x1758E219u, 0x3FF1D1B7u}},
    {{0x84230FD0u, 0xAC471B47u, 0x3FEEA7C5u}},
    {{0x69B6A640u, 0xBD05AF6Cu, 0x3FEB8637u}},
    {{0x42BC3D33u, 0x94D5E57Au, 0x3FE7D6BFu}},
    {{0xCEFDFDC2u, 0x77118461u, 0x3FE4ABCCu}},
    {{0xE15B4C2Fu, 0x94BEC44Du, 0x3FC9E695u}},
    {{0x3B53C492u, 0x14CD4475u, 0x3FAF9ABEu}},
    {{0x94BA67DEu, 0x1EAD4539u, 0x3F94CFB1u}},
    {{0xE2C62324u, 0x313BBABCu, 0x3F7A8B61u}},
    {{0xC1595561u, 0x7C53B17Eu, 0x3F5FBB12u}},
    {{0x8D2FEED7u, 0x8592BE06u, 0x3F44FB15u}},
    {{0xE9A53F24u, 0xEA27A539u, 0x3F2AA87Fu}},
    {{0xE4A1AC7Du, 0x467C64BCu, 0x3E55DDD0u}},
    {{0xCC067B63u, 0x83775423u, 0x3D8191FFu}},
    {{0x193AFA91u, 0x4325637Au, 0x3CACC031u}},
    {{0x38D18921u, 0xB8974782u, 0x3BD7FD00u}},
    {{0x085888DCu, 0xE3E8B11Bu, 0x3B03A686u}},
    {{0x424584C6u, 0x7599B607u, 0x3A2EDB37u}},
    {{0xD21C7133u, 0xEE32DB23u, 0x395A9049u}},
    {{0xC0BE87A6u, 0x82A5DA57u, 0x32B5A2A6u}},
    {{0x11B268E2u, 0x449F52A7u, 0x2C10B759u}},
    {{0x2DE44925u, 0x534F3436u, 0x256BCEAEu}},
    {{0xA404598Fu, 0x7DC2DEC0u, 0x1EC6E8FBu}},
    {{0x5A88E79Eu, 0xBF3C9157u, 0x18228350u}},
    {{0x62654B4Eu, 0xAF8F83FDu, 0x117D9406u}},
    {{0x9FDE2DE4u, 0x04C8D2CEu, 0x0AD8A6DDu}},
};


#pragma pack(push, 2)
struct CrtLongDouble10
{
    unsigned int low;
    unsigned int high;
    unsigned short exponent;
};

struct CrtDecimalOutput
{
    short decpt;
    char sign;
    char length;
    char digits[22];
};
#pragma pack(pop)

struct CrtFloatOutput
{
    int sign;
    int decpt;
    int flag;
    char* mantissa;
};


static int __addl(unsigned int first, unsigned int second, unsigned int* result)
{
    const unsigned int sum = first + second;
    const int carry = sum < first || sum < second;
    *result = sum;
    return carry;
}

static void __add_12(CrtLongDouble12* value, const CrtLongDouble12* addend)
{
    if (__addl(value->limbs[0], addend->limbs[0], &value->limbs[0]))
        if (__addl(value->limbs[1], 1u, &value->limbs[1]))
            ++value->limbs[2];
    if (__addl(value->limbs[1], addend->limbs[1], &value->limbs[1]))
        ++value->limbs[2];
    __addl(value->limbs[2], addend->limbs[2], &value->limbs[2]);
}

static void __shl_12(CrtLongDouble12* value)
{
    const unsigned int low = value->limbs[0];
    const unsigned int middle = value->limbs[1];
    value->limbs[0] = low << 1;
    value->limbs[1] = (middle << 1) | (low >> 31);
    value->limbs[2] = (value->limbs[2] << 1) | (middle >> 31);
}

static void __shr_12(CrtLongDouble12* value)
{
    const unsigned int high = value->limbs[2];
    const unsigned int middle = value->limbs[1];
    value->limbs[1] = (middle >> 1) | (high << 31);
    const unsigned int low = value->limbs[0];
    value->limbs[2] = high >> 1;
    value->limbs[0] = (low >> 1) | (middle << 31);
}

static void __mtold12(const char* digits, unsigned int count, CrtLongDouble12* output)
{
    unsigned short exponent = 16462;
    output->limbs[0] = 0;
    output->limbs[1] = 0;
    output->limbs[2] = 0;
    while (count != 0)
    {
        CrtLongDouble12 addend = *output;
        __shl_12(output);
        __shl_12(output);
        __add_12(output, &addend);
        __shl_12(output);
        addend.limbs[0] = static_cast<unsigned int>(static_cast<int>(static_cast<signed char>(*digits)));
        addend.limbs[1] = 0;
        addend.limbs[2] = 0;
        __add_12(output, &addend);
        ++digits;
        --count;
    }
    while (output->limbs[2] == 0)
    {
        const unsigned int middle = output->limbs[1];
        output->limbs[2] = middle >> 16;
        const unsigned int low = output->limbs[0];
        output->limbs[1] = (middle << 16) | (low >> 16);
        output->limbs[0] = low << 16;
        exponent = static_cast<unsigned short>(exponent - 16);
    }
    while ((output->limbs[2] & 0x8000u) == 0)
    {
        __shl_12(output);
        --exponent;
    }
    output->words[5] = exponent;
}

static void __ld12mul(CrtLongDouble12* value, const CrtLongDouble12* multiplier)
{
    const unsigned short rightExponent = multiplier->words[5] & 0x7FFFu;
    const unsigned short leftExponent = value->words[5] & 0x7FFFu;
    const unsigned short sign = (value->words[5] ^ multiplier->words[5]) & 0x8000u;
    unsigned short exponent = static_cast<unsigned short>(leftExponent + rightExponent);
    if (leftExponent == 32767 || rightExponent == 32767 || exponent > 49149)
    {
        value->limbs[1] = 0;
        value->limbs[0] = 0;
        value->limbs[2] = 0x7FFF8000u + (sign != 0 ? 0x80000000u : 0u);
        return;
    }
    if (exponent <= 16319)
    {
        value->limbs[2] = 0;
        value->limbs[1] = 0;
        value->limbs[0] = 0;
        return;
    }
    if (leftExponent == 0)
    {
        ++exponent;
        if ((value->limbs[2] & 0x7FFFFFFFu) == 0 && value->limbs[1] == 0 && value->limbs[0] == 0)
        {
            value->words[5] = 0;
            return;
        }
    }
    if (rightExponent == 0)
    {
        ++exponent;
        if ((multiplier->limbs[2] & 0x7FFFFFFFu) == 0 && multiplier->limbs[1] == 0 && multiplier->limbs[0] == 0)
        {
            value->limbs[2] = 0;
            value->limbs[1] = 0;
            value->limbs[0] = 0;
            return;
        }
    }
    CrtLongDouble12 product = {};
    for (int column = 0; column < 5; ++column)
    {
        for (int digit = 0; digit < 5 - column; ++digit)
        {
            const unsigned int part = static_cast<unsigned int>(value->words[column + digit]) * multiplier->words[4 - digit];
            unsigned int accumulated = static_cast<unsigned int>(product.words[column]) |
                (static_cast<unsigned int>(product.words[column + 1]) << 16);
            const int carry = __addl(accumulated, part, &accumulated);
            product.words[column] = static_cast<unsigned short>(accumulated);
            product.words[column + 1] = static_cast<unsigned short>(accumulated >> 16);
            if (carry)
                ++product.words[column + 2];
        }
    }
    exponent = static_cast<unsigned short>(exponent - 16382);
    while (static_cast<short>(exponent) > 0 && (product.words[5] & 0x8000u) == 0)
    {
        __shl_12(&product);
        --exponent;
    }
    if (static_cast<short>(exponent) <= 0)
    {
        --exponent;
        if (static_cast<short>(exponent) < 0)
        {
            const int shift = -static_cast<short>(exponent);
            exponent = static_cast<unsigned short>(exponent + shift);
            unsigned int lostBits = 0;
            for (int bit = 0; bit < shift; ++bit)
            {
                if ((product.limbs[0] & 1u) != 0)
                    ++lostBits;
                __shr_12(&product);
            }
            if (lostBits != 0)
                product.words[0] |= 1u;
        }
    }
    if (product.words[0] > 0x8000u || (product.limbs[0] & 0x1FFFFu) == 0x18000u)
    {
        unsigned int low = static_cast<unsigned int>(product.words[1]) |
            (static_cast<unsigned int>(product.words[2]) << 16);
        if (low == 0xFFFFFFFFu)
        {
            product.words[1] = 0;
            product.words[2] = 0;
            unsigned int high = static_cast<unsigned int>(product.words[3]) |
                (static_cast<unsigned int>(product.words[4]) << 16);
            if (high == 0xFFFFFFFFu)
            {
                product.words[3] = 0;
                product.words[4] = 0;
                if (product.words[5] == 0xFFFFu)
                {
                    ++exponent;
                    product.words[5] = 0x8000u;
                }
                else
                    ++product.words[5];
            }
            else
            {
                ++high;
                product.words[3] = static_cast<unsigned short>(high);
                product.words[4] = static_cast<unsigned short>(high >> 16);
            }
        }
        else
        {
            ++low;
            product.words[1] = static_cast<unsigned short>(low);
            product.words[2] = static_cast<unsigned short>(low >> 16);
        }
    }
    if (exponent >= 32767)
    {
        value->limbs[1] = 0;
        value->limbs[0] = 0;
        value->limbs[2] = 0x7FFF8000u + (sign != 0 ? 0x80000000u : 0u);
        return;
    }
    for (int digit = 0; digit < 5; ++digit)
        value->words[digit] = product.words[digit + 1];
    value->words[5] = exponent | sign;
}

static void __multtenpow12(CrtLongDouble12* value, int exponent, int useExtended)
{
    if (exponent == 0)
        return;
    const CrtLongDouble12* table = _pow10pos;
    unsigned int tableOffset = 0;
    if (exponent < 0)
    {
        table = _pow10neg;
        exponent = BitsToSigned32(0u - static_cast<unsigned int>(exponent));
    }
    if (useExtended == 0)
        value->words[0] = 0;
    while (exponent != 0)
    {
        const int digit = exponent & 7;
        exponent >>= 3;
        if (digit != 0)
        {
            const CrtLongDouble12* factor = table + tableOffset + digit - 1;
            CrtLongDouble12 unrounded;
            if (factor->words[0] >= 0x8000u)
            {
                unrounded = *factor;
                unsigned int low = static_cast<unsigned int>(unrounded.words[1]) |
                    (static_cast<unsigned int>(unrounded.words[2]) << 16);
                --low;
                unrounded.words[1] = static_cast<unsigned short>(low);
                unrounded.words[2] = static_cast<unsigned short>(low >> 16);
                factor = &unrounded;
            }
            __ld12mul(value, factor);
        }
        tableOffset += 7;
    }
}

static void __dtold(CrtLongDouble10* output, const double* input)
{
    unsigned int bits[2];
    std::memcpy(bits, input, sizeof(bits));
    const unsigned short sign = static_cast<unsigned short>((bits[1] >> 16) & 0x8000u);
    unsigned short exponent = static_cast<unsigned short>((bits[1] >> 20) & 0x7FFu);
    unsigned int high = bits[1] & 0xFFFFFu;
    const unsigned int low = bits[0];
    unsigned int hidden = 0x80000000u;
    if (exponent != 0)
        exponent = exponent == 2047 ? 32767 : static_cast<unsigned short>(exponent + 15360);
    else
    {
        if (high == 0 && low == 0)
        {
            output->high = 0;
            output->low = 0;
            output->exponent = 0;
            return;
        }
        exponent = 15361;
        hidden = 0;
    }
    high = (high << 11) | (low >> 21) | hidden;
    output->high = high;
    output->low = low << 11;
    while ((high & 0x80000000u) == 0)
    {
        const unsigned int lower = output->low;
        high = (high << 1) | (lower >> 31);
        output->low = lower << 1;
        output->high = high;
        --exponent;
    }
    output->exponent = exponent | sign;
}

static int _I10_OUTPUT(CrtLongDouble10 input, int precision, int flags, CrtDecimalOutput* output)
{
    const unsigned short sign = input.exponent & 0x8000u;
    const unsigned short exponent = input.exponent & 0x7FFFu;
    output->sign = sign != 0 ? '-' : ' ';
    if (exponent == 0 && input.high == 0 && input.low == 0)
    {
        output->decpt = 0;
        output->sign = ' ';
        output->length = 1;
        output->digits[0] = '0';
        output->digits[1] = 0;
        return 1;
    }
    if (exponent == 32767)
    {
        output->decpt = 1;
        const char* text;
        if ((input.high != 0x80000000u || input.low != 0) && (input.high & 0x40000000u) == 0)
        {
            text = "1#SNAN";
            output->length = 6;
        }
        else if (sign != 0 && input.high == 0xC0000000u && input.low == 0)
        {
            text = "1#IND";
            output->length = 5;
        }
        else if (input.high == 0x80000000u && input.low == 0)
        {
            text = "1#INF";
            output->length = 5;
        }
        else
        {
            text = "1#QNAN";
            output->length = 6;
        }
        std::strcpy(output->digits, text);
        return 0;
    }
    CrtLongDouble12 fraction;
    fraction.words[0] = 0;
    fraction.words[1] = static_cast<unsigned short>(input.low);
    fraction.words[2] = static_cast<unsigned short>(input.low >> 16);
    fraction.words[3] = static_cast<unsigned short>(input.high);
    fraction.words[4] = static_cast<unsigned short>(input.high >> 16);
    fraction.words[5] = exponent;
    int decimalExponent = BitsToSigned32(static_cast<unsigned int>(exponent) * 19728u +
        ((static_cast<unsigned int>(exponent) >> 8) + (input.high >> 24) * 2u) * 77u - 323162868u) >> 16;
    __multtenpow12(&fraction, -static_cast<short>(decimalExponent), 1);
    if (fraction.words[5] >= 16383)
    {
        ++decimalExponent;
        const CrtLongDouble12 tenth = {{0xCCCCCCCCu, 0xCCCCCCCCu, 0x3FFBCCCCu}};
        __ld12mul(&fraction, &tenth);
    }
    output->decpt = static_cast<short>(decimalExponent);
    if ((flags & 1) != 0)
    {
        precision = BitsToSigned32(static_cast<unsigned int>(precision) + static_cast<unsigned int>(static_cast<short>(decimalExponent)));
        if (precision <= 0)
        {
            output->decpt = 0;
            output->sign = ' ';
            output->length = 1;
            output->digits[0] = '0';
            output->digits[1] = 0;
            return 1;
        }
    }
    if (precision > 21)
        precision = 21;
    int shift = static_cast<int>(fraction.words[5]) - 16382;
    fraction.words[5] = 0;
    for (int bit = 0; bit < 8; ++bit)
        __shl_12(&fraction);
    if (shift < 0)
        for (int bit = (-shift) & 255; bit > 0; --bit)
            __shr_12(&fraction);
    char* next = output->digits;
    for (int count = BitsToSigned32(static_cast<unsigned int>(precision) + 1u); count > 0; --count)
    {
        const CrtLongDouble12 addend = fraction;
        __shl_12(&fraction);
        __shl_12(&fraction);
        __add_12(&fraction, &addend);
        __shl_12(&fraction);
        const unsigned char digit = static_cast<unsigned char>(fraction.limbs[2] >> 24);
        fraction.limbs[2] &= 0x00FFFFFFu;
        *next++ = static_cast<char>(digit + '0');
    }
    const char roundingDigit = next[-1];
    char* last = next - 2;
    if (static_cast<signed char>(roundingDigit) >= '5')
    {
        while (last >= output->digits && *last == '9')
            *last-- = '0';
        if (last < output->digits)
        {
            ++last;
            ++output->decpt;
        }
        ++*last;
    }
    else
    {
        while (last >= output->digits && *last == '0')
            --last;
        if (last < output->digits)
        {
            output->decpt = 0;
            output->sign = ' ';
            output->length = 1;
            output->digits[0] = '0';
            output->digits[1] = 0;
            return 1;
        }
    }
    output->length = static_cast<char>(last - output->digits + 1);
    output->digits[static_cast<signed char>(output->length)] = 0;
    return 1;
}

static CrtFloatOutput* _fltout(double value, CrtFloatOutput* output, char* mantissa)
{
    CrtLongDouble10 extended;
    __dtold(&extended, &value);
    CrtDecimalOutput decimal;
    output->flag = _I10_OUTPUT(extended, 17, 0, &decimal);
    output->sign = static_cast<signed char>(decimal.sign);
    output->decpt = decimal.decpt;
    std::strcpy(mantissa, decimal.digits);
    output->mantissa = mantissa;
    return output;
}

static void _fptostr(char* output, int digits, CrtFloatOutput* value)
{
    const char* source = value->mantissa;
    char* next = output + 1;
    *output = '0';
    int rounding = digits;
    if (digits > 0)
    {
        rounding = 0;
        do
        {
            const char digit = *source;
            *next++ = digit != 0 ? *source++ : '0';
        }
        while (--digits != 0);
    }
    *next = 0;
    if (rounding >= 0 && static_cast<signed char>(*source) >= '5')
    {
        while (*--next == '9')
            *next = '0';
        ++*next;
    }
    if (*output == '1')
        ++value->decpt;
    else
        std::memmove(output, output + 1, std::strlen(output + 1) + 1);
}

static void shiftFloatText(char* text, int count)
{
    if (count != 0)
        std::memmove(text + count, text, std::strlen(text) + 1);
}

static char* formatExponentialDigits(char* output, int precision, int uppercase, const CrtFloatOutput* value, char general)
{
    if (general != 0)
        shiftFloatText(output + (value->sign == '-'), precision > 0);
    char* next = output;
    if (value->sign == '-')
        *next++ = '-';
    if (precision > 0)
    {
        next[0] = next[1];
        ++next;
        *next = crtDecimalPoint;
    }
    char* exponent = std::strcpy(next + precision + (general == 0), "e+000");
    if (uppercase != 0)
        exponent[0] = 'E';
    ++exponent;
    if (*value->mantissa != '0')
    {
        int magnitude = BitsToSigned32(static_cast<unsigned int>(value->decpt) - 1u);
        if (magnitude < 0)
        {
            magnitude = BitsToSigned32(0u - static_cast<unsigned int>(magnitude));
            *exponent = '-';
        }
        ++exponent;
        if (magnitude >= 100)
        {
            *exponent = static_cast<char>(*exponent + magnitude / 100);
            magnitude %= 100;
        }
        ++exponent;
        if (magnitude >= 10)
        {
            *exponent = static_cast<char>(*exponent + magnitude / 10);
            magnitude %= 10;
        }
        exponent[1] = static_cast<char>(exponent[1] + magnitude);
    }
    return output;
}

static char* formatFixedDigits(char* output, int precision, const CrtFloatOutput* value, char general)
{
    if (general != 0 && BitsToSigned32(static_cast<unsigned int>(value->decpt) - 1u) == precision)
    {
        char* last = output + (value->sign == '-') + precision;
        last[0] = '0';
        last[1] = 0;
    }
    char* next = output;
    if (value->sign == '-')
        *next++ = '-';
    if (value->decpt <= 0)
    {
        shiftFloatText(next, 1);
        *next++ = '0';
    }
    else
        next += value->decpt;
    if (precision > 0)
    {
        shiftFloatText(next, 1);
        *next++ = crtDecimalPoint;
        const int decpt = value->decpt;
        if (decpt < 0)
        {
            const int zeros = BitsToSigned32(0u - static_cast<unsigned int>(decpt));
            if (general != 0 || precision >= zeros)
                precision = zeros;
            shiftFloatText(next, precision);
            std::memset(next, '0', static_cast<unsigned int>(precision));
        }
    }
    return output;
}

static char* _cftoe(const double* value, char* output, int precision, int uppercase)
{
    CrtFloatOutput decimal;
    char mantissa[24];
    volatile double number = *value;
    _fltout(number, &decimal, mantissa);
    _fptostr(output + (decimal.sign == '-') + (precision > 0),
        BitsToSigned32(static_cast<unsigned int>(precision) + 1u), &decimal);
    return formatExponentialDigits(output, precision, uppercase, &decimal, 0);
}

static char* _cftof(const double* value, char* output, int precision)
{
    CrtFloatOutput decimal;
    char mantissa[24];
    volatile double number = *value;
    _fltout(number, &decimal, mantissa);
    _fptostr(output + (decimal.sign == '-'),
        BitsToSigned32(static_cast<unsigned int>(decimal.decpt) + static_cast<unsigned int>(precision)), &decimal);
    return formatFixedDigits(output, precision, &decimal, 0);
}

static char* _cftog(const double* value, char* output, int precision, int uppercase)
{
    CrtFloatOutput decimal;
    char mantissa[24];
    volatile double number = *value;
    _fltout(number, &decimal, mantissa);
    const int oldExponent = BitsToSigned32(static_cast<unsigned int>(decimal.decpt) - 1u);
    char* digits = output + (decimal.sign == '-');
    _fptostr(digits, precision, &decimal);
    const int exponent = BitsToSigned32(static_cast<unsigned int>(decimal.decpt) - 1u);
    const bool rounded = oldExponent < exponent;
    if (exponent >= -4 && exponent < precision)
    {
        if (rounded)
        {
            while (*digits++ != 0)
            {
            }
            digits[-2] = 0;
        }
        return formatFixedDigits(output, precision, &decimal, 1);
    }
    return formatExponentialDigits(output, precision, uppercase, &decimal, 1);
}

static char* __cdecl _cfltcvt(const double* value, char* output, int formatChar, int precision, int uppercase)
{
    if (formatChar == 'e' || formatChar == 'E')
        return _cftoe(value, output, precision, uppercase);
    if (formatChar == 'f')
        return _cftof(value, output, precision);
    return _cftog(value, output, precision, uppercase);
}

static int __cdecl _ZeroTail(const unsigned int* mantissa, int bit)
{
    int word = bit / 32;
    const unsigned int mask = ~(~0u << ((31 - bit % 32) & 31));
    if ((mantissa[word] & mask) != 0u)return 0;
    while (++word < 3)
    {
        if (mantissa[word] != 0u)return 0;
    }
    return 1;
}

static int __cdecl _IncMan(unsigned int* mantissa, int bit)
{
    int word = bit / 32;
    unsigned int addend = 1u << ((31 - bit % 32) & 31);
    unsigned int previous = mantissa[word];
    mantissa[word] = previous + addend;
    int carry = mantissa[word] < previous || mantissa[word] < addend;
    while (--word >= 0 && carry != 0)
    {
        previous = mantissa[word];
        mantissa[word] = previous + 1u;
        carry = mantissa[word] < previous || mantissa[word] < 1u;
    }
    return carry;
}

static int __cdecl _RoundMan(unsigned int* mantissa, int precision)
{
    const int word = precision / 32;
    const int shift = (31 - precision % 32) & 31;
    int carry = 0;
    if ((mantissa[word] & (1u << shift)) != 0u &&
        _ZeroTail(mantissa, precision + 1) == 0)
    {
        carry = _IncMan(mantissa, precision - 1);
    }
    mantissa[word] &= ~0u << shift;
    for (int index = word + 1; index < 3; ++index)mantissa[index] = 0u;
    return carry;
}

static void __cdecl _CopyMan(unsigned int* destination, const unsigned int* source)
{
    for (int index = 0; index < 3; ++index)destination[index] = source[index];
}

static void __cdecl _FillZeroMan(unsigned int* mantissa)
{
    mantissa[0] = 0u;
    mantissa[1] = 0u;
    mantissa[2] = 0u;
}

static int __cdecl _IsZeroMan(const unsigned int* mantissa)
{
    for (int index = 0; index < 3; ++index)
    {
        if (mantissa[index] != 0u)return 0;
    }
    return 1;
}

static void __cdecl _ShrMan(unsigned int* mantissa, int shift)
{
    const int wordShift = shift / 32;
    const int bitShift = shift % 32;
    const unsigned int mask = ~(~0u << (bitShift & 31));
    unsigned int carry = 0u;
    for (int index = 0; index < 3; ++index)
    {
        const unsigned int lowBits = mantissa[index] & mask;
        mantissa[index] = (mantissa[index] >> (bitShift & 31)) | carry;
        carry = lowBits << ((32 - bitShift) & 31);
    }
    for (int index = 2; index >= 0; --index)
    {
        mantissa[index] = index >= wordShift ? mantissa[index - wordShift] : 0u;
    }
}

struct CrtFloatingFormat
{
    int maximumExponent;
    int minimumExponent;
    int significandBits;
    int exponentBits;
    int storageBits;
    int exponentBias;
};

static const CrtFloatingFormat crtDoubleFormat = {1024, -1023, 53, 11, 64, 1023};
static const CrtFloatingFormat crtFloatFormat = {128, -127, 24, 8, 32, 127};

static int __cdecl _ld12cvt(const CrtLongDouble12* value, void* output, const CrtFloatingFormat& format)
{
    const unsigned int sign = value->words[5] & 0x8000u;
    int exponent = static_cast<int>(value->words[5] & 0x7FFFu) - 16383;
    unsigned int mantissa[3] = {
        static_cast<unsigned int>(value->words[3]) |
            (static_cast<unsigned int>(value->words[4]) << 16),
        static_cast<unsigned int>(value->words[1]) |
            (static_cast<unsigned int>(value->words[2]) << 16),
        static_cast<unsigned int>(value->words[0]) << 16
    };
    unsigned int encodedExponent = 0u;
    int status = 0;
    if (exponent == -16383)
    {
        if (_IsZeroMan(mantissa) == 0)
        {
            _FillZeroMan(mantissa);
            status = 2;
        }
    }
    else
    {
        unsigned int originalMantissa[3];
        _CopyMan(originalMantissa, mantissa);
        if (_RoundMan(mantissa, format.significandBits) != 0)++exponent;
        const int minimumExponent = format.minimumExponent;
        if (exponent < minimumExponent - format.significandBits)
        {
            _FillZeroMan(mantissa);
            status = 2;
        }
        else if (exponent <= minimumExponent)
        {
            _CopyMan(mantissa, originalMantissa);
            _ShrMan(mantissa, minimumExponent - exponent);
            _RoundMan(mantissa, format.significandBits);
            _ShrMan(mantissa, format.exponentBits + 1);
            status = 2;
        }
        else if (exponent >= format.maximumExponent)
        {
            _FillZeroMan(mantissa);
            mantissa[0] |= 0x80000000u;
            _ShrMan(mantissa, format.exponentBits);
            encodedExponent = static_cast<unsigned int>(format.exponentBias) +
                static_cast<unsigned int>(format.maximumExponent);
            status = 1;
        }
        else
        {
            mantissa[0] &= 0x7FFFFFFFu;
            encodedExponent = static_cast<unsigned int>(format.exponentBias) +
                static_cast<unsigned int>(exponent);
            _ShrMan(mantissa, format.exponentBits);
        }
    }
    const unsigned int high = mantissa[0] |
        (encodedExponent << ((31 - format.exponentBits) & 31)) |
        (sign != 0u ? 0x80000000u : 0u);
    if (format.storageBits == 64)
    {
        memcpy(static_cast<unsigned char*>(output) + 4, &high, 4);
        memcpy(output, &mantissa[1], 4);
    }
    else if (format.storageBits == 32)
    {
        memcpy(output, &high, 4);
    }
    return status;
}

static int __cdecl _ld12tod(const CrtLongDouble12* value, void* output)
{
    return _ld12cvt(value, output,
        crtDoubleFormat);
}

static int __cdecl _ld12tof(const CrtLongDouble12* value, void* output)
{
    return _ld12cvt(value, output,
        crtFloatFormat);
}

static int __cdecl __strgtold12(CrtLongDouble12* output, const char** end,
    const char* text, int useExtended, int scale, int decimalPosition,
    int allowImplicitExponent)
{
    enum ParseState
    {
        INITIAL, LEADING_ZERO, AFTER_SIGN, INTEGER_DIGITS, FRACTION_DIGITS,
        AFTER_DECIMAL, EXPONENT_START, AFTER_EXPONENT_SIGN, EXPONENT_ZERO,
        EXPONENT_DIGITS, FINISHED, IMPLICIT_EXPONENT
    };
    const char* cursor = text;
    const char* rollback = text;
    char digits[28];
    unsigned int digitCount = 0u;
    unsigned int decimalShift = 0u;
    unsigned int exponentMagnitude = 0u;
    unsigned int sign = 0u;
    bool sawDigit = false;
    bool sawDecimal = false;
    bool sawExponent = false;
    bool negativeExponent = false;
    ParseState state = INITIAL;
    while (*cursor == ' ' || *cursor == '\t' || *cursor == '\n' || *cursor == '\r')
        ++cursor;

    for (;;)
    {
        signed char ch = static_cast<signed char>(*cursor++);
        switch (state)
        {
        case INITIAL:
            if (ch >= '1' && ch <= '9')
            {
                --cursor;
                state = INTEGER_DIGITS;
            }
            else if (ch == static_cast<signed char>(crtDecimalPoint))
                state = AFTER_DECIMAL;
            else if (ch == '+' || ch == '-')
            {
                sign = ch == '-' ? 0x8000u : 0u;
                state = AFTER_SIGN;
            }
            else if (ch == '0')
                state = LEADING_ZERO;
            else
            {
                --cursor;
                goto parsed;
            }
            break;

        case LEADING_ZERO:
            sawDigit = true;
            if (ch >= '1' && ch <= '9')
            {
                --cursor;
                state = INTEGER_DIGITS;
            }
            else if (ch == static_cast<signed char>(crtDecimalPoint))
                state = FRACTION_DIGITS;
            else if (ch == '+' || ch == '-')
            {
                --cursor;
                state = IMPLICIT_EXPONENT;
            }
            else if (ch == '0')
                state = LEADING_ZERO;
            else if (ch == 'D' || ch == 'E' || ch == 'd' || ch == 'e')
                state = EXPONENT_START;
            else
            {
                --cursor;
                goto parsed;
            }
            break;

        case AFTER_SIGN:
            if (ch >= '1' && ch <= '9')
            {
                --cursor;
                state = INTEGER_DIGITS;
            }
            else if (ch == static_cast<signed char>(crtDecimalPoint))
                state = AFTER_DECIMAL;
            else if (ch == '0')
                state = LEADING_ZERO;
            else
            {
                cursor = rollback;
                goto parsed;
            }
            break;

        case INTEGER_DIGITS:
            sawDigit = true;
            while (CrtIsDigit(static_cast<unsigned char>(ch)) != 0u)
            {
                if (digitCount < 25u)
                    digits[digitCount++] = static_cast<char>(ch - '0');
                else
                    ++decimalShift;
                ch = static_cast<signed char>(*cursor++);
            }
            if (ch == static_cast<signed char>(crtDecimalPoint))
            {
                state = FRACTION_DIGITS;
                break;
            }
            goto exponentOrEnd;

        case FRACTION_DIGITS:
            sawDigit = true;
            sawDecimal = true;
            if (digitCount == 0u)
            {
                while (ch == '0')
                {
                    --decimalShift;
                    ch = static_cast<signed char>(*cursor++);
                }
            }
            while (CrtIsDigit(static_cast<unsigned char>(ch)) != 0u)
            {
                if (digitCount < 25u)
                {
                    digits[digitCount++] = static_cast<char>(ch - '0');
                    --decimalShift;
                }
                ch = static_cast<signed char>(*cursor++);
            }
        exponentOrEnd:
            if (ch == '+' || ch == '-')
            {
                --cursor;
                state = IMPLICIT_EXPONENT;
            }
            else if (ch == 'D' || ch == 'E' || ch == 'd' || ch == 'e')
                state = EXPONENT_START;
            else
            {
                --cursor;
                goto parsed;
            }
            break;

        case AFTER_DECIMAL:
            sawDecimal = true;
            if (CrtIsDigit(static_cast<unsigned char>(ch)) != 0u)
            {
                --cursor;
                state = FRACTION_DIGITS;
            }
            else
            {
                cursor = rollback;
                goto parsed;
            }
            break;

        case EXPONENT_START:
            rollback = cursor - 2;
            if (ch >= '1' && ch <= '9')
            {
                --cursor;
                state = EXPONENT_DIGITS;
            }
            else if (ch == '+' || ch == '-')
            {
                negativeExponent = ch == '-';
                state = AFTER_EXPONENT_SIGN;
            }
            else if (ch == '0')
                state = EXPONENT_ZERO;
            else
            {
                cursor = rollback;
                goto parsed;
            }
            break;

        case AFTER_EXPONENT_SIGN:
            if (ch >= '1' && ch <= '9')
            {
                --cursor;
                state = EXPONENT_DIGITS;
            }
            else if (ch == '0')
                state = EXPONENT_ZERO;
            else
            {
                cursor = rollback;
                goto parsed;
            }
            break;

        case EXPONENT_ZERO:
            sawExponent = true;
            while (ch == '0')ch = static_cast<signed char>(*cursor++);
            if (ch >= '1' && ch <= '9')
            {
                --cursor;
                state = EXPONENT_DIGITS;
            }
            else
            {
                --cursor;
                goto parsed;
            }
            break;

        case EXPONENT_DIGITS:
            sawExponent = true;
            exponentMagnitude = 0u;
            while (CrtIsDigit(static_cast<unsigned char>(ch)) != 0u)
            {
                exponentMagnitude = exponentMagnitude * 10u +
                    static_cast<unsigned int>(static_cast<int>(ch) - '0');
                if (BitsToSigned32(exponentMagnitude) > 5200)
                {
                    exponentMagnitude = 5201u;
                    break;
                }
                ch = static_cast<signed char>(*cursor++);
            }
            while (CrtIsDigit(static_cast<unsigned char>(ch)) != 0u)
                ch = static_cast<signed char>(*cursor++);
            --cursor;
            goto parsed;

        case IMPLICIT_EXPONENT:
            if (allowImplicitExponent == 0)
            {
                --cursor;
                goto parsed;
            }
            rollback = cursor - 1;
            if (ch == '+' || ch == '-')
            {
                negativeExponent = ch == '-';
                state = AFTER_EXPONENT_SIGN;
            }
            else
            {
                cursor = rollback;
                goto parsed;
            }
            break;

        case FINISHED:
            goto parsed;
        }
    }

parsed:
    *end = cursor;
    CrtLongDouble12 value;
    int status = 0;
    if (!sawDigit)
    {
        value.limbs[0] = value.limbs[1] = value.limbs[2] = 0u;
        status = 4;
    }
    else
    {
        if (digitCount > 24u)
        {
            if (static_cast<signed char>(digits[23]) >= 5)++digits[23];
            digitCount = 24u;
            ++decimalShift;
        }
        if (digitCount == 0u)
            value.limbs[0] = value.limbs[1] = value.limbs[2] = 0u;
        else
        {
            char* tail = digits + digitCount;
            while (*--tail == 0)
            {
                --digitCount;
                ++decimalShift;
            }
            __mtold12(digits, digitCount, &value);
            unsigned int exponent = (negativeExponent ? 0u - exponentMagnitude :
                exponentMagnitude) + decimalShift;
            if (!sawExponent)exponent += static_cast<unsigned int>(scale);
            if (!sawDecimal)exponent -= static_cast<unsigned int>(decimalPosition);
            if (BitsToSigned32(exponent) > 5200)
            {
                value.limbs[0] = value.limbs[1] = value.limbs[2] = 0u;
                value.words[4] = 0x8000u;
                value.words[5] = 0x7FFFu;
                status = 2;
            }
            else if (BitsToSigned32(exponent) < -5200)
            {
                value.limbs[0] = value.limbs[1] = value.limbs[2] = 0u;
                status = 1;
            }
            else
                __multtenpow12(&value, BitsToSigned32(exponent), useExtended);
        }
    }
    value.words[5] |= static_cast<unsigned short>(sign);
    memcpy(reinterpret_cast<unsigned char*>(output) + 6,
        reinterpret_cast<const unsigned char*>(&value) + 6, 4);
    memcpy(reinterpret_cast<unsigned char*>(output) + 2,
        reinterpret_cast<const unsigned char*>(&value) + 2, 4);
    output->words[5] = value.words[5];
    output->words[0] = value.words[0];
    return status;
}

static int __cdecl _atodbl(void* output, const char* text)
{
    CrtLongDouble12 value;
    __strgtold12(&value, &text, text, 0, 0, 0, 0);
    return _ld12tod(&value, output);
}

static int __cdecl _atoflt(void* output, const char* text)
{
    CrtLongDouble12 value;
    __strgtold12(&value, &text, text, 0, 0, 0, 0);
    return _ld12tof(&value, output);
}

static void __cdecl _fassign(unsigned int mode, void* output, const char* text)
{
    if (mode != 0u)
    {
        unsigned int value[2];
        _atodbl(value, text);
        memcpy(output, &value[0], 4);
        memcpy(static_cast<unsigned char*>(output) + 4, &value[1], 4);
    }
    else
    {
        unsigned int value;
        _atoflt(&value, text);
        memcpy(output, &value, 4);
    }
}

enum CrtFloatInputFlag
{
    FloatInputConversionOverflow = 1,
    FloatInputInvalid = 64,
    FloatInputOverflow = 128,
    FloatInputUnderflow = 256,
    FloatInputNoDigits = 512
};

struct CrtFloatInput
{
    unsigned int flags;
    unsigned int consumed;
    unsigned int reserved[2];
    double value;
};

static CrtFloatInput* __cdecl _fltin(CrtFloatInput* output, const char* text,
    unsigned int length, int scale, int decimalPosition)
{
    (void)length;
    (void)scale;
    (void)decimalPosition;
    CrtLongDouble12 extended;
    const char* end;
    const int parsed = __strgtold12(&extended, &end, text, 0, 0, 0, 0);
    unsigned int flags = 0;
    double value;
    if ((parsed & 4) != 0)
    {
        flags = FloatInputNoDigits;
        value = 0.0;
    }
    else
    {
        const int converted = _ld12tod(&extended, &value);
        if ((parsed & 2) != 0 || converted == 1) flags = FloatInputOverflow;
        if ((parsed & 1) != 0 || converted == 2) flags |= FloatInputUnderflow;
    }
    output->flags = flags;
    output->value = value;
    output->consumed = static_cast<unsigned int>(end - text);
    return output;
}

static double CrtParseDouble(const char* source, char** end)
{
    const char* first = source;
    for (;;)
    {
        const unsigned char character = static_cast<unsigned char>(*first);
        const unsigned int space = crtMaximumMultibyteLength > 1
            ? CrtIsCtype(character, CtypeSpace)
            : crtCharacterTypes[character] & CtypeSpace;
        if (space == 0) break;
        ++first;
    }
    CrtFloatInput parsed;
    _fltin(&parsed, first, static_cast<unsigned int>(std::strlen(first)), 0, 0);
    if (end != 0) *end = const_cast<char*>(first) + parsed.consumed;
    if ((parsed.flags & (FloatInputInvalid | FloatInputNoDigits)) != 0)
    {
        if (end != 0) *end = const_cast<char*>(source);
        return 0.0;
    }
    if ((parsed.flags & (FloatInputConversionOverflow | FloatInputOverflow)) != 0)
    {
        const double huge = std::numeric_limits<double>::infinity();
        const double value = *first == '-' ? -huge : huge;
        errno = ERANGE;
        return value;
    }
    if ((parsed.flags & FloatInputUnderflow) != 0)
    {
        errno = ERANGE;
        return 0.0;
    }
    return parsed.value;
}


static void __cdecl _fptrap()
{
    CrtAmsgExit(2u);
}

struct CrtFloatFunctions
{
    typedef char* (__cdecl *Format)(const double*, char*, int, int, int);
    typedef void (__cdecl *TrimZeroes)(char*);
    typedef void (__cdecl *Assign)(unsigned int, void*, const char*);
    typedef void (__cdecl *ForceDecimal)(char*);
    typedef int (__cdecl *Positive)(const double*);
    Format format;
    TrimZeroes trimZeroes;
    Assign assign;
    ForceDecimal forceDecimal;
    Positive positive;
    Format alternateFormat;
};

static CrtFloatFunctions crtFloatFunctions = {
    reinterpret_cast<CrtFloatFunctions::Format>(&_fptrap),
    reinterpret_cast<CrtFloatFunctions::TrimZeroes>(&_fptrap),
    reinterpret_cast<CrtFloatFunctions::Assign>(&_fptrap),
    reinterpret_cast<CrtFloatFunctions::ForceDecimal>(&_fptrap),
    reinterpret_cast<CrtFloatFunctions::Positive>(&_fptrap),
    reinterpret_cast<CrtFloatFunctions::Format>(&_fptrap)
};
static unsigned int crtFloatingPointDivisionAdjustment = 0;
static unsigned int crtStartupFlag = 0;

static void __cdecl InitializeCrtFloatFormatting()
{
    crtFloatFunctions.trimZeroes = &_cropzeros;
    crtFloatFunctions.format = &_cfltcvt;
    crtFloatFunctions.assign = &_fassign;
    crtFloatFunctions.forceDecimal = &_forcdecpt;
    crtFloatFunctions.positive = &_positive;
    crtFloatFunctions.alternateFormat = &_cfltcvt;


    crtFloatingPointDivisionAdjustment =
        ::IsProcessorFeaturePresent(PF_FLOATING_POINT_PRECISION_ERRATA)?1u:0u;

#if defined(_MSC_VER) && defined(_M_IX86)


    unsigned int x87Control=0u;
    (void)__control87_2(_PC_53, _MCW_PC, &x87Control, 0);
    if (__isa_available >= __ISA_AVAILABLE_SSE2) {
        const unsigned int mxcsr = _mm_getcsr();
        (void)_clearfp();
        _mm_setcsr(mxcsr);
    } else {
        (void)_clearfp();
    }
#endif


}

static void (__cdecl* crtFloatingPointInitializer)() = &InitializeCrtFloatFormatting;

static int __cdecl InitializeCrtNumericConversion()
{
    SetCrtSse2ConversionEnabled(
        ::IsProcessorFeaturePresent(PF_XMMI64_INSTRUCTIONS_AVAILABLE) != FALSE);
    return 0;
}

static unsigned int __cdecl InitializeCrtInitialStorage()
{
    CrtExitCallback* initialStorage = static_cast<CrtExitCallback*>(CrtAllocate(128u));
    crtExitCallbacksBegin = initialStorage;
    if (initialStorage == 0)
    {
        CrtAmsgExit(24u);
        initialStorage = crtExitCallbacksBegin;
    }
    *initialStorage = 0;
    CrtExitCallback* const begin = crtExitCallbacksBegin;
    crtExitCallbacksEnd = begin;
    return PtrToU32(begin);
}

static void __cdecl InitializeCrtStreamTable()
{
    if (crtStreamCount == 0) crtStreamCount = 512;
    else if (crtStreamCount < 20) crtStreamCount = 20;

    crtStreamTable = static_cast<CrtFileRecord**>(
        std::calloc(static_cast<unsigned int>(crtStreamCount), sizeof(CrtFileRecord*)));
    if (!crtStreamTable)
    {
        crtStreamCount = 20;
        crtStreamTable = static_cast<CrtFileRecord**>(
            std::calloc(20u, sizeof(CrtFileRecord*)));
        if (!crtStreamTable) CrtAmsgExit(26u);
    }

    for (int stream = 0; stream < 20; ++stream)
        crtStreamTable[stream] = &crtInitialStreams[stream];

    for (int stream = 0; stream < 3; ++stream)
    {
        std::intptr_t handle = _get_osfhandle(stream);
        if (handle == -2) handle = -1;
        if (handle == -1 || handle == 0) crtInitialStreams[stream].file = -1;
    }
}

static unsigned int __cdecl InitializeCrtStartupFlag()
{


    if (crtStartupFlag == 0u) crtStartupFlag = 1u;
    return 1u;
}




static LPTOP_LEVEL_EXCEPTION_FILTER crtPreviousExceptionFilter = 0;

static LONG WINAPI CrtUnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionPointers)
{
    const EXCEPTION_RECORD& record = *exceptionPointers->ExceptionRecord;
    if (record.ExceptionCode == 0xE06D7363u && record.NumberParameters == 3u &&
        record.ExceptionInformation[0] == 0x19930520u)
    {
        std::terminate();
    }
    const LPTOP_LEVEL_EXCEPTION_FILTER previous = crtPreviousExceptionFilter;
    if (previous && !IsBadCodePtr(reinterpret_cast<FARPROC>(previous)))
    {
        const LPTOP_LEVEL_EXCEPTION_FILTER callback = crtPreviousExceptionFilter;
        return callback(exceptionPointers);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static unsigned int __cdecl InstallCrtExceptionFilter()
{
    const LPTOP_LEVEL_EXCEPTION_FILTER previous = SetUnhandledExceptionFilter(&CrtUnhandledExceptionFilter);
    crtPreviousExceptionFilter = previous;
    return PtrToU32(previous);
}

struct FindTimeZone
{
    long timezone;
    int daylight;
    long dstbias;
    bool windowsRules;
    TIME_ZONE_INFORMATION rules;

    FindTimeZone() : timezone(28800), daylight(1), dstbias(-3600), windowsRules(false), rules
{
}
    {
        const char* zone=std::getenv("TZ");
        if(zone && *zone)
        {
            const char* value=zone+3;
            const bool negative=*value=='-';
            if(negative)++value;
            timezone=BitsToSigned32(CrtAtoiText(value)*3600u);
            while(*value=='+' || (*value>='0' && *value<='9'))++value;
            if(*value==':')
            {
                ++value;
                timezone=BitsToSigned32(static_cast<unsigned int>(timezone)+CrtAtoiText(value)*60u);
                while(*value>='0' && *value<='9')++value;
                if(*value==':')
                {
                    ++value;
                    timezone=BitsToSigned32(static_cast<unsigned int>(timezone)+CrtAtoiText(value));
                    while(*value>='0' && *value<='9')++value;
                }
            }
            if(negative)timezone=BitsToSigned32(0u-static_cast<unsigned int>(timezone));
            daylight=static_cast<signed char>(*value);
            return;
        }
        if(GetTimeZoneInformation(&rules)==TIME_ZONE_ID_INVALID)return;
        windowsRules=true;
        timezone=rules.Bias*60;
        if(rules.StandardDate.wMonth!=0)timezone+=rules.StandardBias*60;
        if(rules.DaylightDate.wMonth!=0 && rules.DaylightBias!=0)
        {
            daylight=1;
            dstbias=(rules.DaylightBias-rules.StandardBias)*60;
        }
        else
        {
            daylight=0;
            dstbias=0;
        }
    }
};

struct FindTimeTransition
{
    int day;
    int milliseconds;
};

static FindTimeTransition FindTimeTransitionForYear(int year, const SYSTEMTIME& rule,
    bool daylightStart, long daylightBias)
{
    const bool leapYear = year%4 == 0 && (year%100 != 0 || year%400 == 0);
    static const int monthStarts[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int firstDay = monthStarts[rule.wMonth]+(leapYear && rule.wMonth>2u);
    FindTimeTransition transition;
    if (rule.wYear == 0u)
    {
        const int yearOffset = year-1900;
        const int firstWeekday = (yearOffset*365+(yearOffset-1)/4-(yearOffset-1)/100+
            (yearOffset+299)/400+firstDay-25563)%7;
        transition.day = firstDay+7*rule.wDay-firstWeekday+rule.wDayOfWeek;
        if (firstWeekday <= rule.wDayOfWeek)
            transition.day -= 7;
        if (rule.wDay == 5u)
        {
            const int nextMonth = rule.wMonth == 12u ? 365+leapYear :
                monthStarts[rule.wMonth+1u]+(leapYear && rule.wMonth+1u>2u);
            if (transition.day >= nextMonth)
                transition.day -= 7;
        }
    }
    else
    {
        transition.day = firstDay+rule.wDay-1;
    }
    const unsigned int seconds = (static_cast<unsigned int>(rule.wHour)*60u+rule.wMinute)*60u+rule.wSecond;
    transition.milliseconds = BitsToSigned32(seconds*1000u+rule.wMilliseconds);
    if (!daylightStart)
    {
        transition.milliseconds = AddSigned32Wrap(transition.milliseconds,
            MultiplySigned32Wrap(daylightBias, 1000));
        if (transition.milliseconds < 0)
        {
            transition.milliseconds = AddSigned32Wrap(transition.milliseconds, 86400000);
            --transition.day;
        }
        else if (transition.milliseconds >= 86400000)
        {
            transition.milliseconds = SubSigned32Wrap(transition.milliseconds, 86400000);
            ++transition.day;
        }
    }
    return transition;
}

static bool FindTimeIsDaylight(const FindTimeZone& zone, const SYSTEMTIME& calendar, int day)
{
    if (zone.daylight == 0)
        return false;
    SYSTEMTIME startRule;
    SYSTEMTIME endRule;
    if (zone.windowsRules)
    {
        startRule = zone.rules.DaylightDate;
        endRule = zone.rules.StandardDate;
    }
    else
    {
        std::memset(&startRule, 0, sizeof(startRule));
        std::memset(&endRule, 0, sizeof(endRule));
        startRule.wMonth = calendar.wYear >= 2007u ? 3u : 4u;
        startRule.wDay = calendar.wYear >= 2007u ? 2u : 1u;
        startRule.wHour = 2u;
        endRule.wMonth = calendar.wYear >= 2007u ? 11u : 10u;
        endRule.wDay = calendar.wYear >= 2007u ? 1u : 5u;
        endRule.wHour = 2u;
    }
    const FindTimeTransition start = FindTimeTransitionForYear(calendar.wYear, startRule, true, zone.dstbias);
    const FindTimeTransition end = FindTimeTransitionForYear(calendar.wYear, endRule, false, zone.dstbias);
    if (start.day < end.day)
    {
        if (day < start.day || day > end.day)
            return false;
        if (day > start.day && day < end.day)
            return true;
    }
    else
    {
        if (day < end.day || day > start.day)
            return true;
        if (day > end.day && day < start.day)
            return false;
    }
    const int milliseconds = ((calendar.wHour*60+calendar.wMinute)*60+calendar.wSecond)*1000;
    return day == start.day ? milliseconds >= start.milliseconds : milliseconds < end.milliseconds;
}

static __time64_t FindDataTime(const FILETIME& time)
{
    if (time.dwLowDateTime == 0u && time.dwHighDateTime == 0u)return -1;
    FILETIME localTime;
    SYSTEMTIME calendar;
    if (!FileTimeToLocalFileTime(&time, &localTime))return -1;
    if (!FileTimeToSystemTime(&localTime, &calendar))return -1;
    const int year = calendar.wYear;
    const unsigned int month = calendar.wMonth;
    const bool leapYear = year%4 == 0 && (year%100 != 0 || year%400 == 0);
    static const int monthStarts[13] = {0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    static const unsigned int monthDays[13] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year<1970 || year>3000 || month<1u || month>12u ||
        calendar.wHour>23u || calendar.wMinute>59u || calendar.wSecond>59u ||
        calendar.wDay<1u || calendar.wDay>monthDays[month]+(leapYear && month == 2u))
    {
        errno = EINVAL;
        return -1;
    }
    static const FindTimeZone zone;
    const int leapDays = (year-1)/4-(year-1)/100+(year-1)/400-477;
    const int day = monthStarts[month]+calendar.wDay-1+(leapYear && month>2u);
    const __time64_t days = static_cast<__time64_t>(year-1970)*365+leapDays+day;
    __time64_t result = (((days*24+calendar.wHour)*60+calendar.wMinute)*60+calendar.wSecond)+zone.timezone;
    if (FindTimeIsDaylight(zone, calendar, day))
        result += zone.dstbias;
    return result;
}

intptr_t __cdecl CrtFindFirst(const char* path, _finddata64i32_t* data)
{
    if (!data || !path)
    {
        errno = EINVAL;
        ::_invalid_parameter_noinfo();
        return -1;
    }
    WIN32_FIND_DATAA found;
    const HANDLE handle = FindFirstFileA(path, &found);
    if (handle == INVALID_HANDLE_VALUE)
    {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_NO_MORE_FILES)
            errno = ENOENT;
        else if (error == ERROR_NOT_ENOUGH_MEMORY)
            errno = ENOMEM;
        else
            errno = EINVAL;
        return -1;
    }
    data->attrib = found.dwFileAttributes == FILE_ATTRIBUTE_NORMAL?0u:found.dwFileAttributes;
    data->time_create = FindDataTime(found.ftCreationTime);
    data->time_access = FindDataTime(found.ftLastAccessTime);
    data->time_write = FindDataTime(found.ftLastWriteTime);
    data->size = found.nFileSizeLow;
    if (::strcpy_s(data->name, sizeof(data->name), found.cFileName) != 0)
        ::_invoke_watson(0, 0, 0, 0, 0);
    return reinterpret_cast<intptr_t>(handle);
}

int __cdecl CrtFindClose(intptr_t handle)
{
    if (FindClose(reinterpret_cast<HANDLE>(handle))) return 0;
    errno = EINVAL;
    return -1;
}


namespace {




static unsigned int __cdecl RegisterSharedStartupCleanup()
{
    unsigned char guard = CrtStaticInitializationGuards[0];
    if ((guard & 1u) == 0u)
        CrtStaticInitializationGuards[0] = static_cast<unsigned char>(guard | 1u);
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}


static int RunCrtStaticInitializers()
{
    LONG state = InterlockedCompareExchange(&crtStaticInitializationState, 1, 0);
    if (state == 2)return 1;
    if (state == 1)
    {
        while (crtStaticInitializationState == 1)Sleep(0);
        return crtStaticInitializationState == 2;
    }


    (void)RegisterCrtExitDrain();

    void (__cdecl* const initializeFloatingPoint)() = crtFloatingPointInitializer;
    if (initializeFloatingPoint != 0) initializeFloatingPoint();


    InitializeCrtStreamTable();
    InitializeCrtInitialStorage();
    InitializeCrtStartupFlag();
    (void)InitializeCrtNumericConversion();
    InstallCrtExceptionFilter();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    ENGINE::InitializePathDots();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    InitializeGraphBlackAtStartup();
    InitializeGraphCyanAtStartup();
    InitializeGraphMagentaAtStartup();
    InitializeGraphBrownAtStartup();
    InitializeGraphWhiteAtStartup();
    InitializeGraphGrayAtStartup();
    InitializeGraphBlueAtStartup();
    InitializeGraphLightBlueAtStartup();
    InitializeGraphGreenAtStartup();
    InitializeGraphLightGreenAtStartup();
    InitializeGraphRedAtStartup();
    InitializeGraphLightRedAtStartup();
    InitializeGraphYellowAtStartup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    InitializeMapLoadingDirectionAtStartup();
    InitializeMapLoadingTextAtStartup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    SharedNoopStaticInitializer();
    SharedNoopStaticInitializer();
    RefreshQuarterTickAtStartup();
    MylibStaticCleanupGuard();
    NetHostListStaticInit();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    R_MAP::InitializeStatic();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    SharedNoopStaticInitializer();
    BaseTexture_StaticCleanupGuard01();
    BaseTexture_StaticCleanupGuard02();
    BaseTexture_StaticCleanupGuard03();
    BaseTexture_StaticCleanupGuard04();
    BaseTexture_StaticCleanupGuard05();
    SharedNoopStaticInitializer();
    InitializeVidRootAtStartup();
    InitializeVidFirstRangeAtStartup();
    InitializeVidSecondRangeAtStartup();
    zIniParser_RegisterSharedCleanupOnce();
    zItemInfo_RegisterSharedCleanupOnce();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    zItemTaker::RegisterNetResponseSlots();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    zScriptExec::RegisterSurviveReference();
    zScriptExec::RegisterCampaignReference();
    SharedNoopStaticInitializer();
    RegisterSharedStartupCleanup();
    zSurvive::RegisterMonsterCountCache();
    zSurvive::RegisterMonsterTimeCache();
    RegisterSharedStartupCleanup();
    for (unsigned int index = 0; index < 4u; ++index)
    {
        const unsigned char guardValue = CrtStaticInitializationGuards[index];
        if ((guardValue & 1u) == 0u)
            CrtStaticInitializationGuards[index] = static_cast<unsigned char>(guardValue | 1u);
        (void)RegisterCrtAtExit(&MapStaticCleanupNoOp);
    }
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    InitializeInputClickPosition();
    SharedNoopStaticInitializer();
    InitializeInputMappingDefaults();
    SharedNoopStaticInitializer();
    InitializeSharedCleanupGuardPair();
    InitializeSharedCleanupGuardGroup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    RegisterSharedStartupCleanup();
    MainSwGlobalStringStaticInit();
    DialogSrvRegisterStaticCleanupSet();
    RegisterSharedStartupCleanup();
    ManStaticInitGlobalColorPalette();
    SharedNoopStaticInitializer();
    PLAYER_ARCADE::InitializeMessage();
    SharedNoopStaticInitializer();
    StaticAtexitGuard();

    InterlockedExchange(&crtStaticInitializationState, 2);
    return 1;
}

}

static int PublishCrtModulePath()
{
    char* const modulePath = crtModulePath;
    const DWORD modulePathLength = GetModuleFileNameA(0, modulePath, 260u);
    if (modulePathLength == 0u)
    {
        modulePath[0] = 0;
        return 0;
    }
    modulePath[259] = 0;
    crtProgramPath = modulePath;
    return 1;
}

int AS2RuntimeInitializeStandalone()
{
#if !defined(_WIN32)
    return 1;
#else

    CrtInitLocks();


    if(!PublishCrtModulePath())return 0;
    if(!RunCrtStaticInitializers()){

        return 0;
    }
    return 1;
#endif
}

#include <stdarg.h>
#include <float.h>
#include <string.h>
#include "myerror.h"
#include "unit.h"


static unsigned int __cdecl MainSwGlobalStringStaticInit();
static void __cdecl MainSwGlobalStringInitialize()
{
    ::new (synchroErrorLogFileNameStorage.Address()) STRING();
}
static unsigned int __cdecl MainSwGlobalStringRegisterCleanup()
{
    return RegisterCrtAtExit(&DialogServiceGlobalStringCleanup);
}
static unsigned int __cdecl MainSwGlobalStringStaticInit()
{
    MainSwGlobalStringInitialize();
    return MainSwGlobalStringRegisterCleanup();
}

void __cdecl DialogServiceGlobalStringCleanup()
{
    synchroErrorLogFileNameStorage.Get().~STRING();
}



