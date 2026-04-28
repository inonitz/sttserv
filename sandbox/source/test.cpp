#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>
#include <whisper.h>
#include <vector>


void SDLCALL onAudioReceiveCallback(
    void*            userdata, 
    SDL_AudioStream* stream, 
    int              additional_amount, 
    int              total_amount
);





int main(int argc, char *argv[]) {
    constexpr uint64_t kMinimumSecondsToCapture = 10;
    int32_t            status  = 0;
    bool               running = true;
    int32_t            streamAvailable = 0;
    int32_t            audioBytesRead  = 0;
    std::vector<float> audioBuffer;
    SDL_AudioSpec      desired_spec;
    SDL_AudioStream*   stream = nullptr;

    if(!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "Failure to initialize SDL3 Audio Subsystem - %s", SDL_GetError());
        status = -1;
        goto main_cleanup;
    }


    desired_spec = (SDL_AudioSpec){
        SDL_AUDIO_F32,
        1,
        44100
    };
    stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_RECORDING, 
        &desired_spec, 
        onAudioReceiveCallback,

    );

SDL_AudioStreamCallback 
    status = (SDL_ResumeAudioStreamDevice(stream) == false) || (stream == nullptr);
    if (status) {
        SDL_LogError(0, "Failed to open microphone: %s", SDL_GetError());
        status = -1;
        goto main_cleanup;
    }


    audioBuffer.resize(kMinimumSecondsToCapture * desired_spec.freq)
    while (running) {
        streamAvailable = SDL_GetAudioStreamAvailable(stream);
        if(!streamAvailable) {
            continue;
        }


        audioBytesRead = SDL_GetAudioStreamData(stream, 
            audioBuffer.data(), 
            audioBuffer.size() * sizeof(float)
        );
        if (audioBytesRead > 0) {

        }
        // SDL_Delay(10); 
    }


main_cleanup:
    if(stream) {
        SDL_DestroyAudioStream(stream);
    }
    SDL_Quit();
    return status;
}




void SDLCALL onAudioReceiveCallback(
    void*            userdata, 
    SDL_AudioStream* stream, 
    int              additional_amount, 
    int              total_amount
) {
    
}