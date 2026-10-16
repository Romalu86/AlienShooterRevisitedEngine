#include "zOptSave.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <climits>
#include <cerrno>

#if defined(_MSC_VER)
void __cdecl DebugAssert(int severity, const char* expression,
                         const char* sourceFile, int sourceLine);
#else
void DebugAssert(int severity, const char* expression,
                 const char* sourceFile, int sourceLine);
#endif

namespace zutl
{
    unsigned int GetHashPJW(const char* text);
    void MoveToTrash(const char* filename);
}

namespace
{
    const char optionsSourceFile[] = "..\\src\\zm\\Eng\\zOptSave.cpp";
    std::string optionsBasePath;

    void CheckOption(bool condition, const char* expression, int sourceLine)
    {
        if (!condition)
            DebugAssert(5, expression, optionsSourceFile, sourceLine);
    }

    int ReadOptionInt(const char* text)
    {
        const long value = std::strtol(text, 0, 10);
        if (value > INT_MAX)
        {
            errno = ERANGE;
            return INT_MAX;
        }
        if (value < INT_MIN)
        {
            errno = ERANGE;
            return INT_MIN;
        }
        return static_cast<int>(value);
    }
}

zOptSaveFile* g_pOptSaveGlobal = 0;
zOptSaveFile* g_pOptSaveUser = 0;

zArg::zArg(const std::string& name)
    : m_name(name), m_intValue(0), m_eArgType(AT_NONE)
{
}

std::string zArg::GetName() const
{
    return m_name;
}

zArg::ARG_TYPE zArg::GetType() const
{
    return m_eArgType;
}

int zArg::GetInt() const
{
    CheckOption(m_eArgType == AT_INT, "m_eArgType == AT_INT", 28);
    return m_intValue;
}

std::string zArg::GetString() const
{
    CheckOption(m_eArgType == AT_STR, "m_eArgType == AT_STR", 44);
    return m_stringValue;
}

zOptSave::zOptSave()
    : m_args(0), m_argumentCount(0), m_argumentCapacity(0), m_dirty(false)
{
}

zOptSave::~zOptSave() noexcept(false)
{
    if (m_args)
    {
        m_ArgsMap.clear();
        for (int index = 0; index < m_argumentCount; ++index)
            delete m_args[index];
        std::free(m_args);
        m_args = 0;
        m_argumentCount = 0;
        m_argumentCapacity = 0;
    }
}

zArg* zOptSave::Find(const std::string& name) const
{
    CheckOption(!name.empty(), "!_sParamName.empty()", 175);
    const int hash = static_cast<int>(zutl::GetHashPJW(name.c_str()));
    const std::map<int, zArg*>::const_iterator position = m_ArgsMap.find(hash);
    return position == m_ArgsMap.end() ? 0 : position->second;
}

zArg* zOptSave::CreateArg(const std::string& name)
{
    CheckOption(!name.empty(), "!_sParamName.empty()", 202);
    zArg* const result = new zArg(name);
    const int hash = static_cast<int>(zutl::GetHashPJW(name.c_str()));
    CheckOption(m_ArgsMap.find(hash) == m_ArgsMap.end(),
                "m_ArgsMap.find(p.first) == m_ArgsMap.end()", 166);
    m_ArgsMap.insert(std::make_pair(hash, result));
    if (m_argumentCount == m_argumentCapacity)
    {
        m_argumentCapacity += 8;
        m_args = static_cast<zArg**>(std::realloc(
            m_args, m_argumentCapacity * sizeof(zArg*)));
    }
    m_args[m_argumentCount++] = result;
    return result;
}

void zOptSave::SetInt(const std::string& name, int value)
{
    zArg* argument = Find(name);
    if (argument)
    {
        CheckOption(argument->GetType() == zArg::AT_INT,
                    "arg->GetType() == zArg::AT_INT", 86);
        if (argument->GetInt() != value)
            m_dirty = true;
    }
    else
    {
        argument = CreateArg(name);
        m_dirty = true;
    }
    argument->m_intValue = value;
    argument->m_eArgType = zArg::AT_INT;
}

int zOptSave::GetInt(const std::string& name, int fallback) const
{
    const zArg* const argument = Find(name);
    return argument ? argument->GetInt() : fallback;
}

void zOptSave::SetString(const std::string& name, const std::string& value)
{
    zArg* argument = Find(name);
    if (argument)
    {
        CheckOption(argument->GetType() == zArg::AT_STR,
                    "arg->GetType() == zArg::AT_STR", 128);
        if (argument->GetString() != value)
            m_dirty = true;
    }
    else
    {
        argument = CreateArg(name);
        m_dirty = true;
    }
    argument->m_stringValue = value;
    argument->m_eArgType = zArg::AT_STR;
}

bool zOptSave::TryGetString(const std::string& name, std::string& value) const
{
    const zArg* const argument = Find(name);
    if (!argument)
        return false;
    value = argument->GetString();
    return true;
}

std::string zOptSave::GetString(const std::string& name,
                              const std::string& fallback) const
{
    std::string value;
    return TryGetString(name, value) ? value : fallback;
}

bool zOptSave::IsDirty() const
{
    return m_dirty;
}

std::size_t zOptSave::GetArgumentCount() const
{
    return static_cast<std::size_t>(m_argumentCount);
}

bool zOptSave::LoadFromFile(const std::string& filename)
{
    std::FILE* const input = std::fopen(filename.c_str(), "rt");
    if (!input)
        return false;

    char line[2048];
    while (!std::feof(input))
    {
        if (!std::fgets(line, sizeof(line), input))
            break;
        std::size_t length = std::strlen(line);
        while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';

        const std::string text(line);
        const std::size_t separator = text.find('=');
        if (separator == std::string::npos)
            continue;

        std::string name = text.substr(0, separator);
        const std::string value = text.substr(separator + 1);
        zArg::ARG_TYPE type = zArg::AT_NONE;
        if (!name.empty() && name[0] == 'i')
            type = zArg::AT_INT;
        else if (!name.empty() && name[0] == 's')
            type = zArg::AT_STR;
        CheckOption(type != zArg::AT_NONE, "type != zArg::AT_NONE", 246);
        name.erase(0, 1);

        if (type == zArg::AT_INT)
            SetInt(name, ReadOptionInt(value.c_str()));
        else
            SetString(name, value);
    }
    (void)std::fclose(input);
    return true;
}

void zOptSave::SaveToFile(const std::string& filename)
{
    zutl::MoveToTrash(filename.c_str());
    std::FILE* const output = std::fopen(filename.c_str(), "w");
    if (output)
    {
        for (std::size_t index = 0; index < static_cast<std::size_t>(m_argumentCount); ++index)
        {
            const zArg& argument = *m_args[index];
            switch (argument.GetType())
            {
            case zArg::AT_INT:
                (void)std::fprintf(output, "i%s=%d\n",
                                   argument.GetName().c_str(), argument.GetInt());
                break;
            case zArg::AT_STR:
                (void)std::fprintf(output, "s%s=%s\n",
                                   argument.GetName().c_str(), argument.GetString().c_str());
                break;
            default:
                CheckOption(false, "0", 279);
                break;
            }
        }
        (void)std::fclose(output);
    }
    m_dirty = false;
}

void zOptSave::SetBasePath(const std::string& path)
{
    optionsBasePath = path;
}

std::string zOptSave::GetBasePath()
{
    return optionsBasePath + "/";
}

std::string zOptSave::ResolvePath(const std::string& filename)
{
    return GetBasePath() + filename;
}

zOptSaveFile::zOptSaveFile(const std::string& filename) : m_filename(filename)
{
    (void)Load();
}

zOptSaveFile::~zOptSaveFile() noexcept(false)
{
    Save();
}

bool zOptSaveFile::Load()
{
    return LoadFromFile(ResolvePath(m_filename));
}

void zOptSaveFile::Save()
{
    SaveToFile(ResolvePath(m_filename));
}
