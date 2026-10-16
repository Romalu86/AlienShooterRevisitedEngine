#pragma once
#include <stddef.h>
#include "mylib.h"

enum SAVE_TYPE : int;

class zProfLdr {
public:
    enum { MAX_PROF_NUM = 10, MAX_LEVEL = 300 };

    STRING m_aProfs[MAX_PROF_NUM];
    unsigned char m_bLoaded;
    unsigned char m_profileChanged[MAX_PROF_NUM];
    unsigned char m_037;
    int m_currentProfile;
    int m_questLevNums[MAX_LEVEL];

    zProfLdr();
    virtual void* QueryInterface(int interfaceId) const;
    virtual int iRelease();
    virtual ~zProfLdr() noexcept(false);
    static void operator delete(void* memory);
    bool GetLoadState();

    bool ExpFileToReg();
    bool ExpRegToFile();
    bool DeleteProfile2(int _iProfileNum);
    STRING GetCurSaveName2(SAVE_TYPE saveType, bool survival);
    STRING GetMaxLevSaveName(SAVE_TYPE _eSType);
    int GetCurLevNum(bool survival);
    void SetMaxLevNum(bool survival);
    void ResetMaxLevelForNewGame();
    int GetMaxLevNum(int _iProfNum, bool survival);
    void SetCurProf(int _iCurProf);
    STRING GetProfName(int profile) const;
    void SetProfName(int profile, STRING name);
    void SetQuestLevNum(int level, int questLevel);
    int GetQuestLevNum(int level) const;

private:
    void ClearObjPrf();
    bool ExpFileToObj();
    bool ExpObjToFile();
    STRING GetPlayerNameFName(int profile);
    STRING GetOptFName(int profile);
    bool DeleteProfileEx(int _iProfileNum);
    STRING GetSaveName2(int profile, SAVE_TYPE _eSType, int _iLevNum);
    STRING GetProfFolderName(int _iProfileNum);
    void CreateFolderIfNeed(int _iProfileNum);
};

