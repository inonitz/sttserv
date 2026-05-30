#include "sherpaonnx_init.hpp"
#include "sherpa-onnx/c-api/c-api.h"
#include <cxxopts.hpp>
#include <util2/C/macro.h>
#include <util2/C/debug_macro.h>


bool parse_commandline_args(int argc, char** argv, CommandLineArguments& outParams) {
    cxxopts::Options cmdOptions(argv[0], "Parakeet.cpp Argument Parser");


    try {
        cmdOptions.add_options()
            // Flag                          Description                                     Value Binding
            // ------------------------------------------------------------------------------------------------------------------------
            ("h,help",            "show this help message and exit")
            ("t,threads",         "number of threads to use",                                cxxopts::value<i32>(outParams.m_numThreads))
            ("step",              "audio step size in milliseconds",                         cxxopts::value<i32>(outParams.m_stepMillisecond))
            ("length",            "audio length in milliseconds",                            cxxopts::value<i32>(outParams.m_lengthMilliseconds))
            ("keep",              "audio to keep from previous step in ms",                  cxxopts::value<i32>(outParams.m_keepMilliseconds))
            ("c,captureid",       "capture  device ID",                                      cxxopts::value<i32>(outParams.capture_id))
            ("p,playbackid",      "playback device ID",                                      cxxopts::value<i32>(outParams.playback_id))
            ("gid,gpudeviceid",   "Device ID of the GPU to run inference on",                cxxopts::value<i32>(outParams.m_deviceID))

            ("ac,audio-ctx",      "audio context size (0 - all)",                            cxxopts::value<i32>(outParams.audio_ctx))
            ("bs,beam-size",      "beam size for beam search",                               cxxopts::value<i32>(outParams.beam_size))
            ("tr,translate",      "translate from source language to english",               cxxopts::value<bool>(outParams.mb_translateEnglish)->implicit_value("false"))
            ("nf,no-fallback",    "do not use temperature fallback while decoding",          cxxopts::value<bool>(outParams.mb_fallback)->implicit_value("false"))
            ("ps,print-special",  "print special tokens",                                    cxxopts::value<bool>(outParams.mb_printSpecial))
            ("kc,keep-context",   "keep context between audio chunks",                       cxxopts::value<bool>(outParams.mb_context))
            ("l,language",        "spoken language",                                         cxxopts::value<std::string>(outParams.m_lang))
            ("mfp,model",         "The Relative/Absolute Path to the Models' Folder",        cxxopts::value<std::string>(outParams.m_modelDirectory))
            ("en,encoder",        "Full name of the encoding head, e.g encoder.fp16.onnx",   cxxopts::value<std::string>(outParams.m_encoderName))
            ("de,decoder",        "Full name of the decoding head, e.g decoder.fp16.onnx",   cxxopts::value<std::string>(outParams.m_decoderName))
            ("jo,joiner",         "Full name of the joining head, e.g joiner.fp16.onnx",     cxxopts::value<std::string>(outParams.m_joinerName))
            ("tok,tokenstxt",     "Full name of the relevant tokens.txt file",               cxxopts::value<std::string>(outParams.m_tokensTxtName))

            // ("f,file",            "text output file name",                           cxxopts::value<std::string>(params.m_fname_out))
            // ("sa,save-audio",     "save the recorded audio to a file",               cxxopts::value<bool>(params.mb_saveAudio))
            ("fa,flash-attn",     "enable flash attention",                          cxxopts::value<bool>(outParams.mb_FlashAttention)->implicit_value("true"));


        auto result = cmdOptions.parse(argc, argv);
        if (result.count("help")) {
            fprintf(stdout, "Command line options:\n%s\n", cmdOptions.help().c_str());
            exit(0);
        }

    } catch (const cxxopts::exceptions::exception& e) {
        fprintf(stderr, "Error parsing Command line options: %s\n%s\n", 
            e.what(),
            cmdOptions.help().c_str()
        );
        return false;
    }


    if (outParams.m_lang != "en") {        
        fprintf(stderr, "error: This branch of whisper.cpp only supports english\nSee Command Line Options:\n%s", 
            cmdOptions.help().c_str()
        );
        return false;
    }


    /* Derive Parameters from command line arguments */
    outParams.m_numThreads = std::min(
        std::max(outParams.m_numThreads, 1), 
        static_cast<i32>(std::thread::hardware_concurrency() / 2)
    );
    outParams.m_keepMilliseconds   = std::min(outParams.m_keepMilliseconds,   outParams.m_stepMillisecond);
    outParams.m_lengthMilliseconds = std::max(outParams.m_lengthMilliseconds, outParams.m_stepMillisecond);
    outParams.mk_numSamplesLength    = __scast( i32, (1e-3f*__scast(f32, outParams.m_lengthMilliseconds)) ) * __scast(i32, CommandLineArguments::kInferenceSampleRate);
    outParams.mk_numSamplesKeep      = __scast( i32, (1e-3f*__scast(f32, outParams.m_keepMilliseconds  )) ) * __scast(i32, CommandLineArguments::kInferenceSampleRate);

    outParams.mb_timestamps = outParams.mkb_useVAD;
    outParams.mb_context   |= !outParams.mkb_useVAD;
    return true;
}


bool sherpaonnx_init_context(
    const CommandLineArguments&       inArgParsed,
    SherpaOnnxContextParametersHandle outCtxParams,
    SherpaOnnxContextHandle*          outContext
) {
    SherpaOnnxFullContextParameters cfg;

    std::memset(&cfg, 0x00, sizeof(cfg));
    auto encoderFullPath = "./" + inArgParsed.m_modelDirectory + "/" + inArgParsed.m_encoderName;
    auto decoderFullPath = "./" + inArgParsed.m_modelDirectory + "/" + inArgParsed.m_decoderName;
    auto joinerFullPath  = "./" + inArgParsed.m_modelDirectory + "/" + inArgParsed.m_joinerName;
    auto tokensFullPath  = "./" + inArgParsed.m_modelDirectory + "/" + inArgParsed.m_tokensTxtName;

    outCtxParams->transducer.encoder = encoderFullPath.c_str();
    outCtxParams->transducer.decoder = decoderFullPath.c_str();
    outCtxParams->transducer.joiner  = joinerFullPath.c_str();
    outCtxParams->tokens             = tokensFullPath.c_str();
	outCtxParams->debug 		     = UTIL2_DEBUG_BUILD;
    outCtxParams->num_threads 	     = inArgParsed.m_numThreads;
	outCtxParams->provider 		     = inArgParsed.m_deviceID == -1 ? "cpu" : "cuda";

    cfg.model_config                = *outCtxParams;
	cfg.decoding_method             = "greedy_search";
	cfg.max_active_paths            = 8;


    auto* ctxt = SherpaOnnxCreateOfflineRecognizer(&cfg);
    *outContext = const_cast<SherpaOnnxContextHandle>(ctxt);

    return *outContext != nullptr; 
}


void sherpaonnx_destroy_context(
    SherpaOnnxContextHandle outContext
) {
    if(outContext) {
        SherpaOnnxDestroyOfflineRecognizer(outContext);
    }
    return;
}
