#ifndef __MINIAUDIO_INIT_DEFINITION_HEADER__
#define __MINIAUDIO_INIT_DEFINITION_HEADER__
#include <util2/C/macro.h>
#include <util2/C/base_type.h>
#include <vector>
#include <miniaudio.h>


struct AudioManager2
{
public:
    using capture_playback_pair = std::pair<std::vector<ma_device_info>, std::vector<ma_device_info>>;
    using cap_plb_pair = capture_playback_pair;
    // bool create(
    //     const u32                 kChannelCount, 
    //     const u32                 kDeviceSampleRate,
    //     void*                     custom_user_defined_pointer, 
    //     const ma_device_data_proc process_audio_chunk_functor
    // );

    void destroy();


    __force_inline bool createContext() 
    {
        bool status = true;

        if( (m_underlyingMem != nullptr) || mb_initAll) {
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


    __force_inline bool getDeviceList(capture_playback_pair& out) {
        if(m_captureDeviceIdx == 0xFF || m_playbackDeviceIdx == 0xFF) {
            return false;
        }
        out = m_deviceList;
        return true;
    }


    inline bool selectDevicesAndFinalize(
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc k_process_audio_chunk_functor,
        const u32                 kOptimalLatencyBetweenAudioReq_ms /* milliseconds */,
        const u32                 kChannelCount,
        const u32                 kDesiredSampleRate,
        u8                        captureDeviceID    = 0xFF,
        u8                        playbackDeviceID   = 0xFF
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
            kChannelCount
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
            kChannelCount, 
            mk_DeviceSampleRate,
            kMaxSecondsToAcquire
        );
        if(!status) {
            destroy();
            return false;
        }

        status = initializeResampler(
            mk_ChannelCount, 
            mk_DeviceSampleRate,
            mk_DesiredResampleRate
        );
        if(!status) {
            destroy();
            return false;
        }


        mb_initAll = true;
        return true;
    }


    __force_inline bool start() {
        return ma_device_start(m_audioDev) == MA_SUCCESS;
    }
    __force_inline bool stop() {
        return ma_device_stop(m_audioDev) == MA_SUCCESS;
    }

    __force_inline auto* ringBufferHandle() const {
        return m_ringBuffer;
    }
    __force_inline auto* resamplerHandle() const {
        return m_resampler;
    }
    __force_inline u32 nativeSampleRate() const {
        return mk_DeviceSampleRate;
    }
    __force_inline ma_format outputFormat() const {
        return ma_format_f32;
    }
    __force_inline u32 channelCount() const {
        return mk_ChannelCount;
    }

private:
    [[nodiscard]] bool initializeMemory();
    [[nodiscard]] bool initializeContext()    noexcept;
    [[nodiscard]] bool initializeDeviceList() noexcept;
    [[nodiscard]] bool initializeAudioDevice(
        u8                        captureDeviceID,
        u8                        playbackDeviceID,
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc k_process_audio_chunk_functor,
        const u32                 k_optimal_latency_btwn_audio_req_ms,
        const u32                 k_desired_resample_rate,
        const u32                 k_desired_channel_count
    ) noexcept;
    [[nodiscard]] bool initializeRingBuffer(
        const u32 kChannelCount,
        const u32 kDeviceSampleRate,
        const u32 kRingBufferSizeInSeconds
    ) noexcept;
    [[nodiscard]] bool initializeResampler(
        const u32 kInChannelCount,
        const u32 kInDeviceSampleRate,
        const u32 kInDesiredSampleRate
    );

private:
    u32                   mk_ChannelCount        = UINT32_MAX;
    u32                   mk_DeviceSampleRate    = UINT32_MAX;
    u32                   mk_DesiredResampleRate = UINT32_MAX;
    u8                    m_reserved0[4];
    void*                 mk_userControlledPointer       = nullptr;
    ma_device_data_proc   mk_AudioProcessingUserFunction = nullptr;
    u8                    m_captureDeviceIdx  = 0xFF;
    u8                    m_playbackDeviceIdx = 0xFF;
    bool                  mb_initMem        = false;
    bool                  mb_initContext    = false;
    bool                  mb_initDevList    = false;
    bool                  mb_initAudioDev   = false;
    bool                  mb_initRingBuffer = false;
    bool                  mb_initResampler  = false;
    bool                  mb_initAll        = false;
    u8                    m_reserved1[7];
    byte*                 m_underlyingMem = nullptr;
    ma_context*           m_ctx           = nullptr;
    ma_device*            m_audioDev      = nullptr;
    ma_pcm_rb*            m_ringBuffer    = nullptr;
    ma_resampler*         m_resampler     = nullptr;
    capture_playback_pair m_deviceList; /* this buddy is a whole cache line >:( */
};

#endif /* __MINIAUDIO_INIT_DEFINITION_HEADER__ */
