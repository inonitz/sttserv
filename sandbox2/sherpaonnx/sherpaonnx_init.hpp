#ifndef __WHISPER_CPP_INIT_DEFINITION_HEADER__
#define __WHISPER_CPP_INIT_DEFINITION_HEADER__
#include <util2/C/base_type.h>
#include <sherpa-onnx/c-api/c-api.h>
#include <thread>
#include <string>


using SherpaOnnxContextParameters        = SherpaOnnxOfflineModelConfig;
using SherpaOnnxFullContextParameters    = SherpaOnnxOfflineRecognizerConfig;
using SherpaOnnxContext                  = SherpaOnnxOfflineRecognizer;
using SherpaOnnxInferenceStream          = SherpaOnnxOfflineStream;
using SherpaOnnxInferenceResult          = SherpaOnnxOfflineRecognizerResult;

using SherpaOnnxContextParametersHandle     = SherpaOnnxContextParameters*;
using SherpaOnnxFullContextParametersHandle = SherpaOnnxFullContextParameters*;
using SherpaOnnxContextHandle               = SherpaOnnxContext*;
using SherpaOnnxStreamHandle                = SherpaOnnxInferenceStream*;
using SherpaOnnxInferenceResultHandle       = SherpaOnnxOfflineRecognizerResult*;


struct CommandLineArguments 
{
    static constexpr u32 kInferenceSampleRate = 16000;

    i32 m_numThreads         = std::thread::hardware_concurrency() / 4;
    i32 m_stepMillisecond    = 3000;
    i32 m_lengthMilliseconds = 10000;
    i32 m_keepMilliseconds   = 200;

    i32 capture_id  = -1;
    i32 playback_id = -1;
    i32 m_deviceID  = -1;
    i32 audio_ctx  = 0;
    i32 beam_size  = -1;

    bool mb_translateEnglish = false;
    bool mb_fallback         = true;
    bool mb_printSpecial     = false;
    bool mb_context          = false;
    bool mb_timestamps       = true;
    bool mb_FlashAttention   = true;
    bool mkb_useVAD;

    std::string m_lang           = "en";
    std::string m_modelDirectory = "models";
    std::string m_encoderName    = "encoder.onnx";
    std::string m_decoderName    = "decoder.onnx";
    std::string m_joinerName     = "joiner.onnx";
    std::string m_tokensTxtName  = "tokens.txt";

    i32 mk_numSamplesLength;
    i32 mk_numSamplesKeep;
};


bool parse_commandline_args(int argc, char** argv, 
    CommandLineArguments& outParameters
);

bool sherpaonnx_init_context(
    const CommandLineArguments&       inArgParsed,
    SherpaOnnxContextParametersHandle outCtxParams,
    SherpaOnnxContextHandle*          outContext
);

void sherpaonnx_destroy_context(
    SherpaOnnxContextHandle outContext
);


// bool crispasr_init_context(
//     const WhisperParameters&   inArgParsed,
//     WhisperContextParameters&  outWhisperCtxParams,
//     CrispASRContextParameters& outCrisprCtxParams,
//     CrispASRContextHandle*     outContext
// );

// void crispasr_destroy_context(
//     CrispASRContextHandle outContext
// );



#endif /* __WHISPER_CPP_INIT_DEFINITION_HEADER__ */
