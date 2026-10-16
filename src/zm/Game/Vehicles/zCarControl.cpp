#include "zm/Utils/zLogConsole.h"
#include "../../../engine_globals.h"
#include "mylib.h"
#include "zm/Game/Vehicles/zCarControl.h"
#include "script.h"
#include "script_exec.h"
#include "sprite.h"
#include "sprite_collector.h"
#include "input.h"
#include "map.h"
#include "zm/Utils/zRefObject.h"
#include <math.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <cstdarg>

CAR_PATH_VECTOR::CAR_PATH_VECTOR()
    : m_owner(static_cast<zContainerProxy<CAR_PATH_VECTOR>*>(CrtNew(sizeof(zContainerProxy<CAR_PATH_VECTOR>)))),
      m_begin(0), m_end(0), m_capacityEnd(0)
{
    if (m_owner)
        m_owner->m_owner = this;
}

CAR_PATH_VECTOR::~CAR_PATH_VECTOR()
{
    CrtFreePointer(m_begin);
    m_begin = m_end = m_capacityEnd = 0;
    CrtFreePointer(m_owner);
}

CAR_PATH_FRAME& CAR_PATH_VECTOR::At(unsigned int index)
{
    const unsigned int count = static_cast<unsigned int>(m_end-m_begin);
    if (index >= count)
        ::_invalid_parameter_noinfo();
    return m_begin[index];
}

void CAR_PATH_VECTOR::Reserve(size_t capacity)
{
    const size_t maximum = 153391689u;
    if (capacity > maximum) throw std::length_error("vector<T> too long");
    const size_t oldCapacity = m_begin?static_cast<size_t>(m_capacityEnd-m_begin):0u;
    if (oldCapacity >= capacity) return;
    CAR_PATH_FRAME* const newBegin = static_cast<CAR_PATH_FRAME*>(CrtNew(capacity*sizeof(CAR_PATH_FRAME)));
    const size_t size = m_begin?static_cast<size_t>(m_end-m_begin):0u;
    if (size) std::memcpy(newBegin, m_begin, size*sizeof(CAR_PATH_FRAME));
    CrtFreePointer(m_begin);
    m_begin = newBegin;
    m_end = newBegin+size;
    m_capacityEnd = newBegin+capacity;
}

void CAR_PATH_VECTOR::Grow(size_t extra)
{
    const size_t maximum = 153391689u;
    const size_t size = m_begin?static_cast<size_t>(m_end-m_begin):0u;
    if (extra > maximum || maximum-extra < size)
        throw std::length_error("vector<T> too long");
    const size_t required = size+extra;
    const size_t capacity = m_begin?static_cast<size_t>(m_capacityEnd-m_begin):0u;
    if (required <= capacity) return;
    const size_t half = capacity/2u;
    const size_t proposed = maximum-half < capacity?0u:capacity+half;
    Reserve(proposed < required?required:proposed);
}

namespace
{
std::string FormatCarPathText(const char* format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    const unsigned int size = static_cast<unsigned int>(::_vscprintf(format, arguments)) + 1u;
    va_end(arguments);
    char* const text = new char[size];
    va_start(arguments, format);
    const int written = std::vsprintf(text, format, arguments);
    va_end(arguments);
    std::string result;
    if (written >= 0)
        result.assign(text);
    delete[] text;
    return result;
}

float InterpolateCarPathAngle(float first, float second, float portion)
{
    if (second < first)
    {
        const float previousFirst = first;
        first = second;
        second = previousFirst;
        portion = static_cast<float>(1.0 - static_cast<double>(portion));
    }
    if (static_cast<double>(second) - first > 3.1415927410125732)
    {
        do
        {
            first = static_cast<float>(static_cast<double>(first) + 6.2831854820251465);
        }
        while (static_cast<double>(second) - first > 3.1415927410125732);
    }
    return static_cast<float>((static_cast<double>(second) - first) * portion + first);
}

CARTYPE vehicleType = {};
}

int zCarControl::m_iControlCarTact = 0;
int zCarControl::m_iGetVehicleConst = 0;
unsigned char carPointSteeringEnabled = 0;

zCarControl::zCarControl()
    : m_stopState(0), m_reverseMotion(0), m_controlStartTick(0), m_inputGrace(0),
      m_goalNode(0), m_nextGoalNode(0), m_moveToGoalFlag(0), m_bMoveToGoalFlag(0),
      m_reverseSteeringLatch(0), m_previousX(0.0f), m_previousY(0.0f),
      m_carPathFile(0), m_carPathFileMarker(0), m_carPathRecording(0),
      m_carPathPlaying(0), m_carPathFrameIndex(0), m_carPathStartTick(0),
      m_carPathDisableInput(0)
{
    InitCar(&vehicleType);
}


std::string zCarControl::CarPathFileName(int pathNumber) const
{
    const STRING currentMap = ::Map->FileName();
    std::string name(currentMap.CharPtr());
    const size_t separator = name.find_last_of("\\/");
    if (separator != std::string::npos)
        name = name.substr(separator+1, name.size());
    if (::gpLogCons)
    {
        const std::string path = FormatCarPathText("Maps/_carpath_%s_%02d.txt", name.c_str(), pathNumber);
        ::gpLogCons->LogStr(LT_INFO, "%s", path.c_str());
    }
    return FormatCarPathText("Maps/_carpath_%s_%02d.txt", name.c_str(), pathNumber);
}

void zCarControl::RecordCarPath(bool enabled, int pathNumber)
{
    if (m_carPathPlaying) return;
    if (m_carPathRecording)
    {
        if (!enabled)
        {
            if (m_carPathFile) std::fclose(m_carPathFile);
            m_carPathFile = 0;
            m_carPathRecording = 0;
        }
        return;
    }
    if (!enabled) return;

    m_carPathStartTick = ::CurrentTime;
    if (m_carPathFile)
        (void)DebugAssert(4u, "!m_file", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 1375u);
    if (m_carPathFile) std::fclose(m_carPathFile);
    const std::string fileName = CarPathFileName(pathNumber);
    m_carPathFile = std::fopen(fileName.c_str(), "w");
    std::fprintf(m_carPathFile, "%d\n", -101);
    m_carPathRecording = 1u;
}

void zCarControl::PlayCarPath(bool enabled, int pathNumber)
{
    if (m_carPathRecording) return;
    const bool wasPlaying = m_carPathPlaying != 0;
    m_carPathPlaying = enabled?1u:0u;
    if (!enabled) return;

    const std::string fileName = CarPathFileName(pathNumber);
    std::FILE* const input = std::fopen(fileName.c_str(), "rt");
    if (!input)
    {
        m_carPathPlaying = 0;
        return;
    }

    if (wasPlaying)
        return;

    m_carPathFrameIndex = 0;
    m_carPathFileMarker = 100;
    m_carPathStartTick = ::CurrentTime;
    m_carPathRecords.m_end = m_carPathRecords.m_begin;

    int slowingFrames = 0;
    CAR_PATH_FRAME frame;
    while (!std::feof(input))
    {
        (void)std::fscanf(input, "%d", &frame.time);
        if (std::feof(input))
            break;
        if (frame.time < 0)
        {
            if (m_carPathRecords.m_begin != m_carPathRecords.m_end)
                (void)DebugAssert(5u, "m_aCarRec.size() == 0", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 1434u);
            m_carPathFileMarker = BitsToSigned32(0u-static_cast<unsigned int>(frame.time));
            (void)std::fscanf(input, "%d", &frame.time);
        }
        (void)std::fscanf(input, "%f", &frame.x);
        (void)std::fscanf(input, "%f", &frame.y);
        (void)std::fscanf(input, "%f", &frame.z);
        (void)std::fscanf(input, "%f", &frame.heading);
        (void)std::fscanf(input, "%f", &frame.steering);
        frame.movementFlags = 0;
        if (m_carPathFileMarker >= 101)
        {
            (void)std::fscanf(input, "%d", &frame.movementFlags);
        }
        else
        {
            const size_t count = m_carPathRecords.m_begin
                ? static_cast<size_t>(m_carPathRecords.m_end - m_carPathRecords.m_begin) : 0u;
            if (count != 0u)
            {
                const CAR_PATH_FRAME& previous = m_carPathRecords.At(static_cast<unsigned int>(count-1u));
                if (frame.x != previous.x || frame.y != previous.y)
                    frame.movementFlags |= 1;

                if (count >= 2u)
                {
                    const CAR_PATH_FRAME& older = m_carPathRecords.At(static_cast<unsigned int>(count-2u));
                    const double oldX = static_cast<double>(older.x) - previous.x;
                    const double oldY = static_cast<double>(older.y) - previous.y;
                    const float oldSquare = static_cast<float>(oldX * oldX + oldY * oldY);
                    const float oldDistance = static_cast<float>(std::sqrt(static_cast<double>(oldSquare)));
                    const double newX = static_cast<double>(frame.x) - previous.x;
                    const double newY = static_cast<double>(frame.y) - previous.y;
                    const float newSquare = static_cast<float>(newX * newX + newY * newY);
                    const float newDistance = static_cast<float>(std::sqrt(static_cast<double>(newSquare)));
                    const float oldTime = static_cast<float>(
                        static_cast<unsigned int>(previous.time) - static_cast<unsigned int>(older.time));
                    const float newTime = static_cast<float>(
                        static_cast<unsigned int>(frame.time) - static_cast<unsigned int>(previous.time));
                    const float oldSpeed = !FloatEqualOrUnordered(oldTime, 0.0f)
                        ? static_cast<float>(static_cast<double>(oldDistance) / oldTime) : 0.0f;
                    const float newSpeed = !FloatEqualOrUnordered(newTime, 0.0f)
                        ? static_cast<float>(static_cast<double>(newDistance) / newTime) : 0.0f;
                    if (newSpeed > 0.001f && oldSpeed > 0.001f)
                    {
                        if (newSpeed < oldSpeed)
                        {
                            ++slowingFrames;
                            if (slowingFrames >= 4)
                                frame.movementFlags |= 2;
                        }
                        else
                            slowingFrames = 0;
                    }
                }
            }
        }
        m_carPathRecords.Grow(1u);
        *m_carPathRecords.m_end++ = frame;
    }
    std::fclose(input);
}

void zCarControl::StopCarPath()
{
    if (m_carPathRecording && !m_carPathPlaying)
    {
        if (m_carPathFile)
            std::fclose(m_carPathFile);
        m_carPathFile = 0;
        m_carPathRecording = 0;
    }
    if (m_carPathPlaying && !m_carPathRecording)
        m_carPathPlaying = 0;
}

int zCarControl::GetCarPathState() const
{
    if (m_carPathRecording) return 2;
    return m_carPathPlaying ? 1 : 0;
}

void zCarControl::SetInputDisabled(bool disabled)
{
    m_carPathDisableInput = disabled ? 1u : 0u;
}

void zCarControl::RecordCarPathFrame(bool moving)
{
    std::fprintf(m_carPathFile, "%d\n", static_cast<int>(::CurrentTime-m_carPathStartTick));
    std::fprintf(m_carPathFile, "%f\n", m_physicsX);
    std::fprintf(m_carPathFile, "%f\n", m_physicsY);
    std::fprintf(m_carPathFile, "%f\n", m_physicsZ);
    std::fprintf(m_carPathFile, "%f\n", m_heading);
    std::fprintf(m_carPathFile, "%f\n", m_steering);
    std::fprintf(m_carPathFile, "%d\n", (moving ? 1 : 0)+(m_stopState ? 2 : 0));
    std::fprintf(m_carPathFile, "\n");
}

zCarControl::~zCarControl() noexcept(false)
{
    if (m_carPathFile) std::fclose(m_carPathFile);
}


void* zCarControl::QueryInterface(int id) const
{
    if (id == 36) return const_cast<zCarControl*>(this);
    (void)DebugAssert(5u, "0", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 74u);
    return 0;
}


void zCarControl::CheckGoalIsReached()
{
    const float dx = static_cast<float>(fabs(static_cast<double>(static_cast<float>(m_goalX-m_physicsX))));
    const float dy = static_cast<float>(fabs(static_cast<double>(static_cast<float>(m_goalY-m_physicsY))));
    const float squaredDistance = static_cast<float>(static_cast<double>(dx)*dx+static_cast<double>(dy)*dy);
    const float metric = static_cast<float>(static_cast<double>(squaredDistance)*10000.0);

    if (m_bMoveToGoalFlag)
        DebugAssert(4u, "!m_bMoveToGoalFlag", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 915u);
    if (metric<28900.0)
        m_moveToGoalFlag = 0;
}
#include "graph.h"

void zCarControl::ControlVehicle(INPUT* inputState, SPRITE* _pFlagman)
{
    if (m_carPathDisableInput) return;
    if (!_pFlagman) DebugAssert(4u, "_pFlagman", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 1046u);
    if (!inputState) m_moveToGoalFlag = 0;
    if (m_moveToGoalFlag == 0)
    {
        m_bMoveToGoalFlag = 0;
        m_goalNode = 0;
        m_nextGoalNode = 0;
        int index = 0;
        SPRITE* first = FindNextGoal(_pFlagman, _pFlagman, 1200.0f, index);
        m_goalNode = first;
        if (first)
        {
            SPRITE* second = FindNextGoal(_pFlagman, first, 1200.0f, index);
            m_nextGoalNode = second;
            if (second && index<16) (void)FindNextGoal(_pFlagman, first, 1200.0f, index);
            first = m_goalNode;
            if (first)
            {
                MoveToWorldCoord(first->X(), first->Y());
                m_bMoveToGoalFlag = 1;
            }
        }
    }
    if (inputState)
    {
        const bool active = inputState->GetAction(1u) != 0 || inputState->GetAction(2u) != 0 || inputState->GetAction(4u) != 0 || inputState->GetAction(3u) != 0;
        if (active && (_pFlagman->RuntimeFlags()&0x7Cu) == 4u) (void)_pFlagman->SetCommandWithoutLink(0, 0);
    }
    const unsigned int delta = ::CurrentTime-::PrevCurrentTime;
    const float scaled = static_cast<float>(static_cast<double>(delta)/1000.0);
    ProcessInput(inputState, scaled);
}

void zCarControl::ResetScriptFunctions()
{
    m_iControlCarTact = 0;
    m_iGetVehicleConst = 0;
}

void zCarControl::StopCarControl()
{
    m_controlStartTick = 0u;
    StopCarPath();
}

void zCarControl::MoveToWorldCoord(float worldX, float worldY)
{
    m_bMoveToGoalFlag = 0u;
    m_goalX = static_cast<float>(static_cast<double>(worldX)/100.0);
    m_goalY = static_cast<float>(-static_cast<double>(worldY)/100.0);
}



float zCarControl::RadianFromDir(int direction)
{
    return static_cast<float>(static_cast<double>(direction)*3.1415927410125732*0.0078125);
}





void zCarControl::InitCar(CARTYPE* owner)
{
    m_carType = owner;
    m_physicsX = 0.0f;
    m_physicsY = 0.0f;
    m_physicsZ = 0.0f;
    m_heading = 0.0f;
    m_velocityX = 0.0f;
    m_velocityY = 0.0f;
    m_yawVelocity = 0.0f;
    m_steering = 0.0f;
    m_lateralOffset = 0.0f;
    m_forwardInput = 0u;
    m_reverseInput = 0u;
    m_brakeInput = 0u;
}

void zCarControl::GetSpeed(float& x, float& y)
{
    const float scale = (100.0f);
    x = m_velocityX*scale;
    y = m_velocityY*scale;
}




void zCarControl::InitializeCarControl(SPRITE* value)
{
    InitCartypes();
    ResetPhysicsCoordinates(value);
    m_controlStartTick = ::CurrentTime;
    m_inputGrace = 1u;
}


void zCarControl::PhysicsTactEx(float dt, bool active)
{
    PhysicsTact(dt, active);
}
#include "basetype.h"


int zCarControl::iRelease()
{
    delete this;
    return 0;
}





VEC2 zCarControl::GetGoalCoord()
{
    VEC2 result = {0.0f, 0.0f};
    if (m_moveToGoalFlag != 0u || m_bMoveToGoalFlag != 0u)
    {
        result.x = m_goalX*(100.0f);
        result.y = m_goalY*(-100.0f);
    }
    return result;
}

VEC2 zCarControl::GetNextNodeCoord()
{
    VEC2 result = {0.0f, 0.0f};
    if (m_bMoveToGoalFlag != 0u)
    {
        SPRITE* const source = m_nextGoalNode;
        if (source)
        {
            result.x = source->X();
            result.y = source->Y();
        }
    }
    return result;
}


void zCarControl::ResetPhysicsCoordinates(SPRITE* source)
{
    m_physicsX = static_cast<float>(static_cast<double>(source->X())/100.0);
    m_physicsY = static_cast<float>(-static_cast<double>(source->Y())/100.0);
    m_heading = RadianFromDir(static_cast<int>(source->Direction().value));
}

int zCarControl::DirFromRadian(float radians)
{
    volatile float phaseValue = static_cast<float>(static_cast<double>(radians)/3.1415927410125732*128.0);
    phaseValue = static_cast<float>(static_cast<double>(phaseValue)+0.5);
    phaseValue = static_cast<float>(floor(static_cast<double>(phaseValue)));
    int phase = TruncateFloatViaCrtToInt32(phaseValue);
    if (phase >= 256)
        phase = BitsToSigned32(static_cast<unsigned int>(phase)-((static_cast<unsigned int>(phase)>>8u)<<8u));
    if (phase<0)
        phase = BitsToSigned32(static_cast<unsigned int>(phase)+(((255u-static_cast<unsigned int>(phase))>>8u)<<8u));
    return phase;
}

void zCarControl::ControlWheels(SPRITE* actor, int wheel, bool moving, bool reverse)
{
    if (actor->Animation() >= 15)
        return;

    int callback = m_iControlCarTact;
    if (callback == 0u)
    {
        const STRING functionName("ControlWheels_cpp");
        callback = Map->m_script.GetNFunction(functionName);
        m_iControlCarTact = callback;
    }

    if (callback != 0u)
    {
        unsigned int mode = moving?1u:0u;
        if (reverse) mode += 2u;
        g_pFucking = actor;
        Map->ScriptRun(callback,
                       "%d%d",
                       wheel, static_cast<int>(mode), 0);
        g_pFucking = 0;
    }
}


void zCarControl::ControlScreenScrolling(SPRITE* actor)
{
    if (m_brakeInput != 0u)
        return;
    if (m_velocityX == 0.0f && m_velocityY == 0.0f)
        return;

    const float heading = RadianFromDir(static_cast<int>(actor->Direction().value));
    const float angle = static_cast<float>(static_cast<double>(heading)+1.5707963705062866);
    GRAPH* const graph = Graph;
    const float halfHeight = static_cast<float>(static_cast<double>(graph->screenHeight)/5.0);
    const float cosine = static_cast<float>(cos(static_cast<double>(angle)));

    float xOffset;
    float radial;
    if (fabs(static_cast<double>(cosine))<0.001)
    {
        xOffset = 0.0f;
        radial = halfHeight;
    }
    else
    {
        const float halfWidth = static_cast<float>(static_cast<double>(graph->screenWidth)/5.0);
        const float width2 = static_cast<float>(static_cast<double>(halfWidth)*halfWidth);
        const float height2 = static_cast<float>(static_cast<double>(halfHeight)*halfHeight);
        const float tangent = static_cast<float>(tan(static_cast<double>(angle)));
        const float radialSquare = static_cast<float>(
            (static_cast<double>(height2)*width2)/
            (static_cast<double>(tangent)*tangent*width2+height2));
        const float xMagnitude = static_cast<float>(sqrt(static_cast<double>(radialSquare)));
        radial = static_cast<float>(static_cast<double>(xMagnitude)/cosine);
        xOffset = (cosine<0.0f)?-xMagnitude:xMagnitude;
    }

    const float sine = static_cast<float>(sin(static_cast<double>(angle)));
    const float ySquare = static_cast<float>(static_cast<double>(radial)*radial-static_cast<double>(xOffset)*xOffset);
    float yOffset = static_cast<float>(sqrt(static_cast<double>(ySquare)));
    if (!(sine>0.0f))
        yOffset = -yOffset;

    const float centerX = static_cast<float>(xOffset+static_cast<double>(graph->screenWidth)*0.5);
    const float centerY = static_cast<float>(static_cast<double>(yOffset)+static_cast<double>(graph->screenHeight)*(0.5f));
    const float margin = (10.0f);
    const VECTOR2 first(static_cast<float>(centerX-margin), centerY-margin);
    const VECTOR2 second(static_cast<float>(centerX+margin), centerY+margin);
    Map->SetScrollType(8, first, second);
    Map->m_scrollFollowSpeedScale = 2.0f;
}


SPRITE* zCarControl::FindNextGoal(SPRITE* headingOwner, SPRITE* origin, float searchRadius, int& outAngleDiff)
{
    outAngleDiff = 0;
    const bool extended = (headingOwner != origin);
    SPRITE* best = 0;
    float bestDistance = 999999.0f;
    SPRITE* candidate = 0;
    const VECTOR2& originXY = const_cast<VECTOR&>(origin->Coor()).XY();

    for (;;)
    {
        SPRITE_COLLECTOR* const collector = ::Sprites;
        candidate = collector->FindNearestSprite(1366, originXY, searchRadius, candidate);
        if (candidate == 0)
            break;
        if (candidate == origin)
            continue;

        const VECTOR2 delta(candidate->X()-origin->X(),
                            candidate->Y()-origin->Y());
        const float distance = delta.Length();
        if (!(distance>(260.0f)) && !extended)
            continue;

        const ANGLE direction(delta);
        unsigned char a = static_cast<unsigned char>(headingOwner->Direction().value-direction.value);
        unsigned char b = static_cast<unsigned char>(direction.value-headingOwner->Direction().value);
        const unsigned int diff = (a<b)?a:b;

        bool directionAccepted = false;
        if (diff<20u && distance>(300.0f))
            directionAccepted = true;
        else if (diff<40u)
            directionAccepted = true;
        else if (extended && diff <= 74u)
            directionAccepted = true;

        if (!directionAccepted || !(distance<bestDistance))
            continue;

        best = candidate;
        bestDistance = distance;
        outAngleDiff = static_cast<int>(diff);
    }
    return best;
}

void zCarControl::InitCartypes()
{
    int parameterReader = m_iGetVehicleConst;
    if (parameterReader == 0u)
    {
        {
            const STRING readerName("GetVehicleConst");
            parameterReader = Map->m_script.GetNFunction(readerName);
            m_iGetVehicleConst = parameterReader;
        }
        if (m_iGetVehicleConst == 0u)
            (void)DebugAssert(4u, "m_iGetVehicleConst", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 149u);
    }

    CARTYPE& carType = vehicleType;
    const char* readerEntry = "%d%d";
    int value;

    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 8, 0);
    carType.m_maxForward = static_cast<float>(static_cast<double>(value)/100.0);

    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 6, 0);
    carType.m_mass = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 7, 0);
    carType.m_inertia = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 2, 0);
    carType.m_rearStiffness = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 3, 0);
    carType.m_frontStiffness = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 4, 0);
    carType.m_tyreLimit = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 0, 0);
    carType.m_drag = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 1, 0);
    carType.m_rollingResistance = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 5, 0);
    carType.m_engineForce = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 9, 0);
    carType.m_frontArm = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 10, 0);
    carType.m_rearArm = (float)value;
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 11, 0);
    carType.m_maxWheel = static_cast<float>(value);
    carType.m_maxWheel = static_cast<float>(static_cast<double>(carType.m_maxWheel)*3.1415927410125732/180.0);
    value = Map->ScriptRun(m_iGetVehicleConst, readerEntry, 1206, 12, 0);
    carType.m_steeringRate = static_cast<float>(value);
    carType.m_steeringRate = static_cast<float>(static_cast<double>(carType.m_steeringRate)*3.1415927410125732/180.0);
    carType.m_dynamicWheelLimit = -1.0f;

    carType.m_rearStiffness = static_cast<float>(static_cast<double>(carType.m_rearStiffness)/1000.0);
    carType.m_frontStiffness = static_cast<float>(static_cast<double>(carType.m_frontStiffness)/1000.0);
    carType.m_tyreLimit = static_cast<float>(static_cast<double>(carType.m_tyreLimit)/1000.0);
    carType.m_drag = static_cast<float>(static_cast<double>(carType.m_drag)/1000.0);
    carType.m_rollingResistance = static_cast<float>(static_cast<double>(carType.m_rollingResistance)/1000.0);
    carType.m_frontArm = static_cast<float>(static_cast<double>(carType.m_frontArm)/1000.0);
    carType.m_rearArm = static_cast<float>(static_cast<double>(carType.m_rearArm)/1000.0);
    carType.m_wheelbase = static_cast<float>(static_cast<double>(carType.m_rearArm)+carType.m_frontArm);
}

void zCarControl::MoveVehicle(SPRITE* _pFlagman, bool scrollScreen)
{
    if (m_controlStartTick == 0u)
        return;
    if (!_pFlagman)
        DebugAssert(5u, "_pFlagman", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 1165u);


    ResetPhysicsCoordinates(_pFlagman);
    const float savedX = m_physicsX;
    const float savedY = m_physicsY;
    const float savedZ = m_physicsZ;
    const float savedAngle = m_heading;

    int playbackFlags = 0;
    if (m_carPathPlaying)
    {
        const size_t count = m_carPathRecords.m_begin
            ? static_cast<size_t>(m_carPathRecords.m_end - m_carPathRecords.m_begin) : 0u;
        const unsigned int elapsed = ::CurrentTime - m_carPathStartTick;
        while (m_carPathFrameIndex < count &&
            elapsed > static_cast<unsigned int>(m_carPathRecords.At(m_carPathFrameIndex).time))
            ++m_carPathFrameIndex;

        if (m_carPathFrameIndex >= count)
        {
            const CAR_PATH_FRAME& frame = m_carPathRecords.At(static_cast<unsigned int>(count-1u));
            m_physicsX = frame.x;
            m_physicsY = frame.y;
            m_physicsZ = frame.z;
            m_heading = frame.heading;
            m_steering = frame.steering;
            if (!m_carPathRecording) m_carPathPlaying = 0;
        }
        else
        {
            const CAR_PATH_FRAME& second = m_carPathRecords.At(m_carPathFrameIndex);
            const CAR_PATH_FRAME& first = m_carPathRecords.At(
                m_carPathFrameIndex > 0u ? m_carPathFrameIndex - 1u : m_carPathFrameIndex);
            float portion = 0.0f;
            if (m_carPathFrameIndex > 0u)
            {
                const float duration = static_cast<float>(
                    static_cast<unsigned int>(second.time) - static_cast<unsigned int>(first.time));
                if (duration > 0.0f)
                    portion = static_cast<float>(static_cast<double>(
                        elapsed - static_cast<unsigned int>(first.time)) / duration);
            }
            m_physicsX = static_cast<float>((static_cast<double>(second.x) - first.x) * portion + first.x);
            m_physicsY = static_cast<float>((static_cast<double>(second.y) - first.y) * portion + first.y);
            m_physicsZ = static_cast<float>((static_cast<double>(second.z) - first.z) * portion + first.z);
            m_heading = InterpolateCarPathAngle(first.heading, second.heading, portion);
            m_steering = InterpolateCarPathAngle(first.steering, second.steering, portion);
            playbackFlags = second.movementFlags;
        }
    }
    else
    {
        float dt = static_cast<float>(static_cast<double>(::CurrentTime-::PrevCurrentTime)/1000.0);
        const float zero = (0.0f);
        if (dt>zero)
        {
            const float fixedStep = (0.000709999993f);
            while (dt >= fixedStep)
            {
                PhysicsTactEx(fixedStep, true);
                dt -= fixedStep;
                if (!(dt>zero))
                    break;
            }
            if (dt>zero)
            {
                if (static_cast<double>(dt)<0.00014199999859556555)
                    dt = (0.000142000004f);
                PhysicsTactEx(dt, true);
            }
        }

    }

    const float zero = (0.0f);
    VECTOR requested(m_physicsX*(100.0f), m_physicsY*(-100.0f), 0.0f);

    MAP* const world = Map;
    const bool outsideWorld = requested.x<zero || requested.y<zero ||
        requested.x >= world->m_sizeXY.x || requested.y >= world->m_sizeXY.y;


    const unsigned int direction = (unsigned int)DirFromRadian(m_heading);
    (void)_pFlagman->ChangeDirection(ANGLE(static_cast<unsigned char>(direction&0xFFu)));

    const float vx = m_localLongitudinal;
    const float vy = m_localLateral;
    const float reverseScale = m_reverseMotion?(-1.0f):(1.0f);
    const float squaredSpeed = static_cast<float>(static_cast<double>(vx)*vx+static_cast<double>(vy)*vy);
    const float speed = static_cast<float>(sqrt(static_cast<double>(squaredSpeed)));
    reinterpret_cast<SPRITE_LAYOUT_ACCESS*>(_pFlagman)->m_speed = static_cast<float>(static_cast<double>(speed)*(reverseScale*2.0)/1000.0);

    bool rejected = outsideWorld;
    if (!rejected)
    {

        rejected = (m_carPathPlaying ? _pFlagman->CanPlaceWithCrush(requested)
            : _pFlagman->CanPlaceWithCrushAndGlide(&requested)) != 0;
        if (!rejected && !(requested.z>20.0f))
        {
            (void)_pFlagman->ChangeCoor(requested);
            const unsigned int correctedDir = (unsigned int)DirFromRadian(m_heading);
            (void)_pFlagman->ChangeDirection(ANGLE(static_cast<unsigned char>(correctedDir&0xFFu)));

            m_physicsX = static_cast<float>(static_cast<double>(requested.x)/100.0);
            m_physicsY = static_cast<float>(-static_cast<double>(requested.y)/100.0);
            m_physicsZ = requested.z;
            if (m_moveToGoalFlag)
                CheckGoalIsReached();
        }
        else
            rejected = true;
    }

    if (rejected)
    {
        m_physicsX = savedX;
        m_physicsY = savedY;
        m_physicsZ = savedZ;
        m_heading = savedAngle;
        m_velocityX = 0.0f;
        m_velocityY = 0.0f;
        m_yawVelocity = 0.0f;


        if (g_iZmDebug != 0u)
        {
            VECTOR debugPoint(savedX*(100.0f), savedY*(-100.0f), savedZ);
            const unsigned int buf = _pFlagman->CanPlaceWithCrushAndGlide(&debugPoint) != 0;
            if (buf)
                DebugAssert(4u, "buf == NULL", "..\\src\\zm\\Game\\Vehicles\\zCarControl.cpp", 1266u);
        }
        m_moveToGoalFlag = 0u;
    }

    int wheel = TruncateDoubleViaCrtToInt32(static_cast<double>(m_steering)*100.0/0.7853981852531433);
    if (wheel>100)wheel = 100;
    else if (wheel<-100)wheel = -100;

    const bool moving = m_carPathPlaying ? playbackFlags % 2 != 0
        : (m_brakeInput == 0u && (m_velocityX != zero || m_velocityY != zero));
    if (m_carPathPlaying) m_stopState = playbackFlags / 2 != 0 ? 1u : 0u;
    ControlWheels(_pFlagman, wheel, moving, m_stopState != 0u);
    if (scrollScreen)
        ControlScreenScrolling(_pFlagman);
    if (m_carPathRecording) RecordCarPathFrame(moving);
}

void zCarControl::ProcessInput(INPUT* input, float dt)
{
    unsigned int buttons = 0u;

    if (input)
    {


        if (m_inputGrace != 0u)
        {
            const bool directionalHeld = input->GetAction(1u) != 0 || input->GetAction(2u) != 0 ||
                                       input->GetAction(3u) != 0 || input->GetAction(4u) != 0;
            if (directionalHeld && (::CurrentTime-m_controlStartTick) <= 500u)
                return;
            m_inputGrace = 0u;
        }

        if (input->GetAction(6u) != 0)
        {
            MoveToWorldCoord(input->MouseXY().x, input->MouseXY().y);
            m_moveToGoalFlag = 1u;
        }

        if (input->GetAction(1u) != 0) buttons |= 0x01u;
        if (input->GetAction(2u) != 0) buttons |= 0x02u;
        if (input->GetAction(3u) != 0) buttons |= 0x04u;
        if (input->GetAction(4u) != 0) buttons |= 0x08u;

        if (carPointSteeringEnabled != 0)
        {
            float targetX = m_physicsX*100.0f;
            float targetY = m_physicsY*-100.0f;
            const float oldX = targetX;
            const float oldY = targetY;
            buttons = 0u;
            if (input->GetAction(3u) != 0) targetX -= 200.0f;
            else if (input->GetAction(4u) != 0) targetX += 200.0f;
            if (input->GetAction(1u) != 0) targetY -= 200.0f;
            else if (input->GetAction(2u) != 0) targetY += 200.0f;
            if (targetX != oldX || targetY != oldY)
            {
                MoveToWorldCoord(targetX, targetY);
                m_moveToGoalFlag = 1u;
            }
        }

        if (m_moveToGoalFlag != 0u)
        {
            if (buttons != 0u) m_moveToGoalFlag = 0u;
            else buttons = 0u;
        }
    }

    CARTYPE* carType = m_carType;
    carType->m_dynamicWheelLimit = -1.0f;

    if (m_moveToGoalFlag != 0u || (m_bMoveToGoalFlag != 0u && buttons == 1u))
    {
        const float dy = m_goalY-m_physicsY;
        const float dx = m_goalX-m_physicsX;
        const float goalAngle = static_cast<float>(atan2(static_cast<double>(dy), static_cast<double>(dx)));
        float angle = static_cast<float>((1.5707963705062866-static_cast<double>(m_heading))-goalAngle);
        float cosAngle = static_cast<float>(cos(static_cast<double>(angle)));
        const float cos7 = static_cast<float>(cos(0.12217304855585098));
        const float distance2 = static_cast<float>(static_cast<double>(dx)*dx+static_cast<double>(dy)*dy);
        const float cosReverseLimit = static_cast<float>(cos(2.0));

        if (cosReverseLimit >= cosAngle && distance2<400.0f && (buttons&1u) == 0u)
            buttons |= 2u;
        else
            buttons |= 1u;

        if ((buttons&2u) != 0u)
        {
            angle = static_cast<float>(static_cast<double>(angle)+3.1415927410125732);
            cosAngle = static_cast<float>(cos(static_cast<double>(angle)));
        }

        bool applySteering = false;

        if (cos7<cosAngle && static_cast<float>(cos(0.03490658476948738)) >= cosAngle)
        {
            carType->m_dynamicWheelLimit = 0.00261799409054219722747802734375f;
            applySteering = true;
        }
        else if (cos7>cosAngle)
        {
            bool allowSteer = true;
            if (m_yawVelocity != 0.0f && static_cast<float>(cos(0.19198621809482574))<cosAngle)
            {
                const float predicted = static_cast<float>(static_cast<double>(angle)-static_cast<double>(m_yawVelocity)*0.3499999940395355);
                if (static_cast<float>(cos(static_cast<double>(predicted)))<cos7)
                    allowSteer = false;
            }
            if (m_bMoveToGoalFlag != 0u && m_localLongitudinal>200.0f &&
                static_cast<float>(static_cast<double>(distance2)*10000.0)<2500.0f)
                allowSteer = false;
            applySteering = allowSteer;
        }

        if (applySteering)
        {
            const float steerSign = static_cast<float>(sin(static_cast<double>(angle)));
            if ((buttons&1u) != 0u)
                buttons |= (steerSign<0.0f)?0x04u:0x08u;
            else if ((buttons&2u) != 0u)
                buttons |= (steerSign >= 0.0f)?0x04u:0x08u;
        }
    }

    if ((buttons&0x01u) != 0u) m_forwardInput = 100u;
    else m_forwardInput = 0u;
    if ((buttons&0x02u) != 0u)
    {
        m_reverseInput = 100u;
        m_forwardInput = 0u;
    }
    else m_reverseInput = 0u;

    if ((buttons&0x20u) != 0u) m_lateralOffset = -0.1f;
    else if ((buttons&0x40u) != 0u) m_lateralOffset = 0.1f;
    else m_lateralOffset = 0.0f;

    double wheel = m_steering;
    const float steeringRate = carType->m_steeringRate;
    if ((buttons&0x04u) != 0u) wheel -= static_cast<double>(dt)*steeringRate;
    else if ((buttons&0x08u) != 0u) wheel += static_cast<double>(dt)*steeringRate;
    else
    {
        float decay = steeringRate;
        if (m_localLongitudinal>200.0f) decay = static_cast<float>(static_cast<double>(decay)*5.0);
        const double step = static_cast<double>(dt)*decay;
        if (wheel>0.0f)
        {
            wheel = static_cast<float>(wheel-step);
            if (wheel<0.0f) wheel = 0.0f;
        }
        else if (wheel<0.0f)
        {
            wheel = static_cast<float>(wheel+step);
            if (wheel>0.0f) wheel = 0.0f;
        }
    }

    wheel = static_cast<float>(wheel);
    const float maxWheel = carType->m_maxWheel;
    if (wheel < -maxWheel) wheel = -maxWheel;
    if (wheel >  maxWheel) wheel = maxWheel;
    const float dynamicLimit = carType->m_dynamicWheelLimit;
    if (dynamicLimit >= 0.0f)
    {
        if (wheel < -dynamicLimit) wheel = -dynamicLimit;
        if (wheel >  dynamicLimit) wheel = dynamicLimit;
    }
    m_steering = static_cast<float>(wheel);
}

void zCarControl::PhysicsTact(float dt, bool active)
{
    (void)active;

    
    const float zero = (0.0f);
    const float epsilon = (9.99999997e-07f);


    m_localLongitudinal = zero;
    m_localLateral = zero;

    if (m_velocityX == zero && m_velocityY == zero &&
       m_forwardInput == 0u && m_reverseInput == 0u)
    {
        m_yawVelocity = zero;
        m_stopState = 0u;
        m_reverseMotion = 0u;
        return;
    }

    CARTYPE* const carType = m_carType;
    const float heading = m_heading;
    const float sinHeading = (float)sin((double)heading);
    const float cosHeading = static_cast<float>(cos(static_cast<double>(heading)));


    float longitudinal = static_cast<float>(static_cast<double>(cosHeading)*m_velocityY+static_cast<double>(sinHeading)*m_velocityX);
    float lateral = static_cast<float>(static_cast<double>(cosHeading)*m_velocityX-static_cast<double>(sinHeading)*m_velocityY);
    if (fabs((double)longitudinal)<(double)epsilon) longitudinal = zero;
    if (fabs((double)lateral)<(double)epsilon) lateral = zero;

    float halfYawVelocity = static_cast<float>(static_cast<double>(carType->m_wheelbase)*0.5*m_yawVelocity);
    if (fabs((double)halfYawVelocity)<(double)epsilon) halfYawVelocity = zero;

    float yawSlip = zero;
    if (longitudinal != zero)
        yawSlip = (float)atan2((double)halfYawVelocity, fabs((double)longitudinal));
    if (fabs((double)yawSlip)<(double)epsilon) yawSlip = zero;

    float bodySlip = zero;
    if (longitudinal != zero)
        bodySlip = static_cast<float>(atan2(static_cast<double>(lateral), fabs(static_cast<double>(longitudinal))));
    if (fabs(static_cast<double>(bodySlip))<static_cast<double>(epsilon)) bodySlip = zero;

    const unsigned int reverseInput = m_reverseInput;
    const unsigned int forwardInput = m_forwardInput;
    const bool inputPresent = (reverseInput != 0u || forwardInput != 0u);
    const bool movingBackwardUnderReverseInput = (longitudinal<zero && reverseInput != 0u);

    m_reverseMotion = movingBackwardUnderReverseInput?1u:0u;
    m_stopState = ((longitudinal>zero && reverseInput != 0u) || !inputPresent)?1u:0u;

    if (longitudinal>zero)
        m_reverseSteeringLatch = 0u;
    if (movingBackwardUnderReverseInput)
        m_reverseSteeringLatch = 1u;
    const bool reverseSteering = (movingBackwardUnderReverseInput || m_reverseSteeringLatch != 0u);

    const float steering = m_steering;
    float frontSlip;
    if (reverseSteering)
        frontSlip = static_cast<float>(static_cast<double>(bodySlip)+yawSlip+steering);
    else
        frontSlip = static_cast<float>(static_cast<double>(bodySlip)+yawSlip-steering);
    float rearSlip = bodySlip-yawSlip;

    if (fabs((double)frontSlip)<(double)epsilon) frontSlip = zero;
    if (fabs((double)rearSlip)<(double)epsilon) rearSlip = zero;

    const float normalHalf = static_cast<float>(static_cast<double>(carType->m_mass)*9.800000190734863*0.5);
    const float tyreLimit = carType->m_tyreLimit;

    float frontForce = static_cast<float>(static_cast<double>(carType->m_frontStiffness)*frontSlip);
    if (frontForce>tyreLimit) frontForce = tyreLimit;
    if (frontForce<-tyreLimit) frontForce = -tyreLimit;
    frontForce = static_cast<float>(static_cast<double>(frontForce)*normalHalf);
    if (fabs((double)frontForce)<(double)epsilon) frontForce = zero;

    double initialRearForce = static_cast<double>(carType->m_rearStiffness)*rearSlip;
    if (m_brakeInput != 0u)
        initialRearForce *= 1.5;
    float rearForce = static_cast<float>(initialRearForce);
    if (rearForce>tyreLimit) rearForce = tyreLimit;
    if (rearForce<-tyreLimit) rearForce = -tyreLimit;
    rearForce = static_cast<float>(static_cast<double>(rearForce)*normalHalf);
    if (fabs((double)rearForce)<(double)epsilon) rearForce = zero;

    const float dragCoefficient = carType->m_drag;
    float rollingResistance = carType->m_rollingResistance;
    if (movingBackwardUnderReverseInput)
        rollingResistance = static_cast<float>(static_cast<double>(rollingResistance)*5.0);

    float longitudinalForce = static_cast<float>(-(static_cast<double>(rollingResistance)*longitudinal+
        static_cast<double>(dragCoefficient)*longitudinal*fabs(static_cast<double>(longitudinal))));
    const float lateralDrag = static_cast<float>(-(static_cast<double>(rollingResistance)*lateral+
        static_cast<double>(dragCoefficient)*lateral*fabs(static_cast<double>(lateral))));

    const float mass = carType->m_mass;
    if (m_brakeInput != 0u)
    {
        if (longitudinal>zero)
            longitudinalForce = static_cast<float>(static_cast<double>(longitudinalForce)-static_cast<double>(mass)*5.0);
        else if (longitudinal<zero)
            longitudinalForce = static_cast<float>(static_cast<double>(mass)*5.0+longitudinalForce);
    }
    else if (reverseInput != 0u)
    {
        if (longitudinal>zero)
            longitudinalForce = static_cast<float>(static_cast<double>(longitudinalForce)-static_cast<double>(mass)*10.0);
        else
            longitudinalForce = static_cast<float>(static_cast<double>(longitudinalForce)-static_cast<double>(mass)*5.0);
    }
    else if (!inputPresent)
    {
        if (longitudinal>zero)
            longitudinalForce = static_cast<float>(static_cast<double>(longitudinalForce)-static_cast<double>(mass)*5.0);
        else if (longitudinal<zero)
            longitudinalForce = static_cast<float>(static_cast<double>(mass)*5.0+longitudinalForce);
    }

    const float engineForce = m_brakeInput == 0u
        ? static_cast<float>(static_cast<double>(static_cast<int>(forwardInput))*carType->m_engineForce)
        : zero;
    const float totalLongitudinalForce = static_cast<float>(static_cast<double>(engineForce)+longitudinalForce);
    const float cosSteering = static_cast<float>(cos(static_cast<double>(steering)));
    const float lateralForce = static_cast<float>(static_cast<double>(cosSteering)*frontForce+rearForce+lateralDrag);
    float yawTorque = static_cast<float>(static_cast<double>(frontForce)*carType->m_frontArm-
        static_cast<double>(rearForce)*carType->m_rearArm);
    if (fabs(static_cast<double>(yawTorque))<static_cast<double>(epsilon)) yawTorque = zero;

    float longitudinalAcceleration = static_cast<float>(static_cast<double>(totalLongitudinalForce)/mass);
    float lateralAcceleration = static_cast<float>(static_cast<double>(lateralForce)/mass);
    if (fabs((double)longitudinalAcceleration)<(double)epsilon) longitudinalAcceleration = zero;
    if (fabs((double)lateralAcceleration)<(double)epsilon) lateralAcceleration = zero;

    float angularAcceleration = static_cast<float>(static_cast<double>(yawTorque)/carType->m_inertia);
    if (fabs(static_cast<double>(angularAcceleration))<static_cast<double>(epsilon)) angularAcceleration = zero;

    longitudinal = static_cast<float>(static_cast<double>(longitudinalAcceleration)*dt+longitudinal);
    lateral = static_cast<float>(static_cast<double>(lateral)+static_cast<double>(lateralAcceleration)*dt);
    m_yawVelocity = static_cast<float>(static_cast<double>(angularAcceleration)*dt+m_yawVelocity);

    const float maxForward = carType->m_maxForward;
    float speedScale = 1.0f;
    if (longitudinal>maxForward)
        speedScale = static_cast<float>(static_cast<double>(longitudinal)/maxForward);
    else
    {
        const double maxReverse = -static_cast<double>(maxForward)/3.0;
        if (longitudinal<maxReverse)
            speedScale = static_cast<float>(static_cast<double>(longitudinal)/maxReverse);
    }
    if (speedScale != 1.0f)
    {
        longitudinal = static_cast<float>(static_cast<double>(longitudinal)/speedScale);
        lateral = static_cast<float>(static_cast<double>(lateral)/speedScale);
        m_yawVelocity = static_cast<float>(static_cast<double>(m_yawVelocity)/speedScale);
    }

    const bool oldXNonNegative = (m_velocityX >= zero);
    const bool oldYNonNegative = (m_velocityY >= zero);

    const double longitudinalWorldX = static_cast<double>(longitudinal)*sinHeading;
    const double longitudinalWorldY = static_cast<double>(longitudinal)*cosHeading;
    const float worldX = static_cast<float>(longitudinalWorldX+static_cast<double>(lateral)*cosHeading);
    const float worldY = static_cast<float>(longitudinalWorldY-static_cast<double>(lateral)*sinHeading);
    const bool newXNonNegative = (worldX >= zero);
    const bool newYNonNegative = (worldY >= zero);

    m_velocityX = static_cast<float>(worldX);
    m_velocityY = static_cast<float>(worldY);
    if (fabs((double)worldX)<(double)epsilon) m_velocityX = zero;
    if (fabs(static_cast<double>(m_velocityY))<static_cast<double>(epsilon)) m_velocityY = zero;
    if (fabs((double)m_yawVelocity)<(double)epsilon) m_yawVelocity = zero;

    m_heading = static_cast<float>(static_cast<double>(m_heading)+static_cast<double>(dt)*m_yawVelocity);

    bool stopMotion = false;
    if (!inputPresent && (oldXNonNegative != oldYNonNegative || newXNonNegative != newYNonNegative))
    {
        if (fabs((double)m_velocityX)<(double)(0.000699999975f) &&
           fabs((double)m_velocityY)<(double)(0.000699999975f) &&
           fabs((double)m_yawVelocity)<(double)(0.00150000001f))
            stopMotion = true;
    }

    const float displacedLateral = static_cast<float>(static_cast<double>(m_lateralOffset)+lateral);
    const float displacementX = static_cast<float>(static_cast<double>(displacedLateral)*cosHeading+longitudinalWorldX);
    const float displacementY = static_cast<float>(longitudinalWorldY-static_cast<double>(displacedLateral)*sinHeading);
    const float nextX = static_cast<float>(static_cast<double>(displacementX)*dt+m_physicsX);
    const float nextY = static_cast<float>(static_cast<double>(displacementY)*dt+m_physicsY);
    m_physicsX = nextX;
    m_physicsY = static_cast<float>(nextY);

    if (!inputPresent)
    {
        const float previousX = m_previousX;
        const float previousY = m_previousY;
        if (previousX != zero || previousY != zero)
        {
            if (fabs(static_cast<double>(static_cast<float>(previousX-nextX)))<(double)(9.99999975e-06f) &&
               fabs(static_cast<double>(static_cast<float>(previousY-nextY)))<(double)(9.99999975e-06f) &&
               fabs((double)m_yawVelocity)<(double)(0.00150000001f))
                stopMotion = true;
        }
        m_previousX = nextX;
        m_previousY = static_cast<float>(nextY);
    }
    else
    {
        m_previousX = zero;
        m_previousY = zero;
    }

    if (stopMotion)
    {
        m_velocityX = zero;
        m_velocityY = zero;
    }

    if (m_velocityX != zero || m_velocityY != zero)
    {
        m_localLongitudinal = static_cast<float>(longitudinal*100.0f);
        m_localLateral = static_cast<float>(static_cast<double>(lateral)*100.0f);
    }
}



