#pragma once
#include "mylib.h"
#include <stddef.h>
#include <cstdio>
#include <string>
#include "zm/Utils/zContainerProxy.h"
class SPRITE;
class INPUT;
struct CARTYPE {
    float m_wheelbase;
    float m_frontArm;
    float m_rearArm;
    float m_mass;
    float m_inertia;
    float m_maxForward;
    float m_rearStiffness;
    float m_frontStiffness;
    float m_tyreLimit;
    float m_drag;
    float m_rollingResistance;
    float m_engineForce;
    float m_maxWheel;
    float m_dynamicWheelLimit;
    float m_steeringRate;
};

struct VEC2 { float x; float y; };

struct CAR_PATH_FRAME
{
    int time;
    float x;
    float y;
    float z;
    float heading;
    float steering;
    int movementFlags;
};


struct CAR_PATH_VECTOR
{
    CAR_PATH_VECTOR();
    ~CAR_PATH_VECTOR();
    void Reserve(size_t capacity);
    void Grow(size_t extra);
    CAR_PATH_FRAME& At(unsigned int index);

    zContainerProxy<CAR_PATH_VECTOR>* m_owner;
    CAR_PATH_FRAME* m_begin;
    CAR_PATH_FRAME* m_end;
    CAR_PATH_FRAME* m_capacityEnd;
};

class zCarControl {
public:
    zCarControl();
    virtual void* QueryInterface(int id) const;
    virtual int iRelease();
    virtual ~zCarControl() noexcept(false);

private:
    static int m_iControlCarTact;
    static int m_iGetVehicleConst;

    CARTYPE* m_carType;
    float m_physicsX;
    float m_physicsY;
    float m_physicsZ;
    float m_heading;
    float m_velocityX;
    float m_velocityY;
    float m_yawVelocity;
    float m_localLongitudinal;
    float m_localLateral;
    float m_steering;
    float m_lateralOffset;
    unsigned int m_forwardInput;
    unsigned int m_reverseInput;
    unsigned int m_brakeInput;
    unsigned char m_stopState;
    unsigned char m_reverseMotion;
    unsigned int m_controlStartTick;
    unsigned char m_inputGrace;
    float m_goalX;
    float m_goalY;
    SPRITE* m_goalNode;
    SPRITE* m_nextGoalNode;
    unsigned char m_moveToGoalFlag;
    unsigned char m_bMoveToGoalFlag;
    unsigned char m_reverseSteeringLatch;
    float m_previousX;
    float m_previousY;
    std::FILE* m_carPathFile;
    int m_carPathFileMarker;
    unsigned char m_carPathRecording;
    unsigned char m_carPathPlaying;
    unsigned int m_carPathFrameIndex;
    unsigned int m_carPathStartTick;
    CAR_PATH_VECTOR m_carPathRecords;
    unsigned char m_carPathDisableInput;

public:
    static void ResetScriptFunctions();
    void InitializeCarControl(SPRITE* sprite);
    void StopCarControl();
    void MoveToWorldCoord(float x,float y);
    VEC2 GetGoalCoord();
    VEC2 GetNextNodeCoord();
    void GetSpeed(float& x,float& y);
    void ControlVehicle(INPUT* input,SPRITE* _pFlagman);
    void MoveVehicle(SPRITE* _pFlagman,bool scrollScreen);
    void RecordCarPath(bool enabled, int pathNumber);
    void PlayCarPath(bool enabled, int pathNumber);
    void StopCarPath();
    int GetCarPathState() const;
    void SetInputDisabled(bool disabled);
    static void ControlWheels(SPRITE* sprite,int wheel,bool moving,bool reverse);

private:
    void InitCartypes();
    void InitCar(CARTYPE* type);
    void PhysicsTact(float dt,bool active);
    void ProcessInput(INPUT* input,float dt);
    void ResetPhysicsCoordinates(SPRITE* sprite);
    float RadianFromDir(int direction);
    int DirFromRadian(float radians);
    void CheckGoalIsReached();
    void ControlScreenScrolling(SPRITE* sprite);
    SPRITE* FindNextGoal(SPRITE* headingOwner,SPRITE* origin,float radius,int& angleDiff);
    void PhysicsTactEx(float dt,bool active);
    std::string CarPathFileName(int pathNumber) const;
    void RecordCarPathFrame(bool moving);
};
#include "basetype.h"






