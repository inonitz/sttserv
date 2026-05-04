#ifndef __WHISPER_CPP_INIT_DEFINITION_HEADER__
#define __WHISPER_CPP_INIT_DEFINITION_HEADER__
#include <util2/C/base_type.h>
#include <thread>
#include <string>


using WhisperContextParameters     = struct whisper_context_params;
using WhisperFullContextParameters = struct whisper_full_params;
using WhisperContext               = struct whisper_context;
using WhisperContextHandle         = WhisperContext*;


struct WhisperParameters 
{
    i32 m_numThreads         = std::thread::hardware_concurrency() / 4;
    i32 m_stepMillisecond    = 3000;
    i32 m_lengthMilliseconds = 10000;
    i32 m_keepMilliseconds   = 200;

    i32 capture_id = -1;
    i32 m_maxToken = 32;
    i32 audio_ctx  = 0;
    i32 beam_size  = -1;

    f32 m_vadThreshold  = 0.6f;
    f32 m_freqThreshold = 100.0f;

    bool mb_fallback       = true;
    bool mb_printSpecial   = false;
    bool mb_context        = false;
    bool mb_timestamps     = true;
    bool mb_tinyDiarize    = false;
    bool mb_useGPU         = true;
    bool mb_FlashAttention = true;
    bool mkb_useVAD;

    std::string m_lang          = "en";
    std::string m_modelFullpath = "models/ggml-base.en.bin";

    i32 mk_numSamplesStep;
    i32 mk_numSamplesLength;
    i32 mk_numSamplesKeep;
    i32 mk_numSamplesThirtySec;
    i32 mk_NumTillNewline;
};


bool parse_args(int argc, char** argv, 
    WhisperParameters& outParameters
);


bool init_context(
    const WhisperParameters&  inArgParsed,
    WhisperContextParameters& outCtxParams,
    WhisperContextHandle*     outContext
);


void destroy_context(
    WhisperContextHandle outContext
);



#endif /* __WHISPER_CPP_INIT_DEFINITION_HEADER__ */
