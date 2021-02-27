#ifndef __WIN32CTRL_H
#define __WIN32CTRL_H

#include <Windows.h>
#include <functional>
#include <exception>
#include <string>
#include <tuple>

class AppMem
{
public:
    AppMem(void* pThisMem = NULL, void* pAppMem = NULL, size_t uLen = 0)
        : m_pThisMem(pThisMem), m_pAppMem(pAppMem), m_uMemLen(uLen)
    {
    }

    inline void Zero()
    {
        if (m_pThisMem)
            ZeroMemory(m_pThisMem, m_uMemLen);
    }

    inline void* This() const { return m_pThisMem; }
    inline void* App() const { return m_pAppMem; }
    inline unsigned Size() const { return m_uMemLen; }

    template<typename T>
    T* As()
    {
        return (T*)This();
    }
private:
    void* m_pThisMem;
    void* m_pAppMem;
    unsigned m_uMemLen;
};

class AppMonitor;

class AppException : public std::exception
{
public:
    AppException(AppMonitor* app,
        const std::string& text);

    virtual AppMonitor* GetApp() const
    {
        return m_pApp;
    }

    virtual DWORD GetError() const
    {
        return m_dwError;
    }

    virtual const char* what() const noexcept;
private:
    AppMonitor* m_pApp;
    std::string m_Text;
    DWORD m_dwError;
};

class AppTimeOut : public AppException
{
public:
    AppTimeOut(AppMonitor* app, UINT uMsg)
        : AppException(app, "[AppTimeOut]"),
        m_uMsg(uMsg)
    {
    }

    virtual UINT GetMessage() const
    {
        return m_uMsg;
    }
private:
    UINT m_uMsg;
};

#define MAX_CMDLINE 1024
#define APP_MSG_TIMEOUT 60*1000
#define MAX_WM_TEXT 4096
#define MAX_TV_TEXT 256

class AppMonitor
{
public:
    static void Init();

    AppMonitor();
    AppMonitor(const std::wstring& exePath);
    virtual ~AppMonitor();

    typedef std::function<bool(AppMonitor*, HWND)> EnumFunc;

    friend BOOL CALLBACK _EnumAppWindows(HWND, LPARAM);
    void EnumAppWindows(EnumFunc func);
    void EnumAppControls(HWND hWnd, EnumFunc func);
    HWND FindAppWindow(const std::string& wndClass);

    virtual HANDLE GetAppProcess() const
    {
        return m_hAppProcess;
    }

    virtual DWORD GetAppProcessId() const
    {
        return m_dwPid;
    }

    virtual bool StartApp(const std::wstring& cmdLine);
    virtual void CloseApp();
    virtual void Terminate();
    virtual bool IsAppRunning() const;
    virtual bool IsWow64() const;
    virtual bool WaitAppIdle(DWORD dwInterval = INFINITE);

    virtual void MonitorSetup();
protected:
    virtual void OnAppWindow(HWND hWnd);
    void SetExePath(const std::wstring& exe);
public:
    virtual std::string GetWindowClass(HWND hWnd);
    virtual HWND GetChild(HWND hWnd, const std::string& wndClass,
        const std::wstring& wndText = L"");

    virtual AppMem MemAlloc(unsigned uLen);
    virtual void MemFree(AppMem& mem);
    virtual void MemWriteApp(const AppMem& mem);
    virtual void MemReadApp(const AppMem& mem);

    virtual bool AppMessage(HWND hWnd, UINT uMsg,
        WPARAM wParam, LPARAM lParam,
        DWORD_PTR* pResult = NULL,
        unsigned uTimeOut = APP_MSG_TIMEOUT);
    virtual void AppPostMessage(HWND hWnd, UINT uMsg,
        WPARAM wParam, LPARAM lParam);

    virtual AppMem NewString(HWND hWnd, DWORD dwChars);
    virtual std::wstring ReadString(HWND hWnd, const AppMem& str);

    virtual std::wstring GetWindowTextStr(HWND hWnd);
    virtual std::wstring GetControlTextStr(HWND hWnd);

    virtual DWORD_PTR TV_GetNextItem(HWND hTree, DWORD dwFlags,
        DWORD_PTR dwItem = 0);
private:
    std::tuple<std::wstring,int>
        TV_GetItem32(HWND hTree, DWORD_PTR dwItem);
    std::tuple<std::wstring,int>
        TV_GetItem64(HWND hTree, DWORD_PTR dwItem);
public:
    virtual std::tuple<std::wstring,int>
        TV_GetItem(HWND hTree, DWORD_PTR dwItem);
private:
    std::wstring m_ExePath;

    HANDLE m_hAppProcess;
    DWORD m_dwPid;
    BOOL m_bWow64;

    struct _app_tmp_s {
        EnumFunc m_Func = NULL;
        std::string m_Class;
        HWND m_hWnd = NULL;
    } m_Tmp;
};

#endif
