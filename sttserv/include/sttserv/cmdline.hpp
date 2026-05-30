#ifndef __LLM_ASR_MODEL_UNIFIED_INTERFACE_COMMAND_LINE_ARGUMENTS_DEFINITION_HEADER__
#define __LLM_ASR_MODEL_UNIFIED_INTERFACE_COMMAND_LINE_ARGUMENTS_DEFINITION_HEADER__
#   include <sttserv/sttserver_api.h>
#   include <sttserv/backendType.hpp>
#   include <util2/C/base_type.h>
#   include <string>


struct STTSERVER_API CommandLineArguments {
    i32 m_numThreads         = 1;
    i32 capture_id           = -1;
    i32 playback_id          = -1;
    i32 m_deviceID           = -1;

    i32 mk_prevChunkSize     = 0;
    i32 mk_currChunkSize     = 0;
    i32 mk_postChunkSize     = 0;

    bool mb_translateEnglish = false;
    bool mb_FlashAttention   = true;

    std::string m_chosenBackend     = "sherpaonnx-parakeet"; /* whisper-whisper, whisper-parakeet, sherpaonnx-parakeet, sherpaonnx-whisper */
    BackendType m_chosenBackendType = BackendType::BACKEND_MAX;
    std::string m_lang           = "en";
    std::string m_modelFullpath  = "models/ggml-base.en.bin"; 
    std::string m_modelDirectory = "models";
    std::string m_encoderName    = "encoder.onnx";
    std::string m_decoderName    = "decoder.onnx";
    std::string m_joinerName     = "joiner.onnx";
    std::string m_tokensTxtName  = "tokens.txt";

    static constexpr u32 kInferenceSampleRate = 16000;
    static constexpr u32 kMillisecondsIn1Second = 1000;
};


STTSERVER_API bool parse_commandline_args(int argc, char** argv, CommandLineArguments& outParams);
STTSERVER_API void print_arguments(CommandLineArguments const& params);


#endif /* __LLM_ASR_MODEL_UNIFIED_INTERFACE_COMMAND_LINE_ARGUMENTS_DEFINITION_HEADER__ */
