#include "win32util.h"
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <memory>

std::wstring TextToWchar(const std::string& text)
{
    size_t uWcharLen = MultiByteToWideChar(CP_UTF8, 0,
        text.c_str(), text.size(), NULL, 0);
    auto pszWchar = std::make_unique<wchar_t[]>(uWcharLen+1);
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), text.size(),
        pszWchar.get(), uWcharLen);
    pszWchar[uWcharLen] = L'\0';
    return std::wstring(pszWchar.get());
}

std::string WcharToText(const std::wstring& text)
{
    size_t uMbsLen = WideCharToMultiByte(CP_UTF8, 0,
        text.c_str(), text.size(), NULL, 0, NULL, NULL);
    auto pszChar = std::make_unique<char[]>(uMbsLen +1);
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), text.size(),
        pszChar.get(), uMbsLen, NULL, NULL);
    pszChar[uMbsLen] = '\0';
    return std::string(pszChar.get());
}

std::wstring AnsiToWchar(const std::string& text, int cp)
{
    size_t uWcharLen = MultiByteToWideChar(cp, 0,
        text.c_str(), text.size(), NULL, 0);
    auto pszWchar = std::make_unique<wchar_t[]>(uWcharLen +1);
    MultiByteToWideChar(cp, 0, text.c_str(), text.size(),
        pszWchar.get(), uWcharLen);
    pszWchar[uWcharLen] = L'\0';
    return std::wstring(pszWchar.get());
}


std::string WcharToAnsi(const std::wstring& text, int cp)
{
    size_t uMbsLen = WideCharToMultiByte(cp, 0,
        text.c_str(), text.size(), NULL, 0, NULL, NULL);
    auto pszChar = std::make_unique<char[]>(uMbsLen +1);
    WideCharToMultiByte(cp, 0, text.c_str(), text.size(),
        pszChar.get(), uMbsLen, NULL, NULL);
    pszChar[uMbsLen] = '\0';
    return std::string(pszChar.get());
}

struct _findwnd_s {
    DWORD m_dwPid;
    const char* m_pClass;
    HWND m_hWnd;
    DWORD m_dwTid;
};

static BOOL CALLBACK _CheckWindow(HWND hWnd, LPARAM lParam)
{
    struct _findwnd_s* wnd = (struct _findwnd_s*)lParam;
    char szClassName[64] = {0};

    DWORD dwPid;
    DWORD dwTid = GetWindowThreadProcessId(hWnd, &dwPid);
    if (dwPid != wnd->m_dwPid)
        return TRUE;

    GetClassNameA(hWnd, szClassName, 64);
    if (!strcmp(szClassName, wnd->m_pClass))
    {
        wnd->m_hWnd = hWnd;
        wnd->m_dwTid = dwTid;
        return FALSE;
    }

    return TRUE;
}

bool FindProcessWindow(DWORD dwPid, const char* pClass,
    HWND& hWnd, DWORD& uiThread)
{
    struct _findwnd_s wnd = { 0 };

    wnd.m_dwPid = dwPid;
    wnd.m_pClass = pClass;
    wnd.m_hWnd = NULL;

    EnumWindows(_CheckWindow, (LPARAM)&wnd);
    if (wnd.m_hWnd)
    {
        hWnd = wnd.m_hWnd;
        uiThread = wnd.m_dwTid;
        return true;
    }

    return false;
}

bool ReconnectIO(bool OpenNewConsole)
{
    int hConHandle;
    intptr_t iStdHandle;
    FILE *fp;
    bool MadeConsole;

    MadeConsole=false;
    if(!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        if(!OpenNewConsole)
            return false;

        MadeConsole=true;
        if(!AllocConsole())
            return false;
    }

    iStdHandle = (intptr_t)GetStdHandle(STD_OUTPUT_HANDLE);
    hConHandle = _open_osfhandle(iStdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "w" );
    *stdout = *fp;
    setvbuf( stdout, NULL, _IONBF, 0 );

    iStdHandle = (intptr_t)GetStdHandle(STD_INPUT_HANDLE);
    hConHandle = _open_osfhandle(iStdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "r" );
    *stdin = *fp;
    setvbuf( stdin, NULL, _IONBF, 0 );

    iStdHandle = (intptr_t)GetStdHandle(STD_ERROR_HANDLE);
    hConHandle = _open_osfhandle(iStdHandle, _O_TEXT);
    fp = _fdopen( hConHandle, "w" );
    *stderr = *fp;
    setvbuf( stderr, NULL, _IONBF, 0 );

    std::ios_base::sync_with_stdio();

    return MadeConsole;
}

listdir ListDirectory(const std::wstring& path)
{
    auto pFind = std::make_unique<WIN32_FIND_DATAW>();
    std::wstring findPath = path + L"\\*";

    std::vector<std::wstring> files, dirs;
    HANDLE hFind = FindFirstFileW(findPath.c_str(), pFind.get());
    if (hFind)
    {
        do {
            if (!wcscmp(pFind->cFileName, L"."))
                continue;
            if (!wcscmp(pFind->cFileName, L".."))
                continue;

            if (pFind->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                dirs.emplace_back(pFind->cFileName);
            else files.emplace_back(pFind->cFileName);
        } while (FindNextFileW(hFind, pFind.get()));
        FindClose(hFind);
    }

    return std::make_tuple(files, dirs);
}

uint64_t GetDirectorySize(const std::wstring& path)
{
    uint64_t uDirSize = 0;
    auto [files, dirs] = ListDirectory(path);

    for (auto& file : files)
    {
        LARGE_INTEGER fileSize = {0};
        HANDLE hFile = CreateFileW((path + L"\\" + file).c_str(),
            GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            continue;
        if (GetFileSizeEx(hFile, &fileSize))
            uDirSize += fileSize.QuadPart;
        CloseHandle(hFile);
    }

    for (auto& dir : dirs)
        uDirSize += GetDirectorySize(path + L"\\" + dir);

    return uDirSize;
}
