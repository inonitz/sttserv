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

        if( (m_underlyingMem != nullptr) || m_init) {
            return true;
        }
        

        status = initializeMemory();
        if(!status) {
            return false;
        }

        status = initializeRingBuffer(
            kChannelCount, 
            kDeviceSampleRate,
            kMaxSecondsToAcquire
        );
        if(!status) {
            return false;
        }

        status = initializeContext();
        if(!status) {
            return false;
        }

        status = initializeDeviceList();
        if(!status) {
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
        uint8_t                   captureDeviceID  = 0xFF,
        uint8_t                   playbackDeviceID = 0xFF
    ) {
        m_init = initializeAudioDevice(
            captureDeviceID, 
            playbackDeviceID, 
            custom_user_defined_pointer, 
            k_process_audio_chunk_functor
        );


        return m_init;
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
        uint8_t                   captureDeviceID,
        uint8_t                   playbackDeviceID,
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc k_process_audio_chunk_functor
    ) noexcept;

private:
    u32                   mk_ChannelCount     = UINT32_MAX;
    u32                   mk_DeviceSampleRate = UINT32_MAX;
    void*                 mk_CustomUserControlledPointer = nullptr;
    ma_device_data_proc   mk_AudioProcessingUserFunction = nullptr;
    uint8_t               m_captureDeviceIdx  = 0xFF;
    uint8_t               m_playbackDeviceIdx = 0xFF;
    bool                  m_init              = false;
    uint8_t               m_reserved[5]{0};
    byte*                 m_underlyingMem     = nullptr;
    ma_context*           m_ctx               = nullptr;
    ma_pcm_rb*            m_ringBuffer        = nullptr;
    ma_device*            m_audioDev          = nullptr;
    capture_playback_pair m_deviceList; /* this buddy is a whole cache line >:( */
};

#endif /* __MINIAUDIO_INIT_DEFINITION_HEADER__ */
