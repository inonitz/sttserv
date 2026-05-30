#include <sttserv/cmdline.hpp>
#include <util2/C/macro.h>
#include <cxxopts.hpp>
#include <thread>


static inline __force_inline BackendType backendStringToType(std::string const& backend) {
    static const std::unordered_map<std::string, BackendType> kAvailableBackendAndModelPair = {
        { "whisper-whisper",     BackendType::WHISPER },
        { "whisper-parakeet",    BackendType::PARAKEET },
        { "sherpaonnx-parakeet", BackendType::SHERPA_ONNX },
        { "sherpaonnx-whisper",  BackendType::SHERPA_ONNX }
    };

    auto betype = kAvailableBackendAndModelPair.find(backend);
    return (betype == kAvailableBackendAndModelPair.end()) ? BackendType::BACKEND_MAX : betype->second;
}


bool parse_commandline_args(int argc, char** argv, CommandLineArguments& outParams) {
    const std::unordered_set<std::string> kAvailableBackendAndModelPair = {
        "whisper-whisper",
        "whisper-parakeet",
        "sherpaonnx-parakeet",
        "sherpaonnx-whisper",
    };


    cxxopts::Options cmdOptions(argv[0], "Unified ASR Argument Parser");
    try {
        cmdOptions.add_options()
            // --- General Options ---
            ("h,help",          "show this help message and exit")
            ("t,threads",       "number of threads to use",   cxxopts::value<i32>(outParams.m_numThreads))
            ("c,captureid",     "capture device ID",          cxxopts::value<i32>(outParams.capture_id))
            ("p,playbackid",    "playback device ID",         cxxopts::value<i32>(outParams.playback_id))
            ("gid,gpudeviceid", "Device ID of a GPU",         cxxopts::value<i32>(outParams.m_deviceID)->default_value("-1"))
            ("l,language",      "spoken language",            cxxopts::value<std::string>(outParams.m_lang))
            ("b,backend",       "Model Backend + Model format\n"
                                "  ==> whisper-whisper, whisper-parakeet, sherpaonnx-parakeet, sherpaonnx-whisper", 
                                cxxopts::value<std::string>(outParams.m_chosenBackend))

            // --- Whisper Backend Only ---
            ("tr,translate",     "(Whisper Backend Only) translate to english",                       cxxopts::value<bool>(outParams.mb_translateEnglish)->implicit_value("true"))
            ("fa,flash-attn",    "(Whisper Backend Only) enable flash attention",                     cxxopts::value<bool>(outParams.mb_FlashAttention)->default_value("true"))
            ("m,model",          "(Whisper Backend Only) whisper-backend Whisper/Parakeet model path",cxxopts::value<std::string>(outParams.m_modelFullpath))

            // --- SherpaOnnx Backend Only ---
            ("mfp,model-dir",    "(SherpaOnnx Only) Sherpa model directory",               cxxopts::value<std::string>(outParams.m_modelDirectory))
            ("en,encoder",       "(SherpaOnnx Only) Sherpaonnx Model encoder name",        cxxopts::value<std::string>(outParams.m_encoderName))
            ("de,decoder",       "(SherpaOnnx Only) Sherpaonnx Model decoder name",        cxxopts::value<std::string>(outParams.m_decoderName))
            ("jo,joiner",        "(SherpaOnnx Only) Sherpaonnx Model joiner name",         cxxopts::value<std::string>(outParams.m_joinerName))
            ("tok,tokenstxt",    "(SherpaOnnx Only) Sherpaonnx Model tokens.txt name",     cxxopts::value<std::string>(outParams.m_tokensTxtName))

            // --- Parakeet Only ---
            ("kpc,keepprevious", "(Parakeet Only) Amount (in ms) of Already-Processed audio to keep from the previous decoding step", 
                                cxxopts::value<i32>(outParams.mk_prevChunkSize)->default_value("0"))
            ("kcc,keepcurrent",  "(Parakeet Only) Amount (in ms) of Already-Processed audio to keep from the previous decoding step", 
                                cxxopts::value<i32>(outParams.mk_currChunkSize)->default_value("0"))
            ("kfc,keepfuture",   "(Parakeet Only) Amount (in ms) of Already-Processed audio to keep from the previous decoding step", 
                                cxxopts::value<i32>(outParams.mk_postChunkSize)->default_value("0"))
            ;

            auto result = cmdOptions.parse(argc, argv);
        if (result.count("help")) {
            fprintf(stdout, "Command line options:\n%s\n", cmdOptions.help().c_str());
            exit(0);
        }
    } catch (const cxxopts::exceptions::exception& e) {
        fprintf(stderr, "CLI Parsing Error: %s\n", e.what());
        return false;
    }


    /* Check if valid backend */    
    if(kAvailableBackendAndModelPair.find(outParams.m_chosenBackend) == kAvailableBackendAndModelPair.end()) {
        fprintf(stderr, "Invalid Backend has been chosen/supplied to --backend -> [%s]\nSee Command line options:\n%s\n",
            outParams.m_chosenBackend.c_str(),
            cmdOptions.help().c_str()
        );
        return false;
    }

    outParams.m_chosenBackendType = backendStringToType(outParams.m_chosenBackend);
    outParams.m_numThreads        = std::min(
        std::max(outParams.m_numThreads, 1), 
        __scast(i32, std::thread::hardware_concurrency() / 2)
    );


    return true;
}


static constexpr const char* backendTypeToString(BackendType type) {
    switch (type) {
        case BackendType::WHISPER:     return "WHISPER";
        case BackendType::PARAKEET:    return "PARAKEET";
        case BackendType::SHERPA_ONNX: return "SHERPA_ONNX";
        case BackendType::BACKEND_MAX: return "BACKEND_MAX";
        default:                       return "UNKNOWN";
    }
}

void print_arguments(const CommandLineArguments& args) {
    fprintf(stdout, "--- Command Line Arguments ---\n");
    fprintf(stdout, "Threads:           %d\n", args.m_numThreads);
    fprintf(stdout, "Audio Capture ID:  %d\n", args.capture_id);
    fprintf(stdout, "Audio Playback ID: %d\n", args.playback_id);
    fprintf(stdout, "Device ID:         %d (%s)\n", args.m_deviceID, 
        args.m_deviceID == -1 ? "CPU" : "GPU"
    );
    fprintf(stdout, "Translate (EN):    %s\n", args.mb_translateEnglish ? "true" : "false");
    fprintf(stdout, "Flash Attention:   %s\n", args.mb_FlashAttention ? "true" : "false");
    fprintf(stdout, "Backend String:    %s\n", args.m_chosenBackend.c_str());
    fprintf(stdout, "Backend Type:      %s\n", backendTypeToString(args.m_chosenBackendType));
    fprintf(stdout, "Language:          %s\n", args.m_lang.c_str());
    fprintf(stdout, "Model Fullpath:    %s\n", args.m_modelFullpath.c_str());
    fprintf(stdout, "Model Directory:   %s\n", args.m_modelDirectory.c_str());
    fprintf(stdout, "Encoder Name:      %s\n", args.m_encoderName.c_str());
    fprintf(stdout, "Decoder Name:      %s\n", args.m_decoderName.c_str());
    fprintf(stdout, "Joiner Name:       %s\n", args.m_joinerName.c_str());
    fprintf(stdout, "Tokens Txt:        %s\n", args.m_tokensTxtName.c_str());
    fprintf(stdout, "Sample Rate:       %u\n", CommandLineArguments::kInferenceSampleRate);
    fprintf(stdout, "MS in 1 Sec:       %u\n", CommandLineArguments::kMillisecondsIn1Second);
    fprintf(stdout, "------------------------------\n");
}


// void print_arguments(CommandLineArguments const& params) 
// {
//     fprintf(stdout, "--- Speech-To-Text Server Configuration ---\n");
//     fprintf(stdout, "Working Directory (CWD)  %s\n", std::filesystem::current_path().c_str());
//     fprintf(stdout, "Threads:                 %d\n", params.m_numThreads);
//     fprintf(stdout, "Keep Context (ms):       %d\n", params.m_keepPreviousCtxtMilliseconds);
//     fprintf(stdout, "Capture Device ID:       %d\n", params.m_capture_id);
//     fprintf(stdout, "Playback Device ID:      %d\n", params.m_playback_id);
//     fprintf(stdout, "Compute Device ID:       %d\n", params.m_deviceID);
//     fprintf(stdout, "Beam Size:               %d\n", params.m_beam_size);
//     fprintf(stdout, "Language:                %s\n", params.m_lang);
//     fprintf(stdout, "Model Path:              %s\n", params.m_modelFullpath);
//     fprintf(stdout, "Save Recordings File:    %s\n", params.m_saveRecordingsFilename);
//     fprintf(stdout, "Translate to English:    %s\n", params.mkb_translateToEnglish ? "true" : "false");
//     fprintf(stdout, "Temperature Fallback:    %s\n", params.mkb_tempFallback ? "true" : "false");
//     fprintf(stdout, "Print Special Tokens:    %s\n", params->mkb_printSpecial ? "true" : "false");
//     fprintf(stdout, "Keep Previous Context:   %s\n", params->mkb_keepPreviousCtxt ? "true" : "false");
//     fprintf(stdout, "Timestamps:              %s\n", params->mkb_timestamps ? "true" : "false");
//     fprintf(stdout, "Flash Attention:         %s\n", params->mkb_FlashAttention ? "true" : "false");
//     fprintf(stdout, "Use VAD:                 %s\n", params->mkb_useVAD ? "true" : "false");
//     fprintf(stdout, "Save Recordings:         %s\n", params->mkb_saveRecordings ? "true" : "false");
//     fprintf(stdout, "-------------------------------------------\n\n");
//     return;
// }