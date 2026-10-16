#include "myerror.h"
#include "engine_globals.h"
#include "exports.h"
#include "basetype.h"
#include "mylib.h"
#include <windows.h>
#include <dbghelp.h>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>

MYERROR* Error=0;

namespace {
STRING& MyErrorGlobalLogPath()
{
    static STRING path;
    return path;
}

STRING& MyErrorGlobalLogDirectory()
{
    static STRING directory;
    return directory;
}

int myErrorExceptionFilterInstalled = 0;

STRING MyErrorModuleFileName()
{
    char path[4096];
    std::memset(path, 0, sizeof(unsigned int));
    (void)GetModuleFileNameA(0, path, sizeof(path));
    return STRING(path);
}

STRING MyErrorExecutableStem()
{
    STRING module = MyErrorModuleFileName();
    STRING baseName = module.AfterLast("\\");
    return baseName.BeforeLast(".");
}

int MyErrorLogExceptionRecord(EXCEPTION_RECORD* record)
{
    MYERROR* const error = ::Error;
    const char* format = 0;
    switch (record->ExceptionCode)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    {
        STRING time = GetTimeString();
        error->Log(record->ExceptionInformation[0] != 0
            ? "!!!ERROR EXCEPTION %s 0x%X!!!: Access violation write to 0x%X"
            : "!!!ERROR EXCEPTION %s 0x%X!!!: Access violation read from 0x%X",
            time.value, record->ExceptionAddress, record->ExceptionInformation[1]);
        return 1;
    }
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_ARRAY_BOUNDS_EXCEEDED"; break;
    case EXCEPTION_BREAKPOINT:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_BREAKPOINT"; break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_DATATYPE_MISALIGNMENT"; break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_DENORMAL_OPERAND"; break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        format = "!!!ERROR EXCEPTION 0x%X!!!:FLT divide by zero"; break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_INEXACT_RESULT"; break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_INVALID_OPERATION"; break;
    case EXCEPTION_FLT_OVERFLOW:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_OVERFLOW"; break;
    case EXCEPTION_FLT_STACK_CHECK:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_STACK_CHECK"; break;
    case EXCEPTION_FLT_UNDERFLOW:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_FLT_UNDERFLOW"; break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_ILLEGAL_INSTRUCTION"; break;
    case EXCEPTION_IN_PAGE_ERROR:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_IN_PAGE_ERROR"; break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        format = "!!!ERROR EXCEPTION 0x%X!!!:INT divide by zero"; break;
    case EXCEPTION_INT_OVERFLOW:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_INT_OVERFLOW"; break;
    case EXCEPTION_INVALID_DISPOSITION:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_INVALID_DISPOSITION"; break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_NONCONTINUABLE_EXCEPTION"; break;
    case EXCEPTION_PRIV_INSTRUCTION:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_PRIV_INSTRUCTION"; break;
    case EXCEPTION_SINGLE_STEP:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_SINGLE_STEP"; break;
    case EXCEPTION_STACK_OVERFLOW:
        format = "!!!ERROR EXCEPTION 0x%X!!!: EXCEPTION_STACK_OVERFLOW"; break;
    default:
        return 1;
    }
    error->Log(format, record->ExceptionAddress);
    return 1;
}

LONG WINAPI MyErrorUnhandledExceptionFilter(EXCEPTION_POINTERS* pointers)
{
    HMODULE library = 0;
    char libraryPath[260];
    if (GetModuleFileNameA(0, libraryPath, sizeof(libraryPath)) != 0)
    {
        char* const baseName = std::strrchr(libraryPath, '\\');
        if (baseName != 0)
        {
            std::strcpy(baseName + 1, "dbghelp.dll");
            library = LoadLibraryA(libraryPath);
        }
    }
    if (library == 0)
        library = LoadLibraryA("dbghelp.dll");
    if (library == 0)
    {
        MYERROR* const error = ::Error;
        if (error != 0) error->Log("dbghelp.dll not found");
        return 0;
    }

    typedef BOOL (WINAPI *MiniDumpWriter)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
        const MINIDUMP_EXCEPTION_INFORMATION*, const MINIDUMP_USER_STREAM_INFORMATION*,
        const MINIDUMP_CALLBACK_INFORMATION*);
    MiniDumpWriter const writeDump = reinterpret_cast<MiniDumpWriter>(
        GetProcAddress(library, "MiniDumpWriteDump"));
    if (writeDump == 0)
    {
        MYERROR* const error = ::Error;
        if (error != 0) error->Log("dbghelp.dll too old");
        return 0;
    }

    STRING dumpName;
    STRING& directory = MyErrorGlobalLogDirectory();
    if (directory.value[0] == 0)
    {
        STRING module = MyErrorModuleFileName();
        STRING stem = module.BeforeLast(".");
        dumpName = stem + "_dump.dmp";
    }
    else
    {
        STRING stem = MyErrorExecutableStem();
        STRING withSlash = directory + "\\";
        STRING prefix = withSlash + stem;
        dumpName = prefix + "_dump.dmp";
    }

    LONG result = 0;
    const HANDLE dump = CreateFileA(dumpName.value, GENERIC_WRITE, FILE_SHARE_WRITE,
        0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (dump == INVALID_HANDLE_VALUE)
    {
        if (::Error != 0)
        {
            const DWORD code = GetLastError();
            MYERROR* const error = ::Error;
            error->Log("Can't create dump file '%s' (error %d)", dumpName.value, code);
        }
    }
    else
    {
        MINIDUMP_EXCEPTION_INFORMATION info;
        info.ThreadId = GetCurrentThreadId();
        info.ExceptionPointers = pointers;
        info.ClientPointers = FALSE;
        const DWORD processId = GetCurrentProcessId();
        const HANDLE process = GetCurrentProcess();
        if (writeDump(process, processId, dump, MiniDumpNormal, &info, 0, 0) != 0)
        {
            MYERROR* const error = ::Error;
            if (error != 0) error->Log("Saved dump file to '%s'", dumpName.value);
            result = 1;
        }
        else if (::Error != 0)
        {
            const DWORD code = GetLastError();
            MYERROR* const error = ::Error;
            error->Log("Failed to save dump file to '%s' (error %d)", dumpName.value, code);
        }
        (void)CloseHandle(dump);
    }
    if (::Error != 0)
        (void)MyErrorLogExceptionRecord(pointers->ExceptionRecord);
    return result;
}

const char* const errorCategories[15]={
    "0x%X Couldn't lock %s",
    "0x%X Couldn't copy %s",
    "%i There was not enough memory for %s",
    "0x%X Couldn't create the %s",
    "0x%X Invalid %s",
    "0x%X Load %s",
    "0x%X Save %s",
    "0x%X Couldn't open '%s'",
    "0x%X Couldn't set the %s",
    "0x%X Couldn't get the %s",
    "%i %s",
    "0x%X Section can't found (%s)",
    "0x%X Unable initialize %s",
    "%i Missing %s",
    "%i Unknownn %s"
};
}

STRING MYERROR::LogFileName()
{
    return MyErrorGlobalLogPath();
}

MYERROR::MYERROR(const char* baseLogDirectory, int timestampedFile, int removeOnDestruction)
{
    if (myErrorExceptionFilterInstalled == 0)
        (void)SetUnhandledExceptionFilter(&MyErrorUnhandledExceptionFilter);
    hwnd = 0;
    removeLogOnDestruction = removeOnDestruction;
    myErrorExceptionFilterInstalled = 1;
    std::strcpy(filename, baseLogDirectory);
    MyErrorGlobalLogDirectory() = STRING(filename);
    STRING& logPath = MyErrorGlobalLogPath();

    {
        STRING timeText = GetTimeString();
        STRING dateText = GetDateString();
        STRING errorStem = "\\error"+dateText;
        STRING stemWithSpace = errorStem+" ";
        STRING timestampedName = stemWithSpace+timeText;
        STRING timestampedLog = timestampedName+".log";
        logPath = timestampedLog;
    }

    logPath.Replace(":", "h");
    logPath.Replace(":", "m");
    {
        STRING base(filename);
        STRING prefixed = base+logPath;
        logPath = prefixed;
    }

    if (timestampedFile == 0)
    {
        STRING base(filename);
        logPath = base + "\\error.log";
    }
    {
        STRING fileMode(timestampedFile != 0 ? "wt" : "at");
        file = logPath.value[0] != 0 ? std::fopen(logPath.value, fileMode.value) : 0;
    }
    if (file == 0)
    {
        logPath = logPath.AfterLast("\\");
        STRING fileMode(timestampedFile != 0 ? "wt" : "at");
        file = logPath.value[0] != 0 ? std::fopen(logPath.value, fileMode.value) : 0;
    }

    STRING version = MyErrorModuleFileName();
    STRING bannerTime = GetTimeString();
    STRING bannerDate = GetDateString();
    Log(
        "----< %s %s >----< %s (%s %s) >----",
        bannerDate.value, bannerTime.value, version.value, "Jun  8 2009", "13:29:37");
}

MYERROR::~MYERROR()
{
    if (file != 0)
        std::fclose(file);
    file = 0;

    STRING& logPath = MyErrorGlobalLogPath();
    if (removeLogOnDestruction != 0)
    {
        (void)std::remove(logPath.value);
        return;
    }
    bool moveErrorLog;
    {
        STRING path(logPath);
        STRING afterLogs = path.After("Logs\\");
        moveErrorLog = std::strcmp(afterLogs.value, "error.log") != 0;
        if (moveErrorLog)
        {
            STRING path(logPath);
            STRING baseName = path.AfterLast("\\");
            moveErrorLog = std::strcmp(baseName.value, "error.log") != 0;
        }
    }
    if (!moveErrorLog)
        return;

    {
        STRING base(filename);
        STRING target = base+"\\error.log";
        (void)std::remove(target.value);
    }
    bool moved;
    {
        STRING base(filename);
        STRING target = base+"\\error.log";
        moved = MoveFileExA(logPath.value, target.value,
                         MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH) != 0;
    }
    if (moved)
        return;

    {
        STRING target("error.log");
        (void)std::remove(target.value);
    }
    {
        STRING target("error.log");
        (void)MoveFileExA(logPath.value, target.value,
                        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);
    }
}

int __cdecl MYERROR::Window(STRING text, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.m_value, args);
    va_end(args);

    if (file != 0)
    {
        std::fputs(buffer, file);
        std::fputs("\n", file);
        std::fflush(file);
    }
    if (hwnd == 0)
        hwnd = GetForegroundWindow();
    const int result = MessageBoxA(hwnd, buffer, "Error", MB_OK);
    return result;
}

void __cdecl MYERROR::LogTmp(STRING text, ...)
{
    if (file == 0)
    {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.m_value, args);
    va_end(args);
    const long position = std::ftell(file);
    std::fputs(buffer, file);
    std::fputs("\n", file);
    std::fflush(file);
    std::fseek(file, position, SEEK_SET);
}

void __cdecl MYERROR::Log(STRING text, ...)
{
    if (file == 0)
    {
        return;
    }
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.m_value, args);
    va_end(args);
    std::fputs(buffer, file);
    std::fputs("\n", file);
    std::fflush(file);
}

void __cdecl MYERROR::LogExit(STRING text, ...)
{
    char buffer[4096];
    va_list args;
    va_start(args, text);
    std::vsprintf(buffer, text.m_value, args);
    va_end(args);

    Log(buffer);
    if (file != 0)
        std::fclose(file);
    file = 0;
    std::exit(1);
}

void __cdecl MYERROR::Error(const STRING& objectFormat, TYPE_ERROR type,
                            const STRING& subject, unsigned long errorCode, ...)
{
    char buffer[4096];
    {
        STRING timeText = GetTimeString();
        std::sprintf(buffer, "!!!ERROR %s/%u!!!", timeText.m_value,
                     ::CurrentTime);
    }

    va_list args;
    va_start(args, errorCode);
    std::vsprintf(buffer+std::strlen(buffer), objectFormat.m_value, args);
    va_end(args);
    std::strcat(buffer, ": ");
    if (static_cast<unsigned int>(type) <= E_UNKNOWN)
        std::strcat(buffer, errorCategories[static_cast<unsigned int>(type)]);

    Log(buffer, errorCode, subject.m_value);
}


void __cdecl MYERROR::Log(const char* format, ...)
{
    FILE* const stream = file;
    if (stream != 0)
    {
        char text[4096];
        va_list args;
        va_start(args, format);
        std::vsprintf(text, format, args);
        va_end(args);
        std::fputs(text, file);
        std::fputs("\n", file);
        std::fflush(file);
    }
}

void __cdecl MYERROR::LogExit(const char* format, ...)
{
    char text[4096];
    va_list args;
    va_start(args, format);
    std::vsprintf(text, format, args);
    va_end(args);

    Log(text);

    FILE* const stream = file;
    if (stream != 0)
        std::fclose(stream);
    file = 0;
    std::exit(1);
}
