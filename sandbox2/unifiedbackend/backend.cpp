#include "backend.hpp"
#include <cxxopts.hpp>
#include <cstdio>
#include <cstring>
#include <algorithm>
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
            ("h,help",            "show this help message and exit")
            ("t,threads",         "number of threads to use",   cxxopts::value<i32>(outParams.m_numThreads))
            ("c,captureid",       "capture device ID",          cxxopts::value<i32>(outParams.capture_id))
            ("p,playbackid",      "playback device ID",         cxxopts::value<i32>(outParams.playback_id))
            ("gid,gpudeviceid",   "Device ID of GPU",           cxxopts::value<i32>(outParams.m_deviceID))
            ("tr,translate",      "translate to english",       cxxopts::value<bool>(outParams.mb_translateEnglish)->implicit_value("true"))
            ("g,gpu",             "enable GPU",                 cxxopts::value<bool>(outParams.mb_useGPU)->implicit_value("true"))
            ("fa,flash-attn",     "enable flash attention",     cxxopts::value<bool>(outParams.mb_FlashAttention)->implicit_value("true"))
            ("b,backend",         "Model Backend + Model format\n  ==> whisper-whisper, whisper-parakeet, sherpaonnx-parakeet, sherpaonnx-whisper",
                                                                cxxopts::value<std::string>(outParams.m_chosenBackend))
            ("l,language",        "spoken language",            cxxopts::value<std::string>(outParams.m_lang))
            ("m,model",           "whisper/parakeet model path",cxxopts::value<std::string>(outParams.m_modelFullpath))
            ("mfp,model-dir",     "Sherpa model directory",     cxxopts::value<std::string>(outParams.m_modelDirectory))
            ("en,encoder",        "Sherpa encoder name",        cxxopts::value<std::string>(outParams.m_encoderName))
            ("de,decoder",        "Sherpa decoder name",        cxxopts::value<std::string>(outParams.m_decoderName))
            ("jo,joiner",         "Sherpa joiner name",         cxxopts::value<std::string>(outParams.m_joinerName))
            ("tok,tokenstxt",     "Sherpa tokens.txt name",     cxxopts::value<std::string>(outParams.m_tokensTxtName));

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


    outParams.m_numThreads        = std::min(std::max(outParams.m_numThreads, 1), __scast(i32, std::thread::hardware_concurrency() / 2));
    return true;
}


bool ModelBackend::create(const CommandLineArguments& args, BackendType backend_type) {
    /* Input Sanitization */
    if(backend_type == BackendType::BACKEND_MAX 
        && 
        args.m_chosenBackendType == BackendType::BACKEND_MAX
    ) {
        fprintf(stderr, "Invalid Backend Supplied\n");
        return false;
    }

    backend_type = (backend_type == BackendType::BACKEND_MAX) ? 
        args.m_chosenBackendType
        :
        backend_type;


    m_type = backend_type;
    m_state.whisper = nullptr; // Zeros union
    switch (m_type) {
        case BackendType::WHISPER:     return init_whisper(args);
        case BackendType::PARAKEET:    return init_parakeet(args);
        case BackendType::SHERPA_ONNX: return init_sherpa(args);
        case BackendType::BACKEND_MAX:
        default:
        return false;
    }
    return false;
}


void ModelBackend::destroy() {
    switch (m_type) {
        case BackendType::WHISPER:
        if (m_state.whisper) {
            whisper_free(m_state.whisper->ctx);
            delete m_state.whisper;
        }
        break;
        case BackendType::PARAKEET:
        if (m_state.parakeet) {
            parakeet_free(m_state.parakeet->ctx);
            delete m_state.parakeet;
        }
        break;
        case BackendType::SHERPA_ONNX:
        if (m_state.sherpa) {
            SherpaOnnxDestroyOfflineRecognizer(m_state.sherpa->ctx);
            delete m_state.sherpa;
        }
        break;
        case BackendType::BACKEND_MAX:
        default:
        break;
    }
    m_state.whisper = nullptr;
}


void ModelBackend::print_timings() {
    switch (m_type) {
        case BackendType::WHISPER:
        whisper_print_timings(m_state.whisper->ctx);
        return;
        case BackendType::PARAKEET:
        parakeet_print_timings(m_state.parakeet->ctx);
        return;
        case BackendType::SHERPA_ONNX:
        case BackendType::BACKEND_MAX:
        default:
        break;
    }
}

void ModelBackend::reset_timings() {
    switch (m_type) {
        case BackendType::WHISPER:
        whisper_reset_timings(m_state.whisper->ctx);
        return;
        case BackendType::PARAKEET:
        parakeet_reset_timings(m_state.parakeet->ctx);
        return;
        case BackendType::SHERPA_ONNX:
        case BackendType::BACKEND_MAX:
        default:
        return;
    }
}




bool ModelBackend::init_whisper(const CommandLineArguments& args) {
    m_state.whisper = new WhisperBackendState{};
    m_state.whisper->ctx_params = whisper_context_default_params();
    m_state.whisper->ctx_params.use_gpu = args.mb_useGPU;
    m_state.whisper->ctx_params.flash_attn = args.mb_FlashAttention;
    m_state.whisper->ctx_params.gpu_device = args.m_deviceID;

    m_state.whisper->ctx = whisper_init_from_file_with_params(args.m_modelFullpath.c_str(), m_state.whisper->ctx_params);
    if (!m_state.whisper->ctx) { delete m_state.whisper; m_state.whisper = nullptr; return false; }

    m_state.whisper->full_params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    m_state.whisper->full_params.n_threads            = args.m_numThreads;
    m_state.whisper->full_params.offset_ms            = 0;
    m_state.whisper->full_params.duration_ms          = 1000;
    m_state.whisper->full_params.translate            = args.mb_translateEnglish;
    m_state.whisper->full_params.no_timestamps        = true;
    m_state.whisper->full_params.single_segment       = true;
    m_state.whisper->full_params.print_special        = false;
    m_state.whisper->full_params.print_progress       = false;
    m_state.whisper->full_params.print_realtime       = false;
    m_state.whisper->full_params.print_timestamps     = false;
    m_state.whisper->full_params.token_timestamps     = false;
    m_state.whisper->full_params.debug_mode           = false;
    m_state.whisper->full_params.initial_prompt       = "You are listening to audio input in a noisy environment.\nThere may be wind, industrial vehicles operating and also man-made noises.\nYou are tasked with deciphering your operators' instructions, who will talk the closest to the microphone";
    m_state.whisper->full_params.carry_initial_prompt = true;
    m_state.whisper->full_params.language             = args.m_lang.c_str();
    m_state.whisper->full_params.detect_language      = (args.m_lang == "auto");
    m_state.whisper->full_params.suppress_blank       = true;
    m_state.whisper->full_params.beam_search.beam_size = 8;
    m_state.whisper->full_params.vad                   = false;

    return true;
}

bool ModelBackend::init_parakeet(const CommandLineArguments& args) {
    m_state.parakeet = new ParakeetBackendState{};
    m_state.parakeet->ctx_params = parakeet_context_default_params();
    m_state.parakeet->ctx_params.use_gpu = args.mb_useGPU;
    m_state.parakeet->ctx_params.gpu_device = args.m_deviceID;

    m_state.parakeet->ctx = parakeet_init_from_file_with_params(args.m_modelFullpath.c_str(), m_state.parakeet->ctx_params);
    if (!m_state.parakeet->ctx) { delete m_state.parakeet; m_state.parakeet = nullptr; return false; }

    m_state.parakeet->full_params = parakeet_full_default_params(PARAKEET_SAMPLING_GREEDY);
    m_state.parakeet->full_params.n_threads        = args.m_numThreads;
    m_state.parakeet->full_params.offset_ms        = 0;
    m_state.parakeet->full_params.chunk_length_ms  = 5000;
    m_state.parakeet->full_params.left_context_ms  = 3000;
    m_state.parakeet->full_params.right_context_ms = 5000;
    m_state.parakeet->full_params.duration_ms      = 1000;

    return true;
}

bool ModelBackend::init_sherpa(const CommandLineArguments& args) {
    m_state.sherpa = new SherpaBackendState{};
    std::memset(&m_state.sherpa->full_params, 0x00, sizeof(m_state.sherpa->full_params));
    std::memset(&m_state.sherpa->model_params, 0x00, sizeof(m_state.sherpa->model_params));

    // Persist strings so c_str() remains valid
    m_state.sherpa->enc_path  = "./" + args.m_modelDirectory + "/" + args.m_encoderName;
    m_state.sherpa->dec_path  = "./" + args.m_modelDirectory + "/" + args.m_decoderName;
    m_state.sherpa->join_path = "./" + args.m_modelDirectory + "/" + args.m_joinerName;
    m_state.sherpa->tok_path  = "./" + args.m_modelDirectory + "/" + args.m_tokensTxtName;

    m_state.sherpa->model_params.transducer.encoder = m_state.sherpa->enc_path.c_str();
    m_state.sherpa->model_params.transducer.decoder = m_state.sherpa->dec_path.c_str();
    m_state.sherpa->model_params.transducer.joiner  = m_state.sherpa->join_path.c_str();
    m_state.sherpa->model_params.tokens             = m_state.sherpa->tok_path.c_str();
    
    // Set parameters
    m_state.sherpa->model_params.debug       = 0; 
    m_state.sherpa->model_params.num_threads = args.m_numThreads;
    m_state.sherpa->model_params.provider    = args.m_deviceID == -1 ? "cpu" : "cuda";

    m_state.sherpa->full_params.model_config     = m_state.sherpa->model_params;
    m_state.sherpa->full_params.decoding_method  = "greedy_search";
    m_state.sherpa->full_params.max_active_paths = 8;

    m_state.sherpa->ctx = const_cast<SherpaOnnxOfflineRecognizer*>(
        SherpaOnnxCreateOfflineRecognizer(&m_state.sherpa->full_params)
    );
    if (!m_state.sherpa->ctx) { delete m_state.sherpa; m_state.sherpa = nullptr; return false; }

    return true;
}


bool ModelBackend::transcribe_whisper(const f32* pcm, size_t frames, u32 duration_ms) {
    m_state.whisper->full_params.duration_ms = static_cast<i32>(duration_ms);
    return whisper_full(
        m_state.whisper->ctx, 
        m_state.whisper->full_params, 
        pcm, 
        static_cast<int>(frames)
    ) == 0;
}

bool ModelBackend::transcribe_parakeet(const f32* pcm, size_t frames, u32 duration_ms) {
    m_state.parakeet->full_params.duration_ms = static_cast<i32>(duration_ms);
    return parakeet_full(
        m_state.parakeet->ctx, 
        m_state.parakeet->full_params, 
        pcm, 
        static_cast<int>(frames)
    ) == 0;
}

bool ModelBackend::transcribe_sherpa(const f32* pcm, size_t frames, u32 sample_rate) {
    m_state.sherpa->tmpstream = const_cast<SherpaOnnxOfflineStream*>(
        SherpaOnnxCreateOfflineStream(m_state.sherpa->ctx)
    );

    SherpaOnnxAcceptWaveformOffline(
        m_state.sherpa->tmpstream, 
        static_cast<i32>(sample_rate), 
        pcm, 
        static_cast<i32>(frames)
    );
    SherpaOnnxDecodeOfflineStream(m_state.sherpa->ctx, m_state.sherpa->tmpstream);
    
    return m_state.sherpa->tmpstream != nullptr;
}


bool ModelBackend::results_whisper(inferenceResultBuffer& fixedSizeOutput) {
    const int kNumSegments = whisper_full_n_segments(m_state.whisper->ctx);


    fixedSizeOutput[0] = '\0'; /* Incase of unsuccessful transcription / no speech */
    for (int i = 0; i < kNumSegments; ++i) {
        const char* text = whisper_full_get_segment_text(m_state.whisper->ctx, i);
        fprintf(stdout, "Transcription [%d]: %s\n", i, text);
        
        // Append to buffer safely
        strncat(fixedSizeOutput.data(), text, fixedSizeOutput.size() - strlen(fixedSizeOutput.data()) - 1);
    }
    fixedSizeOutput.back() = '\0';


    return true;
}

bool ModelBackend::results_parakeet(inferenceResultBuffer& fixedSizeOutput) {
    const int kNumSegments = parakeet_full_n_segments(m_state.parakeet->ctx);


    fixedSizeOutput[0] = '\0'; /* Incase of unsuccessful transcription / no speech */
    for (int i = 0; i < kNumSegments; ++i) {
        const char* text = parakeet_full_get_segment_text(m_state.parakeet->ctx, i);
        fprintf(stdout, "Transcription [%d]: %s\n", i, text);
        
        strncat(fixedSizeOutput.data(), text, fixedSizeOutput.size() - strlen(fixedSizeOutput.data()) - 1);
    }
    fixedSizeOutput.back() = '\0';


    return true;
}

bool ModelBackend::results_sherpa(inferenceResultBuffer& fixedSizeOutput) {
    const SherpaOnnxOfflineRecognizerResult* result = SherpaOnnxGetOfflineStreamResult(m_state.sherpa->tmpstream);
    bool status = (result != nullptr);


    fixedSizeOutput[0] = '\0';
    if (result) {
        fprintf(stdout, "[transcribe_sherpa] Transcription: %s\n", result->text);
        
        strncpy(fixedSizeOutput.data(), result->text, fixedSizeOutput.size() - 1);
        fixedSizeOutput.back() = '\0';
        
        SherpaOnnxDestroyOfflineRecognizerResult(result);
    } else {
        fputs("[transcribe_sherpa] Failed to transcribe Audio\n", stdout);
    }
    
    SherpaOnnxDestroyOfflineStream(m_state.sherpa->tmpstream);
    return status;
}
