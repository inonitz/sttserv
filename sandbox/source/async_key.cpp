#include <util2/C/platform.h>
#include <cstdint>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <queue>
#if defined(UTIL2_OS_WINDOWS)
#   define WIN32_LEAN_AND_MEAN
#   include <Windows.h>
#   undef WIN32_LEAN_AND_MEAN
#elif defined(UTIL2_OS_LINUX)
#   include <regex>
#   include <string>
#   include <fstream>
#   include <linux/input.h>
#   include <fcntl.h>
#   include <unistd.h>
#endif


/* Type Definitions/Function Definitions */
#if defined(UTIL2_OS_WINDOWS)

typedef DWORD ThreadID;

struct KeyMessage {
    UINT16 m_wParam;
    DWORD  m_virtualKey;
    UINT8  m_padding[2];
};

typedef HHOOK HookHandle;


BOOL GetErrorMessage(DWORD dwErrorCode, LPTSTR pBuffer, DWORD cchBufferLength);
void PrintLastError(const char* format, ...);

LRESULT CALLBACK KeyboardCallback(int nCode, WPARAM wParam, LPARAM lParam);

void HookingProducerThread();
void ConsumerThread();

#elif defined(UTIL2_OS_LINUX)

typedef uint32_t ThreadID;

struct KeyMessage {
    uint16_t m_pressType;
    uint16_t m_keyCode;
    uint8_t  m_padding[4];
};


bool findDeviceProcKeyboardPath(std::string& out);
void HookingProducerThread();
void ConsumerThread();

#endif



/* Global Data */
#if defined(UTIL2_OS_WINDOWS)
HHOOK g_keyHook = nullptr;
#elif defined(UTIL2_OS_LINUX)
#endif
std::queue<KeyMessage>  g_keyQueue;
std::mutex              g_processInputMtx;
std::atomic<bool>       gb_exit       = false;
std::condition_variable gcv_signal;
std::atomic<ThreadID>   g_producerID{0xFFFF};
std::atomic<ThreadID>   g_consumerID{0xFFFF};


int main()
{
    std::thread listener = std::thread{ HookingProducerThread };
    std::thread consumer = std::thread{ ConsumerThread };


#if defined(UTIL2_OS_WINDOWS)
    consumer.join();
    /* If not for this message, the producer thread will not stop waiting for new key inputs. */
    PostThreadMessage(g_producerID.load(), WM_QUIT, 0, 0);
    listener.join();
#elif defined(UTIL2_OS_LINUX)
    consumer.join();
    listener.join();
#endif
    return 0;
}






#if defined(UTIL2_OS_WINDOWS)


BOOL GetErrorMessage(DWORD dwErrorCode, LPTSTR pBuffer, DWORD cchBufferLength)
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


void PrintLastError(const char* format, ...) {
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


    printf("Listening for messages on thread %u\n", __scast(std::uint32_t, g_producerID.load()));
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
    printf("Consuming Input on thread %u!\n", __scast(std::uint32_t, g_consumerID.load()));


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


#elif defined(UTIL2_OS_LINUX)


bool findDeviceProcKeyboardPath(std::string& out) {
    std::ifstream file;
    std::string line, handlers, path;
    bool isKeyboard = false;


    file.open("/proc/bus/input/devices");
    if(!file.is_open()) {
        perror("\
    Error Opening File 'proc/bus/input/devices' - make sure to run with sudo, \
    OR set the correct user permissions for this executable\n"
        );
        return false;
    }


    while (std::getline(file, line)) {        
        isKeyboard = (line.find("EV=120013") != std::string::npos) ? true : isKeyboard;
        if (line.find("Handlers=") != std::string::npos) {
            handlers = line;
        }
        
        // At the end of a device block (empty line)
        if (line.empty()) {
            if (isKeyboard) {
                std::regex re("event[0-9]+");
                std::smatch match;
                if (std::regex_search(handlers, match, re)) {
                    out = "/dev/input/" + match.str();
                    return true;
                }
            }
            isKeyboard = false;
        }
    }
    return false;
}


void HookingProducerThread() {
    constexpr auto onErrorBytesRead = (ssize_t)-1;
    std::string kKeyboardDevicePath;
    int fd = -1;

    if(!findDeviceProcKeyboardPath(kKeyboardDevicePath)) {
        perror("Couldn't find the keyboard event-page\n");
        gb_exit = true;
        gcv_signal.notify_one();
    }
    fd = open(kKeyboardDevicePath.c_str(), O_RDONLY);
    if (fd == -1) {
        perror("Cannot open input device");
        gb_exit = true;
        gcv_signal.notify_one();
    }


    struct input_event ev;
    ssize_t bytesRead = 0;
    while (!gb_exit) {
        bytesRead = read(fd, &ev, sizeof(ev));
        
        if (bytesRead == onErrorBytesRead) {
            break;
        }

        // EV_KEY is a keyboard event, value 1 is 'pressed'
        if (ev.type == EV_KEY && ev.value == 1) {
            fprintf(stdout, "\nKeyboard pressed with key %u\n", ev.code);            
            {
                std::lock_guard<std::mutex> lock(g_processInputMtx);
                g_keyQueue.push(KeyMessage{ev.value == 1, ev.code, {0}});
            }
            gcv_signal.notify_one();
        }
    }


    fprintf(bytesRead == onErrorBytesRead ? stderr : stdout, "Exiting HookingProducerThread\n");
    if(fd != -1) { close(fd); }
    return;
}


void ConsumerThread() {
    KeyMessage km;
    while (!gb_exit) {
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

        printf("Got Input From Message Queue -> %u, %u\n", km.m_pressType, km.m_keyCode);
        if(km.m_keyCode == KEY_ESC) {
            gb_exit = true;
        }
    }

    fprintf(stdout, "Exiting ConsumerThread\n");
}


#endif /* Platorm specific code */