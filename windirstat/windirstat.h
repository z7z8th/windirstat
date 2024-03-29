// windirstat.h - Main header for the windirstat application
//
// WinDirStat - Directory Statistics
// Copyright (C) 2003-2005 Bernhard Seifert
// Copyright (C) 2004-2017 WinDirStat Team (windirstat.net)
// Copyright (C) 2010 Chris Wimmer
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#ifndef __WINDIRSTAT_H_VER__
#define __WINDIRSTAT_H_VER__ 2017112219
#if (defined(_MSC_VER) && (_MSC_VER >= 1020)) || defined(__MCPP)
#pragma once
#endif /* Check for "#pragma once" support */

#include <Windows.h>
#include "resource.h"
#include "myimagelist.h"
#include "mountpoints.h"
#include "helpmap.h"
#include <common/tracer.h>

typedef CMap<CString, LPCTSTR, COLORREF, COLORREF> CExtensionColorMap;  // ".bmp" -> color

class CMainFrame;
class CDirstatApp;

// Frequently used "globals"
CMainFrame *GetMainFrame();
CDirstatApp *GetWDSApp();
CMyImageList* GetMyImageList();

// Other application related globals
CString GetAuthorEmail();
CString GetWinDirStatHomepage();

#if SUPPORT_ELEVATION
#   define WINDIRSTAT_EVENT_NAME_FMT L"WinDirStat_ElevationEvent_{72D223E3-1539-461D-980E-0863FE480E84}.%s.%s"
#endif // SUPPORT_ELEVATION

//
// CDirstatApp. The MFC application object.
// Knows about RAM Usage, Mount points, Help files and the CMyImageList.
//
class CDirstatApp : public CWinApp
{
    typedef CWinApp Inherited;
public:
    CDirstatApp();
    ~CDirstatApp();

    virtual BOOL InitInstance();
    virtual int ExitInstance();

    LANGID GetBuiltInLanguage();
    LANGID GetLangid();             // Language as selected in PageGeneral
    LANGID GetEffectiveLangid();    // Language to be used for date/time and number formatting

    void ReReadMountPoints();
    bool IsVolumeMountPoint(CString path);
    bool IsFolderJunction(CString path);

    COLORREF AltColor();                    // Coloring of compressed items
    COLORREF AltEncryptionColor();          // Coloring of encrypted items

    CString GetCurrentProcessMemoryInfo();
    CMyImageList* GetMyImageList();
    void UpdateRamUsage();

    void PeriodicalUpdateRamUsage();

    void DoContextHelp(DWORD topic);

    void GetAvailableResourceDllLangids(CArray<LANGID, LANGID>& arr);

    void RestartApplication();

    static bool getDiskFreeSpace(LPCTSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused);

protected:
    CString FindResourceDllPathByLangid(LANGID& langid);
    CString FindHelpfilePathByLangid(LANGID langid);
    CString FindAuxiliaryFileByLangid(LPCTSTR prefix, LPCTSTR suffix, LANGID& langid, bool checkResource);
    bool ScanResourceDllName(LPCTSTR name, LANGID& langid);
    bool ScanAuxiliaryFileName(LPCTSTR prefix, LPCTSTR suffix, LPCTSTR name, LANGID& langid);
#   ifdef _DEBUG
    void TestScanResourceDllName();
#   endif
    bool IsCorrectResourceDll(LPCTSTR path);

    bool UpdateMemoryInfo();

    // Get the alternative color from Explorer configuration
    COLORREF GetAlternativeColor(COLORREF clrDefault, LPCTSTR which);

    virtual BOOL OnIdle(LONG lCount);       // This is, where scanning is done.
#if SUPPORT_ELEVATION
    static BOOL IsUACEnabled();
#endif // SUPPORT_ELEVATION

    CSingleDocTemplate* m_pDocTemplate;     // MFC voodoo.

    LANGID m_langid;                        // Language we are running
    CReparsePoints m_mountPoints;           // Mount point information
    CMyImageList m_myImageList;             // Our central image list
    ULONGLONG m_workingSet;                 // Current working set (RAM usage)
    ULONGLONG m_pageFaults;                 // Page faults so far (unused)
    ULONGLONG m_lastPeriodicalRamUsageUpdate; // Tick count
    COLORREF m_altColor;                    // Coloring of compressed items
    COLORREF m_altEncryptionColor;          // Coloring of encrypted items
#if SUPPORT_ELEVATION
    HANDLE m_ElevationEvent;
    CString m_ElevationEventName;
#endif // SUPPORT_ELEVATION
#ifdef VTRACE_TO_CONSOLE
    CAutoPtr<CWDSTracerConsole> m_vtrace_console;
#endif // VTRACE_TO_CONSOLE

    DECLARE_MESSAGE_MAP()
    afx_msg void OnFileOpen();
#if SUPPORT_ELEVATION
    afx_msg void OnRunElevated();
    afx_msg void OnUpdateRunElevated(CCmdUI *pCmdUI);
#endif // SUPPORT_ELEVATION
    afx_msg void OnHelpManual();
    afx_msg void OnAppAbout();
};

#endif /* __WINDIRSTAT_H_VER__ */
