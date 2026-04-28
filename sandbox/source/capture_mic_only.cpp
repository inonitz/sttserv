#include <miniaudio.h>
#include <whisper.h>
#include <util2/C/macro.h>
#include <thread>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>


struct ProgramContext 
{
    std::thread           m_readThread;
    std::atomic<bool>     m_exit = false;
    std::atomic<bool>        m_hasData;
    std::condition_variable  m_signalCV;
    std::mutex               m_signalMtx;

    std::atomic<uint32_t> m_produced = 0;
    std::atomic<uint32_t> m_consumed = 0;

    ma_context        m_ctx;
    ma_pcm_rb         m_ringBuffer;
    ma_device         m_audioDev;
};


void audioCaptureCallbackProducer(
    ma_device*  pDevice, 
    void*       pOutput, 
    const void* pInput, 
    ma_uint32   frameCount
);

void audioProcessCallbackConsumer();




static ProgramContext g_ctx;


int main(int argc, char* argv[]) 
{
    constexpr uint32_t kChannelCount      = 1;
    constexpr uint32_t kDeviceSampleRate  = 16000;
    constexpr uint32_t kInitialBufferSize = kDeviceSampleRate * 10;
    static char      inputBuf[100];
    ma_device_info*  pCaptureDeviceInfos;
    ma_uint32        captureDeviceCount;
	ma_device_config device_config;
	ma_result        status = MA_SUCCESS;


    status = ma_pcm_rb_init(
        ma_format_f32, 
        kChannelCount, 
        kInitialBufferSize, 
        NULL, 
        NULL, 
        &g_ctx.m_ringBuffer
    );
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create ring buffer\n");
		goto cleanup;
	}

    status = ma_context_init(NULL, 0, NULL, &g_ctx.m_ctx);
    if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create Miniaudio Context\n");
		goto cleanup;
	}
	

    status = ma_context_get_devices(&g_ctx.m_ctx, 
        NULL, 
        NULL, 
        &pCaptureDeviceInfos, 
        &captureDeviceCount
    );
    if(status != MA_SUCCESS) {
        fprintf(stderr, "Could not iterate over available capture devices\n");
		goto cleanup;
    }
    for (uint32_t i = 0; i < captureDeviceCount; ++i) {
        printf("Device %u: %s %s\n", 
            i, 
            pCaptureDeviceInfos[i].name, 
            pCaptureDeviceInfos[i].isDefault ? "[DEFAULT]" : "");
    }



    // Audio Configuration
	device_config                    = ma_device_config_init(ma_device_type_capture);
	device_config.sampleRate         = kDeviceSampleRate;
    device_config.capture.pDeviceID = &pCaptureDeviceInfos[0].id;
	device_config.capture.pDeviceID  = NULL;
	device_config.capture.format     = ma_format_f32;
	device_config.capture.channels   = kChannelCount;
	device_config.capture.shareMode  = ma_share_mode_shared;
	device_config.playback.pDeviceID = NULL;
	device_config.playback.format    = ma_format_f32;
	device_config.playback.channels  = kChannelCount;
	device_config.dataCallback       = audioCaptureCallbackProducer;
	device_config.periodSizeInFrames = 960;
    device_config.pUserData          = (void*)&g_ctx;


    // Initialize the audio devices
	status = ma_device_init(NULL, &device_config, &g_ctx.m_audioDev);
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not open the default capture and/or playback devices\n");
		goto cleanup;
		return 1;
	}
    fprintf(stdout, "Picked Audio Device %s\n", pCaptureDeviceInfos[0].name);


    // read thread should init first s.t it waits for data.
    g_ctx.m_readThread = std::thread(audioProcessCallbackConsumer);
    status = ma_device_start(&g_ctx.m_audioDev);
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not start the audio device\n");
		goto cleanup;
		return 1;
	}


    fprintf(stdout, "Capturing Audio... Press any key to stop\n");
    std::fgets(inputBuf, 100, stdin);
    fprintf(stdout, "Finished");


    g_ctx.m_exit = true;
    g_ctx.m_signalCV.notify_all();
    g_ctx.m_readThread.join();
    goto cleanup;


cleanup:
    ma_device_uninit(&g_ctx.m_audioDev);
    ma_pcm_rb_uninit(&g_ctx.m_ringBuffer);
    ma_context_uninit(&g_ctx.m_ctx);
    return status;
}




using namespace std::chrono_literals;


void audioCaptureCallbackProducer(
    ma_device*  pDevice, 
    void*       pOutput, 
    const void* pInput, 
    ma_uint32   frameCount
) {
    ma_uint32 framesRemaining     = frameCount;
	ma_uint32 framesToWrite       = frameCount;
	ma_uint32 bytesPerFrame       = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);
    ma_uint8* pInputBytePtr       = (uint8_t*)(pInput);


    while (framesRemaining > 0) {
        framesToWrite = framesRemaining;
        void* pWriteBuffer;

        ma_pcm_rb_acquire_write(&g_ctx.m_ringBuffer, &framesToWrite, &pWriteBuffer);
        if (framesToWrite > 0) {
            std::memcpy(pWriteBuffer, pInputBytePtr, framesToWrite * bytesPerFrame);
            ma_pcm_rb_commit_write(&g_ctx.m_ringBuffer, framesToWrite);

            { /* Lock guard will go out of scope and release the mutex */
                std::lock_guard<std::mutex> lock(g_ctx.m_signalMtx);
                g_ctx.m_hasData = true;
            }
            g_ctx.m_signalCV.notify_one();

            framesRemaining -= framesToWrite;
            pInputBytePtr += (framesToWrite * bytesPerFrame);
        } else {
            // Buffer is literally full. In a real-time callback, 
            // you must break here to avoid a deadlock
            break; 
        }
        ++g_ctx.m_produced;
    }
    return;
}


void audioProcessCallbackConsumer()
{
    ma_uint32 framesAvailable = 0;
    ma_uint32 framesToRead    = 0;
    void*     pReadBuffer = nullptr;

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
        // printf("\r[audioProcessCallbackConsumer] (%6u Frames, P/C -> %7u/%7u) Peak: [%.*s%*s] %3d%%\n", 
        //     framesToRead, g_ctx.m_produced.load(), g_ctx.m_consumed.load(),
        //     scaled, "########################################", 
        //     barWidth - scaled, "", 
        //     (int)(maxAmp * 100));

        fprintf(stdout, "Peak: %3d%%\n", (int)(maxAmp * 100));
        // fflush(stdout);
        return;
    };


    while(!g_ctx.m_exit) 
    {
        /* Wait Until there is data to consume, i.e until cv is true */
        std::unique_lock<std::mutex> lock(g_ctx.m_signalMtx);
        g_ctx.m_signalCV.wait(lock, [](){
            return g_ctx.m_exit.load() || g_ctx.m_hasData.load();
        });

        g_ctx.m_hasData = false; /* Let producer continue */
        lock.unlock();


        framesAvailable = ma_pcm_rb_available_read(&g_ctx.m_ringBuffer);
        while(framesAvailable) {
            // 1. Acquire the read pointer
            framesToRead = framesAvailable;
            ma_pcm_rb_acquire_read(&g_ctx.m_ringBuffer, &framesToRead, &pReadBuffer);

            // process_audio_chunk(pReadBuffer, framesToRead);
            // printf("[audioProcessCallbackConsumer] Pass %u | %u/%u Frames read\n", 
            //     g_ctx.m_consumed.load(), 
            //     framesToRead,
            //     framesAvailable
            // );
            skf_detectVolume();


            // 3. Commit the read so the producer can reuse that space
            ma_pcm_rb_commit_read(&g_ctx.m_ringBuffer, framesToRead);
            framesAvailable -= framesToRead;
            ++g_ctx.m_consumed;
        }
    }
    /* Should only reach here on program termination */
    return;
}
