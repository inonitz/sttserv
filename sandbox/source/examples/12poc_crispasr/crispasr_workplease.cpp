#include "ctx.hpp"
#include "sandbox/whisper_init.hpp"
#include <cstring>
#ifdef TRACY_ENABLE
#   include <util2/C/compiler_warning.h>
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
    constexpr const char* kWhisperSystemPrompt = 
    "You are listening to audio input in a noisy environment.\n"
    "There may be wind, industrial vehicles operating and also man-made noises.\n"
    "You are tasked with deciphering your operators' instructions, who will talk the closest to the microphone";

    bool                         status = true;
    WhisperParameters            commandLineArguments;
    AudioManager2::cap_plb_pair  availableDevices;
    uint8_t                      plbDeviceID = 0xFF;
    uint8_t                      capDeviceID = 0xFF;
    std::unique_lock<std::mutex> genericLock;


    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);


    if(!whisper_parse_args(argc, argv, commandLineArguments)) {
        fprintf(stderr, "Could Not Parse Command-line Arguments\n");
        goto cleanup;
        return 1;
    }
    // status = whisper_init_context(commandLineArguments, g_ctx.m_llmInitialContextParameters, &g_ctx.m_llmContextHandle);
    // if (!status) {
    //     fprintf(stderr, "Could Not Initialize Whisper\n");
    //     goto cleanup;
    //     return 1;
    // }
    status = crispasr_init_context(
        commandLineArguments, 
        g_ctx.m_llmInitialContextParameters, 
        g_ctx.m_crispasrLLMParams,
        &g_ctx.m_crispASRLLMContextHandle
    );
    if (!status) {
        fprintf(stderr, "Could Not Initialize CrispASR\n");
        goto cleanup;
        return 1;
    }


    g_ctx.m_llmFullParams = whisper_full_default_params(whisper_sampling_strategy::CRISPASR_SAMPLING_GREEDY);
    g_ctx.m_llmFullParams.n_threads            = commandLineArguments.m_numThreads;
    g_ctx.m_llmFullParams.offset_ms            = 0;
    g_ctx.m_llmFullParams.duration_ms          = 1000;
    g_ctx.m_llmFullParams.translate            = false;
    g_ctx.m_llmFullParams.no_timestamps        = true;
    g_ctx.m_llmFullParams.single_segment       = true;
    g_ctx.m_llmFullParams.print_special        = false;
    g_ctx.m_llmFullParams.print_progress       = false;
    g_ctx.m_llmFullParams.print_realtime       = false;
    g_ctx.m_llmFullParams.print_timestamps     = false;
    g_ctx.m_llmFullParams.token_timestamps     = false;
    g_ctx.m_llmFullParams.debug_mode           = false;
    g_ctx.m_llmFullParams.initial_prompt       = kWhisperSystemPrompt;
    g_ctx.m_llmFullParams.carry_initial_prompt = true;
    g_ctx.m_llmFullParams.language             = "en";
    g_ctx.m_llmFullParams.detect_language      = false;
    g_ctx.m_llmFullParams.suppress_blank       = true;
    g_ctx.m_llmFullParams.no_speech_thold      = 0.1f;
    g_ctx.m_llmFullParams.temperature_inc      = 0.0f; /* One-Shot guessing, no second attempts */
    g_ctx.m_llmFullParams.vad                  = false;
    // fprintf(stdout, "Translation to english enabled? %s\nModel is even multiligunal? %u\n", 
    //     commandLineArguments.mb_translateEnglish ? "YES" : "NO",
    //     whisper_is_multilingual(g_ctx.m_llmContextHandle)
    // );
    g_ctx.m_llmFullParams.translate = commandLineArguments.mb_translateEnglish;
    g_ctx.m_llmFullParams.language  = commandLineArguments.m_lang.c_str();


    g_ctx.m_keyListener.create();
    g_ctx.m_keyListener.bindKey(KeyCode::A, [](KeyCode key) {
        g_ctx.m_startStopFlag = !g_ctx.m_startStopFlag;

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
    g_ctx.m_keyListener.bindKey(KeyCode::Escape, [](KeyCode key) {
        g_ctx.m_exit = true;
        g_ctx.m_exitSignal.notify_all();
        return;
    });
    g_ctx.m_keyListener.bindKey(KeyCode::D1, [](KeyCode key) { fprintf(stdout, "\nm_produced is %u\n", g_ctx.m_produced.load()); return; });
    g_ctx.m_keyListener.bindKey(KeyCode::D2, [](KeyCode key) { fprintf(stdout, "\nm_dropped is %u\n", g_ctx.m_dropped.load()); return; });
    g_ctx.m_keyListener.bindKey(KeyCode::D3, [](KeyCode key) { fprintf(stdout, "\nm_consumed is %u\n", g_ctx.m_consumed.load()); return; });
    g_ctx.m_keyListener.bindKey(KeyCode::D4, [](KeyCode key) { fprintf(stdout, "\nm_processed is %u\n", g_ctx.m_processed.load()); return; });

    status = g_ctx.m_audioMan.createContext();
    if(!status) {
        fprintf(stderr, "Could Not Initialize Audio Manager\n");
        goto cleanup;
        return 1;
    }

    status = g_ctx.m_audioMan.selectDevicesAndFinalize(
        &g_ctx, 
        audioCaptureCallbackProducer,
        10,
        1,
        ProgramContext::kInferenceSampleRate,
        commandLineArguments.capture_id == -1 ? 0xFF : commandLineArguments.capture_id,
        commandLineArguments.playback_id == -1 ? 0xFF : commandLineArguments.playback_id
    );
    if(!status) {
        fprintf(stderr, "Could Not Finalize Audio-Device Initialization\n");
        goto cleanup;
        return 1;
    }

    g_ctx.m_resampleBufferSize = g_ctx.m_audioMan.nativeSampleRate();
    g_ctx.m_resampleBuffer.resize(g_ctx.m_resampleBufferSize);
    g_ctx.m_inferenceBufferSize = 10 * ProgramContext::kInferenceSampleRate;
    g_ctx.m_inferenceBuf.reserve(g_ctx.m_inferenceBufferSize);
    g_ctx.m_inferSliceBuf.reserve(g_ctx.m_inferenceBufferSize);

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
    crispasr_destroy_context(g_ctx.m_crispASRLLMContextHandle);
    // whisper_destroy_context(g_ctx.m_llmContextHandle);
    __profile({ TracyMessageL("Main End"); })
    return status;
}




void audioCaptureCallbackProducer(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    ma_uint32 framesRemaining = frameCount;
	ma_uint32 framesToWrite   = frameCount;
	ma_uint32 bytesPerFrame   = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);
    ma_uint8* pInputBytePtr   = (uint8_t*)(pInput);
    void*     pWriteBuffer    = nullptr;
    const auto userCtx        = static_cast<ProgramContext*>(pDevice->pUserData);

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
                    std::lock_guard<std::mutex> lock(kUserCtx.m_inferenceLock);
                    kUserCtx.m_inferenceBufReady = kUserCtx.m_inferenceBuf.size() > 0;
                    if(kUserCtx.m_inferenceBufReady) {
                        fprintf(stdout, "Swapped buffers!\n");
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
    bool success = 0;

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
                g_ctx.m_llmFullParams.duration_ms = (elapsedTimeNs+1000000-1) / 1000000;
            }

            g_ctx.mr_Start_Till_EndOfInfer.tick();
            // success = whisper_full(
            //     g_ctx.m_llmContextHandle, 
            //     g_ctx.m_llmFullParams, 
            //     g_ctx.m_inferSliceBuf.data(), 
            //     g_ctx.m_inferSliceBuf.size()
            // );
            // if(success != 0) {
            //     fprintf(stderr, "Failed to process audio\n");
            // } else {
            //     const int n_segments = whisper_full_n_segments(g_ctx.m_llmContextHandle);
            //     for (int i = 0; i < n_segments; ++i) {
            //         const char* text = whisper_full_get_segment_text(g_ctx.m_llmContextHandle, i);
            //         fprintf(stdout, "Transcription [%d, nospeechprob=%3.3f]: %s\n", 
            //             i, 
            //             whisper_full_get_segment_no_speech_prob(g_ctx.m_llmContextHandle, i),
            //             text
            //         );
            //     }
            // }
            auto* sessionResult = crispasr_session_transcribe_lang(
                g_ctx.m_crispASRLLMContextHandle, 
                g_ctx.m_inferSliceBuf.data(), 
                g_ctx.m_inferSliceBuf.size(),
                g_ctx.m_llmFullParams.language
            );
            if(sessionResult == nullptr) {
                fprintf(stderr, "Failed to process audio\n");
            } else {
                for (int i = 0; i < sessionResult->segments.size(); ++i) {
                    const char* text = sessionResult->segments[i].text.c_str();
                    fputs("------------------------------------------------\n", stdout);
                    fprintf(stdout, "Transcription [%d]: %s\n", i, 
                        sessionResult->segments[i].text.c_str()
                    );
                    for(auto& word : sessionResult->segments[i].words) {
                        fprintf(stdout, "\n  word [prob=%3.3f]: %s", word.p, word.text.c_str());
                    }
                    fputs("------------------------------------------------\n", stdout);
                }
            }
            g_ctx.mr_Start_Till_EndOfInfer.tock();
            
            
            const auto elapsedTimeNs = g_ctx.mr_Start_Till_EndOfInfer.duration().count();
            fprintf(stdout, "Whisper GPU Time %llu ns (%llu Microseconds) (%llu Milliseconds)\n",
                __scast(unsigned long long, elapsedTimeNs),
                __scast(unsigned long long, (elapsedTimeNs+999) / 1000),
                __scast(unsigned long long, (elapsedTimeNs+1000000-1) / 1000000)
            );

            {
                std::unique_lock<std::mutex> lock(g_ctx.m_inferenceLock);
                g_ctx.m_inferSliceBuf.clear();
                g_ctx.m_inferenceBufReady = false; 
            }
            
            g_ctx.m_processed.fetch_add(success == 0, std::memory_order_relaxed);
            if(g_ctx.m_processingTimerFlag) {
                g_ctx.mr_ReleasePTT_Till_EndOfInfer.tock();
                const auto elapsedTimeNs = g_ctx.mr_ReleasePTT_Till_EndOfInfer.duration().count();
                fputs("Audio Keybind End-End Query End\n", stdout);
                fprintf(stdout, "  End-End Query Took %llu ns (%llu Microseconds) (%llu Milliseconds)\n",
                    
                    __scast(unsigned long long, elapsedTimeNs), 
                    __scast(unsigned long long, (elapsedTimeNs+999) / 1000), 
                    __scast(unsigned long long, (elapsedTimeNs+1000000-1) / 1000000)
                );
            }
            __profile({ FrameMark; })
        }
    }
}
