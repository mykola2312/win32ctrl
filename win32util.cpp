#include "win32util.h"

#ifdef WIN32
#include <io.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <memory>
#include <map>

byteorder ArchByteOrder()
{
    uint16_t word = 0x0001;
    auto* src = (char*)&word;
    
    return src[0] ? LittleEndian : BigEndian;
}

const char* ArchInternalUCS()
{
    return ArchByteOrder() == BigEndian ? "UCS-4BE" : "UCS-4LE";
}

#ifdef WIN32

bool IsWindowsSystem()
{
    return true;
}

bool IsLinuxSystem()
{
    return false;
}

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

std::wstring TermToWchar(const std::string& text)
{
    return AnsiToWchar(text, GetConsoleCP());
}

std::string WcharToTerm(const std::wstring& text)
{
    return WcharToAnsi(text, GetConsoleOutputCP());
}

std::wstring JoinFilePath(const std::wstring& path,
    const std::wstring& name)
{
    return path + L"\\" + name;
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
        HANDLE hFile = CreateFileW(JoinFilePath(path, file).c_str(),
            GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
            continue;
        if (GetFileSizeEx(hFile, &fileSize))
            uDirSize += fileSize.QuadPart;
        CloseHandle(hFile);
    }

    for (auto& dir : dirs)
        uDirSize += GetDirectorySize(JoinFilePath(path, dir));

    return uDirSize;
}
#else

#include <iconv.h>
#include <string>

bool IsWindowsSystem()
{
    return false;
}

bool IsLinuxSystem()
{
    return true;
}

std::wstring TextToWchar(const std::string& text)
{
    size_t bufLen = text.size() + 1;
    size_t bufSize = bufLen * sizeof(wchar_t);
    auto szOut = std::make_unique<wchar_t[]>(bufLen);
    
    char* in = (char*)text.c_str(), *out = (char*)szOut.get();
    size_t inLen = bufLen, outLen = bufSize;
    memset(out, '\0', outLen);
    
    iconv_t _cv = iconv_open(ArchInternalUCS(), "UTF-8");
    iconv(_cv, &in, &inLen, &out, &outLen);
    iconv_close(_cv);
    
    size_t strLen = (bufSize - outLen) / sizeof(wchar_t);
    szOut[strLen - 1] = 0;
    return std::wstring(szOut.get(), strLen - 1);
}

std::string WcharToText(const std::wstring& text)
{
    size_t bufSize = (text.size()+1)*sizeof(wchar_t);
    auto szOut = std::make_unique<char[]>(bufSize);
    
    char* in = (char*)text.c_str(), *out = szOut.get();
    size_t inLen = (text.size()+1)*sizeof(wchar_t), outLen = bufSize;
    memset(szOut.get(), '\0', bufSize);
    
    iconv_t cv = iconv_open("UTF-8", ArchInternalUCS());
    iconv(cv, &in, &inLen, &out, &outLen);
    iconv_close(cv);
    
    size_t strLen = bufSize - outLen;
    szOut[strLen - 1] = 0;
    return std::string(szOut.get(), strLen - 1);
}

std::wstring AnsiToWchar(const std::string& text, int cp)
{
    if (cp == 3) cp = 1251;
    
    size_t bufLen = text.size() + 1;
    size_t bufSize = bufLen * sizeof(wchar_t);
    auto szOut = std::make_unique<wchar_t[]>(bufLen);
    
    char* in = (char*)text.c_str(), *out = (char*)szOut.get();
    size_t inLen = bufLen, outLen = bufSize;
    memset(out, '\0', outLen);
    
    iconv_t _cv = iconv_open(ArchInternalUCS(),
        ("CP" + std::to_string(cp)).c_str());
    iconv(_cv, &in, &inLen, &out, &outLen);
    iconv_close(_cv);
    
    size_t strLen = (bufSize - outLen) / sizeof(wchar_t);
    szOut[strLen - 1] = 0;
    return std::wstring(szOut.get(), strLen - 1);
}

std::string WcharToAnsi(const std::wstring& text, int cp)
{
    if (cp == 3) cp = 1251;

    size_t bufSize = (text.size()+1)*sizeof(wchar_t);
    auto szOut = std::make_unique<char[]>(bufSize);
    
    char* in = (char*)text.c_str(), *out = szOut.get();
    size_t inLen = (text.size()+1)*sizeof(wchar_t), outLen = bufSize;
    memset(szOut.get(), '\0', bufSize);
    
    iconv_t _cv = iconv_open(("CP"
        + std::to_string(cp)).c_str(), ArchInternalUCS());
    iconv(_cv, &in, &inLen, &out, &outLen);
    iconv_close(_cv);
    
    size_t strLen = bufSize - outLen;
    szOut[strLen - 1] = 0;
    return std::string(szOut.get(), strLen - 1);
}

std::wstring TermToWchar(const std::string& text)
{
    return TextToWchar(text);
}

std::string WcharToTerm(const std::wstring& text)
{
    return WcharToText(text);
}

#endif

#ifdef WIN32
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
#else

FILE* _wfopen(const wchar_t* path, const wchar_t* mode)
{
    return fopen(WcharToText(path).c_str(), WcharToText(mode).c_str());
}

int _wfopen_s(FILE** fpFile, const wchar_t* path, const wchar_t* mode)
{
    *fpFile = fopen(WcharToText(path).c_str(), WcharToText(mode).c_str());
    return !!(*fpFile);
}

std::wstring JoinFilePath(const std::wstring& path,
    const std::wstring& name)
{
    return path + L"/" + name;
}

#include <dirent.h>

listdir ListDirectory(const std::wstring& path)
{
    std::vector<std::wstring> files, dirs;
    std::string utfPath = WcharToText(path);
    
    DIR* dir;
    struct dirent* ent;
    if (dir = opendir(utfPath.c_str()))
    {
        while (ent = readdir(dir))
        {
            if (!strcmp(ent->d_name, "."))
                continue;
            if (!strcmp(ent->d_name, ".."))
                continue;
            
            std::wstring ucsName = TextToWchar(ent->d_name);
            if (ent->d_type == DT_DIR)
                dirs.push_back(ucsName);
            else if (ent->d_type == DT_REG)
                files.push_back(ucsName);
        }
        closedir(dir);
    }

    return std::make_tuple(files, dirs);
}

uint64_t GetDirectorySize(const std::wstring& path)
{
    uint64_t uDirSize = 0;
    auto [files, dirs] = ListDirectory(path);

    for (auto& file : files)
    {
        int64_t fileSize = {0};
        std::string utfPath = WcharToText(JoinFilePath(path, file));
        FILE* pFile = fopen(utfPath.c_str(), "rb");
        if (!pFile) continue;

        fseek(pFile, 0L, SEEK_END);
        fileSize = (int64_t)ftell(pFile);
        fclose(pFile);

        if (fileSize > 0) uDirSize += fileSize;
    }

    for (auto& dir : dirs)
        uDirSize += GetDirectorySize(JoinFilePath(path, dir));

    return uDirSize;
}

#endif
