#ifndef __MINIAUDIO_INIT_DEFINITION_HEADER__
#define __MINIAUDIO_INIT_DEFINITION_HEADER__
#include <miniaudio.h>
#include <util2/C/macro.h>
#include <util2/C/base_type.h>


struct AudioManager
{
public:
    bool create(
        const u32                 kChannelCount, 
        const u32                 kDeviceSampleRate,
        void*                     custom_user_defined_pointer, 
        const ma_device_data_proc process_audio_chunk_functor
    );
    void destroy();


    __force_inline bool start() {
        return ma_device_start(m_audioDev) == MA_SUCCESS;
    }
    __force_inline bool stop() {
        return ma_device_stop(m_audioDev) == MA_SUCCESS;
    }

public:
    byte*       m_underlyingMem = nullptr;
    ma_context* m_ctx;
    ma_pcm_rb*  m_ringBuffer;
    ma_device*  m_audioDev;
};

#endif /* __MINIAUDIO_INIT_DEFINITION_HEADER__ */
