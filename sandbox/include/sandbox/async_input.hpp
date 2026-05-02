#ifndef __CAPTURE_KEY_INPUT_FROM_OPERATING_SYSTEM_DEFINITION_HEADER__
#define __CAPTURE_KEY_INPUT_FROM_OPERATING_SYSTEM_DEFINITION_HEADER__
#include <util2/C/platform.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#if defined(UTIL2_OS_WINDOWS)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#   undef WIN32_LEAN_AND_MEAN
#elif defined(UTIL2_OS_LINUX)
#endif


struct AsyncKeyLog
{
public:
    AsyncKeyLog() = default;
    ~AsyncKeyLog() = default;


#if defined(UTIL2_OS_WINDOWS)

    // inline void test() {
    //     GetAsyncKeyState(int vKey)
    // }


    void create()
    {
        void (AsyncKeyLog::*winHook)(void); /* this is definitely gonna cause a segfault */
        m_dispatcher = std::thread{winHook}; /* The thread that set the hook is the one that'll be called */
    }


    void destroy()
    {

    }




#elif defined(UTIL2_OS_LINUX)

#endif /* platform specific */


private:

    void inputListeningThread() {
        /* this is definitely gonna cause a segfault lol */
        LRESULT (AsyncKeyLog::*LowLevelKeyboardProc)(int nCode, WPARAM wParam, LPARAM lParam) = LowLevelKeyboardProc;


        HHOOK tmp = SetWindowsHookEx(
            WH_KEYBOARD_LL, 
            // internal_KeyboardProc,
            LowLevelKeyboardProc,
            NULL,
            GetCurrentThreadId()
            /* needs to be setup/called from another thread */
        ); 
        
        
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }


    LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
        if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
            {
                std::lock_guard<std::mutex> lock(m_processInput);
                m_inputReady = true;
            }
            m_signal.notify_one();
        }
        return CallNextHookEx(hHook, nCode, wParam, lParam);
    }

private:
    std::mutex              m_processInput;
    std::atomic<bool>       m_inputReady = false;
    std::condition_variable m_signal;
    std::thread             m_dispatcher;
};




#endif /* __CAPTURE_KEY_INPUT_FROM_OPERATING_SYSTEM_DEFINITION_HEADER__ */
