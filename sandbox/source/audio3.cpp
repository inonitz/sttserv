#include "sandbox/audio3.hpp"
#include <util2/C/platform.h>
#include <util2/C/aligned_malloc.h>
#include <cstdio>


void AudioManager3::destroy()
{
    if(isInitializedResampler()) {
        ma_resampler_uninit(m_resampler, NULL);
        m_resampler = nullptr;
    }
    
    if(isInitializedAudioDevice()) {
        ma_device_uninit(m_audioDev);
        m_audioDev = nullptr;
    }

    if(isInitializedDeviceList()) {
        capture_playback_pair tmp{};
        std::swap(tmp, *m_deviceList);
        /* will be dealloc out of scope */
    }
    
    if(isInitializedContext()) {
        ma_context_uninit(m_ctx);
        m_ctx = nullptr;
    }

    if(isInitializedAudioBuffer()) {
        std::vector<byte> tmp;
        std::swap(tmp, *m_audioBuf);
        m_audioBuf = nullptr;
        /* will be dealloc out of scope */
    }

    if(isInitializedMemory()) {
        util2_aligned_free(m_underlyingMem);
        m_underlyingMem = nullptr;
    }

    mb_initializationFlags = 0;
    return;
}


bool AudioManager3::createContext() 
{
    bool status = true;

    if( (m_underlyingMem != nullptr) || isInitializedAll()) {
        return true;
    }
    

    status = initializeMemory();
    if(!status) {
        destroy();
        return false;
    }

    status = initializeContext();
    if(!status) {
        destroy();
        return false;
    }

    status = initializeDeviceList();
    if(!status) {
        destroy();
        return false;
    }


    return status;
}


bool AudioManager3::selectDevicesAndFinalize(
    void*                     custom_user_defined_pointer, 
    const ma_device_data_proc k_process_audio_chunk_functor,
    const u32                 kOptimalLatencyBetweenAudioReq_ms /* milliseconds */,
    const u32                 kDesiredSampleRate,
    u8                        captureDeviceID,
    u8                        playbackDeviceID
) {
    const u32 kMaxSecondsToAcquire = 10;
    bool status = true;
    status = initializeAudioDevice(
        captureDeviceID, 
        playbackDeviceID, 
        custom_user_defined_pointer, 
        k_process_audio_chunk_functor,
        kOptimalLatencyBetweenAudioReq_ms,
        kDesiredSampleRate,
        dataFormat(),
        channelCount()
    );

    if(!status) {
        destroy();
        return false;
    }

    /* 
        We require the Hardware Native Sample Rate to initialize 
        The ringbuffer & the resampler properly
    */
    status = initializeRingBuffer(
        dataFormat(),
        channelCount(),
        mk_nativeDeviceSampleRate,
        kMaxSecondsToAcquire
    );
    if(!status) {
        destroy();
        return false;
    }

    status = initializeResampler(
        dataFormat(),
        channelCount(),
        mk_nativeDeviceSampleRate,
        mk_desiredResampleRate
    );
    if(!status) {
        destroy();
        return false;
    }


    mb_initializationFlags |= initializedAllBit();
    return true;
}




[[nodiscard]] bool AudioManager3::initializeMemory()
{
    ma_result status = MA_SUCCESS;
    constexpr auto kSizeInBytes = sizeof(*m_audioBuf)
        + sizeof(*m_resampler)
        + sizeof(*m_ctx)
        + sizeof(*m_audioDev)
        + sizeof(*m_deviceList);


    m_underlyingMem = __rcast(byte*, util2_aligned_malloc(kSizeInBytes, CACHE_LINE_BYTES));
    status = (m_underlyingMem == nullptr) ? MA_ERROR : MA_SUCCESS;
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not allocate memory for Audio manager\n");
        return false;
	}

    /* Assign Memory regions */
    m_audioBuf  = __rcast( std::vector<byte>*, m_underlyingMem);
    m_resampler = __rcast(ma_resampler*, (m_underlyingMem + sizeof(*m_audioBuf) ));
    m_ctx       = __rcast(ma_context*,   (m_underlyingMem 
        + sizeof(*m_audioBuf) + sizeof(*m_resampler) 
    ));
    m_audioDev  = __rcast(ma_device*,   (m_underlyingMem 
        + sizeof(*m_audioBuf) + sizeof(*m_resampler) + sizeof(*m_ctx)
    ));
    m_deviceList = __rcast(capture_playback_pair*, m_underlyingMem 
        + sizeof(*m_audioBuf) + sizeof(*m_resampler) + sizeof(*m_ctx) + sizeof(*m_audioDev)
    );


    mb_initializationFlags |= initializedMemoryBit();
    return true;
}


[[nodiscard]] bool AudioManager3::initializeContext() noexcept
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
	
    mb_initializationFlags |= initializedContextBit();
    return true;
}


[[nodiscard]] bool AudioManager3::initializeDeviceList() noexcept
{
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
    m_deviceList->first.assign(pCaptureDeviceInfos, pCaptureDeviceInfos + captureDeviceCount);
    m_deviceList->second.assign(pPlaybackDeviceInfos, pPlaybackDeviceInfos + playbackDeviceCount);


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


    /* pick Defaults unless later specified otherwise */
    m_captureDeviceIdx  = static_cast<uint8_t>(cpDeviceIdx);
    m_playbackDeviceIdx = static_cast<uint8_t>(pbDeviceIdx);
    mb_initializationFlags |= initializedDeviceListBit();
    return true;
}


[[nodiscard]] bool AudioManager3::initializeAudioDevice(
    u8                        captureDeviceID,
    u8                        playbackDeviceID,
    void*                     custom_user_defined_pointer, 
    const ma_device_data_proc k_process_audio_chunk_functor,
    const u32                 k_optimal_latency_btwn_audio_req_ms,
    const u32                 k_desired_resample_rate,
    const ma_format           k_desired_data_format,
    const u32                 k_desired_channel_count
) noexcept {
	ma_result        status = MA_SUCCESS;
	ma_device_config device_config;
    ma_device_info   devInfo;


    m_captureDeviceIdx  = (captureDeviceID == 0xFF) ? m_captureDeviceIdx : captureDeviceID;
    m_playbackDeviceIdx = (playbackDeviceID == 0xFF) ? m_playbackDeviceIdx : playbackDeviceID;
    /* Invalid indices/Didn't init device list/Invalid arguments */
    if(m_ctx == nullptr
        || m_captureDeviceIdx > m_deviceList->first.size() 
        || m_playbackDeviceIdx > m_deviceList->second.size()
        || k_optimal_latency_btwn_audio_req_ms < 5
        || k_desired_channel_count > 2
        || k_desired_resample_rate == 0
    ) {
        return false;
    }


    // Audio Configuration
	device_config                   = ma_device_config_init(ma_device_type_duplex);
	device_config.sampleRate        = 0;
    device_config.capture.pDeviceID = &m_deviceList->first[m_captureDeviceIdx].id;
	device_config.capture.format    = k_desired_data_format;
	device_config.capture.channels  = k_desired_channel_count;
	device_config.capture.shareMode = ma_share_mode_shared;

    device_config.playback.pDeviceID = &m_deviceList->second[m_playbackDeviceIdx].id;
	device_config.playback.format     = k_desired_data_format;
	device_config.playback.channels   = k_desired_channel_count;
	device_config.playback.shareMode  = ma_share_mode_shared;

	device_config.dataCallback             = k_process_audio_chunk_functor;
    device_config.periodSizeInMilliseconds = k_optimal_latency_btwn_audio_req_ms;
    device_config.pUserData                = custom_user_defined_pointer;
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
        m_deviceList->second[m_playbackDeviceIdx].name,
        m_deviceList->first[m_captureDeviceIdx].name
    );


    mk_nativeDeviceSampleRate = m_audioDev->sampleRate;
    mk_desiredResampleRate    = k_desired_resample_rate;
    mb_initializationFlags |= initializedAudioDeviceBit();
    return true;
}


[[nodiscard]] bool AudioManager3::initializeRingBuffer(
    const ma_format kInDataFormat,
    const u32 kInChannelCount,
    const u32 kInDeviceSampleRate,
    const u32 kRingBufferSizeInSeconds
) noexcept {
    const u32 kBufferRingSize = kInDeviceSampleRate * kRingBufferSizeInSeconds;

    m_audioBuf->resize(kBufferRingSize * ma_get_bytes_per_frame(kInDataFormat, kInChannelCount));
    // ma_result status = ma_pcm_rb_init(
    //     dataFormat(), 
    //     kInChannelCount, 
    //     kBufferRingSize, 
    //     NULL, 
    //     NULL, 
    //     m_ringBuffer
    // );
	// if (status != MA_SUCCESS) {
    //     fprintf(stderr, "Could not create ring buffer - %s\n", ma_result_description(status));
    //     return false;
	// }

    mb_initializationFlags |= initializedAudioBufferBit();
    return true;
}


[[nodiscard]] bool AudioManager3::initializeResampler(
    const ma_format kInDataFormat,
    const u32 kInChannelCount,
    const u32 kInDeviceSampleRate,
    const u32 kInDesiredSampleRate
) {
    const ma_resampler_config kResampConfig = ma_resampler_config_init(
        kInDataFormat, 
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

    mb_initializationFlags |= initializedResamplerBit();
    return true;
}