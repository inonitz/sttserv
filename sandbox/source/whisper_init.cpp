#include "sandbox/whisper_init.hpp"
#include <whisper.h>
#include <cxxopts.hpp>


bool parse_args(int argc, char** argv, WhisperParameters& outParams) {
    cxxopts::Options cmdOptions(argv[0], "Whisper.cpp Argument Parser");


    try {
        cmdOptions.add_options()
            // Flag                          Description                                     Value Binding
            // ------------------------------------------------------------------------------------------------------------------------
            ("h,help",            "show this help message and exit")
            ("t,threads",         "number of threads to use",                        cxxopts::value<i32>(outParams.m_numThreads))
            ("step",              "audio step size in milliseconds",                 cxxopts::value<i32>(outParams.m_stepMillisecond))
            ("length",            "audio length in milliseconds",                    cxxopts::value<i32>(outParams.m_lengthMilliseconds))
            ("keep",              "audio to keep from previous step in ms",          cxxopts::value<i32>(outParams.m_keepMilliseconds))
            ("c,capture",         "capture device ID",                               cxxopts::value<i32>(outParams.capture_id))
            ("mt,max-tokens",     "maximum number of tokens per audio chunk",        cxxopts::value<i32>(outParams.m_maxToken))
            ("ac,audio-ctx",      "audio context size (0 - all)",                    cxxopts::value<i32>(outParams.audio_ctx))
            ("bs,beam-size",      "beam size for beam search",                       cxxopts::value<i32>(outParams.beam_size))
            ("vth,vad-thold",     "voice activity detection threshold",              cxxopts::value<f32>(outParams.m_vadThreshold))
            ("fth,freq-thold",    "high-pass frequency cutoff",                      cxxopts::value<f32>(outParams.m_freqThreshold))
            // ("tr,translate",      "translate from source language to english",       cxxopts::value<bool>(params.mb_translate))
            ("nf,no-fallback",    "do not use temperature fallback while decoding",  cxxopts::value<bool>(outParams.mb_fallback)->implicit_value("false"))
            ("ps,print-special",  "print special tokens",                            cxxopts::value<bool>(outParams.mb_printSpecial))
            ("kc,keep-context",   "keep context between audio chunks",               cxxopts::value<bool>(outParams.mb_context))
            ("l,language",        "spoken language",                                 cxxopts::value<std::string>(outParams.m_lang))
            ("m,model",           "model path",                                      cxxopts::value<std::string>(outParams.m_modelFullpath))
            // ("f,file",            "text output file name",                           cxxopts::value<std::string>(params.m_fname_out))
            ("tdrz,tinydiarize",  "enable tinydiarize (requires tdrz model)",        cxxopts::value<bool>(outParams.mb_tinyDiarize))
            // ("sa,save-audio",     "save the recorded audio to a file",               cxxopts::value<bool>(params.mb_saveAudio))
            ("ng,no-gpu",         "disable GPU inference",                           cxxopts::value<bool>(outParams.mb_useGPU)->implicit_value("false"))
            ("fa,flash-attn",     "enable flash attention during inference",         cxxopts::value<bool>(outParams.mb_FlashAttention))
            ("nfa,no-flash-attn", "disable flash attention during inference",        cxxopts::value<bool>(outParams.mb_FlashAttention)->implicit_value("false"))
        ;


        auto result = cmdOptions.parse(argc, argv);
        if (result.count("help")) {
            fprintf(stdout, "Command line options:\n%s\n", cmdOptions.help().c_str());
            exit(0);
        }

    } catch (const cxxopts::exceptions::exception& e) {
        fprintf(stderr, "Error parsing Command line options: %s\n", e.what());
        return false;
    }


    if (outParams.m_lang != "auto" && whisper_lang_id(outParams.m_lang.c_str()) == -1) {        
        fprintf(stderr, "error: unknown language '%s'\nSee Command Line Options:\n%s", 
            outParams.m_lang.c_str(),
            cmdOptions.help().c_str()
        );
        return false;
    }


    /* Derive Parameters from command line arguments */
    outParams.m_numThreads         = std::min(
        std::max(outParams.m_numThreads, 1), 
        static_cast<i32>(std::thread::hardware_concurrency() / 2)
    );
    outParams.m_keepMilliseconds   = std::min(outParams.m_keepMilliseconds,   outParams.m_stepMillisecond);
    outParams.m_lengthMilliseconds = std::max(outParams.m_lengthMilliseconds, outParams.m_stepMillisecond);
    outParams.mk_numSamplesStep      = (1e-3*outParams.m_stepMillisecond   ) * WHISPER_SAMPLE_RATE;
    outParams.mk_numSamplesLength    = (1e-3*outParams.m_lengthMilliseconds) * WHISPER_SAMPLE_RATE;
    outParams.mk_numSamplesKeep      = (1e-3*outParams.m_keepMilliseconds  ) * WHISPER_SAMPLE_RATE;
    outParams.mk_numSamplesThirtySec = (1e-3*30000.0                       ) * WHISPER_SAMPLE_RATE;
    outParams.mkb_useVAD             = outParams.mk_numSamplesStep <= 0; /* Sliding Window Mode uses VAD */
    outParams.mk_NumTillNewline      = !outParams.mkb_useVAD ? 
        std::max(1, outParams.m_lengthMilliseconds / outParams.m_stepMillisecond - 1) 
        : 
        1; // number of steps to print new line

    outParams.mb_timestamps = outParams.mkb_useVAD;
    outParams.mb_context   |= !outParams.mkb_useVAD;
    outParams.m_maxToken    = 0;
    return true;
}


bool init_context(
    const WhisperParameters&  inArgParsed,
    WhisperContextParameters& outCtxParams,
    WhisperContextHandle*     outContext
) {
    outCtxParams = whisper_context_default_params();
    outCtxParams.use_gpu    = inArgParsed.mb_useGPU;
    outCtxParams.flash_attn = inArgParsed.mb_FlashAttention;

    *outContext = whisper_init_from_file_with_params(
        inArgParsed.m_modelFullpath.c_str(), 
        outCtxParams
    );


    return *outContext != nullptr;
}