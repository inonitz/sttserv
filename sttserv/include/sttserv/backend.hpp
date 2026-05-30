#ifndef __LLM_ASR_MODEL_UNIFIED_INTERFACE_DEFINITION_HEADER__
#define __LLM_ASR_MODEL_UNIFIED_INTERFACE_DEFINITION_HEADER__
#   include <sttserv/cmdline.hpp>
#   include <util2/C/macro.h>
#   include <array>
// Backend Headers
#   include <whisper.h>
#   include <parakeet.h>
#   include <sherpa-onnx/c-api/c-api.h>


using inferenceResultBuffer = std::array<char, 1024>;


struct STTSERVER_API WhisperBackendState {
    whisper_context* ctx;
    whisper_context_params ctx_params;
    whisper_full_params full_params;
};

struct STTSERVER_API ParakeetBackendState {
    parakeet_context* ctx;
    parakeet_context_params ctx_params;
    parakeet_full_params full_params;
};

struct STTSERVER_API SherpaBackendState {
    SherpaOnnxOfflineRecognizer*      ctx;
    SherpaOnnxOfflineStream*          tmpstream;
    SherpaOnnxOfflineModelConfig      model_params;
    SherpaOnnxOfflineRecognizerConfig full_params;
    
    std::string enc_path, dec_path, join_path, tok_path;
};


struct STTSERVER_API ModelBackend {
    BackendType m_type;

    union {
        WhisperBackendState* whisper;
        ParakeetBackendState* parakeet;
        SherpaBackendState* sherpa;
    } m_state;

    bool create(
        const CommandLineArguments& args, /* backend_type can be supplied in the command line */
        BackendType                 backend_type = BackendType::BACKEND_MAX
    );
    void destroy();
    
    __force_inline __hot bool transcribe(
        const f32* pcm, 
        size_t     frames, 
        u32        duration_ms, 
        u32        sample_rate
    ) {
        switch (m_type) {
            case BackendType::WHISPER:     return transcribe_whisper(pcm, frames, duration_ms);
            case BackendType::PARAKEET:    return transcribe_parakeet(pcm, frames, duration_ms);
            case BackendType::SHERPA_ONNX: return transcribe_sherpa(pcm, frames, sample_rate);
            case BackendType::BACKEND_MAX: default: return false;
        }
        return false;
    }

    __force_inline __hot bool result(inferenceResultBuffer& fixedSizeOutput) {
        switch (m_type) {
            case BackendType::WHISPER:     
            return results_whisper(fixedSizeOutput);
            case BackendType::PARAKEET:    
            return results_parakeet(fixedSizeOutput);
            case BackendType::SHERPA_ONNX: 
            return results_sherpa(fixedSizeOutput);
            case BackendType::BACKEND_MAX: 
            default: 
            return false;
        }
        return false;
    }
    

    void print_timings();
    void reset_timings();

private:
    bool init_whisper(const CommandLineArguments& args);
    bool init_parakeet(const CommandLineArguments& args);
    bool init_sherpa(const CommandLineArguments& args);

    __hot bool transcribe_whisper (const f32* pcm, size_t frames, u32 duration_ms);
    __hot bool transcribe_parakeet(const f32* pcm, size_t frames, u32 duration_ms);
    __hot bool transcribe_sherpa  (const f32* pcm, size_t frames, u32 sample_rate);
    __hot bool results_whisper (inferenceResultBuffer& result);
    __hot bool results_parakeet(inferenceResultBuffer& result);
    __hot bool results_sherpa  (inferenceResultBuffer& result);
};


#endif /* __LLM_ASR_MODEL_UNIFIED_INTERFACE_DEFINITION_HEADER__ */
