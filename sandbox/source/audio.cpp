#include "sandbox/audio.hpp"
#include <util2/C/aligned_malloc.h>
#include <cstdio>


bool AudioManager::create(
    const u32                 kChannelCount, 
    const u32                 kDeviceSampleRate,
    void*                     custom_user_defined_pointer, 
    const ma_device_data_proc process_audio_chunk_functor
) {
    const u32        kInitialRingBufferSize = kDeviceSampleRate * 10;
    ma_device_info*  pCaptureDeviceInfos;
    ma_device_info*  pPlaybackDeviceInfos;
    u32              captureDeviceCount;
    u32              playbackDeviceCount;
    u32              cpDeviceIdx = 0;
    u32              pbDeviceIdx = 0;

	ma_device_config device_config;
	ma_result        status = MA_SUCCESS;



    constexpr auto kSizeInBytes = sizeof(*m_ctx) + sizeof(*m_ringBuffer) + sizeof(*m_audioDev);
    m_underlyingMem = __rcast(byte*, util2_aligned_malloc(kSizeInBytes, CACHE_LINE_BYTES));
    status = m_underlyingMem == nullptr ? MA_ERROR : MA_SUCCESS;
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not allocate memory for Audio manager\n");
        return false;
	}

    m_ctx        = __rcast(ma_context*, (m_underlyingMem + 0));
    m_ringBuffer = __rcast(ma_pcm_rb*, (m_underlyingMem + sizeof(*m_ctx)));
    m_audioDev   = __rcast(ma_device*, (m_underlyingMem + sizeof(*m_ctx) + sizeof(*m_ringBuffer)));


    /* Mini-Audio Init - Circular SPSC Ringbuffer, Custom Context, Duplex, */
    status = ma_pcm_rb_init(
        ma_format_f32, 
        kChannelCount, 
        kInitialRingBufferSize, 
        NULL, 
        NULL, 
        m_ringBuffer
    );
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create ring buffer\n");
        return false;
	}

    status = ma_context_init(NULL, 0, NULL, m_ctx);
    if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not create Miniaudio Context\n");
        ma_pcm_rb_uninit(m_ringBuffer);
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
        fprintf(stderr, "Could not iterate over available capture devices\n");
        ma_context_uninit(m_ctx);
        ma_pcm_rb_uninit(m_ringBuffer);
		return false;
    }


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


    // Audio Configuration
	device_config                    = ma_device_config_init(ma_device_type_duplex);
	device_config.sampleRate         = kDeviceSampleRate;
    device_config.capture.pDeviceID = &pCaptureDeviceInfos[cpDeviceIdx].id;
	device_config.capture.format     = ma_format_f32;
	device_config.capture.channels   = kChannelCount;
	device_config.capture.shareMode  = ma_share_mode_shared;

    device_config.playback.pDeviceID = &pPlaybackDeviceInfos[pbDeviceIdx].id;
	device_config.playback.format     = ma_format_f32;
	device_config.playback.channels   = kChannelCount;
	device_config.playback.shareMode  = ma_share_mode_shared;

	device_config.dataCallback       = process_audio_chunk_functor;
	device_config.periodSizeInFrames = 960;
    device_config.pUserData          = custom_user_defined_pointer;


    // Initialize the audio devices
	status = ma_device_init(NULL, &device_config, m_audioDev);
	if (status != MA_SUCCESS) {
        fprintf(stderr, "Could not open the default capture and/or playback devices\n");
        ma_context_uninit(m_ctx);
        ma_pcm_rb_uninit(m_ringBuffer);
		return false;
	}
    fprintf(stdout, "Picked Audio Device %s\n", pCaptureDeviceInfos[0].name);


    return true;
}


void AudioManager::destroy()
{
    if(m_underlyingMem) {
        ma_device_uninit(m_audioDev);
        ma_pcm_rb_uninit(m_ringBuffer);
        ma_context_uninit(m_ctx);
        util2_aligned_free(m_underlyingMem);
    }
    m_underlyingMem = nullptr;
    return;
}
