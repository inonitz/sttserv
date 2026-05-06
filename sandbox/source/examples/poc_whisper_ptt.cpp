#include <cstring>
#include <filesystem>
#include <chrono>
#include <whisper.h>
#include "sandbox/whisper_init.hpp"
#include "sandbox/audio.hpp"
#include "sandbox/async_key.hpp"


struct ProgramContext 
{
    using signalCV = std::condition_variable;
    using signalMtx = std::mutex;

    static constexpr uint32_t kChannelCount      = 1;
    static constexpr uint32_t kDeviceSampleRate  = WHISPER_SAMPLE_RATE;

    std::thread       m_readThread;
    std::thread       m_processingThread;
    
    /* 
        Keylistener will set the exit signal to true given a certain key 
        The main thread will be sleeping until then.
    */
    signalMtx         m_exitLock;
    std::atomic<bool> m_exit = false;
    signalCV          m_exitSignal;

    /* Transactions between the Producer/Consumer threads */
    std::atomic<bool>        m_audioDataReady = false;
    signalCV                 m_audioCV;
    signalMtx                m_audioLock;
    /* Transactions between the Consumer/Worker threads */
    std::atomic<bool>        m_inferenceBufReady = false;
    signalCV                 m_inferenceCV;
    signalMtx                m_inferenceLock;

    /* will be controlled by the keylistener */
    std::atomic<bool>        m_startStopFlag = false;

    /* Counters for each thread to see if there is any disparity between capturing/processing */
    std::atomic<uint32_t> m_produced  = 0;
    std::atomic<uint32_t> m_consumed  = 0;
    std::atomic<uint32_t> m_processed = 0;


    AudioManager      m_audioMan;
    uint32_t          m_inferenceBufferSize;
    std::vector<f32>  m_inferenceBuf;
    std::vector<f32>  m_inferSliceBuf;

    AsyncKeyHook      m_keyListener;

    WhisperFullContextParameters m_llmFullParams;
    WhisperContextParameters     m_llmInitialContextParameters;
    WhisperContextHandle         m_llmContextHandle;
};


void audioCaptureCallbackProducer(
    ma_device*  pDevice, 
    void*       pOutput, 
    const void* pInput, 
    ma_uint32   frameCount
);
void audioProcessCallbackConsumer();
void audioInferenceWorker();




static ProgramContext g_ctx;


int main(int argc, char* argv[]) 
{
    printf("CWD IS %s\n", std::filesystem::current_path().c_str());
    bool        status = true;
    static char inputBuf[100];
    WhisperParameters commandLineArguments;
    std::unique_lock<std::mutex>* lock_ptr = nullptr;

    /* Initialize Whisper First. */
    parse_args(argc, argv, commandLineArguments);
    status = init_context(
        commandLineArguments, 
        g_ctx.m_llmInitialContextParameters, 
        &g_ctx.m_llmContextHandle
    );
    if (!status) {
        fprintf(stderr, "Could Not Initialize Whisper\n");
        goto cleanup;
        return 1;
    }


    g_ctx.m_llmFullParams = whisper_full_default_params(whisper_sampling_strategy::WHISPER_SAMPLING_GREEDY);
    g_ctx.m_llmFullParams.n_threads      = commandLineArguments.m_numThreads;
    g_ctx.m_llmFullParams.print_progress = false;
    g_ctx.m_llmFullParams.language       = commandLineArguments.m_lang.c_str();

    // Initialize Async Keylog Second
    g_ctx.m_keyListener.create();
    g_ctx.m_keyListener.bindKey(KeyCode::Any, [](KeyCode key) {
        fprintf(stdout, "Pressed Key %s'n", keyCodeToString(key));
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::A, [](KeyCode key) {
        g_ctx.m_startStopFlag = !g_ctx.m_startStopFlag;
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::Escape, [](KeyCode key) {
        g_ctx.m_exit = true;
        g_ctx.m_exitSignal.notify_all();
        return;
    });


    // Initialize Audio manager Lastly
    status = g_ctx.m_audioMan.create(
        ProgramContext::kChannelCount, 
        ProgramContext::kDeviceSampleRate,
        &g_ctx,
        audioCaptureCallbackProducer
    );
    if(!status) {
        fprintf(stderr, "Could Not Initialize Audio Manager\n");
        goto cleanup;
        return 1;
    }


    /* Reserve 10 seconds of data */
    g_ctx.m_inferenceBufferSize = 10 * ProgramContext::kDeviceSampleRate;
    g_ctx.m_inferenceBuf.reserve(3 * g_ctx.m_inferenceBufferSize / 2);
    g_ctx.m_inferSliceBuf.reserve(3 * g_ctx.m_inferenceBufferSize / 2);

    // read thread should init first s.t it waits for data.
    g_ctx.m_readThread       = std::thread(audioProcessCallbackConsumer);
    g_ctx.m_processingThread = std::thread(audioInferenceWorker);
    status = g_ctx.m_audioMan.start();
	if (!status) {
        fprintf(stderr, "Could not start the audio device\n");
		goto cleanup;
		return 1;
	}

    
    // fprintf(stdout, "Capturing Audio... Press any key to stop\n");
    // std::fgets(inputBuf, 100, stdin);
    // fprintf(stdout, "Finished");
    fprintf(stdout, "\
Audio Capture is now available...\n\
    Press the    'A'   Key to Start/Stop\n\
    Press the 'Escape' Key to Stop the program"
    );

    lock_ptr = new std::unique_lock<std::mutex>(g_ctx.m_exitLock);
    g_ctx.m_exitSignal.wait(*lock_ptr, []() {
        return g_ctx.m_exit == true;
    });
    lock_ptr->unlock();
    delete lock_ptr;

    status = g_ctx.m_audioMan.stop();
	if (!status) {
        fprintf(stderr, "Could not stop the audio device\n");
		goto cleanup;
		return 1;
	}


    g_ctx.m_exit = true;
    g_ctx.m_audioCV.notify_all();
    g_ctx.m_readThread.join();
    g_ctx.m_inferenceCV.notify_all();
    g_ctx.m_processingThread.join();
    goto cleanup;


cleanup:
    g_ctx.m_audioMan.destroy(); /* Handles input stream. should be closed first. */
    g_ctx.m_keyListener.destroy();
    destroy_context(g_ctx.m_llmContextHandle);
    return status;
}




using namespace std::chrono_literals;


void audioCaptureCallbackProducer(
    ma_device*  pDevice, 
    void*       pOutput, 
    const void* pInput, 
    ma_uint32   frameCount
) {
    ma_uint32 framesRemaining = frameCount;
	ma_uint32 framesToWrite   = frameCount;
	ma_uint32 bytesPerFrame   = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);
    ma_uint8* pInputBytePtr   = (uint8_t*)(pInput);
    const auto userCtx        = static_cast<ProgramContext*>(pDevice->pUserData);


    if(pOutput) {
        std::memcpy(pOutput, pInput, frameCount * bytesPerFrame);
    }


    while (framesRemaining > 0) {
        framesToWrite = framesRemaining;
        void* pWriteBuffer;

        ma_pcm_rb_acquire_write(userCtx->m_audioMan.m_ringBuffer, &framesToWrite, &pWriteBuffer);
        if (framesToWrite > 0) {
            std::memcpy(pWriteBuffer, pInputBytePtr, framesToWrite * bytesPerFrame);
            ma_pcm_rb_commit_write(userCtx->m_audioMan.m_ringBuffer, framesToWrite);

            { /* Lock guard will go out of scope and release the mutex */
                std::lock_guard<std::mutex> lock(userCtx->m_audioLock);
                userCtx->m_audioDataReady = true;
            }
            userCtx->m_audioCV.notify_one();

            framesRemaining -= framesToWrite;
            pInputBytePtr += (framesToWrite * bytesPerFrame);
        } else {
            // Buffer is literally full. In a real-time callback, 
            // you must break here to avoid a deadlock
            break; 
        }
        ++userCtx->m_produced;
    }
    return;
}


void audioProcessCallbackConsumer()
{
    ma_uint32 framesAvailable = 0;
    ma_uint32 framesToRead    = 0;
    void*     pReadBuffer = nullptr;


    while(!g_ctx.m_exit) 
    {
        /* Wait Until there is data to consume, i.e until cv is true */
        std::unique_lock<std::mutex> lock(g_ctx.m_audioLock);
        g_ctx.m_audioCV.wait(lock, [](){
            return g_ctx.m_exit.load() || g_ctx.m_audioDataReady.load();
        });

        g_ctx.m_audioDataReady = false; /* Let producer continue */
        lock.unlock();

        if(g_ctx.m_exit.load()) { /* The audio data may be ready, but we might need to exit early */
            break;
        }

        framesAvailable = ma_pcm_rb_available_read(g_ctx.m_audioMan.m_ringBuffer);
        while(framesAvailable) {
            // 1. Acquire the read pointer
            framesToRead = framesAvailable;
            ma_pcm_rb_acquire_read(g_ctx.m_audioMan.m_ringBuffer, &framesToRead, &pReadBuffer);

            /* If there is no need for inference we shouldn't be reading in the first. */
            if(g_ctx.m_startStopFlag) {

                // 2. Write the new data to the inference buffer
                g_ctx.m_inferenceBuf.insert(
                    g_ctx.m_inferenceBuf.end(), 
                    reinterpret_cast<f32*>(pReadBuffer),
                    reinterpret_cast<f32*>(pReadBuffer) + framesToRead
                );

                // 3. We have enough data? notify the Worker Thread so it can start working
                if(g_ctx.m_inferenceBuf.size() >= g_ctx.m_inferenceBufferSize) 
                {
                    std::lock_guard<std::mutex> lock(g_ctx.m_inferenceLock);
                    if(!g_ctx.m_inferenceBufReady) { /* We can write to the buffer(?) */
                        printf("Swapping buffers\n");

                        std::swap(g_ctx.m_inferSliceBuf, g_ctx.m_inferenceBuf);

                        g_ctx.m_inferenceBuf.clear();

                        g_ctx.m_inferenceBufReady = true;
                        g_ctx.m_inferenceCV.notify_one();
                    
                    } else {
                        printf("Writing to inference buffer\n");
                    }
                }
            } else { 
                /* 
                    We Either Just Stopped recording, or we never entered in the first place. 
                    the inferenceBuf size check verifies that condition.
                */
                /* Getting rid of the else keeps the PTT, but is still in N-second intervals. */
                std::lock_guard<std::mutex> lock(g_ctx.m_inferenceLock);
                g_ctx.m_inferenceBufReady = g_ctx.m_inferenceBuf.size() > 0;
                if(g_ctx.m_inferenceBufReady) {
                    g_ctx.m_inferenceCV.notify_one();
                }
            }


            // 3. Commit the read so the producer can reuse that space
            ma_pcm_rb_commit_read(g_ctx.m_audioMan.m_ringBuffer, framesToRead);
            framesAvailable -= framesToRead;
            ++g_ctx.m_consumed;
        }
    }
    /* Should only reach here on program termination */
    return;
}


/*
    Sample lambda to detect voice activity:

    static const auto skf_detectVolume = [&framesToRead, &pReadBuffer]() {
    if(!framesToRead || pReadBuffer == nullptr) {
        return;
    }

    float* pSamples = (float*)pReadBuffer;
    float maxAmp = 0.0f;
    // Find the loudest sample in this chunk
    for (ma_uint32 i = 0; i < framesToRead; ++i) {
        maxAmp = std::max(std::abs(pSamples[i]), maxAmp);
    }
    

    // Print a simple VU meter: [#######       ]
    // int barWidth = 40;
    // int scaled = (int)(maxAmp * barWidth);
    // fprintf(stdout, "\r[audioProcessCallbackConsumer] (%6u Frames, P/C -> %7u/%7u) Peak: [%.*s%*s] %3d%%\n", 
    //     framesToRead, g_ctx.m_produced.load(), g_ctx.m_consumed.load(),
    //     scaled, "########################################", 
    //     barWidth - scaled, "", 
    //     (int)(maxAmp * 100));

    fprintf(stdout, "Peak: %3d%%\n", (int)(maxAmp * 100));
    // fflush(stdout);
    return;
};
*/


void audioInferenceWorker()
{
    bool success = 0;

    while(!g_ctx.m_exit) 
    {
        /* Wait Until there is data to consume, i.e until cv is true */
        std::unique_lock<std::mutex> lock(g_ctx.m_inferenceLock);
        g_ctx.m_inferenceCV.wait(lock, [](){
            return g_ctx.m_exit.load() || g_ctx.m_inferenceBufReady.load();
        });
        if(g_ctx.m_exit.load()) {
            break;
        }


        success = whisper_full(
            g_ctx.m_llmContextHandle, 
            g_ctx.m_llmFullParams, 
            g_ctx.m_inferSliceBuf.data(), 
            g_ctx.m_inferSliceBuf.size()
        );
        if(success != 0) {
            fprintf(stderr, "Failed to process audio\n");
        } else {
            const int n_segments = whisper_full_n_segments(g_ctx.m_llmContextHandle);
            for (int i = 0; i < n_segments; ++i) {
                const char* text = whisper_full_get_segment_text(g_ctx.m_llmContextHandle, i);
                fprintf(stdout, "Transcription [%d]: %s\n", i, text);
            }
        }

        g_ctx.m_inferSliceBuf.clear();
        
        g_ctx.m_inferenceBufReady = false; /* Free audio-consumer thread to give us more data */
        lock.unlock();
        g_ctx.m_processed += success;

    }
    /* Should only reach here on program termination */
    return;
}