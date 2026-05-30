#include <miniaudio.h>
#include <string>


struct WavWriter 
{
    ma_encoder encoder;
    bool opened = false;

    ~WavWriter() { 
        close(); 
    }


    bool open(
        const std::string& file, 
        ma_uint32          channels, 
        ma_uint32          rate, 
        ma_format          format = ma_format_f32
    ) {
        close();

        ma_encoder_config cfg = ma_encoder_config_init(
            ma_encoding_format_wav, 
            format, 
            channels, 
            rate
        );
        if (ma_encoder_init_file(file.c_str(), &cfg, &encoder) == MA_SUCCESS) {
            opened = true;
            return true;
        }
        return false;
    }

    void write(const void* pcm, ma_uint64 frames) {
        if (opened) {
            ma_encoder_write_pcm_frames(&encoder, pcm, frames, nullptr);
        }
        return;
    }

    void close() {
        if (opened) {
            ma_encoder_uninit(&encoder);
            opened = false;
        }
        return;
    }
};