// mmsystem.h of WinCC: sounds and the millisecond clock of winmm.
#pragma once
#include <windows.h>
#pragma comment(lib, "winmm")

#define SND_SYNC 0x0000
#define SND_ASYNC 0x0001
#define SND_NODEFAULT 0x0002
#define SND_MEMORY 0x0004
#define SND_LOOP 0x0008
#define SND_NOSTOP 0x0010
#define SND_PURGE 0x0040
#define SND_ALIAS 0x00010000L
#define SND_FILENAME 0x00020000L
#define SND_RESOURCE 0x00040004L
#define TIMERR_NOERROR 0

typedef UINT MMRESULT;

BOOL PlaySoundA(LPCSTR sound, HMODULE mod, DWORD flags);
BOOL PlaySoundW(LPCWSTR sound, HMODULE mod, DWORD flags);
BOOL sndPlaySoundA(LPCSTR sound, UINT flags);
BOOL sndPlaySoundW(LPCWSTR sound, UINT flags);
DWORD timeGetTime(void);
MMRESULT timeBeginPeriod(UINT ms);
MMRESULT timeEndPeriod(UINT ms);

#define PlaySound __AW(PlaySound)
#define sndPlaySound __AW(sndPlaySound)
