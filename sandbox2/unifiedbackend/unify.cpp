#include "unify.hpp"
#include <util2/C/compiler_warning.h>
#include <cstring>


static ProgramContext g_ctx;


int main(int argc, char* argv[]) {
    __profile({ ZoneScopedN("main"); })

    CommandLineArguments commandLineArguments;
    std::unique_lock<std::mutex> genericLock;

    if(!parse_commandline_args(argc, argv, commandLineArguments)) {
        fprintf(stderr, "Could Not Parse Command-line Arguments\n");
        return 1;
    }

    if (!g_ctx.m_backend.create(commandLineArguments)) {
        fprintf(stderr, "Could Not Initialize Backend\n");
        return 1;
    }

    g_ctx.m_keyListener.create();
    g_ctx.m_keyListener.bindKey(KeyCode::A, [](__unused KeyCode key) {
        g_ctx.m_startStopFlag = !g_ctx.m_startStopFlag;
        g_ctx.m_recordTimerFlag = (g_ctx.m_startStopFlag.load() == true);
        if(g_ctx.m_recordTimerFlag) g_ctx.mr_startPTT_Till_ReleasePTT.tick();
        g_ctx.m_processingTimerFlag = (g_ctx.m_startStopFlag.load() == false);
        if(g_ctx.m_processingTimerFlag) g_ctx.mr_ReleasePTT_Till_EndOfInfer.tick();
    });
    g_ctx.m_keyListener.bindKey(KeyCode::Escape, [](__unused KeyCode key) {
        g_ctx.m_exit = true;
        g_ctx.m_exitSignal.notify_all();
    });

    if(!g_ctx.m_audioMan.createContext() || !g_ctx.m_audioMan.selectDevicesAndFinalize(&g_ctx, audioCaptureCallbackProducer, 1, 1, CommandLineArguments::kInferenceSampleRate, 0xFF, 0xFF)) {
        goto cleanup;
    }

    g_ctx.m_resampleBufferSize = g_ctx.m_audioMan.nativeSampleRate();
    g_ctx.m_resampleBuffer.resize(g_ctx.m_resampleBufferSize);
    g_ctx.m_inferenceBufferSize = 10 * CommandLineArguments::kInferenceSampleRate;
    g_ctx.m_inferenceBuf.reserve(g_ctx.m_inferenceBufferSize);
    g_ctx.m_inferSliceBuf.reserve(g_ctx.m_inferenceBufferSize);

    g_ctx.m_readThread       = std::thread(audioProcessCallbackConsumer);
    g_ctx.m_processingThread = std::thread(audioInferenceWorker);
    
    if (!g_ctx.m_audioMan.start()) goto cleanup;
    
    genericLock = std::unique_lock<std::mutex>(g_ctx.m_exitLock);
    g_ctx.m_exitSignal.wait(genericLock, []() { return g_ctx.m_exit == true; });
    genericLock.unlock();

    g_ctx.m_audioMan.stop();
    g_ctx.m_exit = true;
    g_ctx.m_audioCV.notify_all();
    g_ctx.m_readThread.join();
    g_ctx.m_inferenceCV.notify_all();
    g_ctx.m_processingThread.join();

cleanup:
    g_ctx.m_audioMan.destroy();
    g_ctx.m_keyListener.destroy();
    g_ctx.m_backend.destroy();
    return 0;
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
                            fprintf(stdout, "[audioProcessCallbackConsumer] Swapping buffers [Overflow, Audio > 10s]\n");
                            std::swap(kUserCtx.m_inferSliceBuf, kUserCtx.m_inferenceBuf);
                            kUserCtx.m_inferenceBuf.clear(); /* clear m_inferSliceBuf for next iter*/
                            kUserCtx.m_inferenceBufReady = true;
                            kUserCtx.m_inferenceCV.notify_one();
                        } else {
                            fprintf(stdout, "[audioProcessCallbackConsumer] Still Writing to inference buffer\n");
                        }
                    }
                } else { 
                    std::lock_guard<std::mutex> lock(kUserCtx.m_inferenceLock);
                    kUserCtx.m_inferenceBufReady = kUserCtx.m_inferenceBuf.size() > 0;
                    if(kUserCtx.m_inferenceBufReady) {
                        fprintf(stdout, "[audioProcessCallbackConsumer] Swapped buffers [Data Available/Early Release]\n");
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

void audioInferenceWorker() {
    __profile({ tracy::SetThreadName("audioInferenceWorker"); })
    while(!g_ctx.m_exit) {
        {
            std::unique_lock<std::mutex> lock(g_ctx.m_inferenceLock);
            g_ctx.m_inferenceCV.wait(lock, [](){ return g_ctx.m_exit.load() || g_ctx.m_inferenceBufReady.load(); });
        }
        if(g_ctx.m_exit.load()) break;

        {
            u32 duration_ms = 1000;
            if(!g_ctx.m_recordTimerFlag) {
                g_ctx.mr_startPTT_Till_ReleasePTT.tock();
                auto elapsed = g_ctx.mr_startPTT_Till_ReleasePTT.duration().count();
                duration_ms = __scast(u32, (elapsed+1000000-1) / 1000000);
            }

            g_ctx.mr_Start_Till_EndOfInfer.tick();
            
            bool success = g_ctx.m_backend.transcribe(
                g_ctx.m_inferSliceBuf.data(), 
                g_ctx.m_inferSliceBuf.size(), 
                duration_ms, 
                CommandLineArguments::kInferenceSampleRate
            );
            
            if(!success) fprintf(stderr, "Failed to process audio\n");

            g_ctx.mr_Start_Till_EndOfInfer.tock();

            
            auto outputWavName = "output_mic" + std::to_string(g_ctx.m_processed.load()) + "_s" + std::to_string(duration_ms) + ".wav";
            if(g_ctx.m_micToFile.open(
                outputWavName,
                g_ctx.m_audioMan.channelCount(), 
                g_ctx.m_audioMan.resampleRate(),
                g_ctx.m_audioMan.outputFormat()
            )) {
                g_ctx.m_micToFile.write(g_ctx.m_inferSliceBuf.data(), g_ctx.m_inferSliceBuf.size());
                g_ctx.m_micToFile.close();
                fprintf(stdout, "Saved %u milliseconds To Audio File %s\n", 
                    duration_ms, 
                    outputWavName.c_str()
                );
            }

            {
                std::unique_lock<std::mutex> lock(g_ctx.m_inferenceLock);
                g_ctx.m_inferSliceBuf.clear();
                g_ctx.m_inferenceBufReady = false; 
            }
            
            g_ctx.m_processed.fetch_add(success, std::memory_order_relaxed);
            if(g_ctx.m_processingTimerFlag) g_ctx.mr_ReleasePTT_Till_EndOfInfer.tock();

            g_ctx.m_backend.result(g_ctx.m_inferResult);
            g_ctx.m_backend.print_timings();
            g_ctx.m_backend.reset_timings();
        }
    }
}