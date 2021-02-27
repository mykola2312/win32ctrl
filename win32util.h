#ifndef __WIN32UTIL_H
#define __WIN32UTIL_H

#include <Windows.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <tuple>

std::wstring TextToWchar(const std::string& text);
std::string WcharToText(const std::wstring& text);
std::wstring AnsiToWchar(const std::string& text, int cp = CP_THREAD_ACP);
std::string WcharToAnsi(const std::wstring& text, int cp = CP_THREAD_ACP);

bool FindProcessWindow(DWORD dwPid, const char* pClass,
    HWND& hWnd, DWORD& uiThread);

bool ReconnectIO(bool OpenNewConsole);

using listdir = std::tuple<
    std::vector<std::wstring>,
    std::vector<std::wstring>
>;

listdir ListDirectory(const std::wstring& path);
uint64_t GetDirectorySize(const std::wstring& path);

#endif
