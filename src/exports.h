#ifndef ZOMBIESHOOTER2_EXPORTS_H
#define ZOMBIESHOOTER2_EXPORTS_H

#include "basetype.h"
#include <cstdio>

extern char* crtProgramPath;

void __cdecl SFINIT0_MonsterVidAllocate(void);
void __cdecl SFINIT1_Proc(void);
void __cdecl SFINIT2_Proc(void);
int __cdecl SFLB_GetHeroId(int value);


#endif
