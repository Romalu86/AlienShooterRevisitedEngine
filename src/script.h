#pragma once

#include <stddef.h>
#include "mylib.h"
#include "myerror.h"
#include "basetype.h"
#include "list.h"

class STREAM;

struct SCRIPTSTACK {
    int m_intValue;
    STRING m_stringValue;
    int m_elementCount;
    unsigned char m_flags;
    unsigned char m_padding[3];

    SCRIPTSTACK();
    SCRIPTSTACK(const SCRIPTSTACK& source);
    SCRIPTSTACK(const STRING& stringValue, int elementCount = 1, unsigned char flags = 0);
    SCRIPTSTACK(int integerValue, int elementCount = 1, unsigned char flags = 0);
    ~SCRIPTSTACK();
    const SCRIPTSTACK& operator = (const SCRIPTSTACK& source);
    int Int() const;
    const STRING& String();
    void Read(STREAM* stream);
    void BinarOperator(int operation, const SCRIPTSTACK& rhs);
};

extern "C" void*  ScriptStackVectorDeletingDestructorAlias(void* self, unsigned int flags);




struct SCRIPTVAR {
    unsigned char m_type;
    unsigned char m_functionFlags;
    unsigned char m_padding[2];
    int m_valueOrOffset;
    union {
        int m_parameterStackBase;
        int m_labelOffset;
    };
    int m_elementOrParameterCount;
    int m_sourceFileIndex;
    int m_sourceLine;
};

struct SCRIPTLINE {
    unsigned short m_line;
    unsigned short m_bytecodeSpan;
};

struct SCRIPTSOURCE {
    STRING m_fileName;
    unsigned int m_bytecodeBegin;
    unsigned int m_bytecodeEnd;
    unsigned int m_lineCount;
    SCRIPTLINE* m_lines;

    SCRIPTSOURCE();
    ~SCRIPTSOURCE();
    void Begin(const STRING& fileName, unsigned int bytecodeCursor);
    void Finish(unsigned int bytecodeCursor);
    void AddLine(unsigned int bytecodeCursor, unsigned int line);
    void Release();
};

template<> LIST<SCRIPTSTACK>::~LIST();
template<> LIST<NAMED_LIST_STRUCT<SCRIPTVAR> >::~LIST();
template<> LIST<NAMED_LIST_STRUCT<STRING> >::~LIST();

struct SCRIPT {

    SCRIPT();
    ~SCRIPT();

    LIST<SCRIPTSTACK> m_stack;
    NAMED_LIST<SCRIPTVAR> m_functionList;
    NAMED_LIST<STRING> m_defineList;
    SCRIPTVAR* m_currentFunction;
    SCRIPTSOURCE m_sources[256];
    STRING m_scriptFileName;
    void* m_allocation38;
    unsigned char* m_bytecode;
    int m_bytecodeCursor;
    char* m_sourceCursor;
    char* m_sourceEnd;
    void* m_sourceAllocation;
    unsigned int m_sourceFileIndex;
    unsigned int m_sourceFileCount;
    int m_sourceLine;
    int m_conditionalDepth;
    int m_fallbackFunction;
    int m_parseMode;
    int m_variableMap[256];
    int m_eventMap[256];
    unsigned int m_instructionTraceAddresses[2];
    int m_errorLatch;
    int m_parserMode;

    void PushStr(const STRING& value);
    void PushInt(int value);
    void PushObject(void* object, const STRING& tag);
    int IsLastStackString() const;
    void LoadVar(STREAM* stream);
    int DeletePointerToObject(void* objectToClear);

    void Release();
    int LoadLGC(const STRING& fileName);
    int Load(const STRING& fileName);
    void Error(TYPE_ERROR type, const STRING& subject, int errorArgument);
    STRING GetVariableStr(const STRING& expression);
    void SetVariableStr(const STRING& name, const STRING& source);
    int GetVariableActionInt(unsigned int actionIndex);
    void SetVariableActionInt(unsigned int actionIndex, int integerValue);
    int GetNFunction(const STRING& name) const;
    int CallScriptEventFunction(int eventSlot, int argument1, int argument2, int argument3);
    int CallFunction(const STRING& name, const char* signature, int argument1, int argument2, int argument3);
    int CallFunction(int function, const char* signature, int argument1, int argument2, int argument3);
    void RuntimeError(int cursor, const STRING& subject, int errorArgument) const;
    void RuntimeVariableError(int cursor, int stackIndex, const STRING& subject, int errorArgument) const;

private:
    enum TYPE_VAR {
        TYPE_VAR_RUNTIME_LOCAL = 0,
        TYPE_VAR_STATIC_LOCAL = 1,
        TYPE_VAR_GLOBAL = 2,
        TYPE_VAR_STATIC_GLOBAL = 3,
        TYPE_VAR_PARAMETER = 4
    };

    void AddDataCommand(unsigned char opcodeByte);
    void AddDataCommand(unsigned char command, int value);
    void NextSourceLine();
    unsigned int FindSourceLine(unsigned int cursor, unsigned int* line) const;
    int skipempty2();
    void skipempty();
    void GetLine(STRING* lineText);
    int GetName(STRING* name);
    int Word(char* word);
    int WordEnd(char* word);
    int GetInt();
    STRING GetString();
    int PutString(char* text);
    void IntVar(TYPE_VAR type);
    void StringVar(TYPE_VAR type);
    void mnog();
    void vyrag(int parserMode);
    void oper(int* breakPatchList);
    int func();
    void SetNoElement(int elementOrParameterCount);
    void SetOperation(int expressionStart, int operation);
    void slag();
    void cmpslag();
    void logicslag();
    void vyragAnd();
    void vyragXor();
    void vyragOr();
    void vyragCmpAnd();
    void vyrag_oper();
};


