// Copyright 2010-2011, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_WIN32_BASE_IME_IMMDEV_H_
#define MOZC_WIN32_BASE_IME_IMMDEV_H_

#include <imm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _IMM_DDK_DEFINED_
#define _IMM_DDK_DEFINED_

typedef struct tagCOMPOSITIONSTRING {
  DWORD dwSize;
  DWORD dwCompReadAttrLen;
  DWORD dwCompReadAttrOffset;
  DWORD dwCompReadClauseLen;
  DWORD dwCompReadClauseOffset;
  DWORD dwCompReadStrLen;
  DWORD dwCompReadStrOffset;
  DWORD dwCompAttrLen;
  DWORD dwCompAttrOffset;
  DWORD dwCompClauseLen;
  DWORD dwCompClauseOffset;
  DWORD dwCompStrLen;
  DWORD dwCompStrOffset;
  DWORD dwCursorPos;
  DWORD dwDeltaStart;
  DWORD dwResultReadClauseLen;
  DWORD dwResultReadClauseOffset;
  DWORD dwResultReadStrLen;
  DWORD dwResultReadStrOffset;
  DWORD dwResultClauseLen;
  DWORD dwResultClauseOffset;
  DWORD dwResultStrLen;
  DWORD dwResultStrOffset;
  DWORD dwPrivateSize;
  DWORD dwPrivateOffset;
} COMPOSITIONSTRING,
  *PCOMPOSITIONSTRING,
  NEAR *NPCOMPOSITIONSTRING,
  FAR  *LPCOMPOSITIONSTRING;

typedef struct tagGUIDELINE {
  DWORD dwSize;
  DWORD dwLevel;
  DWORD dwIndex;
  DWORD dwStrLen;
  DWORD dwStrOffset;
  DWORD dwPrivateSize;
  DWORD dwPrivateOffset;
} GUIDELINE, *PGUIDELINE, NEAR *NPGUIDELINE, FAR *LPGUIDELINE;

#if (WINVER >= 0x040A)

typedef struct tagTRANSMSG {
  UINT   message;
  WPARAM wParam;
  LPARAM lParam;
} TRANSMSG, *PTRANSMSG, NEAR *NPTRANSMSG, FAR *LPTRANSMSG;

typedef struct tagTRANSMSGLIST {
  UINT     uMsgCount;
  TRANSMSG TransMsg[1];
} TRANSMSGLIST, *PTRANSMSGLIST, NEAR *NPTRANSMSGLIST, FAR *LPTRANSMSGLIST;

#endif  // WINVER >= 0x040A

typedef struct tagCANDIDATEINFO {
  DWORD dwSize;
  DWORD dwCount;
  DWORD dwOffset[32];
  DWORD dwPrivateSize;
  DWORD dwPrivateOffset;
} CANDIDATEINFO, *PCANDIDATEINFO, NEAR *NPCANDIDATEINFO, FAR *LPCANDIDATEINFO;


typedef struct tagINPUTCONTEXT {
  HWND  hWnd;
  BOOL  fOpen;
  POINT ptStatusWndPos;
  POINT ptSoftKbdPos;
  DWORD fdwConversion;
  DWORD fdwSentence;
  union {
    LOGFONTA A;
    LOGFONTW W;
  } lfFont;
  COMPOSITIONFORM cfCompForm;
  CANDIDATEFORM   cfCandForm[4];
  HIMCC hCompStr;
  HIMCC hCandInfo;
  HIMCC hGuideLine;
  HIMCC hPrivate;
  DWORD dwNumMsgBuf;
  HIMCC hMsgBuf;
  DWORD fdwInit;
  DWORD dwReserve[3];
} INPUTCONTEXT, *PINPUTCONTEXT, NEAR *NPINPUTCONTEXT, FAR *LPINPUTCONTEXT;

typedef struct tagIMEINFO {
  DWORD dwPrivateDataSize;
  DWORD fdwProperty;
  DWORD fdwConversionCaps;
  DWORD fdwSentenceCaps;
  DWORD fdwUICaps;
  DWORD fdwSCSCaps;
  DWORD fdwSelectCaps;
} IMEINFO, *PIMEINFO, NEAR *NPIMEINFO, FAR *LPIMEINFO;

typedef struct tagSOFTKBDDATA {
  UINT uCount;
  WORD wCode[1][256];
} SOFTKBDDATA, *PSOFTKBDDATA, NEAR *NPSOFTKBDDATA, FAR * LPSOFTKBDDATA;


BOOL WINAPI ImmGetHotKey(IN DWORD,
                         OUT LPUINT lpuModifiers,
                         OUT LPUINT lpuVKey,
                         OUT LPHKL);
BOOL WINAPI ImmSetHotKey(IN DWORD, IN UINT, IN UINT, IN HKL);
BOOL WINAPI ImmGenerateMessage(IN HIMC);
#if (WINVER >= 0x040A)
LRESULT WINAPI ImmRequestMessageA(IN HIMC, IN WPARAM, IN LPARAM);
LRESULT WINAPI ImmRequestMessageW(IN HIMC, IN WPARAM, IN LPARAM);
#ifdef UNICODE
#define ImmRequestMessage ImmRequestMessageW
#else
#define ImmRequestMessage ImmRequestMessageA
#endif  // !UNICODE
#endif  // WINVER >= 0x040A

HWND WINAPI ImmCreateSoftKeyboard(IN UINT, IN HWND, IN int, IN int);
BOOL WINAPI ImmDestroySoftKeyboard(IN HWND);
BOOL WINAPI ImmShowSoftKeyboard(IN HWND, IN int);

LPINPUTCONTEXT WINAPI ImmLockIMC(IN HIMC);
BOOL  WINAPI ImmUnlockIMC(IN HIMC);
DWORD WINAPI ImmGetIMCLockCount(IN HIMC);

HIMCC  WINAPI ImmCreateIMCC(IN DWORD);
HIMCC  WINAPI ImmDestroyIMCC(IN HIMCC);
LPVOID WINAPI ImmLockIMCC(IN HIMCC);
BOOL   WINAPI ImmUnlockIMCC(IN HIMCC);
DWORD  WINAPI ImmGetIMCCLockCount(IN HIMCC);
HIMCC  WINAPI ImmReSizeIMCC(IN HIMCC, IN DWORD);
DWORD  WINAPI ImmGetIMCCSize(IN HIMCC);

#define IMMGWL_IMC     0
#define IMMGWL_PRIVATE (sizeof(LONG))

#ifdef _WIN64
#undef IMMGWL_IMC
#undef IMMGWL_PRIVATE
#endif  // _WIN64

#define IMMGWLP_IMC     0
#define IMMGWLP_PRIVATE (sizeof(LONG_PTR))

#define IMC_SETCONVERSIONMODE 0x0002
#define IMC_SETSENTENCEMODE   0x0004
#define IMC_SETOPENSTATUS     0x0006

#define IMC_GETSOFTKBDFONT    0x0011
#define IMC_SETSOFTKBDFONT    0x0012
#define IMC_GETSOFTKBDPOS     0x0013
#define IMC_SETSOFTKBDPOS     0x0014
#define IMC_GETSOFTKBDSUBTYPE 0x0015
#define IMC_SETSOFTKBDSUBTYPE 0x0016
#define IMC_SETSOFTKBDDATA    0x0018


#define NI_CONTEXTUPDATED 0x0003

#define IME_SYSINFO_WINLOGON 0x0001
#define IME_SYSINFO_WOW16    0x0002

#define GCS_COMP       (GCS_COMPSTR|GCS_COMPATTR|GCS_COMPCLAUSE)
#define GCS_COMPREAD   (GCS_COMPREADSTR|GCS_COMPREADATTR |GCS_COMPREADCLAUSE)
#define GCS_RESULT     (GCS_RESULTSTR|GCS_RESULTCLAUSE)
#define GCS_RESULTREAD (GCS_RESULTREADSTR|GCS_RESULTREADCLAUSE)


#define INIT_STATUSWNDPOS 0x00000001
#define INIT_CONVERSION   0x00000002
#define INIT_SENTENCE     0x00000004
#define INIT_LOGFONT      0x00000008
#define INIT_COMPFORM     0x00000010
#define INIT_SOFTKBDPOS   0x00000020


#define IME_PROP_END_UNLOAD       0x00000001
#define IME_PROP_KBD_CHAR_FIRST   0x00000002
#define IME_PROP_IGNORE_UPKEYS    0x00000004
#define IME_PROP_NEED_ALTKEY      0x00000008
#define IME_PROP_NO_KEYS_ON_CLOSE 0x00000010
#define IME_PROP_ACCEPT_WIDE_VKEY 0x00000020

#define UI_CAP_SOFTKBD 0x00010000

#define IMN_SOFTKBDDESTROYED 0x0011


#define IME_ESC_PENAUXDATA 0x100c


BOOL WINAPI ImeInquire(IN LPIMEINFO,
                       OUT LPTSTR lpszUIClass,
                       IN DWORD dwSystemInfoFlags);
BOOL WINAPI ImeConfigure(IN HKL, IN HWND, IN DWORD, IN LPVOID);
DWORD WINAPI ImeConversionList(HIMC, LPCTSTR, LPCANDIDATELIST,
                               DWORD dwBufLen, UINT uFlag);
BOOL    WINAPI ImeDestroy(UINT);
LRESULT WINAPI ImeEscape(HIMC, UINT, LPVOID);
BOOL    WINAPI ImeProcessKey(IN HIMC, IN UINT, IN LPARAM, IN CONST LPBYTE);
BOOL    WINAPI ImeSelect(IN HIMC, IN BOOL);
BOOL    WINAPI ImeSetActiveContext(IN HIMC, IN BOOL);
#if (WINVER >= 0x040A)
UINT WINAPI ImeToAsciiEx(IN UINT uVirtKey,
                         IN UINT uScaCode,
                         IN CONST LPBYTE lpbKeyState,
                         OUT LPTRANSMSGLIST lpTransBuf,
                         IN UINT fuState,
                         IN HIMC);
#else
UINT WINAPI ImeToAsciiEx(IN UINT uVirtKey,
                         IN UINT uScaCode,
                         IN CONST LPBYTE lpbKeyState,
                         OUT LPDWORD lpdwTransBuf,
                         IN UINT fuState,
                         IN HIMC);
#endif  // WINVER >= 0x040A
BOOL WINAPI NotifyIME(IN HIMC, IN DWORD, IN DWORD, IN DWORD);
BOOL WINAPI ImeRegisterWord(IN LPCTSTR, IN DWORD, IN LPCTSTR);
BOOL WINAPI ImeUnregisterWord(IN LPCTSTR, IN DWORD, IN LPCTSTR);
UINT WINAPI ImeGetRegisterWordStyle(IN UINT nItem, OUT LPSTYLEBUF);
UINT WINAPI ImeEnumRegisterWord(IN REGISTERWORDENUMPROC,
                                IN LPCTSTR, IN DWORD, IN LPCTSTR, IN LPVOID);
BOOL WINAPI ImeSetCompositionString(IN HIMC,
                                    IN DWORD dwIndex,
                                    IN LPVOID lpComp,
                                    IN DWORD,
                                    IN LPVOID lpRead,
                                    IN DWORD);


typedef struct tagIMEPENDATA {
  DWORD dwVersion;
  DWORD dwFlags;
  DWORD dwCount;
  LPVOID lpExtraInfo;
  ULONG_PTR ulReserve;
  union {
    struct {
      LPDWORD lpSymbol;
      LPWORD lpSkip;
      LPWORD lpScore;
    } wd;
  };
} IMEPENDATA, *PIMEPENDATA, NEAR* NPIMEPENDATA, FAR* LPIMEPENDATA;

#define IME_PEN_SYMBOL 0x00000010
#define IME_PEN_SKIP   0x00000020
#define IME_PEN_SCORE  0x00000040

#endif  // _IMM_DDK_DEFINED_
#ifdef __cplusplus
}       // extern "C"
#endif
#endif  // MOZC_WIN32_BASE_IME_IMMDEV_H_
