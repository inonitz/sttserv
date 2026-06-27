#ifndef __MINIAUDIO_INIT_DEFINITION_HEADER__
#define __MINIAUDIO_INIT_DEFINITION_HEADER__
#include <util2/C/macro.h>
#include <util2/C/base_type.h>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <miniaudio.h>


struct CaptureDevice
{
public:
    using captureDeviceList      = std::vector<ma_device_info>;
    using pcm_data_user_callback = std::function<
        void(
            f32*        outPCMBuf, 
            u32         outPCMBufSize, 
            std::mutex& lockBuf,
            void*       customData
        )
        >;
    static constexpr u32 kMaxSecondsToAcquire = 10;


    void destroy();

    __force_inline bool createContext() 
    {
        bool status = true;

        if( (m_underlyingMem != nullptr) || isInit(kInitStateBitAll()) ) {
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


    __force_inline bool getDeviceList(captureDeviceList& out) {
        if(m_captureDeviceIdx == 0xFF) {
            return false;
        }
        out = m_deviceList;
        return true;
    }


    inline bool selectDevicesAndFinalize(
        pcm_data_user_callback custom_user_defined_pointer, 
        const u32              kOptimalLatencyBetweenAudioReq_ms /* milliseconds */,
        const u32              kDesiredSampleRate,
        u8                     captureDeviceID = 0xFF
    ) {
        bool status = true;
        status = initializeAudioDevice(
            captureDeviceID,
            custom_user_defined_pointer,
            kOptimalLatencyBetweenAudioReq_ms,
            kDesiredSampleRate,
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
            channelCount(), 
            nativeSampleRate(),
            kMaxSecondsToAcquire
        );
        if(!status) {
            destroy();
            return false;
        }

        status = initializeResampler(
            channelCount(), 
            nativeSampleRate(),
            resampleRate()
        );
        if(!status) {
            destroy();
            return false;
        }


        setInitState(kInitStateBitAll());
        return true;
    }


    __force_inline bool start() {
        bool status = (ma_device_start(audio_dev_ptr()) == MA_SUCCESS);
        m_consumer = std::thread([this]() { 
            consumerCallback(*this);
        });
        return status;
    }
    __force_inline bool stop() {
        m_stopRequested = true;
        auto status = ma_device_stop(audio_dev_ptr()) == MA_SUCCESS;
        if(m_consumer.joinable()) {
            m_consumer.join();
        }
        return status;
    }

    __force_inline auto* ringBufferHandle() const {
        return ringbuffer_ptr();
    }
    __force_inline const auto* resamplerHandle() const {
        return resampler_ptr();
    }
    __force_inline u32 nativeSampleRate() const {
        return mk_DeviceSampleRate;
    }
    __force_inline u32 resampleRate() const {
        return mk_DesiredResampleRate;
    }
    __force_inline ma_format outputFormat() const {
        return ma_format_f32;
    }
    __force_inline u32 channelCount() const {
        return 1;
    }

private:
    __force_inline ma_context* context_ptr() const noexcept {
        return reinterpret_cast<ma_context*>(m_underlyingMem + m_ctxOffset);
    }
    __force_inline ma_device* audio_dev_ptr() const noexcept {
        return reinterpret_cast<ma_device*>(m_underlyingMem + m_audioDevOffset);
    }
    __force_inline ma_pcm_rb* ringbuffer_ptr() const noexcept {
        return reinterpret_cast<ma_pcm_rb*>(m_underlyingMem + m_ringBufferOffset);
    }
    __force_inline ma_resampler* resampler_ptr() const noexcept {
        return reinterpret_cast<ma_resampler*>(m_underlyingMem + m_resamplerOffset);
    }

    constexpr u8 kInitStateBitMem()        const noexcept { return 0; }
    constexpr u8 kInitStateBitContext()    const noexcept { return 1; }
    constexpr u8 kInitStateBitDevList()    const noexcept { return 2; }
    constexpr u8 kInitStateBitAudioDev()   const noexcept { return 3; }
    constexpr u8 kInitStateBitRingBuffer() const noexcept { return 4; }
    constexpr u8 kInitStateBitResampler()  const noexcept { return 5; }
    constexpr u8 kInitStateBitAll()        const noexcept { return 7; }
    constexpr void resetInitState()     noexcept { mb_initState = 0; }
    constexpr void setInitState(u8 bit) noexcept { bit &= 8; mb_initState |= (1u << bit); }
    constexpr bool isInit(u8 bit)       noexcept { bit &= 8; return mb_initState & (1u << bit); }


    [[nodiscard]] bool initializeMemory();
    [[nodiscard]] bool initializeContext()    noexcept;
    [[nodiscard]] bool initializeDeviceList() noexcept;
    [[nodiscard]] bool initializeAudioDevice(
        u8                     captureDeviceID,
        pcm_data_user_callback k_custom_user_defined_pointer, 
        const u32              k_optimal_latency_btwn_audio_req_ms,
        const u32              k_desired_resample_rate,
        const u32              k_desired_channel_count
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


    static void producerCallback(
        ma_device*  pDevice, 
        void*       pOutput, 
        const void* pInput, 
        ma_uint32   frameCount
    );
    static void consumerCallback(CaptureDevice& ctx);

private:
    // bool mb_initMem        = false; /* Bit 0 */
    // bool mb_initContext    = false; /* Bit 1 */
    // bool mb_initDevList    = false; /* Bit 2 */
    // bool mb_initAudioDev   = false; /* Bit 3 */
    // bool mb_initRingBuffer = false; /* Bit 4 */
    // bool mb_initResampler  = false; /* Bit 5 */
    // bool mb_initAll        = false; /* Bit 7 */
    /* Hot Path */
    byte*                 m_underlyingMem = nullptr;
    u16                   m_ctxOffset;
    u16                   m_audioDevOffset;
    u16                   m_ringBufferOffset;
    u16                   m_resamplerOffset;

    std::thread             m_consumer;
    std::mutex              m_lock;
    std::condition_variable m_dataCV;
    std::atomic<bool>       m_dataReady;
    std::atomic<bool>       m_stopRequested;
    u8                      m_reserved0[2];
    std::vector<f32>        m_resampleBuffer;

    /* Colder Paths. Will still be fetched given that this struct stays below 128 Bytes */
    u32                    mk_DeviceSampleRate      = UINT32_MAX;
    u32                    mk_DesiredResampleRate   = UINT32_MAX;
    uint8_t                m_reserved1[4];
    pcm_data_user_callback mk_userControlledPointer = nullptr;
    u8                     mb_initState             = 0b0000'0000;
    u8                     m_captureDeviceIdx       = 0xFF;
    u8                     m_reserved2[6];
    captureDeviceList      m_deviceList;

    // ma_context*           m_ctx           = nullptr;
    // ma_device*            m_audioDev      = nullptr;
    // ma_pcm_rb*            m_ringBuffer    = nullptr;
    // ma_resampler*         m_resampler     = nullptr;
};

#endif /* __MINIAUDIO_INIT_DEFINITION_HEADER__ */
