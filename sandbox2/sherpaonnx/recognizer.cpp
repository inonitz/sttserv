#include "recognizer.hpp"
#include "sherpa-onnx/c-api/c-api.h"
#include "sherpaonnx_init.hpp"
#include <atomic>
#include <chrono>
#include <cstring>
#include <util2/time.hpp>
#include <util2/C/marker4.h>
#include <util2/C/compiler_warning.h>
#ifdef TRACY_ENABLE
#   pragma WARN("Tracy is still enabled!")
#   include <tracy/Tracy.hpp>
#   include <windows.h>
#   include <cstdlib>


void* operator new(std::size_t count) {
    auto ptr = malloc(count);
    TracyAlloc(ptr, count);
    return ptr;
}
void operator delete(void* ptr) noexcept {
    TracyFree(ptr);
    free(ptr);
}
#endif


static ProgramContext g_ctx;


int main(int argc, char* argv[]) 
{
    __profile({ 
        ZoneScopedN("main"); 
        printf("HIIIIIIIIIIIIIIIIIIII\n");
    })
    __unused constexpr const char* kWhisperSystemPrompt = 
    "You are listening to audio input in a noisy environment.\n"
    "There may be wind, industrial vehicles operating and also man-made noises.\n"
    "You are tasked with deciphering your operators' instructions, who will talk the closest to the microphone";

    bool                         status = true;
    CommandLineArguments         commandLineArguments;
    AudioManager2::cap_plb_pair  availableDevices;
    __unused uint8_t             plbDeviceID = 0xFF;
    __unused uint8_t             capDeviceID = 0xFF;
    std::unique_lock<std::mutex> genericLock;


    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    if(!parse_commandline_args(argc, argv, commandLineArguments)) {
        fprintf(stderr, "Could Not Parse Command-line Arguments\n");
        goto cleanup;
        return 1;
    }

    // Model Loading
    g_ctx.mh_llmFullParams   = new SherpaOnnxFullContextParameters{};
    g_ctx.mh_modelParams     = new SherpaOnnxContextParameters{};
    std::memset(g_ctx.mh_llmFullParams, 0x00, sizeof(*g_ctx.mh_llmFullParams));
    std::memset(g_ctx.mh_modelParams, 0x00, sizeof(*g_ctx.mh_modelParams));

    fprintf(stdout, "Loading Model...\n");
    status = sherpaonnx_init_context(commandLineArguments, g_ctx.mh_modelParams, &g_ctx.mh_llmContext);
    if(!status) {
        fprintf(stderr, "Could Not Initialize Sherpa-Onnx Recognizer & Runtime\n");
        goto cleanup;
        return 1;
    }
    g_ctx.mh_inferenceStream = const_cast<SherpaOnnxStreamHandle>(SherpaOnnxCreateOfflineStream(g_ctx.mh_llmContext));
    fprintf(stdout, "Model Loading Finished\n");


    // Async Key Listener Init 
    g_ctx.m_keyListener.create();
    g_ctx.m_keyListener.bindKey(KeyCode::A, [](__unused KeyCode key) {
        g_ctx.m_startStopFlag = !g_ctx.m_startStopFlag;

        /*
            TODO: When Audio becomes > 10 seconds this isn't called.
            In turn, we don't get accurate data regarding transcription performance,
            we tock() in the inference thread but never tick()
        */
        g_ctx.m_recordTimerFlag = (g_ctx.m_startStopFlag.load() == true);
        if(g_ctx.m_recordTimerFlag == true) {
            g_ctx.mr_startPTT_Till_ReleasePTT.tick();
        }
        g_ctx.m_processingTimerFlag = (g_ctx.m_startStopFlag.load() == false);
        if(g_ctx.m_processingTimerFlag) {
            g_ctx.mr_ReleasePTT_Till_EndOfInfer.tick();
            fputs("Audio Keybind End-End Query Begin\n", stdout);
        }
        fprintf(stdout, "\nAudio Keybind %s\n", g_ctx.m_startStopFlag.load() ? "START" : "STOP ");
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::Escape, [](__unused KeyCode key) {
        g_ctx.m_exit = true;
        g_ctx.m_exitSignal.notify_all();
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::D1, [](__unused KeyCode key) { fprintf(stdout, "\nm_produced is %u\n", g_ctx.m_produced.load()); return; });
    g_ctx.m_keyListener.bindKey(KeyCode::D2, [](__unused KeyCode key) { fprintf(stdout, "\nm_dropped is %u\n", g_ctx.m_dropped.load()); return; });
    g_ctx.m_keyListener.bindKey(KeyCode::D3, [](__unused KeyCode key) { fprintf(stdout, "\nm_consumed is %u\n", g_ctx.m_consumed.load()); return; });
    g_ctx.m_keyListener.bindKey(KeyCode::D4, [](__unused KeyCode key) { fprintf(stdout, "\nm_processed is %u\n", g_ctx.m_processed.load()); return; });


    // Audio Manager Init
    status = g_ctx.m_audioMan.createContext();
    if(!status) {
        fprintf(stderr, "Could Not Initialize Audio Manager\n");
        goto cleanup;
        return 1;
    }


    status = g_ctx.m_audioMan.selectDevicesAndFinalize(
        &g_ctx, 
        audioCaptureCallbackProducer,
        1,
        1,
        CommandLineArguments::kInferenceSampleRate,
        __scast(u8, commandLineArguments.capture_id == -1 ? 0xFF : commandLineArguments.capture_id),
        __scast(u8, commandLineArguments.playback_id == -1 ? 0xFF : commandLineArguments.playback_id)
    );
    if(!status) {
        fprintf(stderr, "Could Not Finalize Audio-Device Initialization\n");
        goto cleanup;
        return 1;
    }


    g_ctx.m_resampleBufferSize = g_ctx.m_audioMan.nativeSampleRate();
    g_ctx.m_resampleBuffer.resize(g_ctx.m_resampleBufferSize);
    g_ctx.m_inferenceBufferSize = 10 * CommandLineArguments::kInferenceSampleRate;
    g_ctx.m_inferenceBuf.reserve(g_ctx.m_inferenceBufferSize);
    g_ctx.m_inferSliceBuf.reserve(g_ctx.m_inferenceBufferSize);


    // Finally, Start All Threads and Audio Manager. Wait until Exit Flag.
    g_ctx.m_readThread       = std::thread(audioProcessCallbackConsumer);
    g_ctx.m_processingThread = std::thread(audioInferenceWorker);
    status = g_ctx.m_audioMan.start();
	if (!status) {
        fprintf(stderr, "Could not start the audio device\n");
		goto cleanup;
		return 1;
	}

    fprintf(stdout, "\nAudio Capture is now available...\n    Press the 'A' Key to Start/Stop\n    Press the 'Escape' Key to Stop the program\n");
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
    g_ctx.m_audioMan.destroy();
    g_ctx.m_keyListener.destroy();

    if(g_ctx.mh_llmFullParams) {
        delete g_ctx.mh_llmFullParams;
    }
    if(g_ctx.mh_modelParams) {
        delete g_ctx.mh_modelParams;
    }
    if(g_ctx.mh_inferenceStream) {
        SherpaOnnxDestroyOfflineStream(g_ctx.mh_inferenceStream);
    }
    sherpaonnx_destroy_context(g_ctx.mh_llmContext);


    __profile({ TracyMessageL("Main End"); })
    return status;
}




void audioCaptureCallbackProducer(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_uint32 framesRemaining = frameCount;
	ma_uint32 framesToWrite   = frameCount;
	ma_uint32 bytesPerFrame   = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);
    const ma_uint8* pInputBytePtr = __rcast(const ma_uint8*, pInput);
    void*           pWriteBuffer  = nullptr;
    const auto      userCtx       = static_cast<ProgramContext*>(pDevice->pUserData);

    __profile({ tracy::SetThreadName("audioCaptureCallbackProducer"); })
    
    if(pOutput && pInput) {
        std::memcpy(pOutput, pInput, frameCount * bytesPerFrame);
    }

    while (framesRemaining > 0) {
        __profile({ ZoneScopedN("AudioRingBuffer"); })
        framesToWrite = framesRemaining;
        ma_pcm_rb_acquire_write(userCtx->m_audioMan.ringBufferHandle(), &framesToWrite, &pWriteBuffer);

        if(framesToWrite == 0) { 
            userCtx->m_dropped.fetch_add(framesRemaining, std::memory_order_relaxed);
            break;
        }

        std::memcpy(pWriteBuffer, pInputBytePtr, framesToWrite * bytesPerFrame);
        ma_pcm_rb_commit_write(userCtx->m_audioMan.ringBufferHandle(), framesToWrite);

        { 
            std::unique_lock<std::mutex> _(userCtx->m_audioLock);
            userCtx->m_audioDataReady = true;
        }
        userCtx->m_audioCV.notify_one();

        framesRemaining -= framesToWrite;
        pInputBytePtr += (framesToWrite * bytesPerFrame);
        userCtx->m_produced.fetch_add(framesToWrite, std::memory_order_relaxed);
        __profile({ FrameMark; })
    }
}

void audioProcessCallbackConsumer()
{
    auto&     kUserCtx        = g_ctx;
    ma_uint32 framesAvailable = 0;
    ma_uint32 framesToRead    = 0;
    ma_uint64 framesToRead64  = 0;
    ma_uint64 framesToWrite64 = 0;
    void* pReadBuffer     = nullptr;

    __profile({ tracy::SetThreadName("audioProcessCallbackConsumer"); })
    
    while(!kUserCtx.m_exit) 
    {
        {
            __profile({ ZoneScopedN("Consumer_Wait"); })
            std::unique_lock<std::mutex> lock(kUserCtx.m_audioLock);
            kUserCtx.m_audioCV.wait(lock, [](){
                return kUserCtx.m_exit.load() || kUserCtx.m_audioDataReady.load();
            });
            kUserCtx.m_audioDataReady = false; 
        }

        if(kUserCtx.m_exit.load()) break;

        {
            __profile({ ZoneScopedN("Consumer_Work_Resample"); })
            framesAvailable = ma_pcm_rb_available_read(kUserCtx.m_audioMan.ringBufferHandle());
            
            while(framesAvailable) {
                framesToRead = framesAvailable;
                ma_pcm_rb_acquire_read(kUserCtx.m_audioMan.ringBufferHandle(), &framesToRead, &pReadBuffer);

                if(kUserCtx.m_startStopFlag) {
                    framesToRead64 = framesToRead;
                    framesToWrite64 = kUserCtx.m_resampleBufferSize;
                    ma_resampler_process_pcm_frames(
                        kUserCtx.m_audioMan.resamplerHandle(), 
                        pReadBuffer,
                        &framesToRead64,
                        kUserCtx.m_resampleBuffer.data(),
                        &framesToWrite64
                    );

                    kUserCtx.m_inferenceBuf.insert(kUserCtx.m_inferenceBuf.end(), 
                        kUserCtx.m_resampleBuffer.data(),
                        kUserCtx.m_resampleBuffer.data() + framesToWrite64
                    );

                    if(kUserCtx.m_inferenceBuf.size() >= kUserCtx.m_inferenceBufferSize) 
                    {
                        std::lock_guard<std::mutex> lock(kUserCtx.m_inferenceLock);
                        if(!kUserCtx.m_inferenceBufReady) { 
                            fprintf(stdout, "Swapping buffers [Overflow, Audio > 10s]\n");
                            std::swap(kUserCtx.m_inferSliceBuf, kUserCtx.m_inferenceBuf);
                            kUserCtx.m_inferenceBuf.clear();
                            kUserCtx.m_inferenceBufReady = true;
                            kUserCtx.m_inferenceCV.notify_one();
                        } else {
                            fprintf(stdout, "Still Writing to inference buffer\n");
                        }
                    }
                } else { 
                    std::lock_guard<std::mutex> lock(kUserCtx.m_inferenceLock);
                    kUserCtx.m_inferenceBufReady = kUserCtx.m_inferenceBuf.size() > 0;
                    if(kUserCtx.m_inferenceBufReady) {
                        fprintf(stdout, "Swapped buffers [Data Available/Early Release]\n");
                        std::swap(kUserCtx.m_inferSliceBuf, kUserCtx.m_inferenceBuf);
                        kUserCtx.m_inferenceBuf.clear();
                        kUserCtx.m_inferenceCV.notify_one();
                    }
                }

                ma_pcm_rb_commit_read(kUserCtx.m_audioMan.ringBufferHandle(), framesToRead);
                framesAvailable -= framesToRead;
                kUserCtx.m_consumed.fetch_add(framesToRead, std::memory_order_relaxed);
                __profile({ FrameMark; })
            }
        }
    }
}


void audioInferenceWorker()
{
    __profile({ tracy::SetThreadName("audioInferenceWorker"); })


    while(!g_ctx.m_exit) 
    {
        {
            __profile({ ZoneScopedN("Inference_Wait"); })
            std::unique_lock<std::mutex> lock(g_ctx.m_inferenceLock);
            g_ctx.m_inferenceCV.wait(lock, [](){
                return g_ctx.m_exit.load() || g_ctx.m_inferenceBufReady.load();
            });
        }

        if(g_ctx.m_exit.load()) {
            break;
        }

        {
            __profile({ ZoneScopedN("Inference_Whisper_Work"); })

            if(g_ctx.m_recordTimerFlag == false) {
                g_ctx.mr_startPTT_Till_ReleasePTT.tock();
                const auto elapsedTimeNs = g_ctx.mr_startPTT_Till_ReleasePTT.duration().count();
                fprintf(stdout, "End-End Transcription Took %llu ns (%llu Microseconds) (%llu Milliseconds)\n",
                    __scast(unsigned long long, elapsedTimeNs),
                    __scast(unsigned long long, (elapsedTimeNs+999) / 1000),
                    __scast(unsigned long long, (elapsedTimeNs+1000000-1) / 1000000)
                );
                // g_ctx.mh_llmFullParams->duration_ms = __scast(i32, (elapsedTimeNs+1000000-1) / 1000000);
            }


            g_ctx.mr_Start_Till_EndOfInfer.tick();
            
            g_ctx.mh_inferenceStream = const_cast<SherpaOnnxStreamHandle>(
                SherpaOnnxCreateOfflineStream(g_ctx.mh_llmContext)
            );
            SherpaOnnxAcceptWaveformOffline(g_ctx.mh_inferenceStream, 
                CommandLineArguments::kInferenceSampleRate,
                g_ctx.m_inferSliceBuf.data(), 
                __scast(i32, g_ctx.m_inferSliceBuf.size())
            );
            SherpaOnnxDecodeOfflineStream(g_ctx.mh_llmContext, g_ctx.mh_inferenceStream);
            g_ctx.mh_inferenceResult = const_cast<SherpaOnnxInferenceResultHandle>(
                SherpaOnnxGetOfflineStreamResult(g_ctx.mh_inferenceStream)
            );

            g_ctx.mr_Start_Till_EndOfInfer.tock();


            // Measurements
            float       elapsedTimeNs    = __scast(f32, g_ctx.mr_Start_Till_EndOfInfer.duration().count());
            const float kAudioDurationNs = __scast(f32, g_ctx.mr_startPTT_Till_ReleasePTT.duration().count());
            const float kRealTimeFactor = elapsedTimeNs / kAudioDurationNs;
            if(g_ctx.mh_inferenceResult) {
                fprintf(stdout, "[InferenceThread] Transcription: %s\n", 
                    g_ctx.mh_inferenceResult->text
                );
            } else {
                fputs("[InferenceThread] Failed to transcribe Audio\n", stdout);
            }
            fprintf(stdout, "[InferenceThread] (%.3f Seconds Of Audio) Transcribed in %3.3fms [RTF=%3.3f]\n", 
                1e-9 * kAudioDurationNs, 
                1e-6 * elapsedTimeNs,
                kRealTimeFactor
            );


            if(g_ctx.m_micToFile.open(
                "output_mic" + std::to_string(g_ctx.m_processed.load()) + std::to_string(elapsedTimeNs) + ".wav", 
                g_ctx.m_audioMan.channelCount(), 
                g_ctx.m_audioMan.resampleRate(),
                g_ctx.m_audioMan.outputFormat()
            )) {
                g_ctx.m_micToFile.write(g_ctx.m_inferSliceBuf.data(), g_ctx.m_inferSliceBuf.size());
                g_ctx.m_micToFile.close();
            }

            {
                std::unique_lock<std::mutex> lock(g_ctx.m_inferenceLock);
                g_ctx.m_inferSliceBuf.clear();
                g_ctx.m_inferenceBufReady = false; 
            }
            
            
            g_ctx.m_processed.fetch_add(g_ctx.mh_inferenceResult != nullptr, std::memory_order_relaxed);
            if(g_ctx.m_processingTimerFlag) {
                g_ctx.mr_ReleasePTT_Till_EndOfInfer.tock();
                elapsedTimeNs = __scast(f32, g_ctx.mr_ReleasePTT_Till_EndOfInfer.duration().count());
                fputs("[InferenceThread] Audio Keybind End-End Query End\n", stdout);
                fprintf(stdout, "[InferenceThread] End-End Query Took %llu ns (%llu Microseconds) (%llu Milliseconds)\n",
                    __scast(unsigned long long, elapsedTimeNs), 
                    __scast(unsigned long long, (elapsedTimeNs+999) / 1000), 
                    __scast(unsigned long long, (elapsedTimeNs+1000000-1) / 1000000)
                );
            }
            __profile({ FrameMark; })


            SherpaOnnxDestroyOfflineRecognizerResult(g_ctx.mh_inferenceResult);
            SherpaOnnxDestroyOfflineStream(g_ctx.mh_inferenceStream);
            g_ctx.mh_inferenceResult = nullptr;
            g_ctx.mh_inferenceStream = nullptr;
            // parakeet_print_timings(g_ctx.mh_llmContext);
            // parakeet_reset_timings(g_ctx.mh_llmContext);
        }
    }
}
