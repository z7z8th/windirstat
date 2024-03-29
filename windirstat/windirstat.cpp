// windirstat.cpp - Implementation of CDirstatApp and some globals
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
//

#include "stdafx.h"
#include "windirstat.h"
#include <common/mdexceptions.h>
#include <common/commonhelpers.h>
#include "mainframe.h"
#include "selectdrivesdlg.h"
#include "aboutdlg.h"
#include "dirstatdoc.h"
#include "graphview.h"
#include "osspecific.h"
#include "globalhelpers.h"
#include "WorkLimiter.h"
#pragma warning(push)
#pragma warning(disable : 4091)
#include <Dbghelp.h> // for mini dumps
#pragma warning(pop)

#ifdef _DEBUG
#   include <common/tracer.cpp>
#   define new DEBUG_NEW
#endif

CMainFrame *GetMainFrame()
{
    // Not: return (CMainFrame *)AfxGetMainWnd();
    // because CWinApp::m_pMainWnd is set too late.
    return CMainFrame::GetTheFrame();
}

CDirstatApp *GetWDSApp()
{
    return reinterpret_cast<CDirstatApp *>(AfxGetApp());
}

CString GetAuthorEmail()
{
    return _T("team") _T("\x40") _T("windirstat.net"); // FIXME into common string file
}

CString GetWinDirStatHomepage()
{
    return _T("windirstat.net"); // FIXME into common string file
}

CMyImageList* GetMyImageList()
{
    return GetWDSApp()->GetMyImageList();
}


// CDirstatApp

BEGIN_MESSAGE_MAP(CDirstatApp, CWinApp)
    ON_COMMAND(ID_APP_ABOUT, OnAppAbout)
    ON_COMMAND(ID_FILE_OPEN, OnFileOpen)
#if SUPPORT_ELEVATION
    ON_COMMAND(ID_RUNELEVATED, OnRunElevated)
    ON_UPDATE_COMMAND_UI(ID_RUNELEVATED, OnUpdateRunElevated)
#endif // SUPPORT_ELEVATION
    ON_COMMAND(ID_HELP_MANUAL, OnHelpManual)
END_MESSAGE_MAP()


CDirstatApp _theApp;

CDirstatApp::CDirstatApp()
    : Inherited()
    , m_pDocTemplate(0)
    , m_langid(0)
    , m_workingSet(0)
    , m_pageFaults(0)
#   if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
    , m_lastPeriodicalRamUsageUpdate(0)
#   else
    , m_lastPeriodicalRamUsageUpdate(_GetTickCount64())
#   endif /* (_WIN32_WINNT < _WIN32_WINNT_VISTA) */
    , m_altColor(GetAlternativeColor(RGB(0x00, 0x00, 0xFF), _T("AltColor")))
    , m_altEncryptionColor(GetAlternativeColor(RGB(0x00, 0x80, 0x00), _T("AltEncryptionColor")))
#   if SUPPORT_ELEVATION
    , m_ElevationEvent(NULL)
    , m_ElevationEventName()
#   endif // SUPPORT_ELEVATION
#   ifdef VTRACE_TO_CONSOLE
    , m_vtrace_console(new CWDSTracerConsole())
#   endif // VTRACE_TO_CONSOLE
{
#   ifdef _DEBUG
    TestScanResourceDllName();
#   endif

#   if (_WIN32_WINNT < _WIN32_WINNT_VISTA)
    InitGetTickCount64();
    m_lastPeriodicalRamUsageUpdate = _GetTickCount64();
#   endif /* (_WIN32_WINNT < _WIN32_WINNT_VISTA) */

#   if SUPPORT_ELEVATION
    m_ElevationEventName.Format(WINDIRSTAT_EVENT_NAME_FMT, GetCurrentDesktopName().GetBuffer(), GetCurrentWinstaName().GetBuffer());
    VTRACE(_T("Elevation event: %s"), m_ElevationEventName.GetBuffer());
#   endif // SUPPORT_ELEVATION
}

CDirstatApp::~CDirstatApp()
{
#if SUPPORT_ELEVATION
    if (m_ElevationEvent)
    {
        ::CloseHandle(m_ElevationEvent); //make sure this is the very last thing that is destroyed (way after WM_CLOSE)
    }
#endif // SUPPORT_ELEVATION
}

CMyImageList* CDirstatApp::GetMyImageList()
{
    m_myImageList.initialize();
    return &m_myImageList;
}

void CDirstatApp::UpdateRamUsage()
{
    CWinThread::OnIdle(0);
}

void CDirstatApp::PeriodicalUpdateRamUsage()
{
    if(_GetTickCount64() - m_lastPeriodicalRamUsageUpdate > 1200)
    {
        UpdateRamUsage();
        m_lastPeriodicalRamUsageUpdate = _GetTickCount64();
    }
}

CString CDirstatApp::FindResourceDllPathByLangid(LANGID& langid)
{
    return FindAuxiliaryFileByLangid(
        wds::strLangPrefix
        , wds::strLangSuffix
        , langid
        , true
        );
}

CString CDirstatApp::FindHelpfilePathByLangid(LANGID langid)
{
    CString s;
    if(langid == GetBuiltInLanguage())
    {
        // The English help file is named windirstat.chm.
        s = GetAppFolder() + _T("\\windirstat.chm");
        if(::PathFileExists(s))
        {
            return s;
        }
    }

    // Help files for other languages are named wdshxxxx.chm (xxxx = LANGID).
    s = FindAuxiliaryFileByLangid(_T("wdsh"), _T(".chm"), langid, false);
    if(!s.IsEmpty())
    {
        return s;
    }

    // Else, try windirstat.chm again.
    s = GetAppFolder() + _T("\\windirstat.chm");
    if(::PathFileExists(s))
    {
        return s;
    }

    // Not found.
    return wds::strEmpty;
}

void CDirstatApp::GetAvailableResourceDllLangids(CArray<LANGID, LANGID>& arr)
{
    arr.RemoveAll();

    CFileFind finder;
    BOOL b = finder.FindFile(GetAppFolder() + _T("\\wdsr*") _T(STR_LANG_SUFFIX));
    while(b)
    {
        b = finder.FindNextFile();
        if(finder.IsDirectory())
        {
            continue;
        }

        LANGID langid;
        if(ScanResourceDllName(finder.GetFileName(), langid) && IsCorrectResourceDll(finder.GetFilePath()))
        {
            arr.Add(langid);
        }
    }
}

void CDirstatApp::RestartApplication()
{
    // First, try to create the suspended process
    STARTUPINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    BOOL success = CreateProcess(GetAppFileName(), NULL, NULL, NULL, false, CREATE_SUSPENDED, NULL, NULL, &si, &pi);
    if(!success)
    {
        CString s;
        s.FormatMessage(IDS_CREATEPROCESSsFAILEDs, GetAppFileName().GetString(), MdGetWinErrorText(::GetLastError()).GetString());
        AfxMessageBox(s);
        return;
    }

    // We _send_ the WM_CLOSE here to ensure that all CPersistence-Settings
    // like column widths an so on are saved before the new instance is resumed.
    // This will post a WM_QUIT message.
    GetMainFrame()->SendMessage(WM_CLOSE);

    DWORD dw = ::ResumeThread(pi.hThread);
    if(dw != 1)
    {
        VTRACE(_T("ResumeThread() didn't return 1"));
    }

    ::CloseHandle(pi.hProcess);
    ::CloseHandle(pi.hThread);
}

bool CDirstatApp::getDiskFreeSpace(LPCTSTR pszRootPath, ULONGLONG& total, ULONGLONG& unused)
{
    static ULARGE_INTEGER u64available = {0};
    ULARGE_INTEGER u64total = {0};
    ULARGE_INTEGER u64free = {0};

    // On NT 4.0, the 2nd Parameter to this function must NOT be NULL.
    // TODO: verify whether Windows 2000 behaves correctly
    BOOL b = GetDiskFreeSpaceEx(pszRootPath, &u64available, &u64total, &u64free);
    if(!b)
    {
        VTRACE(_T("GetDiskFreeSpaceEx(%s) failed."), pszRootPath);
    }

    // FIXME: need to retrieve total via IOCTL_DISK_GET_PARTITION_INFO instead
    total = u64total.QuadPart;
    unused = u64free.QuadPart;

    // Race condition ...
    ASSERT(unused <= total);
    return (FALSE != b);
}

bool CDirstatApp::ScanResourceDllName(LPCTSTR name, LANGID& langid)
{
    return ScanAuxiliaryFileName(
        wds::strLangPrefix
        , wds::strLangSuffix
        , name
        , langid
        );
}

// suffix contains the dot (e.g. ".chm")
bool CDirstatApp::ScanAuxiliaryFileName(LPCTSTR prefix, LPCTSTR suffix, LPCTSTR name, LANGID& langid)
{
    using wds::iLangCodeLength;
    ASSERT(_tcslen(prefix) == _tcslen(wds::strLangPrefix)); // FIXME: Also add .chm part or split
    ASSERT(_tcslen(suffix) == _tcslen(wds::strLangSuffix)); // FIXME: see above

    CString s(name);   // [prefix][lngcode].[suffix]
    s.MakeLower();
    if(s.Left(((int)_tcslen(prefix))) != prefix)
    {
        return false;
    }
    s = s.Mid(((int)_tcslen(prefix))); // remove prefix from the front -> [lngcode].[suffix]

    if(s.GetLength() != (iLangCodeLength + ((int)_tcslen(suffix))))
    {
        return false;
    }

    if(s.Mid(iLangCodeLength) != CString(suffix).MakeLower())
    {
        return false;
    }

    s = s.Left(iLangCodeLength); // retain the language code -> [lngcode]

    for(int i = 0; i < iLangCodeLength; i++)
    {
        if(!_istxdigit(s[i]))
        {
            return false;
        }
    }

    int id;
    VERIFY(1 == _stscanf_s(s, _T("%04x"), &id));
    langid = (LANGID)id;

    return true;
}

#ifdef _DEBUG
void CDirstatApp::TestScanResourceDllName()
{
    LANGID id;
    ASSERT(!ScanResourceDllName(wds::strEmpty, id));
    ASSERT(!ScanResourceDllName(_T(STR_RESOURCE_PREFIX) _T(STR_LANG_SUFFIX), id));
    ASSERT(!ScanResourceDllName(_T(STR_RESOURCE_PREFIX) _T("123") _T(STR_LANG_SUFFIX), id));
    ASSERT(!ScanResourceDllName(_T(STR_RESOURCE_PREFIX) _T("12345") _T(STR_LANG_SUFFIX), id));
    ASSERT(!ScanResourceDllName(_T(STR_RESOURCE_PREFIX) _T("1234.exe"), id));
    ASSERT(ScanResourceDllName (_T(STR_RESOURCE_PREFIX) _T("0123") _T(STR_LANG_SUFFIX), id));
    ASSERT(id == 0x0123);
    ASSERT(ScanResourceDllName (CString(_T(STR_RESOURCE_PREFIX) _T("a13F") _T(STR_LANG_SUFFIX)).MakeUpper(), id));
    ASSERT(id == 0xa13f);
}
#endif

CString CDirstatApp::FindAuxiliaryFileByLangid(LPCTSTR prefix, LPCTSTR suffix, LANGID& langid, bool checkResource)
{
    CString number;
    number.Format(_T("%04x"), langid);

    CString exactName;
    exactName.Format(_T("%s%s%s"), prefix, number.GetString(), suffix);

    CString exactPath = GetAppFolder() + _T("\\") + exactName;
    if(::PathFileExists(exactPath) && (!checkResource || IsCorrectResourceDll(exactPath)))
    {
        return exactPath;
    }

    CString search;
    search.Format(_T("%s*%s"), prefix, suffix);

    CFileFind finder;
    BOOL b = finder.FindFile(GetAppFolder() + _T("\\") + search);
    while(b)
    {
        b = finder.FindNextFile();
        if(finder.IsDirectory())
        {
            continue;
        }

        LANGID id;
        if(!ScanAuxiliaryFileName(prefix, suffix, finder.GetFileName(), id))
        {
            continue;
        }

        if(PRIMARYLANGID(id) == PRIMARYLANGID(langid) && (!checkResource || IsCorrectResourceDll(finder.GetFilePath())))
        {
            langid = id;
            return finder.GetFilePath();
        }
    }

    return wds::strEmpty;
}

bool CDirstatApp::IsCorrectResourceDll(LPCTSTR path)
{
    HMODULE module = ::LoadLibraryEx(path, NULL, LOAD_LIBRARY_AS_DATAFILE);
    if(module == NULL)
    {
        return false;
    }

    // TODO/FIXME: introduce some method of checking the resource version

    CString reference = LoadString(IDS_RESOURCEVERSION);

    int bufsize = reference.GetLength() * 2;
    CString s;
    int r = LoadString(module, IDS_RESOURCEVERSION, s.GetBuffer(bufsize), bufsize);
    s.ReleaseBuffer();

    FreeLibrary(module);

    if(r == 0 || s != reference)
    {
        return false;
    }

    return true;
}

void CDirstatApp::ReReadMountPoints()
{
    m_mountPoints.Initialize();
}

bool CDirstatApp::IsVolumeMountPoint(CString path)
{
    return m_mountPoints.IsVolumeMountPoint(path);
}

bool CDirstatApp::IsFolderJunction(CString path)
{
    return m_mountPoints.IsFolderJunction(path);
}

// Get the alternative colors for compressed and encrypted files/folders.
// This function uses either the value defined in the Explorer configuration
// or the default color values.
COLORREF CDirstatApp::GetAlternativeColor(COLORREF clrDefault, LPCTSTR which)
{
    COLORREF x; DWORD cbValue = sizeof(x); CRegKey key;

    // Open the explorer key
    key.Open(HKEY_CURRENT_USER, wds::strExplorerKey, KEY_READ);

    // Try to read the REG_BINARY value
    if(ERROR_SUCCESS == key.QueryBinaryValue(which, &x, &cbValue))
    {
        // Return the read value upon success
        return x;
    }
    else
    {
        // Return the default upon failure
        return clrDefault;
    }
}

COLORREF CDirstatApp::AltColor()
{
    // Return property value
    return m_altColor;
}

COLORREF CDirstatApp::AltEncryptionColor()
{
    // Return property value
    return m_altEncryptionColor;
}

CString CDirstatApp::GetCurrentProcessMemoryInfo()
{
    UpdateMemoryInfo();

    if(m_workingSet == 0)
    {
        return wds::strEmpty;
    }

    CString n = PadWidthBlanks(FormatBytes(m_workingSet), 11);

    CString s;
    s.FormatMessage(IDS_RAMUSAGEs, n.GetString());

    return s;
}

bool CDirstatApp::UpdateMemoryInfo()
{
    PROCESS_MEMORY_COUNTERS pmc;
    ZeroMemory(&pmc, sizeof(pmc));
    pmc.cb = sizeof(pmc);

    if(!::GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc)))
    {
        return false;
    }

    m_workingSet = pmc.WorkingSetSize;

    bool ret = false;
    if(pmc.PageFaultCount > m_pageFaults + 500)
    {
        ret = true;
    }

    m_pageFaults = pmc.PageFaultCount;

    return ret;
}

LANGID CDirstatApp::GetBuiltInLanguage()
{
    return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

BOOL CDirstatApp::InitInstance()
{
    Inherited::InitInstance();

    ::InitCommonControls();           // InitCommonControls() is necessary for Windows XP.
    VERIFY(AfxOleInit());           // For ::SHBrowseForFolder()
    AfxEnableControlContainer();    // For our rich edit controls in the about dialog
    VERIFY(AfxInitRichEdit());      // Rich edit control in out about box
    VERIFY(AfxInitRichEdit2());     // On NT, this helps.
    Inherited::EnableHtmlHelp();

    Inherited::SetRegistryKey(_T("Seifert"));
    Inherited::LoadStdProfileSettings(4);

    m_langid = GetBuiltInLanguage();

    LANGID langid = CLanguageOptions::GetLanguage();
    if(langid != GetBuiltInLanguage())
    {
        CString resourceDllPath = FindResourceDllPathByLangid(langid);
        if(!resourceDllPath.IsEmpty())
        {
            // Load language resource DLL
            HINSTANCE dll = ::LoadLibrary(resourceDllPath);
            if(dll)
            {
                // Set default module handle for loading of resources
                AfxSetResourceHandle(dll);
                m_langid = langid;
            }
            else
            {
                VTRACE(_T("LoadLibrary(%s) failed: %u"), resourceDllPath.GetString(), ::GetLastError());
            }
        }
        // else: We use our built-in English resources.

        CLanguageOptions::SetLanguage(m_langid);
    }

#if SUPPORT_ELEVATION
    //check for an elevation event
    m_ElevationEvent = ::OpenEvent(SYNCHRONIZE, FALSE, m_ElevationEventName);

    if (m_ElevationEvent)
    {
        //and if so, wait for it, so previous instance can store its config that we reload next
        ::WaitForSingleObject(m_ElevationEvent, 20 * 1000);
        ::CloseHandle(m_ElevationEvent);
        m_ElevationEvent = NULL;
    }
    else
    {
        VTRACE(_T("OpenEvent failed with %d"), GetLastError());
    }
#endif // SUPPORT_ELEVATION

    GetOptions()->LoadFromRegistry();

    m_pDocTemplate = new CSingleDocTemplate(
        IDR_MAINFRAME,
        RUNTIME_CLASS(CDirstatDoc),
        RUNTIME_CLASS(CMainFrame),
        RUNTIME_CLASS(CGraphView));
    if(!m_pDocTemplate)
    {
        return FALSE;
    }
    AddDocTemplate(m_pDocTemplate);

    CCommandLineInfo cmdInfo;
    ParseCommandLine(cmdInfo);

    m_nCmdShow = SW_HIDE;
    if(!ProcessShellCommand(cmdInfo))
    {
        return FALSE;
    }
    FileIconInit(TRUE);

    GetMainFrame()->InitialShowWindow();
    m_pMainWnd->UpdateWindow();

    // When called by setup.exe, WinDirStat remained in the
    // background, so we do a
    m_pMainWnd->BringWindowToTop();
    m_pMainWnd->SetForegroundWindow();

    if(cmdInfo.m_nShellCommand != CCommandLineInfo::FileOpen)
    {
        OnFileOpen();
    }

    return TRUE;
}

int CDirstatApp::ExitInstance()
{
    return Inherited::ExitInstance();
}

LANGID CDirstatApp::GetLangid()
{
    return m_langid;
}

LANGID CDirstatApp::GetEffectiveLangid()
{
    if(GetOptions()->IsUseWdsLocale())
    {
        return GetLangid();
    }
    else
    {
        return ::GetUserDefaultLangID();
    }
}

void CDirstatApp::OnAppAbout()
{
    StartAboutDialog();
}

void CDirstatApp::OnFileOpen()
{
    CSelectDrivesDlg dlg;
    if(IDOK == dlg.DoModal())
    {
        CString path = CDirstatDoc::EncodeSelection((RADIO)dlg.m_radio, dlg.m_folderName, dlg.m_drives);
        m_pDocTemplate->OpenDocumentFile(path, true);
    }
}

#if SUPPORT_ELEVATION
BOOL CDirstatApp::IsUACEnabled()
{
    OSVERSIONINFOEX osInfo;
    DWORDLONG conditionMask = 0;

    ZeroMemory(&osInfo, sizeof(osInfo));
    osInfo.dwOSVersionInfoSize = sizeof(osInfo);
    osInfo.dwMajorVersion = 6;
    osInfo.dwMinorVersion = 0;
    VER_SET_CONDITION(conditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
    VER_SET_CONDITION(conditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);

    if (::VerifyVersionInfo(&osInfo, VER_MAJORVERSION | VER_MINORVERSION, conditionMask))
    {
        HKEY hKey;
        if (::RegOpenKeyW(HKEY_LOCAL_MACHINE, L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", &hKey) == ERROR_SUCCESS)
        {
            DWORD value = 0;
            if (::RegQueryValueExW(hKey, L"EnableLUA", NULL, NULL, NULL, &value) == ERROR_SUCCESS)
            {
                return (value != 0);
            }
            else
            {
                VTRACE(_T("IsUACEnabled::RegQueryValueExW failed"));
            }

            ::RegCloseKey(hKey);
        }
        else
        {
            VTRACE(_T("IsUACEnabled::RegOpenKeyW failed"));
        }
    }
    
    return FALSE;
}

void CDirstatApp::OnUpdateRunElevated(CCmdUI *pCmdUI)
{
    pCmdUI->Enable(!IsAdmin() && IsUACEnabled());
}

#ifndef SEE_MASK_DEFAULT
#   define SEE_MASK_DEFAULT           0x00000000
#endif

void CDirstatApp::OnRunElevated()
{
    if (IsAdmin() || !IsUACEnabled())
    {
        return;
    }

    CString sAppName = GetAppFileName();

    SHELLEXECUTEINFO shellInfo;
    ZeroMemory(&shellInfo, sizeof(shellInfo));
    shellInfo.cbSize = sizeof(shellInfo);
    shellInfo.fMask = SEE_MASK_DEFAULT;
    shellInfo.lpFile = sAppName;
    shellInfo.lpVerb = _T("runas"); //DO NOT LOCALIZE!
    shellInfo.nShow = SW_NORMAL;


    if (m_ElevationEvent)
    {
        ::CloseHandle(m_ElevationEvent);
    }
    m_ElevationEvent = ::CreateEvent(NULL, TRUE, FALSE, m_ElevationEventName); 
    if (!m_ElevationEvent)
    {
        VTRACE(_T("CreateEvent failed with %d"), GetLastError());
        m_ElevationEvent = NULL;
        return;
    }
    if (ERROR_ALREADY_EXISTS == ::GetLastError())
    {
        VTRACE(_T("Event already exists"));
        ::CloseHandle(m_ElevationEvent);
        m_ElevationEvent = NULL;
        return;
    }

    if (!::ShellExecuteEx(&shellInfo))
    {
        VTRACE(_T("ShellExecuteEx failed to elevate %d"), GetLastError());
        
        ::CloseHandle(m_ElevationEvent);
        m_ElevationEvent = NULL;

        //TODO: Display message to user?
    }
    else
    {
        //TODO: Store configurations for the new app
        
        GetMainFrame()->SendMessage(WM_CLOSE);
        ::SetEvent(m_ElevationEvent); //Tell other process that we finished saving data (it waits only 20s)

        ::CloseHandle(m_ElevationEvent);
        m_ElevationEvent = NULL;
    }
}
#endif // SUPPORT_ELEVATION

BOOL CDirstatApp::OnIdle(LONG lCount)
{
    bool more = false;

    CDirstatDoc *doc = GetDocument();
    CWorkLimiter limiter;
    limiter.Start(600);
    if((doc) && (!doc->Work(&limiter)))
    {
        more = true;
    }

    if(Inherited::OnIdle(lCount))
    {
        more = true;
    }

    // The status bar (RAM usage) is updated only when count == 0.
    // That's why we call an extra OnIdle(0) here.
    if(CWinThread::OnIdle(0))
    {
        more = true;
    }

    return more;
}

void CDirstatApp::OnHelpManual()
{
    // FIXME: open browser, point to Wiki (via windirstat.net short link), based on current language
    DoContextHelp(IDH_StartPage);
}

void CDirstatApp::DoContextHelp(DWORD)
{
    CString msg;
    msg.FormatMessage(IDS_HELPFILEsCOULDNOTBEFOUND, _T("windirstat.chm"));
    AfxMessageBox(msg);
}

