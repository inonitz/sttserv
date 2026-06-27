#include <cstring>
#include <sttserv/capture.hpp>
#include <util2/C/platform.h>
#include <util2/C/aligned_malloc.h>
#include <cstdio>


// bool AudioManager2::create(
//     const u32                 kChannelCount, 
//     const u32                 kDeviceSampleRate,
//     void*                     custom_user_defined_pointer, 
//     const ma_device_data_proc process_audio_chunk_functor
// ) {
//     const u32        kInitialRingBufferSize = kDeviceSampleRate * 10;
//     ma_device_info*  pCaptureDeviceInfos;
//     ma_device_info*  pPlaybackDeviceInfos;
//     u32              captureDeviceCount;
//     u32              playbackDeviceCount;
//     u32              cpDeviceIdx = 0;
//     u32              pbDeviceIdx = 0;

// 	ma_device_config device_config;
// 	ma_result        status = MA_SUCCESS;


//     /* Member init */
//     mk_ChannelCount                = kChannelCount;
//     mk_DeviceSampleRate            = kDeviceSampleRate;
//     mk_userControlledPointer = custom_user_defined_pointer;
//     mk_AudioProcessingUserFunction = process_audio_chunk_functor;

//     /* mini-audio related init */
//     constexpr auto kSizeInBytes = sizeof(*context_ptr()) + sizeof(*m_ringBuffer) + sizeof(*m_audioDev);
//     m_underlyingMem = __rcast(byte*, util2_aligned_malloc(kSizeInBytes, CACHE_LINE_BYTES));
//     status = m_underlyingMem == nullptr ? MA_ERROR : MA_SUCCESS;
// 	if (status != MA_SUCCESS) {
//         fprintf(stderr, "Could not allocate memory for Audio manager\n");
//         return false;
// 	}

//     context_ptr()        = __rcast(ma_context*, (m_underlyingMem + 0));
//     m_ringBuffer = __rcast(ma_pcm_rb*, (m_underlyingMem + sizeof(*context_ptr())));
//     m_audioDev   = __rcast(ma_device*, (m_underlyingMem + sizeof(*context_ptr()) + sizeof(*m_ringBuffer)));
//     mb_initMem    = true;


//     /* Init Circular SPSC Ringbuffer, Custom Context, Duplex, */
//     status = ma_pcm_rb_init(
//         ma_format_f32, 
//         kChannelCount, 
//         kInitialRingBufferSize, 
//         NULL, 
//         NULL, 
//         m_ringBuffer
//     );
// 	if (status != MA_SUCCESS) {
//         fprintf(stderr, "Could not create ring buffer\n");
//         return false;
// 	}
//     mb_initRingBuffer = true;


// #if defined(UTIL2_OS_LINUX)
//     ma_backend backends[] = { ma_backend_pulseaudio, ma_backend_alsa };
//     const ma_backend* pBackendHandle = backends;
//     const ma_uint32 backendCount = 2;
// #else
//     const ma_backend* pBackendHandle = NULL;
//     const ma_uint32   backendCount = 0;
// #endif

//     status = ma_context_init(pBackendHandle, backendCount, NULL, context_ptr());
//     if (status != MA_SUCCESS) {
//         fprintf(stderr, "Could not create Miniaudio Context\n");
//         ma_pcm_rb_uninit(m_ringBuffer);
// 		return false;
// 	}
//     mb_initContext = true;


//     /* Find Default devices */
//     status = ma_context_get_devices(context_ptr(), 
//         &pPlaybackDeviceInfos, 
//         &playbackDeviceCount, 
//         &pCaptureDeviceInfos, 
//         &captureDeviceCount
//     );
//     if(status != MA_SUCCESS) {
//         fprintf(stderr, "Could not iterate over available capture/playback devices\n");
//         ma_context_uninit(context_ptr());
//         ma_pcm_rb_uninit(m_ringBuffer);
//         util2_aligned_free(m_underlyingMem);
//         mb_initMem        = false;
//         mb_initRingBuffer = false;
//         mb_initContext    = false;
// 		return false;
//     }
    
//     /* Save the data */
//     m_deviceList.first.assign(pCaptureDeviceInfos, pCaptureDeviceInfos + captureDeviceCount);
//     m_deviceList.second.assign(pPlaybackDeviceInfos, pPlaybackDeviceInfos + playbackDeviceCount);
//     mb_initDevList = true;


//     fprintf(stdout, "----------------------------------------\nCapture Devices:\n");
//     for (uint32_t i = 0; i < captureDeviceCount; ++i) {
//         fprintf(stdout, "  %u: %s %s\n", 
//             i, 
//             pCaptureDeviceInfos[i].name, 
//             pCaptureDeviceInfos[i].isDefault ? "[DEFAULT]" : ""
//         );
//         cpDeviceIdx = pCaptureDeviceInfos[i].isDefault ? i : cpDeviceIdx;
//     }
//     fprintf(stdout, "----------------------------------------\nPlayback Devices:\n");
//     for (uint32_t i = 0; i < playbackDeviceCount; ++i) {
//         fprintf(stdout, "  %u: %s %s\n", 
//             i, 
//             pPlaybackDeviceInfos[i].name, 
//             pPlaybackDeviceInfos[i].isDefault ? "[DEFAULT]" : ""
//         );
//         pbDeviceIdx = pPlaybackDeviceInfos[i].isDefault ? i : pbDeviceIdx;
//     }
//     fprintf(stdout, "----------------------------------------\n");


//     m_captureDeviceIdx  = static_cast<uint8_t>(cpDeviceIdx);
//     m_playbackDeviceIdx = static_cast<uint8_t>(pbDeviceIdx);


//     // Audio Configuration
// 	device_config                    = ma_device_config_init(ma_device_type_duplex);
// 	device_config.sampleRate         = kDeviceSampleRate;
//     device_config.capture.pDeviceID = &pCaptureDeviceInfos[m_captureDeviceIdx].id;
// 	device_config.capture.format     = ma_format_f32;
// 	device_config.capture.channels   = kChannelCount;
// 	device_config.capture.shareMode  = ma_share_mode_shared;

//     device_config.playback.pDeviceID = &pPlaybackDeviceInfos[m_playbackDeviceIdx].id;
// 	device_config.playback.format     = ma_format_f32;
// 	device_config.playback.channels   = kChannelCount;
// 	device_config.playback.shareMode  = ma_share_mode_shared;

// 	device_config.dataCallback       = process_audio_chunk_functor;
// 	device_config.periodSizeInFrames = 960;
//     device_config.pUserData          = custom_user_defined_pointer;


//     // Initialize the audio devices
// 	status = ma_device_init(NULL, &device_config, m_audioDev);
// 	if (status != MA_SUCCESS) {
//         fprintf(stderr, "Could not open the default capture and/or playback devices\n");
//         ma_context_uninit(context_ptr());
//         ma_pcm_rb_uninit(m_ringBuffer);
//         util2_aligned_free(m_underlyingMem);
//         mb_initMem        = false;
//         mb_initRingBuffer = false;
//         mb_initContext    = false;
// 		return false;
// 	}
//     fprintf(stdout, "Picked Audio Device %s\n", pCaptureDeviceInfos[0].name);
//     mb_initAudioDev = true;
//     mb_initAll      = true;


//     return true;
// }


void CaptureDevice::destroy()
{
    if(isInit(kInitStateBitResampler())) {
        ma_resampler_uninit(resampler_ptr(), NULL);
        m_resamplerOffset = 0;

        decltype(m_resampleBuffer) tmp{};
        std::swap(tmp, m_resampleBuffer);
    }
    
    if(isInit(kInitStateBitRingBuffer())) {
        ma_pcm_rb_uninit(ringbuffer_ptr());
        m_ringBufferOffset = 0;
    }

    if(isInit(kInitStateBitAudioDev())) {
        ma_device_uninit(audio_dev_ptr());
        m_audioDevOffset = 0;
    }

    if(isInit(kInitStateBitDevList())) {
        captureDeviceList tmp{};
        std::swap(tmp, m_deviceList);
        /* will be dealloc out of scope */
    }

    if(isInit(kInitStateBitContext())) {
        ma_context_uninit(context_ptr());
        m_ctxOffset = 0;
    }

    if(isInit(kInitStateBitMem())) {
        util2_aligned_free(m_underlyingMem);
        m_underlyingMem = nullptr;
    }


    resetInitState();
    return;
}




[[nodiscard]] bool CaptureDevice::initializeMemory()
{
    ma_result status = MA_SUCCESS;
    constexpr auto kSizeInBytes = sizeof(ma_context) 
        + sizeof(ma_device)
        + sizeof(ma_pcm_rb)
        + sizeof(ma_resampler);


    m_underlyingMem = __rcast(byte*, util2_aligned_malloc(kSizeInBytes, CACHE_LINE_BYTES));
    status = (m_underlyingMem == nullptr) ? MA_ERROR : MA_SUCCESS;
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not allocate memory for Audio manager\n");
        return false;
	}

    /* Assign Memory regions */
    m_ctxOffset = 0;
    m_audioDevOffset   = m_ctxOffset + sizeof(ma_context);
    m_ringBufferOffset = m_audioDevOffset + sizeof(ma_device);
    m_resamplerOffset  = m_ringBufferOffset + sizeof(ma_pcm_rb);
    setInitState(kInitStateBitMem());
    return true;
}


[[nodiscard]] bool CaptureDevice::initializeContext() noexcept
{
#if defined(UTIL2_OS_LINUX)
    ma_backend backends[] = { ma_backend_pulseaudio, ma_backend_alsa };
    const ma_backend* pBackendHandle = backends;
    const ma_uint32 backendCount = 2;
#else
    const ma_backend* pBackendHandle = NULL;
    const ma_uint32   backendCount = 0;
#endif
    ma_context_config contextConfig = ma_context_config_init();
    contextConfig.threadPriority = ma_thread_priority_realtime;

    ma_result status = ma_context_init(pBackendHandle, backendCount, &contextConfig, context_ptr());
    if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create Miniaudio Context\n");
		return false;
	}
	

    setInitState(kInitStateBitContext());
    return true;
}


[[nodiscard]] bool CaptureDevice::initializeDeviceList() noexcept
{
    const auto print_single_device_func = [](const ma_device_info& info, uint8_t index) {
        fprintf(stdout, "[%u, Default=%s, Native Formats=%u] %s\n", 
            index, 
            info.isDefault ? "Yes" : "No ", 
            info.nativeDataFormatCount,
            info.name
        );
        
        for (ma_uint32 j = 0; j < info.nativeDataFormatCount; ++j) {
            fprintf(stdout, "   [%u] Format: %d | Channels: %u | Rate: %u Hz | Flags: 0x%X\n",
                j,
                info.nativeDataFormats[j].format,
                info.nativeDataFormats[j].channels,
                info.nativeDataFormats[j].sampleRate,
                info.nativeDataFormats[j].flags
            );
        }
        return;
    };


    ma_device_info*  pCaptureDeviceInfos;
    ma_device_info*  pPlaybackDeviceInfos;
    u32              captureDeviceCount;
    u32              playbackDeviceCount;
    ma_result        status      = MA_SUCCESS;
    u32              cpDeviceIdx = 0xFF;
    u32              pbDeviceIdx = 0xFF;
    if(context_ptr() == nullptr) {
        return false;
    }


    /* Find Default devices */
    status = ma_context_get_devices(context_ptr(), 
        &pPlaybackDeviceInfos, 
        &playbackDeviceCount, 
        &pCaptureDeviceInfos, 
        &captureDeviceCount
    );
    if(status != MA_SUCCESS) {
        fprintf(stderr, "Could not iterate over available capture/playback devices\n");
		return false;
    }

    /* Save the data */
    m_deviceList.assign(pCaptureDeviceInfos, pCaptureDeviceInfos + captureDeviceCount);


    /* save default devices unless specified later otherwise */
    fprintf(stdout, "----------------------------------------\nCapture Devices:\n");
    for (uint32_t i = 0; i < captureDeviceCount; ++i) {
        fprintf(stdout, "  %u: %s %s\n", 
            i, 
            pCaptureDeviceInfos[i].name, 
            pCaptureDeviceInfos[i].isDefault ? "[DEFAULT]" : ""
        );
        cpDeviceIdx = pCaptureDeviceInfos[i].isDefault ? i : cpDeviceIdx;
    }
    fprintf(stdout, "----------------------------------------\nPlayback Devices:\n");
    for (uint32_t i = 0; i < playbackDeviceCount; ++i) {
        fprintf(stdout, "  %u: %s %s\n", 
            i, 
            pPlaybackDeviceInfos[i].name, 
            pPlaybackDeviceInfos[i].isDefault ? "[DEFAULT]" : ""
        );
        pbDeviceIdx = pPlaybackDeviceInfos[i].isDefault ? i : pbDeviceIdx;
    }
    fprintf(stdout, "----------------------------------------\n");
    

    for(uint32_t i = 0; i < m_deviceList.size(); ++i) {
        print_single_device_func(m_deviceList[i], __scast(u8, i));
    }


    /* pick Defaults unless later specified otherwise */
    m_captureDeviceIdx  = static_cast<uint8_t>(cpDeviceIdx);
    setInitState(kInitStateBitDevList());
    return true;
}


[[nodiscard]] bool CaptureDevice::initializeAudioDevice(
    u8                     captureDeviceID,
    pcm_data_user_callback custom_user_defined_pointer, 
    const u32              k_optimal_latency_btwn_audio_req_ms,
    const u32              k_desired_resample_rate,
    const u32              k_desired_channel_count
) noexcept {
    // const u32        kOptimalLatencyBetweenDeviceRequestsInMilliseconds = 1; /* 1ms */
    // const u32 kPeriodSizeInFrames = mk_DeviceSampleRate * k_optimal_latency_btwn_audio_req_ms / 1000;
	ma_result        status = MA_SUCCESS;
	ma_device_config device_config;


    m_captureDeviceIdx = (captureDeviceID == 0xFF) ? m_captureDeviceIdx : captureDeviceID;
    /* Error During initialization, indices are invalid/didn't create context */
    if(context_ptr() == nullptr || m_captureDeviceIdx > m_deviceList.size() 
    ) {
        return false;
    }

    /* Member init after proper init */
    mk_userControlledPointer = custom_user_defined_pointer;

    // Audio Configuration
	device_config                   = ma_device_config_init(ma_device_type_capture);
	device_config.sampleRate        = 0;
    device_config.capture.pDeviceID = &m_deviceList[m_captureDeviceIdx].id;
	device_config.capture.format    = ma_format_f32;
	device_config.capture.channels  = k_desired_channel_count;
	device_config.capture.shareMode = ma_share_mode_shared;

	device_config.dataCallback             = producerCallback;
    device_config.periodSizeInMilliseconds = k_optimal_latency_btwn_audio_req_ms;
    device_config.pUserData                = this;
    device_config.performanceProfile       = ma_performance_profile_low_latency;
    device_config.periods = 3;

    // Initialize the audio devices
	status = ma_device_init(context_ptr(), &device_config, audio_dev_ptr());
	if (status != MA_SUCCESS) {
        fprintf(stderr, 
            "Could not open the desired capture and/or playback devices, Error Code (%d): %s\n",
            status,
            ma_result_description(status)
        );
		return false;
	}
    fprintf(stdout, "Picked Audio Devices -> \n  Capture: %s\n", 
        m_deviceList[m_captureDeviceIdx].name
    );


    mk_DeviceSampleRate    = audio_dev_ptr()->sampleRate;
    mk_DesiredResampleRate = k_desired_resample_rate;
    setInitState(kInitStateBitAudioDev());
    return true;
}


[[nodiscard]] bool CaptureDevice::initializeRingBuffer(
    const u32 kInChannelCount,
    const u32 kInDeviceSampleRate,
    const u32 kRingBufferSizeInSeconds
) noexcept {
    const u32 kBufferRingSize = kInDeviceSampleRate * kRingBufferSizeInSeconds;

    ma_result status = ma_pcm_rb_init(
        ma_format_f32, 
        kInChannelCount, 
        kBufferRingSize, 
        NULL, 
        NULL, 
        ringbuffer_ptr()
    );
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create ring buffer - %s\n", ma_result_description(status));
        return false;
	}

    setInitState(kInitStateBitRingBuffer());
    return true;
}


[[nodiscard]] bool CaptureDevice::initializeResampler(
    const u32 kInChannelCount,
    const u32 kInDeviceSampleRate,
    const u32 kInDesiredSampleRate
) {
    const ma_resampler_config kResampConfig = ma_resampler_config_init(
        ma_format_f32, 
        kInChannelCount, 
        kInDeviceSampleRate, 
        kInDesiredSampleRate, 
        ma_resample_algorithm_linear
    );
    ma_result status = MA_SUCCESS;
    status = ma_resampler_init(&kResampConfig, NULL, resampler_ptr());

	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create Software Resampler - %s\n", ma_result_description(status));
        return false;
	}
    
    m_resampleBuffer.resize(CaptureDevice::kMaxSecondsToAcquire * kInDesiredSampleRate);
    setInitState(kInitStateBitResampler());
    return true;
}



void CaptureDevice::producerCallback(
    ma_device*     pDevice, 
    __unused void* pOutput, 
    const void*    pInput, 
    ma_uint32      frameCount
) {
    ma_uint32 framesRemaining = frameCount;
	ma_uint32 framesToWrite   = frameCount;
	ma_uint32 bytesPerFrame   = ma_get_bytes_per_frame(pDevice->capture.format, pDevice->capture.channels);
    
    const ma_uint8* pkInputBytePtr = __rcast(const ma_uint8*, pInput);
    void*           pWriteBuffer   = nullptr;
    const auto      kCtx           = static_cast<CaptureDevice*>(pDevice->pUserData);


    while (framesRemaining > 0) {
        framesToWrite = framesRemaining;
        ma_pcm_rb_acquire_write(kCtx->ringbuffer_ptr(), &framesToWrite, &pWriteBuffer);

        if(framesToWrite == 0) { 
            break; /* packets have been dropped */
        }

        std::memcpy(pWriteBuffer, pkInputBytePtr, framesToWrite * bytesPerFrame);
        ma_pcm_rb_commit_write(kCtx->ringbuffer_ptr(), framesToWrite);

        { 
            std::unique_lock<std::mutex> _(kCtx->m_lock);
            kCtx->m_dataReady = true;
        }
        kCtx->m_dataCV.notify_one();

        framesRemaining -= framesToWrite;
        pkInputBytePtr += (framesToWrite * bytesPerFrame);
    }


    return;
}


void CaptureDevice::consumerCallback(CaptureDevice& ctx)
{
    ma_uint32 framesAvailable = 0;
    ma_uint32 framesToRead    = 0;
    ma_uint64 framesToRead64  = 0;
    ma_uint64 framesToWrite64 = 0;
    void*     pReadBuffer     = nullptr;


    while(!ctx.m_stopRequested) 
    {
        {
            std::unique_lock<std::mutex> lock(ctx.m_lock);
            ctx.m_dataCV.wait(lock, [&ctx](){
                return ctx.m_stopRequested.load() || ctx.m_dataReady.load();
            });
            ctx.m_dataReady = false;
        }
        if(ctx.m_stopRequested.load()) {
            break;
        }


        framesAvailable = ma_pcm_rb_available_read(ctx.ringBufferHandle());
        while(framesAvailable) {
            framesToRead = framesAvailable;
            ma_pcm_rb_acquire_read(ctx.ringBufferHandle(), &framesToRead, &pReadBuffer);


            // mk_userControlledPointer-
            framesToRead64 = framesToRead;
            framesToWrite64 = ctx.m_resampleBuffer.size();
            ma_resampler_process_pcm_frames(
                ctx.resampler_ptr(), 
                pReadBuffer,
                &framesToRead64,
                ctx.m_resampleBuffer.data(),
                &framesToWrite64
            );

            if(ctx.mk_userControlledPointer) {
                ctx.mk_userControlledPointer(
                    ctx.m_resampleBuffer.data(), 
                    ctx.m_resampleBuffer.size() 
                
                )
            }

            if(ctx.m_inferenceBuf.size() >= ctx.m_inferenceBufferSize) 
            {
                std::lock_guard<std::mutex> lock(ctx.m_inferenceLock);
                if(!ctx.m_inferenceBufReady) { 
                    fprintf(stdout, "[audioProcessCallbackConsumer] Swapping buffers [Overflow, Audio > 10s]\n");
                    std::swap(ctx.m_inferSliceBuf, ctx.m_inferenceBuf);
                    ctx.m_inferenceBuf.clear(); /* clear m_inferSliceBuf for next iter*/
                    ctx.m_inferenceBufReady = true;
                    ctx.m_inferenceCV.notify_one();
                } else {
                    fprintf(stdout, "[audioProcessCallbackConsumer] Still Writing to inference buffer\n");
                }
            }

            ma_pcm_rb_commit_read(ctx.ringBufferHandle(), framesToRead);
            framesAvailable -= framesToRead;
        }
    }

    return;
}