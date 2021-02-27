#include "win32ctrl.h"
#include "win32util.h"
#include <commctrl.h>
#include <stdint.h>
#include <string.h>
#include <strsafe.h>
#include <exception>
#include <memory>

/* AppException */

AppException::AppException(AppMonitor* app,
    const std::string& text)
{
    m_pApp = app;
    m_Text = text;
    m_dwError = GetLastError();
}

static char s_szExcBuf[256] = {0};

const char* AppException::what() const noexcept
{
    StringCchPrintfA(s_szExcBuf, 256, "AppMonitor#%05d: %s",
        GetApp()->GetAppProcessId(), m_Text.c_str());
    return s_szExcBuf;
}

/* AppMonitor */

void AppMonitor::Init()
{
    InitCommonControls();
}

AppMonitor::AppMonitor()
    : m_ExePath(L"")
{
    m_hAppProcess = INVALID_HANDLE_VALUE;
    m_dwPid = 0;
    m_bWow64 = FALSE;
}

AppMonitor::AppMonitor(const std::wstring& app)
    : m_ExePath(app)
{
    m_hAppProcess = INVALID_HANDLE_VALUE;
    m_dwPid = 0;
    m_bWow64 = FALSE;
}

AppMonitor::~AppMonitor()
{
    if (m_hAppProcess != INVALID_HANDLE_VALUE)
        CloseHandle(m_hAppProcess);
}

struct _enumapp_s {
    AppMonitor* m_pMonitor;
    AppMonitor::EnumFunc m_Func;
};

BOOL CALLBACK _EnumAppWindows(HWND hWnd, LPARAM lParam)
{
    AppMonitor* app = (AppMonitor*)lParam;

    DWORD dwPid = 0;
    GetWindowThreadProcessId(app->m_Tmp.m_hWnd
        ? app->m_Tmp.m_hWnd : hWnd, &dwPid);
    if (dwPid && dwPid != app->GetAppProcessId())
        return TRUE;

    return app->m_Tmp.m_Func(app, hWnd);
}

void AppMonitor::EnumAppWindows(EnumFunc func)
{
    m_Tmp.m_hWnd = NULL;
    m_Tmp.m_Func = func;

    EnumWindows(_EnumAppWindows, (LPARAM)this);
}

void AppMonitor::EnumAppControls(HWND hWnd, EnumFunc func)
{
    m_Tmp.m_hWnd = hWnd;
    m_Tmp.m_Func = func;

    EnumChildWindows(hWnd, _EnumAppWindows, (LPARAM)this);
}

HWND AppMonitor::FindAppWindow(const std::string& wndClass)
{
    m_Tmp.m_hWnd = NULL;
    m_Tmp.m_Class = wndClass;

    EnumAppWindows(
        [](AppMonitor* app, HWND hWnd) {
            if (app->GetWindowClass(hWnd) == app->m_Tmp.m_Class)
            {
                app->m_Tmp.m_hWnd = hWnd;
                return false;
            }

            return true;
        }
    );

    return m_Tmp.m_hWnd;
}

bool AppMonitor::StartApp(const std::wstring& cmdLine)
{
    if (m_ExePath.empty())
        throw AppException(this, "m_ExePath.empty()");

    auto pszCmdLine = std::make_unique<wchar_t[]>(MAX_CMDLINE);
    StringCchPrintfW(pszCmdLine.get(), MAX_CMDLINE, L"\"%s\" %s",
        m_ExePath.c_str(), cmdLine.c_str());

    PROCESS_INFORMATION pi;
    STARTUPINFOW si;

    ZeroMemory(&pi, sizeof(pi));
    ZeroMemory(&si, sizeof(si));

    si.cb = sizeof(si);
    BOOL bCreated = CreateProcessW(m_ExePath.c_str(), pszCmdLine.get(),
        NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    if (!bCreated)
        return false;

    m_hAppProcess = pi.hProcess;
    m_dwPid = pi.dwProcessId;

    if (!IsWow64Process(m_hAppProcess, &m_bWow64))
        m_bWow64 = FALSE;

    CloseHandle(pi.hThread);
    return true;
}

void AppMonitor::CloseApp()
{
    EnumAppWindows(
        [](AppMonitor* app, HWND hWnd) {
            SendMessage(hWnd, WM_CLOSE, 0, 0);
            return true;
        }
    );

    CloseHandle(m_hAppProcess);
    m_hAppProcess = INVALID_HANDLE_VALUE;
}

void AppMonitor::Terminate()
{
    TerminateProcess(m_hAppProcess, 0);
    CloseHandle(m_hAppProcess);
    m_hAppProcess = INVALID_HANDLE_VALUE;
}

bool AppMonitor::IsAppRunning() const
{
    DWORD dwWait = WaitForSingleObject(m_hAppProcess, 0);
    if (dwWait != WAIT_OBJECT_0)
        return false;

    return true;
}

bool AppMonitor::IsWow64() const
{
    return !!m_bWow64;
}

bool AppMonitor::WaitAppIdle(DWORD dwInterval)
{
    return WaitForInputIdle(GetAppProcess(), dwInterval) != 0;
}

void AppMonitor::MonitorSetup()
{
    EnumAppWindows(
        [](AppMonitor* app, HWND hWnd) {
            app->OnAppWindow(hWnd);
            return true;
        }
    );
}

void AppMonitor::OnAppWindow(HWND hWnd)
{
}

void AppMonitor::SetExePath(const std::wstring& exePath)
{
    m_ExePath = exePath;
}

std::string AppMonitor::GetWindowClass(HWND hWnd)
{
    char szClass[64] = {0};
    GetClassNameA(hWnd, szClass, 64);
    return std::string(szClass);
}

HWND AppMonitor::GetChild(HWND hWnd, const std::string& wndClass,
    const std::wstring& wndText)
{
    return FindWindowExW(hWnd, NULL, TextToWchar(wndClass).c_str(),
        wndText.empty() ? NULL : wndText.c_str());
}

AppMem AppMonitor::MemAlloc(unsigned uLen)
{
    void* pThisMem;
    void* pAppMem;

    pThisMem = (char*)malloc(uLen);
    if (!pThisMem)
        throw AppException(this, "!malloc");
    memset(pThisMem, '\0', uLen);
    pAppMem = VirtualAllocEx(m_hAppProcess,
        NULL, uLen, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!pAppMem)
    {
        free(pThisMem);
        throw AppException(this, "!VirtualAllocEx");
    }
    else if (IsWow64() && (uintptr_t)pAppMem > 0xFFFFFFFF)
    {
        free(pThisMem);
        throw AppException(this, "WOW64 VirtualAllocEx >4GB addr");
    }

    return AppMem(pThisMem, pAppMem, uLen);
}

void AppMonitor::MemFree(AppMem& mem)
{
    free(mem.This());
    VirtualFreeEx(m_hAppProcess, mem.App(), 0, MEM_RELEASE);
    mem = AppMem();
}

void AppMonitor::MemWriteApp(const AppMem& mem)
{
    SIZE_T szTmp = mem.Size();
    if(!WriteProcessMemory(m_hAppProcess, mem.App(),
        mem.This(), mem.Size(), &szTmp))
    {
        throw AppException(this, "!WriteProcessMemory");
    }
}

void AppMonitor::MemReadApp(const AppMem& mem)
{
    SIZE_T szTmp = mem.Size();
    if(!ReadProcessMemory(m_hAppProcess, mem.App(),
        mem.This(), mem.Size(), &szTmp))
    {
        throw AppException(this, "!ReadProcessMemory");
    }
}

bool AppMonitor::AppMessage(HWND hWnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam,
    DWORD_PTR* pResult,
    unsigned uTimeOut)
{
    DWORD_PTR dwResult = 0;
    LRESULT lResult;

    if (IsWindowUnicode(hWnd))
    {
        lResult = SendMessageTimeoutW(
            hWnd, uMsg, wParam, lParam,
            SMTO_ABORTIFHUNG, uTimeOut,
            &dwResult);
    }
    else
    {
        lResult = SendMessageTimeoutA(
            hWnd, uMsg, wParam, lParam,
            SMTO_ABORTIFHUNG, uTimeOut,
            &dwResult);
    }

    if (pResult) *pResult = dwResult;

    if (lResult) // ок
        return true;
    else if (GetLastError() == ERROR_TIMEOUT) // таймаут
        throw AppTimeOut(this, uMsg);
    else // ошибка
    {
        throw AppException(this, "!SendMessageTimeoutW");
    }
}

void AppMonitor::AppPostMessage(HWND hWnd, UINT uMsg,
    WPARAM wParam, LPARAM lParam)
{
    if (!hWnd)
        throw AppException(this, "!hWnd");
    if (!PostMessageW(hWnd, uMsg, wParam, lParam))
        throw AppException(this, "!PostMessageW");
}

AppMem AppMonitor::NewString(HWND hWnd, DWORD dwChars)
{
    if (!dwChars) dwChars = 256;
    return MemAlloc(IsWindowUnicode(hWnd)
        ? sizeof(wchar_t) * dwChars
        : sizeof(char) * dwChars
    );
}

std::wstring AppMonitor::ReadString(HWND hWnd, const AppMem& str)
{
    std::wstring ret;
    MemReadApp(str);

    if (IsWindowUnicode(hWnd))
    {
        wchar_t* pszText = (wchar_t*)str.This();
        pszText[(uint64_t)(str.Size()-1)/sizeof(wchar_t)] = L'\0';
        ret = std::wstring(pszText);
    }
    else
    {
        char* pszText = (char*)str.This();
        pszText[(uint64_t)(str.Size()-1)/sizeof(char)] = '\0';
        ret = AnsiToWchar(pszText);
    }

    return ret;
}

std::wstring AppMonitor::GetWindowTextStr(HWND hWnd)
{
    auto pszText = std::make_unique<wchar_t[]>(MAX_WM_TEXT);

    SetLastError(0);
    int iLen = GetWindowTextW(hWnd, pszText.get(), MAX_WM_TEXT);
    if (iLen < 0)
        throw AppException(this, "!GetWindowTextW");

    pszText[iLen] = L'\0';
    return std::wstring(pszText.get());
}

std::wstring AppMonitor::GetControlTextStr(HWND hWnd)
{
    DWORD_PTR dwLength = 0, dwRead = 0;
    AppMessage(hWnd, WM_GETTEXTLENGTH, 0, 0,
        &dwLength);
    if (dwLength <= 0)
        return L"";

    std::wstring ret;
    if (IsWindowUnicode(hWnd))
    {
        auto pszText = std::make_unique<wchar_t[]>(dwLength * 2);
        AppMessage(hWnd, WM_GETTEXT,
            (WPARAM)dwLength+1, (LPARAM)pszText.get(),
            &dwRead);
        //dwRead = (DWORD_PTR)SendMessage(hWnd, WM_GETTEXT,
        //  (WPARAM)dwLength+1, (LPARAM)pszText.get());
        pszText[dwRead] = L'\0';
        ret = std::wstring(pszText.get());
    }
    else
    {
        auto pszText = std::make_unique<char[]>(dwLength * 2);
        AppMessage(hWnd, WM_GETTEXT,
            (WPARAM)dwLength+1, (LPARAM)pszText.get(),
            &dwRead);
        //dwRead = (DWORD_PTR)SendMessage(hWnd, WM_GETTEXT,
        //  (WPARAM)dwLength+1, (LPARAM)pszText.get());
        pszText[dwRead] = '\0';
        ret = AnsiToWchar(std::string(pszText.get()));
    }

    return ret;
}

DWORD_PTR AppMonitor::TV_GetNextItem(HWND hTree, DWORD dwFlags,
    DWORD_PTR dwItem)
{
    DWORD_PTR dwRetItem = 0;
    AppMessage(hTree, TVM_GETNEXTITEM,
        (WPARAM)dwFlags, dwItem, &dwRetItem);
    return dwRetItem;
}

typedef struct {
    uint32_t mask;
    uint32_t hItem;
    uint32_t state;
    uint32_t stateMask;
    uint32_t dwText;
    int32_t cchTextMax;
    int32_t iImage;
    int32_t iSelectedImage;
    int32_t cChildren;
    uint32_t lParam;
} tvitem32_t;

std::tuple<std::wstring,int>
AppMonitor::TV_GetItem32(HWND hTree, DWORD_PTR dwItem)
{
    DWORD_PTR dwRet;
    AppMem item = MemAlloc(sizeof(tvitem32_t));
    AppMem str = NewString(hTree, MAX_TV_TEXT);

    std::wstring text = L"";
    int icon = 0;

    tvitem32_t* tvItem = item.As<tvitem32_t>();
    tvItem->mask = TVIF_HANDLE | TVIF_TEXT | TVIF_IMAGE;
    tvItem->hItem = (uint32_t)dwItem;
    tvItem->cchTextMax = MAX_TV_TEXT;
    tvItem->dwText = (uint32_t)((uintptr_t)str.App());

    MemWriteApp(item);
    AppMessage(hTree, IsWindowUnicode(hTree)
        ? TVM_GETITEMW : TVM_GETITEMA,
        0, (LPARAM)item.App(), &dwRet
    );
    if ((BOOL)dwRet)
    {
        MemReadApp(item);

        text = ReadString(hTree, str);
        icon = tvItem->iImage;

        MemFree(item);
        MemFree(str);
    }
    else
    {
        MemFree(item);
        MemFree(str);
        throw AppException(this, "TVM_GETITEM !dwRet");
    }

    return std::make_tuple(text, icon);
}

typedef struct {
    uint32_t mask;
    uint64_t hItem;
    uint32_t state;
    uint32_t stateMask;
    uint64_t dwText;
    int32_t cchTextMax;
    int32_t iImage;
    int32_t iSelectedImage;
    int32_t cChildren;
    uint64_t lParam;
} tvitem64_t;

std::tuple<std::wstring,int>
AppMonitor::TV_GetItem64(HWND hTree, DWORD_PTR dwItem)
{
    DWORD_PTR dwRet;
    AppMem item = MemAlloc(sizeof(tvitem64_t));
    AppMem str = NewString(hTree, MAX_TV_TEXT);

    std::wstring text = L"";
    int icon = 0;

    tvitem64_t* tvItem = item.As<tvitem64_t>();
    tvItem->mask = TVIF_HANDLE | TVIF_TEXT | TVIF_IMAGE;
    tvItem->hItem = (uint64_t)dwItem;
    tvItem->cchTextMax = MAX_TV_TEXT;
    tvItem->dwText = (uint64_t)((uintptr_t)str.App());

    MemWriteApp(item);
    AppMessage(hTree, IsWindowUnicode(hTree)
        ? TVM_GETITEMW : TVM_GETITEMA,
        0, (LPARAM)item.App(), &dwRet
    );
    if ((BOOL)dwRet)
    {
        MemReadApp(item);

        text = ReadString(hTree, str);
        icon = tvItem->iImage;

        MemFree(item);
        MemFree(str);
    }
    else
    {
        MemFree(item);
        MemFree(str);
        throw AppException(this, "TVM_GETITEM !dwRet");
    }

    return std::make_tuple(text, icon);
}

std::tuple<std::wstring,int>
AppMonitor::TV_GetItem(HWND hTree, DWORD_PTR dwItem)
{
    if (IsWow64()) return TV_GetItem32(hTree, dwItem);
    else return TV_GetItem64(hTree, dwItem);
}
