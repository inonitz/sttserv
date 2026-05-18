#ifndef __MINIAUDIO_INIT_DEFINITION_HEADER__
#define __MINIAUDIO_INIT_DEFINITION_HEADER__
#include <util2/C/macro.h>
#include <util2/C/base_type.h>
#include <vector>
#include <miniaudio.h>


struct AudioManager
{
public:
    using capture_playback_pair = std::pair<std::vector<ma_device_info>, std::vector<ma_device_info>>;

    bool create(
        const u32                 kChannelCount, 
        const u32                 kDeviceSampleRate,
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc process_audio_chunk_functor
    );

    void destroy();


    __force_inline bool createContext(
        const u32 kChannelCount, 
        const u32 kDeviceSampleRate
    ) {
        const u32 kMaxSecondsToAcquire = 10;
        bool status = true;

        if( (m_underlyingMem != nullptr) || mb_initAll) {
            return true;
        }
        

        status = initializeMemory();
        if(!status) {
            destroy();
            return false;
        }

        status = initializeRingBuffer(
            kChannelCount, 
            kDeviceSampleRate,
            kMaxSecondsToAcquire
        );
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


    __force_inline bool selectDevicesAndFinalize(
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc k_process_audio_chunk_functor,
        const u32                 k_optimal_latency_btwn_audio_req_ms = 1 /* milliseconds */,
        u8                        captureDeviceID  = 0xFF,
        u8                        playbackDeviceID = 0xFF
    ) {
        mb_initAll = initializeAudioDevice(
            captureDeviceID, 
            playbackDeviceID, 
            custom_user_defined_pointer, 
            k_process_audio_chunk_functor,
            k_optimal_latency_btwn_audio_req_ms
        );


        return mb_initAll;
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

private:
    [[nodiscard]] bool initializeMemory();
    [[nodiscard]] bool initializeRingBuffer(
        const u32 kChannelCount,
        const u32 kDeviceSampleRate,
        const u32 kRingBufferSizeInSeconds
    ) noexcept;
    [[nodiscard]] bool initializeContext()    noexcept;
    [[nodiscard]] bool initializeDeviceList() noexcept;
    [[nodiscard]] bool initializeAudioDevice(
        u8                        captureDeviceID,
        u8                        playbackDeviceID,
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc k_process_audio_chunk_functor,
        const u32                 k_optimal_latency_btwn_audio_req_ms
    ) noexcept;

private:
    u32                 mk_ChannelCount        = UINT32_MAX;
    u32                 mk_DeviceSampleRate    = UINT32_MAX;
    void*               mk_CustomUserControlledPointer = nullptr;
    ma_device_data_proc mk_AudioProcessingUserFunction = nullptr;
    u8                  m_captureDeviceIdx  = 0xFF;
    u8                  m_playbackDeviceIdx = 0xFF;
    union {
        struct pack {
            bool mb_initMem;
            bool mb_initRingBuffer;
            bool mb_initContext;
            bool mb_initDevList;
            bool mb_initAudioDev;
            bool mb_initAll;
        };
        bool mb_init[6] = {false};
    };
    byte*                 m_underlyingMem     = nullptr;
    ma_pcm_rb*            m_ringBuffer        = nullptr;
    ma_context*           m_ctx               = nullptr;
    ma_device*            m_audioDev          = nullptr;
    capture_playback_pair m_deviceList; /* this buddy is a whole cache line >:( */
};

#endif /* __MINIAUDIO_INIT_DEFINITION_HEADER__ */
