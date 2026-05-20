#pragma once
#include <atomic>
#include <util2/time.hpp>
#include "sandbox/audio3.hpp"
#include "sandbox/async_key.hpp"
#include "sandbox/whisper_init.hpp"


struct ProgramContext 
{
    using signalCV = std::condition_variable;
    using signalMtx = std::mutex;

    std::thread       m_readThread;
    std::thread       m_processingThread;
    
    signalMtx         m_exitLock;
    std::atomic<bool> m_exit = false;
    signalCV          m_exitSignal;

    std::atomic<bool> m_audioDataReady = false;
    signalCV          m_audioCV;
    signalMtx         m_audioLock;

    std::atomic<bool> m_startStopFlag = false;

    std::atomic<uint32_t> m_produced  = 0;
    std::atomic<uint32_t> m_dropped   = 0;
    std::atomic<uint32_t> m_consumed  = 0;

    AudioManager3 m_audioMan;
    AsyncKeyHook  m_keyListener;
};


void audioCaptureCallbackProducer(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount);
void audioProcessCallbackConsumer();
void audioInferenceWorker();
int  main(int argc, char* argv[]);
