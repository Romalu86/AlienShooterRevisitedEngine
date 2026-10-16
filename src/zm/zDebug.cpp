#include "basetype.h"
#include "../engine_globals.h"
#include "../myerror.h"
#include "zm/Utils/zLogConsole.h"

void __cdecl DebugAssert(int severity, const char* expression, const char* sourceFile, int sourceLine)
{
    iLogConsole* const logger =
        ::gpLogCons;
    if (logger != 0)
    {
        logger->LogStr(static_cast<LOG_TYPE>(severity), "Assert(%s) in file %s line %d",
            expression, sourceFile, sourceLine);
    }
    else
    {
        MYERROR* const error = ::Error;
        if (error != 0)
            error->Log("Assert(%s) in file %s line %d", expression, sourceFile, sourceLine);
    }
}

