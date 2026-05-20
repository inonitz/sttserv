#include <atomic>
#include <cstring>
#include <filesystem>
#include <utility>
#include <whisper.h>
#include "sandbox/key_codes.hpp"
#include "sandbox/whisper_init.hpp"
#include "sandbox/audio2.hpp"
#include "sandbox/async_key.hpp"
#include "util2/C/debugbreak.h"


struct ProgramContext 
{
    using signalCV = std::condition_variable;
    using signalMtx = std::mutex;


    std::thread m_readThread;
    std::thread m_processingThread;
    
    /* 
        Keylistener will set the exit signal to true given a certain key 
        The main thread will be sleeping until then.
    */
    signalMtx         m_exitLock;
    std::atomic<bool> m_exit = false;
    signalCV          m_exitSignal;

    /* Transaction between user & program for device capture/playback selection */
    signalMtx            m_selectDeviceLock;
    std::atomic<uint8_t> m_devicesSelected = 0;
    signalCV             m_deviceSelectionCV;

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
    std::atomic<uint32_t> m_dropped   = 0;
    std::atomic<uint32_t> m_consumed  = 0;
    std::atomic<uint32_t> m_processed = 0;


    AudioManager2    m_audioMan;
    uint32_t         m_resampleBufferSize;
    uint32_t         m_inferenceBufferSize;
    std::vector<f32> m_resampleBuffer;
    std::vector<f32> m_inferenceBuf;
    std::vector<f32> m_inferSliceBuf;

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
    WhisperParameters commandLineArguments;

    AudioManager2::capture_playback_pair availableDevices;
    uint8_t plbDeviceID = 0xFF;
    uint8_t capDeviceID = 0xFF;
    std::unique_lock<std::mutex> genericLock;


    /* Initialize Whisper First. */
    if(!parse_args(argc, argv, commandLineArguments)) {
        fprintf(stderr, "Could Not Parse Command-line Arguments\n");
        goto cleanup;
        return 1;
    }
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
    fprintf(stdout, "Translation to english enabled? %s\nModel is even multiligunal? %u\n", 
        commandLineArguments.mb_translateEnglish ? "YES" : "NO",
        whisper_is_multilingual(g_ctx.m_llmContextHandle)
    );
    g_ctx.m_llmFullParams.translate = commandLineArguments.mb_translateEnglish;
    g_ctx.m_llmFullParams.language  = commandLineArguments.m_lang.c_str();


    // Initialize Async Keylog Second
    g_ctx.m_keyListener.create();
    // g_ctx.m_keyListener.bindKey(KeyCode::Any, [](KeyCode key) {
    //     fprintf(stdout, "\nPressed Key %s\n", keyCodeToString(key));
    //     return;
    // }); 
    g_ctx.m_keyListener.bindKey(KeyCode::A, [](KeyCode key) {
        g_ctx.m_startStopFlag = !g_ctx.m_startStopFlag;
        fprintf(stdout, "\nAudio Keybind %s\n", g_ctx.m_startStopFlag.load() ? "START" : "STOP ");
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::Escape, [](KeyCode key) {
        g_ctx.m_exit = true;
        g_ctx.m_exitSignal.notify_all();
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::D1, [](KeyCode key) {
        fprintf(stdout, "\nm_produced is %u\n", g_ctx.m_produced.load()); return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::D2, [](KeyCode key) {
        fprintf(stdout, "\nm_dropped is %u\n", g_ctx.m_dropped.load()); return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::D3, [](KeyCode key) {
        fprintf(stdout, "\nm_consumed is %u\n", g_ctx.m_consumed.load()); return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::D4, [](KeyCode key) {
        fprintf(stdout, "\nm_processed is %u\n", g_ctx.m_processed.load()); return;
    });
    





    // Initialize Audio manager Lastly
    status = g_ctx.m_audioMan.createContext();
    if(!status) {
        fprintf(stderr, "Could Not Initialize Audio Manager\n");
        goto cleanup;
        return 1;
    }

    // status = g_ctx.m_audioMan.getDeviceList(availableDevices);
    // if(!status) {
    //     fprintf(stderr, "Could Not Initialize Audio Managers' Device List\n");
    //     goto cleanup;
    //     return 1;
    // }

    status = g_ctx.m_audioMan.selectDevicesAndFinalize(
        &g_ctx, 
        audioCaptureCallbackProducer,
        1,
        1,
        WHISPER_SAMPLE_RATE,
        commandLineArguments.capture_id == -1 ? 0xFF : commandLineArguments.capture_id,
        commandLineArguments.playback_id == -1 ? 0xFF : commandLineArguments.playback_id
    );
    if(!status) {
        fprintf(stderr, "Could Not Finalize Audio-Device Initialization\n");
        goto cleanup;
        return 1;
    }


    /* Reserve 10 seconds of data */
    g_ctx.m_resampleBufferSize = g_ctx.m_audioMan.nativeSampleRate();
    g_ctx.m_resampleBuffer.resize(g_ctx.m_resampleBufferSize);

    g_ctx.m_inferenceBufferSize = 10 * WHISPER_SAMPLE_RATE;
    g_ctx.m_inferenceBuf.reserve(g_ctx.m_inferenceBufferSize);
    g_ctx.m_inferSliceBuf.reserve(g_ctx.m_inferenceBufferSize);

    // read thread should init first s.t it waits for data.
    g_ctx.m_readThread       = std::thread(audioProcessCallbackConsumer);
    g_ctx.m_processingThread = std::thread(audioInferenceWorker);
    status = g_ctx.m_audioMan.start();
	if (!status) {
        fprintf(stderr, "Could not start the audio device\n");
		goto cleanup;
		return 1;
	}




    fprintf(stdout, "\
Audio Capture is now available...\n\
    Press the    'A'   Key to Start/Stop\n\
    Press the 'Escape' Key to Stop the program"
    );
    fflush(stdout);
    genericLock = std::unique_lock<std::mutex>(g_ctx.m_exitLock);
    g_ctx.m_exitSignal.wait(genericLock, []() {
        return g_ctx.m_exit == true;
    });
    genericLock.unlock();




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
    void*     pWriteBuffer    = nullptr;
    const auto userCtx        = static_cast<ProgramContext*>(pDevice->pUserData);


    if(pOutput && pInput) {
        std::memcpy(pOutput, pInput, frameCount * bytesPerFrame);
    }


    while (framesRemaining > 0) {
        framesToWrite = framesRemaining;
        ma_pcm_rb_acquire_write(userCtx->m_audioMan.ringBufferHandle(), &framesToWrite, &pWriteBuffer);


        if(framesToWrite == 0) { /* If there is no space to write to the buffer then it is full */
            userCtx->m_dropped.fetch_add(framesRemaining, std::memory_order_relaxed);
            break;
        }

        std::memcpy(pWriteBuffer, pInputBytePtr, framesToWrite * bytesPerFrame);
        ma_pcm_rb_commit_write(userCtx->m_audioMan.ringBufferHandle(), framesToWrite);

        { /* Lock guard will go out of scope and release the mutex */
            // std::lock_guard<std::mutex> lock(userCtx->m_audioLock);
            userCtx->m_audioDataReady = true;
        }
        userCtx->m_audioCV.notify_one();

        framesRemaining -= framesToWrite;
        pInputBytePtr += (framesToWrite * bytesPerFrame);
        userCtx->m_produced.fetch_add(framesToWrite, std::memory_order_relaxed);
    }
    return;
}


void audioProcessCallbackConsumer()
{
    auto&     kUserCtx        = g_ctx;
    ma_uint32 framesAvailable = 0;
    ma_uint32 framesToRead    = 0;

    ma_uint64 framesToRead64  = 0;
    ma_uint64 framesToWrite64 = 0;
    void*     pReadBuffer     = nullptr;


    while(!kUserCtx.m_exit) 
    {
        /* Wait Until there is data to consume, i.e until cv is true */
        std::unique_lock<std::mutex> lock(kUserCtx.m_audioLock);
        kUserCtx.m_audioCV.wait(lock, [](){
            return kUserCtx.m_exit.load() || kUserCtx.m_audioDataReady.load();
        });

        kUserCtx.m_audioDataReady = false; /* Let producer continue */
        lock.unlock();

        if(kUserCtx.m_exit.load()) { /* The audio data may be ready, but we might need to exit early */
            break;
        }

        framesAvailable = ma_pcm_rb_available_read(kUserCtx.m_audioMan.ringBufferHandle());
        while(framesAvailable) {
            // 1. Acquire the read pointer
            framesToRead = framesAvailable;
            ma_pcm_rb_acquire_read(kUserCtx.m_audioMan.ringBufferHandle(), &framesToRead, &pReadBuffer);

            /* If there is no need for inference we shouldn't be reading in the first. */
            if(kUserCtx.m_startStopFlag) {
                // util2_debugbreak();
                framesToRead64 = framesToRead;
                framesToWrite64 = kUserCtx.m_resampleBufferSize;
                ma_resampler_process_pcm_frames(
                    kUserCtx.m_audioMan.resamplerHandle(), 
                    pReadBuffer,
                    &framesToRead64,
                    kUserCtx.m_resampleBuffer.data(),
                    &framesToWrite64
                );

                // 2. Write the new data to the inference buffer
                // kUserCtx.m_inferenceBuf.insert(kUserCtx.m_inferenceBuf.end(), 
                //     kUserCtx.m_resampleBuffer.begin(),
                //     kUserCtx.m_resampleBuffer.begin() + framesToWrite64 * ma_get_bytes_per_frame(ma_format_f32, 1)
                // );
                kUserCtx.m_inferenceBuf.insert(kUserCtx.m_inferenceBuf.end(), 
                    kUserCtx.m_resampleBuffer.data(),
                    kUserCtx.m_resampleBuffer.data() + framesToWrite64
                );
                // kUserCtx.m_inferenceBuf.insert(
                //     kUserCtx.m_inferenceBuf.end(), 
                //     reinterpret_cast<f32*>(pReadBuffer),
                //     reinterpret_cast<f32*>(pReadBuffer) + framesToRead
                // );

                // 3. We have enough data? notify the Worker Thread so it can start working
                if(kUserCtx.m_inferenceBuf.size() >= kUserCtx.m_inferenceBufferSize) 
                {
                    std::lock_guard<std::mutex> lock(kUserCtx.m_inferenceLock);
                    if(!kUserCtx.m_inferenceBufReady) { /* We can write to the buffer(?) */
                        printf("Swapping buffers\n");

                        std::swap(kUserCtx.m_inferSliceBuf, kUserCtx.m_inferenceBuf);

                        kUserCtx.m_inferenceBuf.clear();

                        kUserCtx.m_inferenceBufReady = true;
                        kUserCtx.m_inferenceCV.notify_one();
                    
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
                std::lock_guard<std::mutex> lock(kUserCtx.m_inferenceLock);
                kUserCtx.m_inferenceBufReady = kUserCtx.m_inferenceBuf.size() > 0;
                if(kUserCtx.m_inferenceBufReady) {
                    fprintf(stdout, "Swapped buffers!\n");
                    std::swap(kUserCtx.m_inferSliceBuf, kUserCtx.m_inferenceBuf);
                    kUserCtx.m_inferenceBuf.clear();
                    kUserCtx.m_inferenceCV.notify_one();
                }
            }


            // 3. Commit the read so the producer can reuse that space
            ma_pcm_rb_commit_read(kUserCtx.m_audioMan.ringBufferHandle(), framesToRead);
            framesAvailable -= framesToRead;
            kUserCtx.m_consumed.fetch_add(framesToRead, std::memory_order_relaxed);
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