#include <miniaudio.h>
#include <stdio.h>


ma_resampler g_resampler;
FILE* g_pFile = NULL;


void data_callback(ma_device* pDevice, void* pOutput, const void* pInput, ma_uint32 frameCount) {
    float tempBuffer[4096]; // Buffer for 16kHz data
    ma_uint64 framesIn = frameCount;
    ma_uint64 framesOut = 4096;

    // 1. Resample Native Input -> 16kHz Mono
    ma_resampler_process_pcm_frames(&g_resampler, pInput, &framesIn, tempBuffer, &framesOut);

    // 2. Write to file for quality verification
    if (g_pFile) {
        fwrite(tempBuffer, sizeof(float), framesOut, g_pFile);
    }

    // 3. To "Listen": Resample 16kHz back to Native Rate for pOutput
    // Or, for a simple loopback test, just copy pInput to pOutput
    // Here we pass input to output so you hear the 'live' mic
    if (pOutput != NULL) {
        ma_copy_pcm_frames(pOutput, pInput, frameCount, ma_format_f32, 1);
    }
}

int main() {
    g_pFile = fopen("capture_16k.raw", "wb");
    ma_device_config config = ma_device_config_init(ma_device_type_duplex);
    
    config.sampleRate = 0;
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.playback.format = ma_format_f32;
    config.playback.channels = 1;
    config.dataCallback = data_callback;

    ma_device device;
    if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) return -1;

    // Initialize Resampler: Native -> 16000
    ma_resampler_config resampConfig = ma_resampler_config_init(
        ma_format_f32, 1, device.sampleRate, 16000, ma_resample_algorithm_linear
    );
    ma_resampler_init(&resampConfig, NULL, &g_resampler);

    printf("Hardware Rate: %d Hz. Recording to capture_16k.raw...\n", device.sampleRate);
    ma_device_start(&device);
    
    printf("Press Enter to stop...");
    getchar();

    ma_device_uninit(&device);
    ma_resampler_uninit(&g_resampler, NULL);
    fclose(g_pFile);
    return 0;
}