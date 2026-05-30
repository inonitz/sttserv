#include "audio2.hpp"
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
//     constexpr auto kSizeInBytes = sizeof(*m_ctx) + sizeof(*m_ringBuffer) + sizeof(*m_audioDev);
//     m_underlyingMem = __rcast(byte*, util2_aligned_malloc(kSizeInBytes, CACHE_LINE_BYTES));
//     status = m_underlyingMem == nullptr ? MA_ERROR : MA_SUCCESS;
// 	if (status != MA_SUCCESS) {
//         fprintf(stderr, "Could not allocate memory for Audio manager\n");
//         return false;
// 	}

//     m_ctx        = __rcast(ma_context*, (m_underlyingMem + 0));
//     m_ringBuffer = __rcast(ma_pcm_rb*, (m_underlyingMem + sizeof(*m_ctx)));
//     m_audioDev   = __rcast(ma_device*, (m_underlyingMem + sizeof(*m_ctx) + sizeof(*m_ringBuffer)));
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

//     status = ma_context_init(pBackendHandle, backendCount, NULL, m_ctx);
//     if (status != MA_SUCCESS) {
//         fprintf(stderr, "Could not create Miniaudio Context\n");
//         ma_pcm_rb_uninit(m_ringBuffer);
// 		return false;
// 	}
//     mb_initContext = true;


//     /* Find Default devices */
//     status = ma_context_get_devices(m_ctx, 
//         &pPlaybackDeviceInfos, 
//         &playbackDeviceCount, 
//         &pCaptureDeviceInfos, 
//         &captureDeviceCount
//     );
//     if(status != MA_SUCCESS) {
//         fprintf(stderr, "Could not iterate over available capture/playback devices\n");
//         ma_context_uninit(m_ctx);
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
//         ma_context_uninit(m_ctx);
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


void AudioManager2::destroy()
{
    if(mb_initResampler) {
        ma_resampler_uninit(m_resampler, NULL);
        m_resampler = nullptr;
    }
    
    if(mb_initRingBuffer) {
        ma_pcm_rb_uninit(m_ringBuffer);
        m_ringBuffer = nullptr;
    }

    if(mb_initAudioDev) {
        ma_device_uninit(m_audioDev);
        m_audioDev = nullptr;
    }

    if(mb_initDevList) {
        capture_playback_pair tmp{};
        std::swap(tmp, m_deviceList);
        /* will be dealloc out of scope */
    }

    if(mb_initContext) {
        ma_context_uninit(m_ctx);
        m_ctx = nullptr;
    }

    if(mb_initMem) {
        util2_aligned_free(m_underlyingMem);
        m_underlyingMem = nullptr;
    }

    /* Reset all boolean flags */
    mb_initMem        = false;
    mb_initContext    = false;
    mb_initDevList    = false;
    mb_initAudioDev   = false;
    mb_initRingBuffer = false;
    mb_initResampler  = false;
    mb_initAll        = false;
    return;
}




[[nodiscard]] bool AudioManager2::initializeMemory()
{
    ma_result status = MA_SUCCESS;
    constexpr auto kSizeInBytes = sizeof(*m_ctx) 
        + sizeof(*m_audioDev)
        + sizeof(*m_ringBuffer)
        + sizeof(*m_resampler);


    m_underlyingMem = __rcast(byte*, util2_aligned_malloc(kSizeInBytes, CACHE_LINE_BYTES));
    status = (m_underlyingMem == nullptr) ? MA_ERROR : MA_SUCCESS;
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not allocate memory for Audio manager\n");
        return false;
	}

    /* Assign Memory regions */
    m_ctx        = __rcast(ma_context*,   (m_underlyingMem + 0));
    m_audioDev   = __rcast(ma_device*,    (m_underlyingMem + sizeof(*m_ctx) ));
    m_ringBuffer = __rcast(ma_pcm_rb*,    (m_underlyingMem + sizeof(*m_ctx) + sizeof(*m_audioDev)));
    m_resampler  = __rcast(ma_resampler*, (
        m_underlyingMem + sizeof(*m_ctx) + sizeof(*m_audioDev) + sizeof(*m_ringBuffer)
    ) );
    mb_initMem = true;
    return true;
}


[[nodiscard]] bool AudioManager2::initializeContext() noexcept
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

    ma_result status = ma_context_init(pBackendHandle, backendCount, &contextConfig, m_ctx);
    if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create Miniaudio Context\n");
		return false;
	}
	

    mb_initContext = true;
    return true;
}


[[nodiscard]] bool AudioManager2::initializeDeviceList() noexcept
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
    if(m_ctx == nullptr) {
        return false;
    }


    /* Find Default devices */
    status = ma_context_get_devices(m_ctx, 
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
    m_deviceList.first.assign(pCaptureDeviceInfos, pCaptureDeviceInfos + captureDeviceCount);
    m_deviceList.second.assign(pPlaybackDeviceInfos, pPlaybackDeviceInfos + playbackDeviceCount);


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
    

    for(uint32_t i = 0; i < m_deviceList.first.size(); ++i) {
        print_single_device_func(m_deviceList.first[i], __scast(u8, i));
    }
    for(uint32_t i = 0; i < m_deviceList.second.size(); ++i) {
        print_single_device_func(m_deviceList.second[i], __scast(u8, i));
    }


    /* pick Defaults unless later specified otherwise */
    m_captureDeviceIdx  = static_cast<uint8_t>(cpDeviceIdx);
    m_playbackDeviceIdx = static_cast<uint8_t>(pbDeviceIdx);
    mb_initDevList      = true;
    return true;
}


[[nodiscard]] bool AudioManager2::initializeAudioDevice(
    u8                        captureDeviceID,
    u8                        playbackDeviceID,
    void*                     custom_user_defined_pointer, 
    const ma_device_data_proc k_process_audio_chunk_functor,
    const u32                 k_optimal_latency_btwn_audio_req_ms,
    const u32                 k_desired_resample_rate,
    const u32                 k_desired_channel_count
) noexcept {
    // const u32        kOptimalLatencyBetweenDeviceRequestsInMilliseconds = 1; /* 1ms */
    // const u32 kPeriodSizeInFrames = mk_DeviceSampleRate * k_optimal_latency_btwn_audio_req_ms / 1000;
	ma_result        status = MA_SUCCESS;
	ma_device_config device_config;
    // ma_device_info   devInfo;


    m_captureDeviceIdx  = (captureDeviceID == 0xFF) ? m_captureDeviceIdx : captureDeviceID;
    m_playbackDeviceIdx = (playbackDeviceID == 0xFF) ? m_playbackDeviceIdx : playbackDeviceID;
    /* Error During initialization, indices are invalid/didn't create context */
    if(m_ctx == nullptr
        || m_captureDeviceIdx > m_deviceList.first.size() 
        || m_playbackDeviceIdx > m_deviceList.second.size()
    ) {
        return false;
    }

    /* Member init after proper init */
    mk_userControlledPointer = custom_user_defined_pointer;
    mk_AudioProcessingUserFunction = k_process_audio_chunk_functor;


    // Audio Configuration
	device_config                   = ma_device_config_init(ma_device_type_duplex);
	device_config.sampleRate        = 0;
    device_config.capture.pDeviceID = &m_deviceList.first[m_captureDeviceIdx].id;
	device_config.capture.format    = ma_format_f32;
	device_config.capture.channels  = k_desired_channel_count;
	device_config.capture.shareMode = ma_share_mode_shared;

    device_config.playback.pDeviceID = &m_deviceList.second[m_playbackDeviceIdx].id;
	device_config.playback.format     = ma_format_f32;
	device_config.playback.channels   = k_desired_channel_count;
	device_config.playback.shareMode  = ma_share_mode_shared;

	device_config.dataCallback             = mk_AudioProcessingUserFunction;
    device_config.periodSizeInMilliseconds = k_optimal_latency_btwn_audio_req_ms;
    device_config.pUserData                = mk_userControlledPointer;
    device_config.performanceProfile       = ma_performance_profile_low_latency;


    // Initialize the audio devices
	status = ma_device_init(m_ctx, &device_config, m_audioDev);
	if (status != MA_SUCCESS) {
        fprintf(stderr, 
            "Could not open the desired capture and/or playback devices, Error Code (%d): %s\n",
            status,
            ma_result_description(status)
        );
		return false;
	}
    fprintf(stdout, "Picked Audio Devices -> \n  Playback: %s\n  Capture: %s\n", 
        m_deviceList.second[m_playbackDeviceIdx].name,
        m_deviceList.first[m_captureDeviceIdx].name
    );

    
    
    // status = ma_context_get_device_info(
    //     m_ctx, 
    //     ma_device_type_capture, 
    //     &m_deviceList.first[m_captureDeviceIdx].id,
    //     &devInfo
    // );
	// if (status != MA_SUCCESS) {
    //     fprintf(stderr, "Could not Find Essential Device metadata, Error Code (%d): %s\n",
    //         status,
    //         ma_result_description(status)
    //     );
	// 	return false;
	// }

    mk_ChannelCount        = k_desired_channel_count;
    mk_DeviceSampleRate    = m_audioDev->sampleRate;
    mk_DesiredResampleRate = k_desired_resample_rate;
    mb_initAudioDev = true;
    return true;
}


[[nodiscard]] bool AudioManager2::initializeRingBuffer(
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
        m_ringBuffer
    );
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create ring buffer - %s\n", ma_result_description(status));
        return false;
	}

    mb_initRingBuffer = true;
    return true;
}


[[nodiscard]] bool AudioManager2::initializeResampler(
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
    status = ma_resampler_init(&kResampConfig, NULL, m_resampler);


	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create Software Resampler - %s\n", ma_result_description(status));
        return false;
	}

    mb_initResampler = true;
    return true;
}