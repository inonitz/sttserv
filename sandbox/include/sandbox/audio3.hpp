#ifndef __MINIAUDIO_INIT3_DEFINITION_HEADER__
#define __MINIAUDIO_INIT3_DEFINITION_HEADER__
#include <util2/C/macro.h>
#include <util2/C/base_type.h>
#include <vector>
#include <miniaudio.h>


struct AudioManager3
{
public:
    using capture_playback_pair = std::pair<std::vector<ma_device_info>, std::vector<ma_device_info>>;
    using cap_plb_pair = capture_playback_pair;
    using byte  = ma_format;
    // bool create(
    //     const u32                 kChannelCount, 
    //     const u32                 kDeviceSampleRate,
    //     void*                     custom_user_defined_pointer, 
    //     const ma_device_data_proc process_audio_chunk_functor
    // );

    void destroy();


    [[nodiscard]] bool createContext();
    [[nodiscard]] bool selectDevicesAndFinalize(
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc k_process_audio_chunk_functor,
        const u32                 kOptimalLatencyBetweenAudioReq_ms /* milliseconds */,
        const u32                 kDesiredSampleRate,
        u8                        captureDeviceID    = 0xFF,
        u8                        playbackDeviceID   = 0xFF
    );


    __force_inline bool getDeviceList(capture_playback_pair& out) {
        if(m_captureDeviceIdx == 0xFF || m_playbackDeviceIdx == 0xFF) {
            return false;
        }
        out = *m_deviceList;

        return true;
    }


    __force_inline bool start() {
        return ma_device_start(m_audioDev) == MA_SUCCESS;
    }
    __force_inline bool stop() {
        return ma_device_stop(m_audioDev) == MA_SUCCESS;
    }

    __force_inline auto* resamplerHandle() const {
        return m_resampler;
    }
    __force_inline u32 nativeSampleRate() const {
        return mk_nativeDeviceSampleRate;
    }
    __force_inline ma_format dataFormat() const {
        return ma_format_f32;
    }
    __force_inline u32 channelCount() const {
        return 1;
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
        const ma_format           k_desired_data_format,
        const u32                 k_desired_channel_count
    ) noexcept;
    [[nodiscard]] bool initializeRingBuffer(
        const ma_format kInDataFormat,
        const u32       kInChannelCount,
        const u32       kDeviceSampleRate,
        const u32       kRingBufferSizeInSeconds
    ) noexcept;
    [[nodiscard]] bool initializeResampler(
        const ma_format kInDataFormat,
        const u32       kInChannelCount,
        const u32       kInDeviceSampleRate,
        const u32       kInDesiredSampleRate
    );

private:
    constexpr u8 initializedMemoryBit()      const { return 0b00000001; }
    constexpr u8 initializedAudioBufferBit() const { return 0b00000010; }
    constexpr u8 initializedContextBit()     const { return 0b00000100; }
    constexpr u8 initializedDeviceListBit()  const { return 0b00001000; }
    constexpr u8 initializedAudioDeviceBit() const { return 0b00010000; }
    constexpr u8 initializedResamplerBit()   const { return 0b00100000; }
    constexpr u8 initializedAllBit()         const { return 0b01000000; }

    constexpr bool isInitializedMemory()      const { return mb_initializationFlags & initializedMemoryBit(); }
    constexpr bool isInitializedAudioBuffer() const { return mb_initializationFlags & initializedAudioBufferBit(); }
    constexpr bool isInitializedContext()     const { return mb_initializationFlags & initializedContextBit(); }
    constexpr bool isInitializedDeviceList()  const { return mb_initializationFlags & initializedDeviceListBit(); }
    constexpr bool isInitializedAudioDevice() const { return mb_initializationFlags & initializedAudioDeviceBit(); }
    constexpr bool isInitializedResampler()   const { return mb_initializationFlags & initializedResamplerBit(); }
    constexpr bool isInitializedAll()         const { return mb_initializationFlags & initializedAllBit(); }

private:
    /*
        bool mb_initMem         = false;
        bool mb_initAudioBuffer = false;
        bool mb_initContext     = false;
        bool mb_initDevList     = false;
        bool mb_initAudioDev    = false;
        bool mb_initResampler   = false;
        bool mb_initAll         = false;
    */
    bool mb_initializationFlags = 0;
    u8   m_captureDeviceIdx     = 0xFF;
    u8   m_playbackDeviceIdx    = 0xFF;
    u8   m_reserved[5];
    u32  mk_nativeDeviceSampleRate = UINT32_MAX;
    u32  mk_desiredResampleRate    = UINT32_MAX;

    byte*                  m_underlyingMem = nullptr;
    std::vector<byte>*     m_audioBuf      = nullptr;
    ma_resampler*          m_resampler     = nullptr;
    ma_context*            m_ctx           = nullptr;
    ma_device*             m_audioDev      = nullptr;
    capture_playback_pair* m_deviceList; /* this buddy is a whole cache line >:( */
};

#endif /* __MINIAUDIO3_INIT_DEFINITION_HEADER__ */
