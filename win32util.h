#ifndef __WIN32UTIL_H
#define __WIN32UTIL_H

#include <stdint.h>

#ifdef WIN32
#include <Windows.h>
#endif

#ifdef min
#undef min
#endif

#include <algorithm>
#include <string>
#include <vector>
#include <tuple>

enum byteorder {
    BigEndian = 0,
    LittleEndian = 1
};

byteorder ArchByteOrder();
const char* ArchInternalUCS();
bool IsWindowsSystem();
bool IsLinuxSystem();

std::wstring TextToWchar(const std::string& text);
std::string WcharToText(const std::wstring& text);
std::wstring AnsiToWchar(const std::string& text, int cp = 3);
std::string WcharToAnsi(const std::wstring& text, int cp = 3);

std::wstring TermToWchar(const std::string& text);
std::string WcharToTerm(const std::wstring& text);

#ifdef WIN32
bool FindProcessWindow(DWORD dwPid, const char* pClass,
    HWND& hWnd, DWORD& uiThread);

bool ReconnectIO(bool OpenNewConsole);
#else
#define CP_UTF7 65000
#define CP_UTF8 65001

#define _fseeki64 fseeko64
#define _ftelli64 ftello64

FILE* _wfopen(const wchar_t* path, const wchar_t* mode);
int _wfopen_s(FILE** fpFile, const wchar_t* path, const wchar_t* mode);

#include <stdlib.h>
#include <string.h>

#define scanf_s scanf
#define sscanf_s sscanf
#define sprintf_s sprintf
#define ssprintf_s ssprintf

#endif

std::wstring JoinFilePath(const std::wstring& path,
    const std::wstring& name);

using listdir = std::tuple<
    std::vector<std::wstring>,
    std::vector<std::wstring>
>;

listdir ListDirectory(const std::wstring& path);
uint64_t GetDirectorySize(const std::wstring& path);

#endif
