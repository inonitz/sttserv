// #include "sandbox/async_input.hpp"
#include <util2/C/debugbreak.h>
#include <windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>


inline BOOL GetErrorMessage(DWORD dwErrorCode, LPTSTR pBuffer, DWORD cchBufferLength)
{
    if (cchBufferLength == 0) {
        return FALSE;
    }
    DWORD cchMsg = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,  /* (not used with FORMAT_MESSAGE_FROM_SYSTEM) */
        dwErrorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        pBuffer,
        cchBufferLength,
        NULL
    );
    return (cchMsg > 0);
}


inline void PrintLastError(const char* format, ...) {
    va_list arg_list;
    va_start(arg_list, format);
    vfprintf(stderr, format, arg_list);
    va_end(arg_list);


    static TCHAR errBuf[1024] = {0};
    BOOL  status = false;
    DWORD errCode = GetLastError();
    status = GetErrorMessage(errCode, errBuf, 1024);
    fprintf(stderr, "    Optional System Message (Windows errCode=%lu): %s\n", 
        (unsigned long)errCode,
        status > 0 ? errBuf : "None"
    );
    return;
}


struct KeyMessage {
    UINT16 m_wParam;
    DWORD  m_virtualKey;
};


HHOOK g_keyHook = nullptr;
std::queue<KeyMessage>  g_keyQueue;
std::mutex              g_processInputMtx;
std::atomic<bool>       gb_exit       = false;
std::condition_variable gcv_signal;
std::atomic<UINT>       g_producerID{0xFFFF};
std::atomic<UINT>       g_consumerID{0xFFFF};



LRESULT CALLBACK KeyboardCallback(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        {
            std::lock_guard<std::mutex> lock(g_processInputMtx);
            
            KBDLLHOOKSTRUCT* kbStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam); 
            g_keyQueue.push(KeyMessage{ (UINT16)wParam, kbStruct->vkCode });
            
            printf("Callback Key Input on thread %lu\n", GetCurrentThreadId());
        }
        gcv_signal.notify_one();
    }
    return CallNextHookEx(g_keyHook, nCode, wParam, lParam);
}


void HookingProducerThread() {
    g_keyHook = SetWindowsHookEx(
        WH_KEYBOARD_LL, 
        KeyboardCallback,
        GetModuleHandle(NULL),
        0
        /* needs to be setup/called from another thread */
    );
    if(g_keyHook == nullptr) {
        PrintLastError("Error Hooking Low-level Keyboard hook\n");
        gb_exit = true;
        return;
    }
    g_producerID = GetCurrentThreadId();


    

    printf("Listening for messages on thread %u\n", g_producerID.load());
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    if(!UnhookWindowsHookEx(g_keyHook)) {
        fprintf(stderr, "Error Unhooking Low-level Keyboard hook\n");
    } else {
        fprintf(stdout, "Unhooked Low-level Keyboard hook\n");
    }
    return;
}


void ConsumerThread()
{
    g_consumerID = GetCurrentThreadId();
    printf("Consuming Input on thread %u!\n", g_consumerID.load());


    KeyMessage km;
    while(!gb_exit) {
        std::unique_lock<std::mutex> lock(g_processInputMtx);
        gcv_signal.wait(lock, []() -> bool {
            return !g_keyQueue.empty() || gb_exit.load() == true;
        });

        if(gb_exit.load() && g_keyQueue.empty()) {
            break;
        }

        km = g_keyQueue.front();
        g_keyQueue.pop();

        lock.unlock();

        printf("Got Input From Message Queue -> %u, %lu\n", km.m_wParam, km.m_virtualKey);
        gb_exit = (km.m_virtualKey == VK_ESCAPE);
        // util2_debugbreakif(gb_exit == true);
    }


    return;
}



int main()
{
    std::thread listener = std::thread{ HookingProducerThread };
    std::thread consumer = std::thread{ ConsumerThread };

    consumer.join();
    PostThreadMessage(g_producerID.load(), WM_QUIT, 0, 0);
    listener.join();
    return 0;
}