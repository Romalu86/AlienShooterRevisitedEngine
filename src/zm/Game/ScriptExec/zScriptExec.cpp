#include "list.h"
#include "../../../engine_globals.h"
#include "basetype.h"
#include "zm/RPGHero/zRPGArmor.h"
#include "exports.h"
int __cdecl SFLB_GetHeroId(int value);

#include "zScriptExec.h"
#include "zm/Menu/zShopDlgMenu.h"
#include "zm/Menu/zShopItemInfo.h"
#include "zm/Menu/zParamDlgMenu.h"
#include "zm/Game/ScriptExec/ScriptExecExternalCommand.h"
#include "mylib.h"
#include "zm/Input/zInputMap.h"
#include "input.h"
#include "myerror.h"
#include "sprite_collector.h"
#include "sprite.h"
#include "vid.h"
#include "zm/Game/zVidCopier.h"
#include "zm/Utils/zLogConsole.h"
#include "zm/Utils/zRefObject.h"
#include "zm/Utils/zStaticStorage.h"
#include "zm/Eng/zProfLdr/zProfLdr.h"
#include "zm/Eng/zOptSave.h"
#include "zm/Eng/zProfLdr/zProfMngr.h"
#include "zm/Game/RecordDB/zRecordDB.h"
#include "zm/Survive/zSurvive.h"
#include "zm/Quest/zQuestServer.h"
#include "zm/RPGHero/zRPGQuest.h"
#include "man.h"
#include "zm/Game/Campaign/zCampaign.h"
#include "zm/Utils/String/zString.h"
#include "zm/IniParser/zIniParser.h"
#include "zm/Inventory/zInventory.h"
#include "zm/Inventory/zInventoryItem.h"
#include "zm/RPGHero/zRPGWeapon.h"
#include "zm/RPGHero/zRPGAmmo.h"
#include <new>

int __cdecl SFLB_GetHeroId(int value)
{
    return AddSigned32Wrap(MultiplySigned32Wrap(value, 3), 25);
}


void* zScriptExec::QueryInterface(int interfaceId) const
{
    if (static_cast<unsigned int>(interfaceId) == 37u)
        return const_cast<zScriptExec*>(this);
    DebugAssert(5u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 110u);
    return 0;
}
#include "graph.h"
#include "map.h"
#include "net.h"
#include "player.h"
#include "dialogSrv.h"
#include "constant.h"
#include "zm/Menu/zHeroInvMenu.h"
namespace {
static int vidReduceScale[4096];
static char hotKeyCharacter[2];
static int cachedDamageMinimum;
static int cachedDamageMaximum;
static unsigned char damageRangeInitialized;
static int shopItemInfoX;
static int shopItemInfoY;
StaticObjectStorage<iRef<iSurvive> > surviveReference;
StaticObjectStorage<iRef<iCampaign> > campaignReference;
static STRING GetStackCommandName(int command)
{
    switch (command)
    {
    case ACT_MOVE: return STRING("ACT_MOVE");
    case ACT_FOLLOW: return STRING("ACT_FOLLOW");
    case ACT_STAND_FIRM: return STRING("ACT_STAND_FIRM");
    case ACT_ATTACK: return STRING("ACT_ATTACK");
    case ACT_COOR_ATTACK: return STRING("ACT_COOR_ATTACK");
    case ACT_BUILD_UNIT: return STRING("ACT_BUILD_UNIT");
    case ACT_DESTROY_UNIT: return STRING("ACT_DESTROY_UNIT");
    case ACT_STOP: return STRING("ACT_STOP");
    case ACT_RANDOM: return STRING("ACT_RANDOM");
    case ACT_PATROL: return STRING("ACT_PATROL");
    case ACT_MOVE_TO: return STRING("ACT_MOVE_TO");
    case ACT_MOVE_ACCURATELY: return STRING("ACT_MOVE_ACCURATELY");
    case ACT_ROTATE: return STRING("ACT_ROTATE");
    case ACT_PAUSE: return STRING("ACT_PAUSE");
    case ACT_DAMAGE: return STRING("ACT_DAMAGE");
    case ACT_SET_LINK: return STRING("ACT_SET_LINK");
    case ACT_SET_UPLINK: return STRING("ACT_SET_UPLINK");
    case ACT_SET_HP: return STRING("ACT_SET_HP");
    case ACT_SET_INVISIBLE: return STRING("ACT_SET_INVISIBLE");
    case ACT_SET_GOAL_COOR: return STRING("ACT_SET_GOAL_FOR_TELEPORT");
    case ACT_SET_DEATH_TIMER: return STRING("ACT_SET_DEATH_TIMER");
    case ACT_SET_BEHAVE: return STRING("ACT_SET_BEHAVE");
    case ACT_STOP_STACK: return STRING("ACT_STOP_STACK");
    case ACT_GOTO_STACK: return STRING("ACT_GOTO_STACK");
    case ACT_CLEAR_STACK: return STRING("ACT_CLEAR_STACK");
    case ACT_WHILE_NOT_QUEST: return STRING("ACT_WHILE_NOT_QUEST");
    case ACT_WHILE_NOT_SCRIPT_VAR: return STRING("ACT_WHILE_NOT_SCRIPT_VAR");
    case ACT_SET_QUEST: return STRING("ACT_SET_QUEST");
    case ACT_FLAGMAN_TRIGGER: return STRING("ACT_FLAGMAN_TRIGGER");
    case ACT_SCRIPT_VAR: return STRING("ACT_SCRIPT_VAR");
    case ACT_SCRIPT_EVENT: return STRING("ACT_SCRIPT_EVENT");
    case ACT_SET_DIALOG: return STRING("ACT_SET_DIALOG");
    case ACT_SET_FLAGMAN_DIALOG: return STRING("ACT_SET_FLAGMAN_DIALOG");
    case ACT_CHANGE_COOR: return STRING("ACT_CHANGE_COOR");
    case ACT_CHANGE_VID: return STRING("ACT_CHANGE_VID");
    case ACT_PLAY_SFX: return STRING("ACT_PLAY_SFX");
    default: break;
    }
    static const char* const animationNames[] =
    {
        "ANI_STAND",
        "ANI_STOP_MOVE/ANI_BUILD",
        "ANI_GO",
        "ANI_START_MOVE",
        "ANI_L_ROTATE",
        "ANI_R_ROTATE",
        "ANI_OPEN",
        "ANI_HIT",
        "ANI_FIGHT",
        "ANI_SALUT",
        "ANI_STAND_OPEN",
        "ANI_CLASH_VERT",
        "ANI_LAZY/ANI_CLASH",
        "ANI_WOUND",
        "ANI_BIRTH",
        "ANI_DEATH",
        "ANI_DEATH2",
    };
    if (static_cast<unsigned int>(command) < sizeof(animationNames)/sizeof(animationNames[0]))
        return STRING(animationNames[command]);
    return Printf("UNKNOWN%i", command);
}
}

iRef<iSurvive>& zScriptExec::m_pSurvive =
    *reinterpret_cast<iRef<iSurvive>*>(&surviveReference.memory);
iRef<iCampaign>& zScriptExec::m_pCampaign =
    *reinterpret_cast<iRef<iCampaign>*>(&campaignReference.memory);

unsigned int __cdecl zScriptExec::RegisterSurviveReference()
{
    ::new (&m_pSurvive) iRef<iSurvive>;
    return RegisterCrtAtExit(&zScriptExec::DestroySurviveReference);
}

unsigned int __cdecl zScriptExec::RegisterCampaignReference()
{
    ::new (&m_pCampaign) iRef<iCampaign>;
    return RegisterCrtAtExit(&zScriptExec::DestroyCampaignReference);
}

void __cdecl zScriptExec::DestroySurviveReference()
{
    m_pSurvive.~iRef<iSurvive>();
}

void __cdecl zScriptExec::DestroyCampaignReference()
{
    m_pCampaign.~iRef<iCampaign>();
}

iRPGFrameWork* zScriptExec::m_pRPGFrameWork = 0;
iQuestServer* zScriptExec::m_pQuestServer = 0;
iShopDlgMenu* zScriptExec::m_pShopDlgMenu = 0;
iShopItemInfo* zScriptExec::m_pShopItemInfo = 0;
iHeroInvMenu* zScriptExec::m_pHeroInvMenu = 0;
iParamDlgMenu* zScriptExec::m_pParamDlgMenu = 0;

zScriptExec::~zScriptExec()
{
    iIniParser* parser = ::g_pParser;
    if (parser)
    {
        (void)parser->iRelease();
        ::g_pParser = 0;
    }
    if (::g_pProfLdr)
    {
        delete ::g_pProfLdr;
        ::g_pProfLdr = 0;
    }
}

zScriptExec::zScriptExec()
    : zVidCopier(), m_modeFlags(0), m_loadHeroFromMaxSave(0),
      m_itemTaker(0), m_curAmmoDir(0), m_reloadBarDirection(0),
      m_flagmanControlEnabled(1), m_messageAvailableTick(0), m_messageId(0),
      m_mapType(0), m_radioIconDirection(0), m_radioIconVid(0),
      m_radioFontVid(0), m_radioMessageDirty(0), m_ctrlWheelsCar(0),
      m_lastFlyIconTick(0), m_paramDialog(0), m_showSurvivalHero(0)
{
    if (::g_pProfLdr == nullptr)
    {
        void* loader = CrtNew(sizeof(zProfLdr));
        if (loader)
        {
            try
            {
                loader = new (loader) zProfLdr();
            }
            catch (...)
            {
                CrtFreePointer(loader);
                throw;
            }
        }
        ::g_pProfLdr = static_cast<zProfLdr*>(loader);
    }

    int* const idTable = vidReduceScale;
    for (unsigned int i = 0u;i<4096u;++i)idTable[i] = 0u;

    bool* const enabledMonsterVids = g_abMonsterVid;
    if (enabledMonsterVids)
        for (unsigned int i = 0u;i<4096u;++i) enabledMonsterVids[i] = false;

    if (::g_pParser == nullptr)
    {
        iIniParser* parser;
        GetNew_zIniParser(&parser);
        ::g_pParser = parser;
    }

    if (RecordDatabase == nullptr)
    {
        void* recordDb = CrtNew(sizeof(zRecordDB));
        if (recordDb)
        {
            try
            {
                recordDb = new (recordDb) zRecordDB();
            }
            catch (...)
            {
                CrtFreePointer(recordDb);
                throw;
            }
        }
        RecordDatabase = static_cast<zRecordDB*>(recordDb);
    }
}

void zScriptExec::SetCtrlWheelsCar(SPRITE* sprite)
{
    m_ctrlWheelsCar = sprite;
}

int zScriptExec::GetArcadeMod_() const
{
    return static_cast<int>(m_modeFlags);
}


unsigned int __cdecl ScriptexecZscriptexec_StaticCleanupRegister01()
{
    return RegisterCrtAtExit(&MapStaticCleanupNoOp);
}

zScriptExecIntVector::zScriptExecIntVector()
{
}

zScriptExecIntVector::~zScriptExecIntVector() noexcept(false)
{
}

zScriptExecBoolVectorOwner::zScriptExecBoolVectorOwner(zScriptExecBoolVector* owner)
{
    proxy = static_cast<zContainerProxy<zScriptExecBoolVector>*>(CrtNew(sizeof(*proxy)));
    if (proxy)
        proxy->m_owner = owner;
}

zScriptExecBoolVectorOwner::~zScriptExecBoolVectorOwner() noexcept(false)
{
    CrtFreePointer(proxy);
}

zScriptExecBoolVector::zScriptExecBoolVector()
    : zScriptExecBoolVectorOwner(this), bitCount(0u)
{
}

zScriptExecBoolVector::~zScriptExecBoolVector() noexcept(false)
{
    bitCount = 0u;
}

bool zScriptExecBoolVector::at(unsigned int index) const
{
    const unsigned int word = static_cast<unsigned int>(words.begin[index / 32u]);
    return (word & (1u << (index % 32u))) != 0u;
}

void zScriptExecBoolVector::push_back(bool value)
{
    const unsigned int wordIndex = bitCount / 32u;
    const unsigned int usedWords = words.begin
        ? static_cast<unsigned int>(words.end - words.begin) : 0u;
    if (wordIndex == usedWords)
    {
        const int zero = 0;
        words.insert(words.end, zero);
    }
    unsigned int word = static_cast<unsigned int>(words.begin[wordIndex]);
    const unsigned int mask = 1u << (bitCount % 32u);
    word = value ? word | mask : word & ~mask;
    words.begin[wordIndex] = static_cast<int>(word);
    ++bitCount;
}

void zScriptExecBoolVector::erase_first()
{
    const unsigned int count = bitCount;
    if (count == 0u) return;
    for (unsigned int index = 1u; index < count; ++index)
    {
        const unsigned int destination = index - 1u;
        unsigned int word = static_cast<unsigned int>(words.begin[destination / 32u]);
        const unsigned int mask = 1u << (destination % 32u);
        word = at(index) ? word | mask : word & ~mask;
        words.begin[destination / 32u] = static_cast<int>(word);
    }
    --bitCount;
    const unsigned int remainingWords = bitCount / 32u + (bitCount % 32u != 0u ? 1u : 0u);
    if (words.begin)
        words.end = words.begin + remainingWords;
    if (bitCount != 0u && bitCount % 32u != 0u)
        words.begin[remainingWords - 1u] &= static_cast<int>((1u << (bitCount % 32u)) - 1u);
}

void zScriptExecBoolVector::clear()
{
    words.end = words.begin;
    bitCount = 0u;
}



void zScriptExec::SetDeathMatchMode(bool enabled)
{
    m_modeFlags = enabled?(m_modeFlags|2u):(m_modeFlags&~2u);
}

iRPGHero* zScriptExec::GetHeroForShow()
{
    if (m_showSurvivalHero)
    {
        iRPGObj* object = m_refHead.m_object;
        if (object) return static_cast<iRPGHero*>(object->QueryInterface(0));
    }
    return m_refLegs.m_object;
}


void zScriptExec::SwitchHeroCS(bool enabled)
{
    m_showSurvivalHero = static_cast<unsigned char>(enabled);
    if (enabled) CreateContSurvHero();
}





void zScriptExec::ReleaseArmyHero()
{
    ProfMngr()->ReleaseArmyHero();
}


int zScriptExec::DirFromRPGObj(const iRPGObj* source)
{
    if (!source)return -1;
    const unsigned int key = static_cast<unsigned int>(source->GetVidNum(1));
    const int divisor = GetVidDirectionsNum(static_cast<int>(key));
    if (divisor == 0)return -1;
    const unsigned int raw = static_cast<unsigned int>(source->GetDirNum(1));
    const int numerator = static_cast<int>(raw<<8);
    return (numerator/divisor)&0xFF;
}



void zScriptExec::CreateContSurvHero()
{
    zProfLdr* const profileLoader = ::g_pProfLdr;
    const unsigned int currentId = static_cast<unsigned int>(profileLoader->m_currentProfile);
    iRPGObj* const oldObject = m_refHead.m_object;
    if (oldObject && static_cast<unsigned int>(m_weaponState) == currentId)return;
    m_weaponState = static_cast<int>(currentId);
    if (oldObject) (void)m_pRPGFrameWork->ReleaseObject(oldObject);
    m_refHead.Set(m_pRPGFrameWork->GetNewRPGObj(static_cast<OBJ_TYPE>(TYPE_HERO), false));
    m_refHead.m_object->SetClone(1);

    STRING saveFileName;
    {
        STRING savedName = ::g_pProfLdr->GetMaxLevSaveName(static_cast<SAVE_TYPE>(1));
        saveFileName = savedName;
    }
    iIniParser* const sink = ::g_pParser;
    (void)sink->LoadObjFromFile(m_refHead.m_object, saveFileName.m_value);
}

void zScriptExec::ClearFlyIcons()
{
    for (unsigned int i = 0u;i<4u;++i)
    {
        AS_RECORD_DWORD_VECTOR& vector = m_flyIconDwords[i];
        MsvcNoOpDestroyRange(PtrToU32(vector.begin), PtrToU32(vector.end));
        vector.end = vector.begin;
    }

    for (unsigned int i = 4u;i<6u;++i)
    {
        AS_RECORD_DWORD_VECTOR& vector = m_flyIconDwords[i];
        unsigned int* const newEnd = zScriptExec_DwordRangeCopy(
            reinterpret_cast<const unsigned int*>(vector.end),
            reinterpret_cast<const unsigned int*>(vector.end),
            reinterpret_cast<unsigned int*>(vector.begin));
        MsvcNoOpDestroyRange(PtrToU32(newEnd), PtrToU32(vector.end));
        vector.end = reinterpret_cast<int*>(newEnd);
    }
    m_flyIconTags.clear();
}

void zScriptExec::SetSurviveMode(bool enabled, const char* name)
{
    m_modeFlags = enabled?(m_modeFlags|1u):(m_modeFlags&~1u);
    if (!name || !*name)return;

    iQuestServer* const factory = m_pQuestServer;
    iRPGFrameWork* const family = static_cast<iRPGFrameWork*>(factory->QueryInterface(23));
    iRPGObj* const context = family->GetObjByNamee(name, true);
    if (!context)return;

    if (enabled)
    {
        iSurvive* const next = static_cast<iSurvive*>(context->QueryInterface(29));
        m_pSurvive.Set(next);
    }
    else
    {
        iCampaign* const next = static_cast<iCampaign*>(context->QueryInterface(39));
        m_pCampaign.Set(next);
    }
}

void zScriptExec::ExecCommandC(int opcode, const char* textValue, int& resultValue, STRING& resultText)
{
    char* text = const_cast<char*>(textValue);
    resultValue = 0;
    resultText = StringGlobals::EmptyText();

    switch (opcode)
    {
    case EXT_GETINVBYNAME:
        resultValue = -1;
        if (m_refLegs.m_object != 0 && text)
            resultValue = m_refLegs.m_object->GetInvServer()->GetInvByName(text);
        return;

    case EXT_SETUSERNAME:
        if (!text)return;
        {
            iRPGFrameWork* first = m_pRPGFrameWork;
            if (!first)
            {
                DebugAssert(4u, "m_pRPGFrameWork", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1813u);
                first = m_pRPGFrameWork;
            }
            if (first) first->SetPlayerName(text);
            iQuestServer* const second = m_pQuestServer;
            if (second)
            {
                iRPGFrameWork* const target = static_cast<iRPGFrameWork*>(second->QueryInterface(23));
                target->SetPlayerName(text);
            }
        }
        return;

    case EXT_GetIdByName:
        resultValue = -1;
        {
            iRPGFrameWork* const service = m_pRPGFrameWork;
            if (service && text)
            {
                iRPGObj* const found = service->GetObjByNamee(text, true);
                if (found)resultValue = found->GetBaseId();
            }
        }
        return;

    case EXT_SETKEYWAITMODE:
        if (!text)DebugAssert(5u, "var1", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1847u);
        {
            char* first = std::strchr(text, '_');
            if (first)
            {
                char* firstNumber = first+1;
                char* second = std::strchr(firstNumber, '_');
                if (second)
                {
                    *second = 0;
                    const int x1 = BitsToSigned32(CrtAtoiText(firstNumber));
                    const int secondValue = BitsToSigned32(CrtAtoiText(second+1));
                    if (x1<0 || x1 >= zInputMap::ACTION_COUNT)
                        DebugAssert(5u, "x1 >= 0 && x1 < IA_COUNT", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1859u);
                    zInputMap::m_bKeyWaitAct = static_cast<unsigned int>(x1);
                    zInputMap::m_iKeyWaitNum = secondValue;
                }
            }
        }
        return;

    case EXT_PushStrVar1:
        m_strVar1 = text;
        return;

    case EXT_PushStrVar2:
        m_strVar2 = text;
        return;

    case EXT_AddHallData:
        m_hallRecord.m_playerName = text;
        return;

    default:
        DebugAssert(4u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1873u);
        return;
    }
}



void ScriptExecMapControl::insert(int key, int value)
{
    InsertUnique(key, value);
}

void zScriptExec::AddFlyIcon(int vid, int direction, int count, int x, int y, int z, bool tag)
{
    if (!vid)return;
    int values[6] = {vid, direction, count, x, y, z};
    for (unsigned int i = 0;i<6u;++i)
    {
        AS_RECORD_DWORD_VECTOR& vector = m_flyIconDwords[i];
        vector.insert(vector.end, 1u, values[i]);
    }

    m_flyIconTags.push_back(tag);
}

void zScriptExec::GetHotKey(STRING& hotKeyText, int actionIndex, int bindingIndex)
{
    if (actionIndex <= 0 || actionIndex >= zInputMap::ACTION_COUNT)
    {
        DebugAssert(5,
            "var1 > 0 && var1 < IA_COUNT",
            "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp",
            2625);
    }

    const InpAction binding = zInputMap::GetHotKey(
        static_cast<unsigned int>(actionIndex), bindingIndex);

    const unsigned int inputMode = binding.inputMode;
    const int inputValue = static_cast<int>(binding.inputValue);
    PROFILE* const profile =
        ::Strings;

    if (inputMode != IM_KEYDOWN)
    {
        const char* const modeName = zInputMap::InpMsgToString(inputMode);
        if (!modeName)
            return;

        STRING defaultValue;
        STRING key;
        STRING section;
        defaultValue = StringGlobals::EmptyText();
        key = modeName;
        section = "menu";

        STRING localized = profile->GetString(section, key, defaultValue);
        hotKeyText = localized;

        return;
    }

    const char* const keyName = zInputMap::KeyToString(static_cast<unsigned int>(inputValue), false);
    if (!keyName)
    {
        (void)std::sprintf(hotKeyCharacter, "%c",
            static_cast<unsigned int>(inputValue));
        hotKeyText = hotKeyCharacter;
        return;
    }

    STRING key;
    key = "VK_";
    key += keyName;

    STRING defaultValue;
    STRING section;
    defaultValue = StringGlobals::EmptyText();
    section = "menu";

    STRING localized = profile->GetString(section, key, defaultValue);
    key = localized;

    if (key.IsEmpty() != 0)
        hotKeyText = keyName;
    else
        hotKeyText = key;

}

bool zScriptExec::ExecCommandA(
    int commandValue, int argument1, int argument2, int argument3, int& resultValueRef, STRING& resultText)
{
    zScriptExec* self = this;
    const unsigned int command = static_cast<unsigned int>(commandValue);
    const unsigned int argument1Bits = static_cast<unsigned int>(argument1);
    const unsigned int argument2Bits = static_cast<unsigned int>(argument2);
    const unsigned int argument3Bits = static_cast<unsigned int>(argument3);


    resultValueRef = 0;
    int resultValue = 0;
    resultText = StringGlobals::EmptyText();
    if (command>static_cast<unsigned int>(EXT_SETPERKSKILL))
    {
        DebugAssert(4,
            "0",
            "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2595);
        return false;
    }

    switch (command)
    {
    case EXT_PROC_SHOWINVENTORY:
        {
        const unsigned int enabled = (argument1Bits != 0u)?1u:0u;
        static_cast<iVidCopier*>(self)->ShowInventory(enabled != 0u);
        static_cast<iVidCopier*>(self)->InventorySetActive(enabled != 0u);
        break;
    }
    case EXT_GETITEMNUM:
        {
        if (argument1Bits == 1u)
            {
            iRPGHero* owner = m_refLegs.m_object;
            const iRPGObj* selected = owner->GetEquip(static_cast<int>(argument2Bits));
            if (!selected) break;
            if (argument3Bits == 0u)
                resultValue = selected->GetNumInCell();
            else
                resultValue = owner->GetInvServer()->GetObjectCnt(selected->GetBaseId());
        }
        else if (argument1Bits == 2u)
            {
            resultValue = m_refLegs.m_object->GetInvServer()->GetObjectCnt(static_cast<int>(argument2Bits));
        }
        break;
    }
    case EXT_GETDIRTYSTATE:
        resultValue = BitsToSigned32(static_cast<unsigned int>(static_cast<iVidCopier*>(self)->GetDirtyState()));
        break;
    case EXT_GETARMORDIR:
        {
        iRPGHero* owner = m_refLegs.m_object;
        const iRPGObj* source = owner->GetArmor();
        resultValue = BitsToSigned32(static_cast<unsigned int>(DirFromRPGObj(source)));
        break;
    }
    case EXT_GETWEAPONID:
        {
        iRPGHero* owner = m_refLegs.m_object;
        iRPGObj* child = owner->GetCurWpn();
        if (child) resultValue = argument1Bits == 0u
            ? BitsToSigned32(static_cast<unsigned int>(child->GetExtType()))
            : child->GetBaseId();
        break;
    }
    case EXT_GETARMORID:
        {
        iRPGHero* context = GetHeroForShow();
        const iRPGObj* child = context->GetArmor();
        if (child) resultValue = BitsToSigned32(static_cast<unsigned int>(child->GetExtType()));
        break;
    }
    case EXT_GETHEROID:
        {

        iRPGObj* const owner = self->m_refBody.m_object;
        resultValue = BitsToSigned32(static_cast<unsigned int>(owner->GetExtType()));
        break;
    }
    case EXT_IFHASITEM:
        {
        iRPGHero* owner = m_refLegs.m_object;
        iInvServer* child = owner->GetInvServer();
        const iRPGObj* const selected = argument2Bits == 0u
            ? child->GetEquipEx(static_cast<int>(argument1Bits), 1)
            : child->GetItemByBaseId(static_cast<int>(argument1Bits), 3);
        const unsigned int raw = static_cast<unsigned int>(selected != 0);
        resultValue = BitsToSigned32((raw&0xFFu)?1u:0u);
        break;
    }
    case EXT_GETITEMDISABLED:
        {
        iRPGHero* owner = m_refLegs.m_object;
        iRPGObj* selected = 0;
        selected = (argument1Bits == 0xFFFFFFFFu)?owner->GetCurWpn():const_cast<iRPGObj*>(owner->GetEquip(static_cast<int>(argument1Bits)));
        if (selected)
            {
            iInvKeeper* const context = static_cast<iInvKeeper*>(m_refLegs.m_object->QueryInterface(1));
            resultValue = BitsToSigned32(context->CanHoldObj(selected)?0u:1u);
        }
        break;
    }
    case EXT_GETRELOADBARDIR:
        resultValue = BitsToSigned32(self->m_reloadBarDirection);
        break;
    case EXT_INVENTORYSETACTIVE:
        DebugAssert(4, "0",
            "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2321);
        break;
    case EXT_ADDSKILL:
        {
        if (argument1Bits == static_cast<unsigned int>(RPGHERO__HP))
            {
            iRPGHero* owner = m_refLegs.m_object;
            (void)owner->AddHp(static_cast<int>(argument2Bits), argument3Bits != 0u);
        }
            else if (argument1Bits == static_cast<unsigned int>(RPGHERO_EXP))
            {
            iRPGHero* owner = m_refLegs.m_object;
            owner->AddExperience(static_cast<int>(argument2Bits), argument3Bits != 0u);
        }
            else if (argument3Bits != 0u && argument1Bits == static_cast<unsigned int>(RPGHERO_MONEY))
            {
            iRPGHero* owner = m_refLegs.m_object;
            (void)owner->AddMoney(static_cast<int>(argument2Bits), false);
        }
            else
            {
            iRPGObj* const owner = m_refBody.m_object;
            (void)owner->IncSkillValue(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        }
        break;
    }
    case EXT_GETCURAMMOBARDIR:
        if (argument1Bits != 0u) (void)static_cast<iVidCopier*>(self)->RecalcCurAmmoClip(false);
        resultValue = BitsToSigned32(static_cast<unsigned int>(self->m_curAmmoDir));
        break;
    case EXT_GETPERKSKILL:
        {
        iRPGHero* context = GetHeroForShow();
        if (!context)
            {
                resultValue = BitsToSigned32(0u);
                break;
            }
        iRPGObj* typed = static_cast<iRPGObj*>(GetHeroForShow()->QueryInterface(9));
        resultValue = BitsToSigned32(static_cast<unsigned int>(typed->GetPerkSkill()));
        break;
    }
    case EXT_SETPERKSKILL:
        m_paramDialog->SetPerkSkill(static_cast<int>(argument1Bits));

    case EXT_GETITEMVIDBYID:
        {
        iRPGFrameWork* registry = m_pRPGFrameWork;
        iRPGObj* selected = registry->GetItemById(static_cast<int>(argument1Bits));
        if (!selected) break;
        if (argument2Bits == 0u)
            {
            resultValue = BitsToSigned32(static_cast<unsigned int>(selected->GetVidNum(0)));
            break;
        }
        if (argument2Bits == 1u)
            {
            resultValue = BitsToSigned32(static_cast<unsigned int>(selected->GetVidNum(2)));
            break;
        }
        if (argument2Bits == 2u)
            {
            resultValue = BitsToSigned32(static_cast<unsigned int>(selected->GetDirNum(2)));
            break;
        }
        if (argument2Bits != 3u) break;

        resultValue = BitsToSigned32(argument1Bits);
        if (static_cast<unsigned int>(selected->GetGameObj()) == 6u && m_refLegs.m_object &&
           static_cast<unsigned int>(selected->GetSkillValue(RPGEQUIP_ADDEXISTINGAMMO)) == 2u)
            {
            iRPGObj* child = m_refLegs.m_object->GetCurWpn();
            if (!child) break;
            zRPGWeapon* typed = static_cast<zRPGWeapon*>(child->QueryInterface(50));
            zRPGAmmo* ammo = typed->GetAmmo();
            iRPGObj* buf = ammo->m_pEquipOwner;
            if (buf)
                {
                resultValue = BitsToSigned32(static_cast<unsigned int>(buf->GetBaseId()));
                break;
            }
            if (static_cast<zRPGObj*>(ammo)->IsInfinite()) break;
            DebugAssert(4, "buf || ammo->IsInfinite()",
                "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2003);
            break;
        }

        if (static_cast<unsigned int>(selected->GetVidNum(0)) == 1682u)
            {


            if (m_refLegs.m_object->GetArmor() == 0) resultValue = BitsToSigned32(0u);
        }
        break;
    }
    case EXT_ADDITEMBYBASEID:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (!owner) break;
        if (argument2Bits != 0u)
            {
            int remainingCount = argument3 == 0 ? 1 : argument3;
            while (remainingCount > 0)
                {
                (void)owner->TakeItem(static_cast<int>(argument1Bits));
                --remainingCount;
            }
            break;
        }
        iRPGFrameWork* registry = m_pRPGFrameWork;
        iRPGObj* selected = registry->GetItemById(static_cast<int>(argument1Bits));
        if (!selected)
            {
            DebugAssert(4, "0",
                "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2557);
            break;
        }
        const unsigned int acceptedRaw = static_cast<unsigned int>(m_refLegs.m_object->PutItem(selected));
        if ((acceptedRaw&1u) == 0u)
            {
            iLogConsole* logger = ::gpLogCons;
            (void)logger->LogStr(LT_INFO, "EXT_ADDITEMBYBASEID: was not put %d", acceptedRaw);
        }
        if (static_cast<unsigned int>(selected->GetGameObj()) != 2u) break;
        zRPGWeapon* typed = static_cast<zRPGWeapon*>(selected->QueryInterface(50));
        zRPGAmmo* payload = typed->GetAmmo();
        iRPGObj* item = static_cast<zRPGObj*>(payload);
        if (item->IsInfinite()) break;
        iRPGObj* current = item->GetObjCopy();
        const int value = current->GetGrndTakeNum();
        current->SetNumInCell(value);
        (void)m_refLegs.m_object->PutItem(current);
        break;
    }
    case EXT_DISABLEFLAGMANCONTROL:
        {
        self->m_flagmanControlEnabled = static_cast<unsigned char>((argument1Bits == 0u)?1u:0u);
        iLogConsole* logger = ::gpLogCons;
        (void)logger->LogStr(LT_NOTICE, "Flagman control changed: %d", argument1Bits);
        break;
    }
    case EXT_GETFLYITEMVID:
        {
        if (argument1Bits == 0u)
            {
            const unsigned int sentinel = 0xFFFFFDD5u;
            unsigned int value = sentinel;
            AS_RECORD_DWORD_VECTOR& vector = m_flyIconDwords[0];
            if (vector.begin != 0 && vector.end != vector.begin)
                {
                value = static_cast<unsigned int>(*vector.begin);
                if (value != sentinel)
                    {
                    for (int* source = vector.begin+1;source != vector.end;++source)
                        source[-1] = *source;
                    --vector.end;
                    self->m_lastFlyIconTick = ::CurrentTime;
                }
            }
            resultValue = BitsToSigned32(value);
        }
            else
            {
            if (m_flyIconTags.bitCount != 0u)
            {
                resultValue = m_flyIconTags.at(0u) ? 1 : 0;
                m_flyIconTags.erase_first();
            }
        }
        break;
    }
    case EXT_GetCurDroneId:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (!owner) break;
        iRPGObj* replacement = const_cast<iRPGObj*>(owner->GetEquip(INV_ITEM_4));
        self->m_selectedEquipmentRef.Set(replacement);
        iRPGObj* current = self->m_selectedEquipmentRef.m_object;
        if (current) resultValue = BitsToSigned32(static_cast<unsigned int>(current->GetExtType()));
        break;
    }
    case EXT_SELECTBESTWEAPON:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (owner) owner->SelectBestWeapon();
        break;
    }
    case EXT_GETITEMDIR:
        {
        iRPGHero* context = GetHeroForShow();
        const iRPGObj* selected = context->GetEquip(static_cast<int>(argument1Bits));
        resultValue = BitsToSigned32(static_cast<unsigned int>(DirFromRPGObj(selected)));
        break;
    }
    case EXT_GETITEMVID:
        {
        iRPGHero* context = GetHeroForShow();
        const iRPGObj* selected = context->GetEquip(static_cast<int>(argument1Bits));
        resultValue = BitsToSigned32(selected?static_cast<unsigned int>(selected->GetVidNum(1)):0xFFFFFFFFu);
        break;
    }
    case EXT_GETHEROFACEDIR:
        {
        iRPGHero* context = GetHeroForShow();
        iRPGObj* typed = static_cast<iRPGObj*>(context->QueryInterface(9));
        const int value = typed->GetSkillValue(RPGHERO_VIDHEAD);
        resultValue = BitsToSigned32(static_cast<unsigned int>(static_cast<iVidCopier*>(self)->GetFaceDir(value)));
        break;
    }
    case EXT_GETITEMDIRTY:
        {
        iRPGFrameWork* registry = m_pRPGFrameWork;
        iRPGObj* hero = registry->GetItemById(static_cast<int>(argument1Bits));
        if (!hero)
            DebugAssert(5, "hero",
                "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2441);

        iRPGHero* const typed = static_cast<iRPGHero*>(hero->QueryInterface(0));
        resultValue = BitsToSigned32(static_cast<unsigned int>(typed->GetItemDirty(static_cast<int>(argument2Bits))));
        break;
    }
    case EXT_GETINVSIZEX:
        {
        iRPGHero* const owner = m_refLegs.m_object;
        if (!owner) break;
        iInventory* const selected = owner->GetInvServer()->GetInv(static_cast<int>(argument1Bits));
        iInventory2D* const metric = static_cast<iInventory2D*>(selected->QueryInterface(17));
        int first = static_cast<int>(argument2Bits), second = static_cast<int>(argument3Bits);
        metric->GetSizeInCells(first, second);
        const unsigned int x = static_cast<unsigned int>(metric->GetCellSizeX());
        (void)metric->GetCellSizeY();
        resultValue = BitsToSigned32(x*static_cast<unsigned int>(first));
        break;
    }
    case EXT_GETINVSIZEY:
        {
        iRPGHero* const owner = m_refLegs.m_object;
        if (!owner) break;
        iInventory* const selected = owner->GetInvServer()->GetInv(static_cast<int>(argument1Bits));
        iInventory2D* const metric = static_cast<iInventory2D*>(selected->QueryInterface(17));
        int first = static_cast<int>(argument2Bits), second = static_cast<int>(argument3Bits);
        metric->GetSizeInCells(second, first);
        (void)metric->GetCellSizeX();
        const unsigned int y = static_cast<unsigned int>(metric->GetCellSizeY());
        resultValue = BitsToSigned32(y*static_cast<unsigned int>(first));
        break;
    }
    case EXT_AUTOTAKINGDIST:
        {
        self->m_itemTaker->SetItemTakeDist(static_cast<float>(static_cast<int>(argument1Bits)));
        break;
    }
    case EXT_GETITEMID:
        {
        iRPGHero* owner = m_refLegs.m_object;
        const iRPGObj* child = owner->GetEquip(static_cast<int>(argument1Bits));
        if (child) resultValue = BitsToSigned32(static_cast<unsigned int>(child->GetBaseId()));
        break;
    }
    case EXT_GETMESSAGEDIRTY:
        {
        iPlayer* const player = static_cast<iPlayer*>(self);
        const int limit = player->GetCurTime();
        if (static_cast<int>(self->m_messageAvailableTick) <= limit)resultValue = BitsToSigned32(self->m_messageId);
        break;
    }
    case EXT_REMOVEFLAGMANDIALOG:
        {
        static_cast<iVidCopier*>(self)->RemoveFlagmanDialog();
        break;
    }
    case EXT_GETMESSAGE:
        {
        if (self->m_messageId != 0u)
            {
            const char* text = self->m_debugText.m_pBuf;
            resultText = text?text:StringGlobals::EmptyText();
            self->m_debugText.operator = (StringGlobals::EmptyText());
            self->m_messageId = 0u;
            self->m_messageAvailableTick = 0u;
        }
        resultValue = BitsToSigned32(0u);
        resultValueRef = resultValue;
        return true;
    }
    case EXT_GETCARSPEED:
        resultValue = BitsToSigned32(static_cast<unsigned int>(static_cast<iVidCopier*>(self)->GetCarSpeed(0)));
        break;
    case EXT_PREVHERO:
        m_paramDialog->PrevHero();
        break;
    case EXT_NEXTHERO:
        m_paramDialog->NextHero();
        break;
    case EXT_CHANGESEX:
        m_paramDialog->ChangeCurSex();
        break;
    case EXT_PREVCOLOR:
        (void)self->PrevHeroColor();
        break;
    case EXT_NEXTCOLOR:
        (void)self->NextHeroColor();
        break;
    case EXT_PDADDSKILL:
        {
        if (!m_paramDialog)
            {
            DebugAssert(5, "m_pRPGParamDlg",
                "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2063);
        }
        m_paramDialog->AddSkill(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        break;
    }
    case EXT_PDCANINCSKILL:
        resultValue = BitsToSigned32(m_paramDialog->CanIncSkill(static_cast<int>(argument1Bits))?1u:0u);
        break;
    case EXT_PDCANDECSKILL:
        resultValue = BitsToSigned32(m_paramDialog->CanDecSkill(static_cast<int>(argument1Bits))?1u:0u);
        break;
    case EXT_PDASSIGNSKILLS:
        m_paramDialog->AssignSkills();
        break;
    case EXT_PDREMOVESKILLINCREASE:
        m_paramDialog->RemoveSkillIncrease();
        break;
    case EXT_GETRADIOMSGDIRTY:
        {
        const unsigned int value = self->m_radioMessageDirty?1u:0u;
        self->m_radioMessageDirty = 0u;
        resultValue = BitsToSigned32(value);
        break;
    }
    case EXT_GETRADIOMSG:
        {
        zString* const pendingText = &self->m_pendingText;
        if (!pendingText->IsEmpty())
            {
            const char* text = static_cast<const char*>(*pendingText);
            resultText = text;
        }
        resultValue = BitsToSigned32(0u);
        resultValueRef = resultValue;
        return true;
    }
    case EXT_GETRADIOICONVID:
        resultValue = BitsToSigned32(self->m_radioIconVid);
        break;
    case EXT_GETRADIOICONDIR:
        resultValue = BitsToSigned32(self->m_radioIconDirection);
        break;
    case EXT_GETRADIOFONTVID:
        resultValue = BitsToSigned32(self->m_radioFontVid);
        break;
    case EXT_SETHERODAMAGECOEFF:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (owner)
            {
            const float value = static_cast<float>(static_cast<double>(argument1)/100.0);
            owner->SetDamageCoeff(value);
        }
        break;
    }
    case EXT_REMOVEITEMIFHAS:
        {
        const unsigned int selector = (argument1Bits == 1u)?1u:3u;
        iRPGHero* owner = m_refLegs.m_object;
        iInvServer* context = owner->GetInvServer();
        iRPGObj* selected = context->GetEquipEx(static_cast<int>(argument1Bits), static_cast<int>(selector));
        if (selected)
            {
            (void)selected->AddNumInCell(-1);
            if (argument1Bits == 8u) static_cast<iVidCopier*>(self)->SetTankCanShot();
        }
        break;
    }
    case EXT_FIRSTHERO:
        m_paramDialog->FirstHero(1);
        break;
    case EXT_RESETHEROHP:
        {
        iRPGHero* owner = m_refLegs.m_object;
        (void)owner->SetHpToMax();
        break;
    }
    case EXT_OnLevelWin:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (!owner)
            DebugAssert(5, "m_pRPGHero",
                "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2091u);

        iRPGObj* profile = static_cast<iRPGObj*>(m_refLegs.m_object->QueryInterface(9));
        iPlayer* const player = static_cast<iPlayer*>(self);
        const bool enabled = ((player->GetArcadeMod_()&1) != 0) || argument1Bits != 0u;
        iQuestServer* const questServer = player->IsCurPlayer()?m_pQuestServer:0;

        self->ProfMngr()->OnLevelWin(
            profile,
            questServer,
            enabled);
        break;
    }
    case EXT_GETFLYITEMDIR:
    case EXT_GETFLYITEMCNT:
    case EXT_GETFLYITEMX:
    case EXT_GETFLYITEMY:
    case EXT_GETFLYITEMZ:
        {
        AS_RECORD_DWORD_VECTOR& vector = m_flyIconDwords[command-EXT_GETFLYITEMVID];
        if (vector.begin != 0 && vector.end != vector.begin)
            {
            const int value = *vector.begin;
            for (int* source = vector.begin+1;source != vector.end;++source)
                source[-1] = *source;
            --vector.end;
            resultValue = value;
        }
        break;
    }
    case EXT_SETSKILL:
        {
        iRPGObj* const owner = m_refBody.m_object;
        if (owner) (void)owner->SetSkillValue(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        break;
    }
    case EXT_REPAIRARMOR:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (!owner) break;
        const iRPGObj* child = owner->GetArmor();
        if (!child) break;
        iRPGArmor* typed = static_cast<iRPGArmor*>(child->QueryInterface(51));
        (void)typed->Repair(99999999);
        break;
    }
    case EXT_GetAddWpnCnt:
        {
        iSurvive* owner = m_pSurvive.m_object;
        if (owner) resultValue = BitsToSigned32(static_cast<unsigned int>(owner->GetAddWpnCnt(static_cast<int>(argument1Bits))));
        break;
    }
    case EXT_GetAddWpnId:
        {
        iSurvive* owner = m_pSurvive.m_object;
        if (owner)
            {
            resultValue = BitsToSigned32(static_cast<unsigned int>(owner->GetAddWpnId(
                static_cast<int>(argument1Bits), static_cast<int>(argument2Bits),
                m_refLegs.m_object, m_pRPGFrameWork)));
        }
        break;
    }
    case EXT_ADDHYPNMONSTER:
        static_cast<iVidCopier*>(self)->AddHypnMonst(reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits)), static_cast<int>(argument2Bits));
        break;
    case EXT_SetPlayerMenuType:
        {
        m_paramDialog->SetPlayerMenuType(static_cast<int>(argument1Bits));
        iPlayer* const player = static_cast<iPlayer*>(self);
        if (m_pParamDlgMenu != 0 && player->IsCurPlayer())
            {
            iParamDlgMenu* const observer =
                m_pParamDlgMenu;
            observer->SetRPGParamDialog(m_paramDialog);
        }
        resultValue = BitsToSigned32(0u);
        break;
    }
    case EXT_SetCurDroneHp:
        {
        iRPGObj* const owner = self->m_selectedEquipmentRef.m_object;
        if (owner) (void)owner->SetSkillValue(RPGEQUIP_DroneHp, static_cast<int>(argument1Bits));
        break;
    }
    case Ext_SwitchToStandFirm:
        {
        iRPGHero* owner = m_refLegs.m_object;
        if (!owner) break;
        if (argument1Bits >= 1u && argument1Bits <= 3u)
            {
            (void)owner->SwitchToMode(static_cast<iRPGHero::M_MODE>(argument1Bits-1u));
        }
            else
            {
            DebugAssert(5, "0",
                "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2483);
        }
        break;
    }
    case Ext_GetGamObjType:
        {
        iRPGFrameWork* registry = m_pRPGFrameWork;
        iRPGObj* selected = registry->GetItemById(static_cast<int>(argument1Bits));
        if (selected) resultValue = BitsToSigned32(static_cast<unsigned int>(selected->GetType()));
        break;
    }
    case EXT_AddItemsToBackPack:
        {
        iRPGFrameWork* registry = m_pRPGFrameWork;
        iRPGObj* selected = registry->GetItemById(static_cast<int>(argument1Bits));
        if (!selected) break;
        iRPGObj* const targetObject = selected->GetObjCopy();
        (void)self->m_itemTaker->CreateGroundSprite(targetObject);

        iRPGHero* owner = m_refLegs.m_object;
        iRPGObj* anchor = owner->GetCurWpn();
        iLogConsole* logger = ::gpLogCons;
        const char* const anchorName = anchor->GetName();
        (void)logger->LogStr(LT_INFO, "Cur wpn %s", anchorName);

        anchor = m_refLegs.m_object->GetCurWpn();
        for (unsigned int index = 0u;index<INV_GRND;++index)
            {
            iInvServer* context = m_refLegs.m_object->GetInvServer();
            iInventory* node = context->GetInv(static_cast<int>(index));
            zInventory* inventory = static_cast<zInventory*>(node->QueryInterface(46));

            while (inventory->m_apItems.begin != 0 && inventory->m_apItems.end != inventory->m_apItems.begin)
                {
                zItem* candidate = inventory->GetItem(0);
                (void)inventory->TakeItem(candidate);
                const unsigned int firstType = static_cast<unsigned int>(candidate->GetGameObj());
                bool attach = (firstType == 2u && candidate == anchor);


                if (!attach) attach = (candidate->GetGameObj() == 3);
                if (attach)
                    {
                    targetObject->AddItem(candidate);
                    logger = ::gpLogCons;
                    const int detail = candidate->GetNumInCell();
                    const char* name = candidate->GetName();
                    (void)logger->LogStr(LT_INFO, "Item added %s %d", name, detail);
                }
            }
        }
        break;
    }
    default:


        DebugAssert(4, "0",
            "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2595);
        break;
    }
    resultValueRef = resultValue;
    return false;
}

static int ScriptExecCartesianAngle(int x, int y)
{

    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    const float angle = static_cast<float>(atan2(static_cast<double>(fy), static_cast<double>(fx)));
    const double pi = static_cast<double>(3.1415927410125732f);
    return TruncateDoubleViaCrtToInt32((static_cast<double>(angle)*128.0)/pi);
}

static int ScriptExecCartesianLength(int x, int y)
{
    return TruncatedPlanarDistance(static_cast<float>(x), static_cast<float>(y));
}

static unsigned int ScriptExecVectorDwordCount(const AS_RECORD_DWORD_VECTOR& vectorOwner)
{
    if (!vectorOwner.begin)return 0u;
    return static_cast<unsigned int>(vectorOwner.end-vectorOwner.begin);
}

static void ScriptExecAppendHallDword(AS_RECORD_DWORD_VECTOR* vectorOwner, unsigned int value)
{

    const int localValue = BitsToSigned32(value);
    vectorOwner->insert(vectorOwner->end, 1u, localValue);
}

static void ScriptExecAssignFormattedHallValue(STRING& outString, const char* format, unsigned int value)
{
    const STRING formatted = Printf(format, value);
    outString = formatted;
}

int ScriptExecMapControl::find(int key) const
{
    const int* const value = FindValue(key);
    return value ? *value : -777;
}

void zScriptExec::ShowStackOfAllObjects(STRING fileName)
{
    std::string name(fileName.value);
    const std::string::size_type separator = name.find_last_of("/\\");
    if (separator != std::string::npos)
        name = name.substr(separator + 1u, name.size());

    const STRING userPath = Map->WindowsUserPath();
    const STRING logPath = Printf("%sLogs\\_stacks\\%s.txt", userPath.value, name.c_str());
    ::gpLogCons->LogStr(LT_INFO, "ShowStackOfAllObjects: %s", logPath.value);
    std::FILE* const file = std::fopen(logPath.value, "w");
    ScriptExecMapControl nameHashes;
    if (file)
    {
        for (SPRITE* sprite = ::Sprites->FirstAllSpriteFrom0(); sprite; sprite = ::Sprites->NextAllSpriteFrom0())
        {
            EX_SPRITE_DATA* const extra = sprite->ExtraData();
            bool hasStack = false;
            if (extra)
                for (int laneIndex = 0; laneIndex < 4; ++laneIndex)
                    if (extra->actionLane[laneIndex].m_count != 0)
                        hasStack = true;
            if ((extra && !extra->nameText.IsEmpty()) || hasStack)
            {
                std::fprintf(file, ";---------------------------------------------------------------------------\n");
                const STRING coordinates = Printf("vid= %d. coor = (%5d,%5d)\n",
                    sprite->Vid()->nVid,
                    TruncateFloatViaCrtToInt32(sprite->X()),
                    TruncateFloatViaCrtToInt32(sprite->Y()));
                std::fprintf(file, "%s", coordinates.value);
            }
            if (extra)
            {
                for (int laneIndex = 0; laneIndex < 4; ++laneIndex)
                {
                    const LIST<ACT>& lane = extra->actionLane[laneIndex];
                    if (lane.m_count <= 0)
                        continue;
                    std::fprintf(file, "--------stack %d\n", laneIndex);
                    for (int index = lane.m_count - 1; index >= 0; --index)
                    {
                        const ACT& action = lane.m_data[index];
                        if (action.command == ACT_GOTO_STACK && action.arg1 >= lane.m_count)
                        {
                            std::fprintf(file, "!!!!!!!!!!!Error in stack!!!!!!!!\n");
                            DebugAssert(5u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2735u);
                        }
                        const STRING commandName = GetStackCommandName(action.command);
                        std::fprintf(file, "%s, ", commandName.value);
                        std::fprintf(file, "%d, ", action.arg1);
                        std::fprintf(file, "%d, ", action.arg2);
                        std::fprintf(file, "%d", action.arg3);
                        std::fprintf(file, "\n");
                    }
                }
                if (!extra->nameText.IsEmpty())
                {
                    const STRING spriteName = sprite->GetName();
                    const std::string objectName(spriteName.value);
                    const bool automaticName =
                        (objectName.find("Obj") != std::string::npos || objectName.find("obj") != std::string::npos) &&
                        objectName.find('_') == std::string::npos && objectName.find(' ') == std::string::npos;
                    std::fprintf(file, "Name = %s\n", objectName.c_str());
                    const int hash = static_cast<int>(zutl::GetHashPJW(objectName.c_str()));
                    if (automaticName)
                    {
                        if (nameHashes.FindValue(hash))
                        {
                            std::fprintf(file, "!!!!!!!!!!!Error: Dupicate Name!!!!!!!!\n");
                            DebugAssert(3u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 2766u);
                        }
                        else
                            nameHashes.insert(hash, 0);
                    }
                }
            }
        }
        std::fclose(file);
    }
}

bool zScriptExec::ExecCommand(
    int commandValue,
    int argument1,
    int argument2,
    int argument3,
    int& intResultRef,
    STRING& stringResultRef,
    SPRITE*& specialResultRef)
{
    zScriptExec* self = this;
    const unsigned int command = static_cast<unsigned int>(commandValue);
    const unsigned int argument1Bits = static_cast<unsigned int>(argument1);
    const unsigned int argument2Bits = static_cast<unsigned int>(argument2);
    const unsigned int argument3Bits = static_cast<unsigned int>(argument3);

    unsigned int intResult = 0u;
    bool returnsString = false;
    intResultRef = 0;
    stringResultRef = StringGlobals::EmptyText();
    specialResultRef = 0;

    switch (command)
    {
    case EXT_GETSKILL:
    {
        if (argument3Bits != 0u)
            {
            iRPGHero* const object = GetHeroForShow();
            iRPGObj* const typedObject = static_cast<iRPGObj*>(object->QueryInterface(9));
            const int type = typedObject->GetExtType();
            intResult = static_cast<unsigned int>(m_pRPGFrameWork->GetSkillValue(type, static_cast<int>(argument2Bits)));
        }
            else
            {
            intResult = static_cast<unsigned int>(m_pRPGFrameWork->GetSkillValue(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits)));
        }
        break;
    }
    case EXT_GETSKILLBASE:
    {
        iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
        iRPGObj* object = objectRegistry->GetItemById(static_cast<int>(argument1Bits));
        if (object) intResult = static_cast<unsigned int>(object->GetSkillValueBase(static_cast<int>(argument2Bits)));
        break;
    }
    case EXT_GETMONSTERVID:
    case EXT_GETMONSTERCOUNT:
    case EXT_GETMONSTERTYPESCNT:
    case EXT_GetBossSize:
    case EXT_GetBossHpCoeff:
    case EXT_IsBossPeriod:
    case EXT_GETBONUSID:
    case EXT_GetBossColor:
    case EXT_GetScoresCoeff:
    {
        iSurvive* periodOwner = m_pSurvive.m_object;
        if (periodOwner)
            {
            iSurvive* selectedObject = 0;
            int selectedValue = 0;
            (void)periodOwner->GetIntrval(static_cast<int>(argument1Bits), selectedObject, selectedValue, 0);
            if (selectedObject)
                {
                switch (command)
                    {
                case EXT_GETMONSTERVID:
                    intResult = static_cast<unsigned int>(selectedObject->GetMonstersVid(selectedValue, static_cast<int>(argument2Bits)));
                    break;
                case EXT_GETMONSTERCOUNT:
                    intResult = static_cast<unsigned int>(selectedObject->GetMonstersCount(selectedValue, static_cast<int>(argument2Bits)));
                    break;
                case EXT_GETMONSTERTYPESCNT:
                    intResult = static_cast<unsigned int>(selectedObject->GetMonsterVidsCnt(selectedValue));
                    break;
                case EXT_GetBossSize:
                    intResult = static_cast<unsigned int>(selectedObject->GetBossSize());
                    break;
                case EXT_GetBossHpCoeff:
                    intResult = static_cast<unsigned int>(selectedObject->GetBossHpCoeff());
                    break;
                case EXT_IsBossPeriod:
                    intResult = selectedObject->IsBossPeriod(selectedValue)?1u:0u;
                    break;
                case EXT_GETBONUSID:
                    intResult = static_cast<unsigned int>(selectedObject->GetPeriodBonusId(selectedValue));
                    break;
                case EXT_GetBossColor:
                    intResult = static_cast<unsigned int>(selectedObject->GetBossColor(static_cast<int>(argument2Bits)));
                    break;
                case EXT_GetScoresCoeff:
                    intResult = static_cast<unsigned int>(selectedObject->GetScoresCoeff());
                    break;
                default:
                    break;
                }
            }
        }
        break;
    }
    case EXT_GETQUESTNAME:
    {
        iQuestServer* questOwner = m_pQuestServer;
        iRPGQuest* quest = questOwner->GetQuest(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), false, true);
        const char* text = argument3Bits == 777u
            ? quest->GetInfoStr(static_cast<iRPGQuest::INFO_STR>(0))
            : m_pQuestServer->GetSubQuestName(quest);
        if (text) stringResultRef = text;
        returnsString = true;
        break;
    }
    case EXT_IFINCREASEDBYIMPLANT:
    {
        iRPGObj* object = 0;
        if (argument3Bits != 0u)
            {
            iRPGHero* hero = GetHeroForShow();
            object = static_cast<iRPGObj*>(hero->QueryInterface(9));
        }
            else
            {
            iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
            object = objectRegistry->GetItemById(static_cast<int>(argument1Bits));
        }
        if (object) intResult = object->IfIncreasedByImplant(static_cast<int>(argument2Bits))?1u:0u;
        break;
    }
    case EXT_GETCOLORLINEDIR:
    {
        unsigned int absoluteDirection = 0u;
        iRPGHero* hero = GetHeroForShow();
        if (hero)
            {
            iRPGObj* object = static_cast<iRPGObj*>(GetHeroForShow()->QueryInterface(9));
            absoluteDirection = static_cast<unsigned int>(object->GetSkillValue(RPGHERO_COLOR));
        }
        intResult = absoluteDirection;
        const int directionCount = static_cast<iVidCopier*>(self)->GetVidDirectionsNum(739);
        if (directionCount != 0)
            {
            const int scaledDirection = static_cast<int>(absoluteDirection<<8);
            intResult = static_cast<unsigned int>(scaledDirection/directionCount)&0xFFu;
        }
        break;
    }
    case EXT_RESETQUESTSYSTEM:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        if (!player->IsCurPlayer())
            DebugAssert(4u, "this->IsCurPlayer()", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 705u);
        iQuestServer* questOwner = m_pQuestServer;
        if (questOwner) questOwner->PushCurState(0);
        break;
    }
    case EXT_FIND_ID_FOR_SPRITE:
    {
        iItemTaker* const spriteIndex = self->m_itemTaker;
        iRPGFrameWork* const objectRegistry = m_pRPGFrameWork;
        if (spriteIndex && objectRegistry)
            {
            SPRITE* const sprite = reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits));
            const unsigned int nVid = static_cast<unsigned int>(sprite->Vid()->nVid);
            if (!spriteIndex->IsNeededVid(static_cast<int>(nVid)))
                DebugAssert(4u, "m_pItemTaker->IsNeededVid( nVid )", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 854u);
            self->m_itemTaker->FindIdForSprite(sprite);
        }
        break;
    }
    case EXT_GETINFOTEXT:
    {
        iRPGFrameWork* const objectRegistry = m_pRPGFrameWork;
        iTextDB* const textOwner = static_cast<iTextDB*>(objectRegistry->QueryInterface(28));
        const char** const record = textOwner->GetExtInfo(0, static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        if (record != 0)
            {
            const char* const text = textOwner->GetSpeciallyDecodedString(record, -1);
            stringResultRef = text;
        }
        returnsString = true;
        break;
    }
    case EXT_GETINVNAME:
    {
        iRPGHero* const hero = m_refLegs.m_object;
        if (hero)
            {
            iInventory* const item = hero->GetInvServer()->GetInv(static_cast<int>(argument1Bits));
            stringResultRef = item->GetName();
        }
        returnsString = true;
        break;
    }
    case EXT_ResetAllQuests:
    {
        iQuestServer* questOwner = m_pQuestServer;
        if (!questOwner)
            {
            DebugAssert(5u, "m_pQuestServer", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1073u);
            questOwner = m_pQuestServer;
        }
        questOwner->ResetAllQuests();
        break;
    }
    case EXT_CopyVid:
    {
        VID* const destinationVid = Map->Vid(static_cast<int>(argument1Bits));
        VID* const sourceVid = Map->Vid(static_cast<int>(argument2Bits));
        Map->CopyVid(destinationVid, sourceVid);
        break;
    }
    case EXT_GetSQuestData:
    {
        iQuestServer* questOwner = m_pQuestServer;
        iRPGQuest* quest = questOwner->GetQuest(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), true, true);
        if (quest)
            {
            if (argument3Bits == 0u)
                {
                intResult = static_cast<unsigned int>(quest->GetSubQNum());
            }
                else if (argument3Bits == 1u)
                {
                questOwner = m_pQuestServer;
                if (questOwner) intResult = questOwner->InCurLevel(quest)?1u:0u;
            }
        }
        break;
    }
    case EXT_SETINVULNERABLE:
    {
        const unsigned int army = argument1Bits;
        const unsigned int baseHeroId = static_cast<unsigned int>(g_iHeroIdForInvuln);
        const unsigned int mappedHeroId = static_cast<unsigned int>(SFLB_GetHeroId(static_cast<int>(army)));
        const unsigned int heroId = baseHeroId+mappedHeroId-army*2u;
        iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
        iRPGObj* hero = objectRegistry->GetItemById(static_cast<int>(heroId));
        if (!hero || hero->GetGameObj() != 0)
            DebugAssert(5u, "hero && hero->GetType() == TYPE_HERO", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 249u);
        iRPGHero* invulnerabilityOwner = static_cast<iRPGHero*>(hero->QueryInterface(0));
        invulnerabilityOwner->SetInvulnerable(static_cast<int>(argument2Bits)>0);
        break;
    }
    case EXT_GETCHARSKILL:
    {
        iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
        iTextDB* skillService = objectRegistry->AsTextDB();
        iRPGObj* object = 0;
        if (argument3Bits != 0u)
            {
            iRPGHero* hero = GetHeroForShow();
            object = static_cast<iRPGObj*>(hero->QueryInterface(9));
        }
            else
            {
            object = objectRegistry->GetItemById(static_cast<int>(argument1Bits));
        }
        const char* text = 0;
        if (object)
            {
            if (argument2Bits == static_cast<unsigned int>(RPGHERO_FULLNAME)) text = objectRegistry->GetItemName(object);
            else text = skillService->GetFormattedSkill(object, static_cast<int>(argument2Bits));
        }
        if (text) stringResultRef = text;
        returnsString = true;
        break;
    }
    case EXT_GETPERIODLENGTH:
    {
        iSurvive* periodOwner = m_pSurvive.m_object;
        if (!periodOwner) break;
        iSurvive* selectedObject = 0;
        int selectedValue = 0;
        const unsigned int selectorValue = static_cast<unsigned int>(periodOwner->GetIntrval(static_cast<int>(argument1Bits), selectedObject, selectedValue, 0));
        if (selectedObject)
            {
            if (argument2Bits == 0u) intResult = static_cast<unsigned int>(selectedObject->GetPeriodLength(selectedValue));
            if (argument2Bits == 1u)
                {
                    intResult = selectorValue;
                    break;
                }
            if (argument2Bits == 2u) intResult = static_cast<unsigned int>(selectedObject->GetPauseLength(selectedValue));
            if (argument2Bits == 4u)
                {
                zSurvive* typed = static_cast<zSurvive*>(selectedObject->QueryInterface(53));
                intResult = static_cast<unsigned int>(typed->GetWithoutMenu(selectedValue));
            }
            if (argument2Bits == 5u)
                {
                intResult = selectorValue;
                for (int i = 0;i<static_cast<int>(selectedValue);++i)
                    {
                    intResult += static_cast<unsigned int>(selectedObject->GetPauseLength(i));
                    intResult += static_cast<unsigned int>(selectedObject->GetPeriodLength(i));
                }
            }
            if (argument2Bits == 6u)
                {
                intResult = 0u;
                const int count = selectedObject->GetPerCnt();
                for (int i = 0;i<count;++i)
                    {
                    intResult += static_cast<unsigned int>(selectedObject->GetPauseLength(i));
                    intResult += static_cast<unsigned int>(selectedObject->GetPeriodLength(i));
                }
            }
        }
        if (argument2Bits == 3u || argument2Bits == 7u)
            {
            if (!selectedObject || selectedValue == -1)
                {
                    intResult = 1u;
                    break;
                }
            zSurvive* typed = static_cast<zSurvive*>(selectedObject->QueryInterface(53));
            const unsigned int count = static_cast<unsigned int>(static_cast<iSurvive*>(typed)->GetPerCnt());
            if (static_cast<unsigned int>(selectedValue) == count-1u &&
                (argument2Bits == 7u || static_cast<zSurvive*>(selectedObject->QueryInterface(53))->GetNextInt() == 0)) intResult = 1u;
            else intResult = 0u;
        }
        break;
    }
    case EXT_GETQUESTSSTATE:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        if (player->GetArcadeMod_() == 0)
            {
            iQuestServer* questOwner = m_pQuestServer;
            iRPGQuest* quest = questOwner->GetQuest(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), argument3Bits == 0u, true);
            if (quest) intResult = static_cast<unsigned int>(m_pQuestServer->GetQuestState(quest));
        }
        break;
    }
    case EXT_SPRITEHASNAME:
    {
        SPRITE* const sprite = reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits));
        if (sprite->Vid()->spriteClass == 19u)
            {
            if (sprite->Action(ACT_GET_TEXT, 0, 0, 0) != 0) intResult = 1u;
        }
            else
            {
            EX_SPRITE_DATA* const linked = sprite->ExtraData();
            if (linked && *linked->nameText.value != '\0') intResult = 1u;
        }
        break;
    }
    case EXT_GETTEXTHEIGHT:
        if (argument1Bits != 0u) intResult = ReadGlobalU32(argument1Bits+120u);
        break;
    case EXT_SETQUESTSTATE:
    {
        iQuestServer* questOwner = m_pQuestServer;
        iRPGQuest* quest = questOwner->GetQuest(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), false, false);
        if (quest) intResult = m_pQuestServer->SetQuestState(quest, static_cast<int>(argument3Bits), false)?1u:0u;
        iLogConsole* logger = ::gpLogCons;
        (void)logger->LogStr(LT_NOTICE, "qs from EXT_SETQUESTSTATE %d %d", argument1Bits, argument2Bits);
        break;
    }
    case EXT_GETREALVAL:
    {
        VID* damageRangeVid = ::EmptyVid;
        if (Map->VidCount()>1402)
            {
            VID* const candidate = Map->StoredVid(1402);
            if (candidate != 0) damageRangeVid = candidate;
        }
        unsigned char cacheFlags = damageRangeInitialized;
        if ((cacheFlags&1u) == 0u)
            {
            cacheFlags = static_cast<unsigned char>(cacheFlags|1u);
            damageRangeInitialized = cacheFlags;
            cachedDamageMinimum = damageRangeVid?damageRangeVid->deathDamageMin:0;
        }
        if ((cacheFlags&2u) == 0u)
            {
            cacheFlags = static_cast<unsigned char>(cacheFlags|2u);
            damageRangeInitialized = cacheFlags;
            cachedDamageMaximum = damageRangeVid?damageRangeVid->deathDamageMax:0;
        }
        if (argument1Bits == 0u) intResult = static_cast<unsigned int>(cachedDamageMinimum);
        else if (argument1Bits == 1u) intResult = static_cast<unsigned int>(cachedDamageMaximum);
        else DebugAssert(4u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1033u);
        break;
    }
    case EXT_GetPeriodNum:
    {
        iSurvive* periodOwner = m_pSurvive.m_object;
        if (!periodOwner)
            {
                intResult = 0u;
                break;
            }
        iSurvive* selectedObject = 0;
        int selectedValue = 0;
        (void)periodOwner->GetIntrval(static_cast<int>(argument1Bits), selectedObject, selectedValue, 0);
        intResult = static_cast<unsigned int>(selectedValue);
        if (selectedObject)
            {
            zSurvive* typed = static_cast<zSurvive*>(selectedObject->QueryInterface(53));
            intResult = static_cast<unsigned int>(typed->GetAbsPeriodNum(selectedValue));
        }
        break;
    }
    case EXT_bShowInvGrnd:
    {
        iRPGHero* const hero = m_refLegs.m_object;
        if (!hero) break;
        iInvServer* const inventoryList = hero->GetInvServer();
        iInventory* const item = inventoryList->GetInv(INV_GRND);
        zInventory* const typed = static_cast<zInventory*>(item->QueryInterface(46));
        if (typed->m_apItems.begin != 0 && typed->m_apItems.end != typed->m_apItems.begin)
            {
            intResult = 1u;
            break;
        }
        if (inventoryList->GetTakenObj() != 0) intResult = 1u;
        break;
    }
    case EXT_GetSurvInt:
    {
        returnsString = (argument2Bits == 0u);
        stringResultRef = StringGlobals::EmptyText();
        iSurvive* periodOwner = m_pSurvive.m_object;
        if (!periodOwner) break;
        iSurvive* selectedObject = 0;
        int selectedValue = 0;
        int alternateValue = 0;
        intResult = static_cast<unsigned int>(periodOwner->GetIntrval(static_cast<int>(argument1Bits), selectedObject, selectedValue, &alternateValue));
        if (!selectedObject) break;
        if (returnsString)
            {
            iRPGObj* typed = static_cast<iRPGObj*>(selectedObject->QueryInterface(9));
            const char* text = typed->GetName();
            if (text) stringResultRef = text;
        }
            else
            {
            intResult = static_cast<unsigned int>(alternateValue);
        }
        break;
    }
    case Ext_Monst2Create:
    {
        iSurvive* periodOwner = m_pSurvive.m_object;
        if (!periodOwner) break;
        const int timeValue = static_cast<int>(argument1Bits);
        const int seconds = timeValue/1000;
        const int milliseconds = timeValue%1000;
        iSurvive* selectedObject = 0;
        int selectedValue = 0;
        const unsigned int periodBase = static_cast<unsigned int>(periodOwner->GetIntrval(seconds, selectedObject, selectedValue, 0));
        if (selectedObject)
            {
            const unsigned int localTime = periodBase*1000u+static_cast<unsigned int>(milliseconds);
            zSurvive* typed = static_cast<zSurvive*>(selectedObject->QueryInterface(53));
            intResult = static_cast<unsigned int>(typed->GetMonst2Create(selectedValue, static_cast<int>(localTime), static_cast<int>(argument2Bits)));
        }
        break;
    }
    case EXT_GETSPRITENAME:
    {
        SPRITE* const sprite = reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits));
        EX_SPRITE_DATA* const linked = sprite->ExtraData();
        if (linked && *linked->nameText.value != '\0')
            {
            STRING builtName = sprite->GetName();
            stringResultRef = builtName;
        }
        returnsString = true;
        break;
    }
    case EXT_RK_SAVE:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        STRING path = profileState->GetCurSaveName2(static_cast<SAVE_TYPE>(3), false);
        (void)zInputMap::Save(path.m_value);
        break;
    }
    case EXT_RK_CANCEL:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        STRING path = profileState->GetCurSaveName2(static_cast<SAVE_TYPE>(3), false);
        const unsigned int parsed = zInputMap::Load(path.m_value)?1u:0u;
        if (parsed == 0u) (void)zInputMap::Load("input.cfg");
        break;
    }
    case EXT_RK_INIT:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        STRING path = profileState->GetCurSaveName2(static_cast<SAVE_TYPE>(3), false);
        (void)zInputMap::Load(path.m_value);
        break;
    }
    case EXT_SetCurProf:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        if (static_cast<unsigned int>(profileState->m_currentProfile) != argument1Bits)
            {
            self->ProfMngr()->ReleaseArmyHero();
        }
        profileState->SetCurProf(static_cast<int>(argument1Bits));
        if (profileState->m_currentProfile != -1)
        {
            STRING path = profileState->GetCurSaveName2(static_cast<SAVE_TYPE>(3), false);
            (void)zInputMap::Load(path.m_value);
        }
        break;
    }
    case EXT_LEVELMONEY:
    {
        iQuestServer* const questOwner = m_pQuestServer;
        if (questOwner && argument1Bits != 0xFFFFFFFFu)
            {
            STRING levelText = Printf("Lvl_%.2d", g_pProfLdr->GetQuestLevNum(argument1));
            iRPGFrameWork* const levelService = static_cast<iRPGFrameWork*>(m_pQuestServer->QueryInterface(23));
            iRPGObj* const levelRecord = levelService->GetObjByNamee(levelText.m_value, true);
            if (levelRecord) intResult = static_cast<unsigned int>(levelRecord->GetSkillValue(RPGSURVIVE_PERIODS));
        }
        break;
    }
    case EXT_GetLogFaceDir:
    {
        zProfMngr* logOwner = self->ProfMngr();
        if (argument2Bits == 0u)
            {
            intResult = logOwner->GetLogFaceDir(static_cast<int>(argument1Bits));
            break;
        }
        const int timeValue = static_cast<int>(logOwner->GetRealLogNum(static_cast<int>(argument1Bits)));
        intResult = static_cast<unsigned int>(timeValue);
        const int seconds = timeValue/1000;
        const int milliseconds = timeValue%1000;
        {
            const STRING formatted = milliseconds == 0
                ? Printf("%d", static_cast<unsigned int>(seconds))
                : Printf("%d_%d", static_cast<unsigned int>(seconds), static_cast<unsigned int>(milliseconds));
            stringResultRef = formatted;
        }
        returnsString = true;
        break;
    }
    case EXT_SETGAMMAITEMDISABLED:
    {
        iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
        objectRegistry->SetGammaItemDisabled(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), static_cast<int>(argument3Bits));
        break;
    }
    case EXT_GETCURCADR:
        intResult = static_cast<unsigned int>(static_cast<iVidCopier*>(self)->GetCurCadr(static_cast<int>(argument1Bits)));
        break;
    case EXT_SETVIDREDUCESCALE:
        if (static_cast<int>(argument1Bits) >= 4096)
            DebugAssert(5u, "var1 < Z_MAX_VID_NUM", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 498u);
        vidReduceScale[argument1] = argument2;
        break;
    case EXT_GETVIDREDUCESCALE:
        if (static_cast<int>(argument1Bits) >= 4096)
            DebugAssert(5u, "var1 < Z_MAX_VID_NUM", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 504u);
        intResult = static_cast<unsigned int>(vidReduceScale[argument1]);
        break;
    case EXT_GETQUESTSNUM:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        if (player->GetArcadeMod_() == 0)
            {
            iQuestServer* questOwner = m_pQuestServer;
            intResult = static_cast<unsigned int>(questOwner->GetQuestNum());
        }
        break;
    }
    case EXT_GETQUESTSDIRTYSTATE:
    {
        iQuestServer* questOwner = m_pQuestServer;
        intResult = questOwner->GetDirtyState()?1u:0u;
        break;
    }
    case EXT_GETSUBQUESTSNUM:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        if (player->GetArcadeMod_() == 0)
            {
            iQuestServer* questOwner = m_pQuestServer;
            intResult = static_cast<unsigned int>(questOwner->GetSubQuestNum(static_cast<int>(argument1Bits)));
        }
        break;
    }
    case EXT_CONVERFROMCARTESIAN:
        if (argument3Bits == 1u)
            intResult = static_cast<unsigned int>(ScriptExecCartesianAngle(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits)));
        else if (argument3Bits == 2u)
            intResult = static_cast<unsigned int>(ScriptExecCartesianLength(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits)));
        else
            DebugAssert(4u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 520u);
        break;
    case EXT_KILLOBJECTS:
    {
        self->m_itemTaker->KillObjects(static_cast<int>(argument1Bits));
        break;
    }
    case EXT_GETSHOPITEMSNUM:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        intResult = static_cast<unsigned int>(shop->GetItemsNum(argument1));
        break;
    }
    case EXT_GETSHOWNSHOPITEMSNUM:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        intResult = static_cast<unsigned int>(shop->GetListItemsNum(argument1));
        break;
    }
    case EXT_GETFIRSTSHOWNITEM:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        intResult = static_cast<unsigned int>(shop->GetFirstShownItemNum(argument1));
        break;
    }
    case EXT_SHOPLISTPAGEUP:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        shop->PageUp(argument1);
        break;
    }
    case EXT_SHOPLISTPAGEDOWN:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        shop->PageDown(argument1);
        break;
    }
    case EXT_GETABSDIR:
    {
        const int directionCount = static_cast<iVidCopier*>(self)->GetVidDirectionsNum(static_cast<int>(argument1Bits));
        if (directionCount != 0)
            {
            const int scaledDirection = static_cast<int>(argument2Bits<<8);
            intResult = static_cast<unsigned int>(scaledDirection/directionCount)&0xFFu;
        }
        break;
    }
    case EXT_GETRANDBONUSID:
    {
        iCampaign* bonusOwner = m_pCampaign.m_object;
        if (bonusOwner) intResult = static_cast<unsigned int>(bonusOwner->GetBonusBaseId(static_cast<int>(argument1Bits)));
        break;
    }
    case EXT_SETSHOPLEVELNUM:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        shop->SetCurLevel(static_cast<int>(argument1Bits));
        iRPGImplant::generationSequence += argument1Bits;
        break;
    }
    case EXT_GET_MONST_GENERATORS_CNT:
    {
        for (SPRITE* sprite = ::Sprites->FirstAllSprite(); sprite; sprite = ::Sprites->NextAllSprite())
        {
            if ((sprite->RuntimeFlags()&SPRITE::ArmyMask) == (6u<<SPRITE::ArmyShift))
                continue;
            unsigned int cumulativeGeneratorMatches = 0u;
            for (int laneIndex = 0; laneIndex<4; ++laneIndex)
            {
                EX_SPRITE_DATA* const extra = sprite->ExtraData();
                if (!extra)
                    continue;
                const LIST<ACT>& lane = extra->actionLane[laneIndex];
                if (lane.m_count == 0 || lane.m_data[lane.m_count-1].command == ACT_STOP_STACK)
                    continue;

                unsigned int commandFlags = 0u;
                const int last = BitsToSigned32(static_cast<unsigned int>(sprite->ActionStack(laneIndex).m_count)-1u);
                for (int index = last; index >= 0; --index)
                {
                    const ACT& record = sprite->ActionStack(laneIndex).m_data[index];
                    if (record.command == ACT_BUILD_UNIT)
                    {
                        if ((commandFlags&4u) == 0u)
                        {
                            commandFlags |= 1u;
                            if (record.arg1>0 && g_abMonsterVid[record.arg1])
                                ++cumulativeGeneratorMatches;
                        }
                    }
                    else if (record.command == ACT_GOTO_STACK)
                    {
                        if ((commandFlags&4u) == 0u)
                            commandFlags |= 2u;
                    }
                    else if (record.command == ACT_STOP_STACK)
                    {
                        commandFlags |= 4u;
                    }
                }
                if ((commandFlags&3u) == 3u)
                    (void)sprite->Action(ACT_ADD_ITEM, -666, 0, 0);
                else
                    intResult += cumulativeGeneratorMatches;
            }
        }
        break;
    }
    case EXT_SETALLQUESTSUNACTUAL:
    {
        iQuestServer* questOwner = m_pQuestServer;
        questOwner->SetAllQuestsUnActual();
        break;
    }
    case EXT_GETBRIEFINGTEXT:
        DebugAssert(2u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 877u);
        break;
    case EXT_GETSURVIVEMODE:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        intResult = static_cast<unsigned int>(player->GetArcadeMod_())&1u;
        break;
    }
    case EXT_FORDEBUG:
        if (argument1Bits == 1u)
        {
            *reinterpret_cast<volatile unsigned char*>(10u) = 99u;
        }
        else if (argument1Bits == 2u)
        {
            DebugAssert(4u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 911u);
        }
        else if (argument1Bits == 4u)
        {
            ShowStackOfAllObjects(STRING(""));
        }
        break;
    case EXT_GETDRAWSHADOW:
        intResult = Graph->deviceState&1u;
        break;
    case EXT_SETDRAWSHADOW:
    {
        Graph->SetDrawShadow(BitsToSigned32(argument1Bits));
        break;
    }
    case EXT_GETHOTKEY:
        GetHotKey(stringResultRef, static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        returnsString = true;
        break;
    case EXT_GETKEYWAITMODE:
        intResult = zInputMap::m_bKeyWaitAct != 65535u?1u:0u;
        break;
    case EXT_RK_DEFAULT:
        (void)zInputMap::Load("input.cfg");
        break;
    case EXT_SET_TEXT_DELAY:
        ::g_pDlgSrv->m_textDurationPercent = argument1Bits;
        break;
    case EXT_SHOPSCROLL:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        shop->PageScroll(argument1, argument2);
        break;
    }
    case EXT_SaveOptToFile:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        (void)profileState->ExpRegToFile();
        break;
    }
    case EXT_DELETEPROFILE:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        (void)profileState->DeleteProfile2(static_cast<int>(argument1Bits));
        break;
    }
    case EXT_SetCtrlWheelsCar:
        static_cast<iVidCopier*>(self)->SetCtrlWheelsCar(reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits)));
        break;
    case EXT_GetLogText:
    {
        zProfMngr* logOwner = self->ProfMngr();
        const char* text = logOwner->GetLogText(static_cast<int>(argument1Bits));
        stringResultRef = text;
        returnsString = true;
        break;
    }
    case EXT_GetLogsNum:
    {
        zProfMngr* logOwner = self->ProfMngr();
        intResult = logOwner->GetLogsNum();
        break;
    }
    case EXT_RefreshShop:
    {
        iShopDlgMenu* const shop = m_pShopDlgMenu;
        shop->RefreshAll();
        iHeroInvMenu* const heroMenu = m_pHeroInvMenu;
        heroMenu->InitOnShow();
        break;
    }
    case EXT_SET_TEXT_DELAY_STRS13:
    {
        DialogSrv* const dialogOwner = ::g_pDlgSrv;
        dialogOwner->m_oneLineDuration = argument1Bits;
        dialogOwner->m_twoLineDuration = argument2Bits;
        dialogOwner->m_threeLineDuration = argument3Bits;
        break;
    }
    case EXT_SET_TEXT_DELAY_STRS45:
    {
        DialogSrv* const dialogOwner = ::g_pDlgSrv;
        dialogOwner->m_fourLineDuration = argument1Bits;
        dialogOwner->m_fiveLineDuration = argument2Bits;
        break;
    }
    case EXT_PlaceTip:
    {
        SPRITE* const sprite = reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits));
        if (!sprite)DebugAssert(5u, "var1", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1112u);
        if (sprite->Action(ACT_GET_TEXT, 0, 0, 0) == 0)break;
        const VECTOR2& camera = Map->ShiftXY();
        const float screenXOffset = static_cast<float>(static_cast<double>(sprite->X())-static_cast<double>(camera.x));
        int screenX = TruncateFloatViaCrtToInt32(screenXOffset);
        const int depth = TruncateFloatViaCrtToInt32(sprite->Z());
        const float screenYOffset = static_cast<float>((static_cast<double>(sprite->Y())-static_cast<double>(depth))-static_cast<double>(camera.y));
        int screenY = TruncateFloatViaCrtToInt32(screenYOffset);
        GRAPH* const graph = Graph;
        const int width = BitsToSigned32(ReadU32(sprite, 116u));
        const int maxX = SubSigned32Wrap(SubSigned32Wrap(
            TruncateFloatViaCrtToInt32(graph->viewRight), width), 10);
        if (screenX>maxX)screenX = maxX;
        if (screenX<10)screenX = 10;
        if (screenY<15)screenY = 15;
        const float worldXOffset = static_cast<float>(static_cast<double>(static_cast<float>(screenX))+static_cast<double>(camera.x));
        const float worldYOffset = static_cast<float>(static_cast<double>(camera.y)+static_cast<double>(static_cast<float>(screenY)));
        const int worldY = AddSigned32Wrap(TruncateFloatViaCrtToInt32(worldYOffset), depth);
        const int worldX = TruncateFloatViaCrtToInt32(worldXOffset);
        (void)sprite->Action(ACT_CHANGE_COOR, worldX, worldY, depth);
        break;
    }
    case EXT_GetProgVer:
        intResult = g_iZmDebug;
        break;
    case EXT_ReloadDB:
        static_cast<iVidCopier*>(self)->ReloadDB();
        break;
    case EXT_GetQuestAccomplished:
    {
        iQuestServer* questOwner = m_pQuestServer;
        iRPGQuest* quest = questOwner->GetQuest(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), true, true);
        if (quest)
            {
            iRPGObj* typedQuest = static_cast<iRPGObj*>(quest->QueryInterface(9));
            intResult = static_cast<unsigned int>(typedQuest->GetSkillValue(RPGQUEST_ACCOMPLISHED));
        }
        break;
    }
    case EXT_ShowItemInfoById:
    {
        if (argument2 == 0)
        {
            iRPGObj* object = 0;
            if (argument1 > 0)
                object = m_pRPGFrameWork->GetItemById(argument1);
            m_pHeroInvMenu->ControlItemInfo(object, Map->InputState(), true);
        }
        else if (m_pShopDlgMenu)
        {
            (void)m_pShopDlgMenu->ScrollToItem(argument1, false);
        }
        break;
    }
    case Ext_ToggleLogCons:
    {
        iLogConsole* const logger = ::gpLogCons;
        logger->ToggleShow();
        break;
    }
    case EXT_GetNamedMonst:
        if (static_cast<int>(argument1Bits) >= 0 && static_cast<int>(argument1Bits) <= 9)
        {
            intResult = Map->State00C(argument1Bits);
        }
        break;
    case EXT_GetCurProf:
    {
        zProfLdr* const profileState = ::g_pProfLdr;
        intResult = static_cast<unsigned int>(profileState->m_currentProfile);
        break;
    }
    case EXT_GetMaxLevNum:
    {
        const unsigned int profileId = argument1Bits;
        const unsigned int survivalMode = argument2Bits != 0u?1u:0u;
        zProfLdr* const profileState = ::g_pProfLdr;
        intResult = profileState->GetMaxLevNum(static_cast<int>(profileId), survivalMode != 0u);
        break;
    }
    case EXT_GetMenuUnderCursor:
    {
        intResult = PtrToU32(Map->MenuSelectedSprite());
        break;
    }
    case EXT_GetBlinkStatebar:
    {
        iRPGHero* const hero = m_refLegs.m_object;
        if (hero) intResult = hero->GetUseMedTime() != 0?1u:0u;
        break;
    }
    case EXT_SetHeroPDId:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        player->SetHeroPDId(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits), argument3Bits != 0u);
        break;
    }
    case EXT_GetHeroPDId:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        intResult = static_cast<unsigned int>(player->GetHeroPDId(static_cast<int>(argument1Bits)));
        break;
    }
    case EXT_SetHeroPDClr:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        player->SetHeroPDClr(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        break;
    }
    case EXT_GetHeroPDClr:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        intResult = static_cast<unsigned int>(player->GetHeroPDClr(static_cast<int>(argument1Bits)));
        break;
    }
    case EXT_SetMainMenu:
        self->m_loadHeroFromMaxSave = 1u;
        break;
    case Ext_SetFulldamagedArmorCost:
        zRPGArmor::m_iFullDmgCost = BitsToSigned32(argument1Bits);
        break;
    case Ext_GetObjectId:
        if (argument1Bits != 0u)
        {
            SPRITE* const object = reinterpret_cast<SPRITE*>(U32ToPtr(argument1Bits));
            EX_SPRITE_DATA* const linked = object->ExtraData();
            intResult = linked?static_cast<unsigned int>(linked->objectId):0xFFFFFFFFu;
        }
        break;
    case EXT_SetNetRespItem:
    {
        self->m_itemTaker->SetNetRespItem(static_cast<int>(argument1Bits), reinterpret_cast<SPRITE*>(U32ToPtr(argument2Bits)));
        break;
    }
    case EXT_GetNetRespItem:
    {
        specialResultRef = self->m_itemTaker->GetNetRespItem(static_cast<int>(argument1Bits));
        break;
    }
    case EXT_SwitchHeroCS:
        SwitchHeroCS(argument1Bits != 0u);
        break;
    case EXT_SetSurvNetCoeff:
    {
        const unsigned int coefficientSelector = argument1Bits;
        const unsigned int coefficientValue = argument2Bits;
        if (coefficientSelector == SNC_MONSTCNT) zSurvive::ms_MonstCntCoeff = BitsToSigned32(coefficientValue);
        else if (coefficientSelector == SNC_BONUSCNT) zSurvive::ms_EmptyBonusWeightCoeff = BitsToSigned32(coefficientValue);
        else DebugAssert(5u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1341u);
        break;
    }
    case EXT_GetHallRecCnt:
    {
        const unsigned int hallType = argument1Bits;
        zRecordDB* const hallOfFame = RecordDatabase;
        intResult = hallOfFame->GetRecCnt(static_cast<int>(hallType));
        break;
    }
    case EXT_GetHallData:
    {
        const int hallType = static_cast<int>(argument1Bits);
        const int recordIndex = static_cast<int>(argument2Bits);
        const int selector = static_cast<int>(argument3Bits);
        zRecordDB* const hallOfFame = RecordDatabase;
        const AS_RECORD* buf = hallOfFame->GetRecData(static_cast<int>(argument1Bits), static_cast<int>(argument2Bits));
        if (!buf)
            {
            DebugAssert(5u, "buf", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1355u);
            iLogConsole* logger = ::gpLogCons;
            (void)logger->LogStr(LT_ERR, "error in GetRecData %d, %d", static_cast<unsigned int>(hallType), static_cast<unsigned int>(recordIndex));
            break;
        }
        const unsigned int weaponScores = ScriptExecVectorDwordCount(buf->m_weaponScores);
        const unsigned int monsterCounts = ScriptExecVectorDwordCount(buf->m_weaponKills);
        const unsigned int pairCount = weaponScores>monsterCounts?weaponScores:monsterCounts;
        returnsString = true;
        if (selector == 7)
            {
            returnsString = false;
            intResult = buf->m_cheater != 0u?1u:0u;
        }
            else if (selector == 5 || selector == 6)
            {
            const char* text = selector == 5 ? buf->m_playerName.c_str() : buf->m_recordTimestamp.c_str();
            if (!text)text = "";
            stringResultRef = text;
        }
            else if (selector <= 3)
            {
            if (selector<0)DebugAssert(5u, "var3 >= 0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1381u);
            if (selector == 2)
                {
                    returnsString = false;
                    intResult = static_cast<unsigned int>(buf->m_elapsedSeconds);
                }
            else if (selector == 3) ScriptExecAssignFormattedHallValue(stringResultRef, "%u", buf->m_checksum);
            else
                {
                const int* const scalarValues = &buf->m_totalScore;
                const unsigned int value = static_cast<unsigned int>(scalarValues[selector]);
                ScriptExecAssignFormattedHallValue(stringResultRef, "%d", value);
            }
        }
            else if (selector == 8)
            {
            returnsString = false;
            intResult = weaponScores;
        }
            else if (selector >= 9 && selector<static_cast<int>(9u+pairCount*2u))
            {
            returnsString = false;
            const unsigned int delta = static_cast<unsigned int>(selector-9);
            const unsigned int index = delta>>1;
            if ((delta&1u) == 0u)
                {
                if (index<weaponScores)intResult = static_cast<unsigned int>(buf->m_weaponScores.begin[index]);
            }
                else
                {
                if (index<monsterCounts)intResult = static_cast<unsigned int>(buf->m_weaponKills.begin[index]);
            }
        }
            else
            {
            DebugAssert(5u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1418u);
            returnsString = false;
        }
        break;
    }
    case EXT_AddHallData:
        if (argument1Bits == 0u)
        {
            self->m_hallRecord.m_totalScore = static_cast<int>(argument2Bits);
            self->m_hallRecord.m_weaponScores.end = self->m_hallRecord.m_weaponScores.begin;
            self->m_hallRecord.m_weaponKills.end = self->m_hallRecord.m_weaponKills.begin;
        }
        else if (argument1Bits == 1u)
        {
            self->m_hallRecord.m_monsterKills = static_cast<int>(argument2Bits);
            self->m_hallRecord.m_elapsedSeconds = static_cast<int>(argument3Bits);
        }
        else if (argument1Bits == 2u)
        {
            ScriptExecAppendHallDword(&self->m_hallRecord.m_weaponScores, argument2Bits);
            ScriptExecAppendHallDword(&self->m_hallRecord.m_weaponKills, argument3Bits);
        }
        else if (argument1Bits == 3u)
        {
            self->m_hallRecord.m_cheater = argument3Bits != 0u?1u:0u;
            zRecordDB* const hallOfFame = RecordDatabase;
            hallOfFame->AddRecord(static_cast<int>(argument2Bits), self->m_hallRecord);
        }
        break;
    case EXT_ClearHallData:
    {
        const int hallType = static_cast<int>(argument1Bits);
        if (hallType >= 0 && hallType<REC_TYPES_CNT)
            {
            zRecordDB* const hallOfFame = RecordDatabase;
            hallOfFame->ClearTable(hallType);
        }
        break;
    }
    case EXT_PopPushLifeTIme:
    {
        ScriptExecMapControl& lifetimeValues = m_lifetimeValues;
        if (argument1Bits == 1u)
            {
            lifetimeValues.insert(BitsToSigned32(argument2Bits), BitsToSigned32(argument3Bits));
        }
            else if (argument1Bits == 2u)
            {
            intResult = static_cast<unsigned int>(lifetimeValues.find(BitsToSigned32(argument2Bits)));
        }
            else
            {
            DebugAssert(5u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1479u);
        }
        break;
    }
    case EXT_IsDebugMode:
    {
        intResult = static_cast<unsigned int>(::Const->DebugMode);
        break;
    }
    case Ext_ShowDialogsSub:
    {
        DialogSrv* const dialogOwner = ::g_pDlgSrv;
        dialogOwner->m_showDialogSubtitles = argument1Bits != 0u?1u:0u;
        break;
    }
    case EXT_CheatHappen:
    {
        iRPGObj* const eventSink = m_refBody.m_object;
        if (eventSink) (void)eventSink->IncSkillValue(RPGHERO_Cheater, 1);
        break;
    }
    case EXT_RandomNetDbg:
    {
        const unsigned int maxValue = argument1Bits;
        const unsigned int networkDebugId = argument2Bits;
        networkTransmissionId = networkDebugId;
        intResult = static_cast<unsigned int>(Random(static_cast<int>(maxValue)));
        networkTransmissionId = 0u;
        break;
    }
    case EXT_GetNameById:
    {
        returnsString = true;
        iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
        const unsigned int objectId = argument1Bits;
        if (objectRegistry && objectId != 0u)
            {
            iRPGObj* object = objectRegistry->GetItemById(static_cast<int>(objectId));
            if (object)
                {
                const char* name = object->GetName();
                stringResultRef = name;
            }
        }
        break;
    }
    case EXT_GetStrVal:
    {
        returnsString = true;
        const unsigned int stringValueId = argument1Bits;
        const unsigned int stringValue = argument2Bits;
        iRPGFrameWork* objectRegistry = m_pRPGFrameWork;
        iTextDB* stringService = objectRegistry->AsTextDB();
        const char* text = stringService->GetStringValue(static_cast<int>(stringValueId), static_cast<int>(stringValue));
        if (text) stringResultRef = text;
        break;
    }
    case EXT_GetInvAmmoVid:
        if (m_refLegs.m_object)
            intResult = static_cast<unsigned int>(m_refLegs.m_object->GetInvAmmoVid(argument1, argument2));
        break;
    case EXT_SetArgValI:
    case EXT_GetArgValI:
    case EXT_SetArgValS:
    case EXT_GetArgValS:
    {
        if (command == EXT_GetArgValS)
            returnsString = true;
        zOptSaveFile* options = 0;
        if (argument1 == 1)
            options = g_pOptSaveGlobal;
        else if (argument1 == 2)
        {
            if (!g_pOptSaveUser)
            {
                const unsigned int sourceLine = command == EXT_SetArgValI ? 1536u :
                    command == EXT_SetArgValS ? 1552u : command == EXT_GetArgValI ? 1568u : 1585u;
                DebugAssert(5u, "g_pOptSaveUser", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", sourceLine);
            }
            options = g_pOptSaveUser;
            if (!options)
                break;
        }
        else
        {
            const unsigned int sourceLine = command == EXT_SetArgValI ? 1541u :
                command == EXT_SetArgValS ? 1557u : command == EXT_GetArgValI ? 1573u : 1590u;
            DebugAssert(5u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", sourceLine);
            break;
        }
        if (command == EXT_SetArgValI)
            options->SetInt(m_strVar1, argument2);
        else if (command == EXT_GetArgValI)
            intResult = static_cast<unsigned int>(options->GetInt(m_strVar1, argument2));
        else if (command == EXT_SetArgValS)
            options->SetString(m_strVar1, m_strVar2);
        else
        {
            const std::string value = options->GetString(m_strVar1, m_strVar2);
            stringResultRef = value.c_str();
        }
        break;
    }
    case EXT_SetUserName:
    {
        STRING name(m_strVar1.c_str());
        g_pProfLdr->SetProfName(argument1, name);
        break;
    }
    case EXT_GetUserName:
        returnsString = true;
        stringResultRef = g_pProfLdr->GetProfName(argument1);
        break;
    case EXT_SetQuestLevNum:
        g_pProfLdr->SetQuestLevNum(argument1, argument2);
        break;
    case EXT_GetSpriteByName:
        specialResultRef = ::Sprites->FindNamedSprite(STRING(m_strVar1.c_str()));
        break;
    case EXT_ClearSpriteName:
        if (argument1)
            reinterpret_cast<SPRITE*>(argument1Bits)->SetName(STRING(""));
        break;
    case EXT_CarUtils:
    {
        SPRITE* actor = static_cast<iPlayer*>(self)->Flagman();
        if (argument3)
        {
            actor = reinterpret_cast<SPRITE*>(argument3Bits);
            SetCarToStop(actor);
        }
        if (!actor) break;
        zCarControl* car = static_cast<MAN*>(actor);
        switch (argument1)
        {
        case 1:
            car->RecordCarPath(true, argument2);
            break;
        case 2:
            car->PlayCarPath(true, argument2);
            break;
        case 3:
            car->StopCarPath();
            break;
        case 4:
            car->SetInputDisabled(argument2 != 0);
            break;
        default:
            break;
        }
        break;
    }
    case EXT_PushInt:
        if (argument1Bits <= 9u)
            m_extraCommandArgs[argument1Bits] = argument2;
        break;
    case EXT_AddCommandEx:
        if (argument1)
            reinterpret_cast<SPRITE*>(argument1Bits)->AddActionAfterStop(
                argument2, argument3, m_extraCommandArgs[0],
                m_extraCommandArgs[1], m_extraCommandArgs[2]);
        break;
    case EXT_GetCarUtilsState:
    {
        iPlayer* const player = static_cast<iPlayer*>(self);
        if (player->Flagman())
            intResult = static_cast<unsigned int>(static_cast<MAN*>(player->Flagman())->GetCarPathState());
        break;
    }
    case EXT_AddBonuses:
        {
            iRPGQuest* quest = m_pQuestServer->GetQuest(argument1, argument2, false, true);
            if (quest) m_pQuestServer->AddBonuses(quest);
        }
        break;
    case EXT_AS21ShopProc:
    {
        switch (argument1)
        {
        case SP_GetClickedInv:
            intResult = static_cast<unsigned int>(m_pHeroInvMenu->GetClickedInv());
            break;
        case SP_ResetClickedInv:
            m_pHeroInvMenu->ResetClickedInv();
            break;
        case SP_TryToBuyItemByBaseId:
        {
            iRPGObj* const object = m_pRPGFrameWork->GetItemById(argument2);
            if (object)
                (void)m_pShopDlgMenu->TryToBuyItem(object->GetObjCopy());
            break;
        }
        case SP_GetClickedInvMBL:
            intResult = m_pHeroInvMenu->GetClickedInvMBL() ? 1u : 0u;
            break;
        case SP_TryToSellItemByBaseId:
        {
            iRPGObj* const object = m_refLegs.m_object->GetInvServer()->GetItemByBaseId(argument2, 3);
            if (object)
                (void)m_pShopDlgMenu->TryToSellItem(object, 0u);
            break;
        }
        case SP_ShowInfoByBaseId1:
            shopItemInfoX = argument2;
            shopItemInfoY = argument3;
            break;
        case SP_ShowInfoByBaseId2:
        {
            iRPGObj* const object = argument2 > 0 ? m_pRPGFrameWork->GetItemById(argument2) : 0;
            if (object)
            {
                m_pShopItemInfo->SetScreenCoordinates(static_cast<float>(shopItemInfoX), static_cast<float>(shopItemInfoY));
                m_pShopItemInfo->ShowObjInfo(object, true, 0);
            }
            else
                m_pShopItemInfo->ShowObjInfo(0, false, 0);
            break;
        }
        case SP_GetMouseOverInv:
        {
            const VECTOR2& mouse = Map->InputState()->ClientMouseXY();
            const iInventory* const inventory = m_refLegs.m_object->GetInvServer()->GetInventoryByPoint(mouse.x, mouse.y);
            if (inventory)
                intResult = static_cast<unsigned int>(inventory->GetId());
            break;
        }
        case SP_ResetHeroInv:
            if (m_refLegs.m_object)
                m_refLegs.m_object->ResetHeroInv();
            break;
        case SP_GetSynById:
            intResult = static_cast<unsigned int>(m_pRPGFrameWork->GetSynById(argument2));
            break;
        case SP_GetCostById:
        {
            iRPGObj* const object = m_pRPGFrameWork->GetItemById(argument2);
            if (object)
            {
                zItem* const item = static_cast<zItem*>(object->QueryInterface(49));
                intResult = static_cast<unsigned int>(item->GetBaseCostFull());
            }
            break;
        }
        default:
            break;
        }
        break;
    }
    case EXT_ResetMaxLevelForNewGame:
        if (!g_pProfLdr)
            DebugAssert(5u, "g_pProfLdr", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1760u);
        if (g_pProfLdr)
            g_pProfLdr->ResetMaxLevelForNewGame();
        break;
    default:
        DebugAssert(4u, "0", "..\\src\\zm\\Game\\ScriptExec\\zScriptExec.cpp", 1767u);

        break;
    }

    intResultRef = BitsToSigned32(intResult);
    return returnsString?1u:0u;
}

unsigned int* __cdecl zScriptExec_DwordRangeCopy(
    const unsigned int* first,const unsigned int* last,unsigned int* destination);
unsigned int* __cdecl zScriptExec_DwordRangeCopy(
    const unsigned int* sourceBegin, const unsigned int* sourceEnd, unsigned int* destination)
{
    unsigned int* writeCursor = destination;
    while (sourceBegin != sourceEnd)
    {
        *writeCursor = *sourceBegin;
        ++sourceBegin;
        ++writeCursor;
    }
    return writeCursor;
}

