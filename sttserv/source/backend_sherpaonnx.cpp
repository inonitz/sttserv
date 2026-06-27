#include <sttserv/backend.hpp>
#include <sherpa-onnx/c-api/c-api.h>
#include <cstdio>
#include <cstring>
#include <string>


struct SherpaBackendState {
    SherpaOnnxOfflineRecognizer*      ctx;
    SherpaOnnxOfflineStream*          tmpstream;
    SherpaOnnxOfflineModelConfig      model_params;
    SherpaOnnxOfflineRecognizerConfig full_params;
    std::string enc_path, dec_path, join_path, tok_path;
};


bool init_sherpa_impl(ModelBackend* self, const CommandLineArguments& args) {
    auto* state = new SherpaBackendState{};
    std::memset(&state->full_params, 0, sizeof(state->full_params));
    std::memset(&state->model_params, 0, sizeof(state->model_params));

    state->enc_path  = "./" + args.m_modelDirectory + "/" + args.m_encoderName;
    state->dec_path  = "./" + args.m_modelDirectory + "/" + args.m_decoderName;
    state->join_path = "./" + args.m_modelDirectory + "/" + args.m_joinerName;
    state->tok_path  = "./" + args.m_modelDirectory + "/" + args.m_tokensTxtName;

    state->model_params.transducer.encoder = state->enc_path.c_str();
    state->model_params.transducer.decoder = state->dec_path.c_str();
    state->model_params.transducer.joiner  = state->join_path.c_str();
    state->model_params.tokens             = state->tok_path.c_str();
    state->model_params.num_threads        = args.m_numThreads;
    state->model_params.provider           = args.m_deviceID == -1 ? "cpu" : "cuda";

    state->full_params.model_config     = state->model_params;
    state->full_params.decoding_method  = "greedy_search";
    state->full_params.max_active_paths = 8;

    state->ctx = const_cast<SherpaOnnxOfflineRecognizer*>(
        SherpaOnnxCreateOfflineRecognizer(&state->full_params)
    );
    if (!state->ctx) 
    { 
        delete state;
        return false;
    }

    self->m_internalState = state;
    return true;
}


void destroy_sherpa_impl(void* ptr) {
    auto* state = static_cast<SherpaBackendState*>(ptr);
    SherpaOnnxDestroyOfflineRecognizer(state->ctx);
    delete state;
}


bool transcribe_sherpa_impl(void* ptr, const f32* pcm, size_t frames, u32 sample_rate) {
    auto* state = static_cast<SherpaBackendState*>(ptr);
    state->tmpstream = const_cast<SherpaOnnxOfflineStream*>(SherpaOnnxCreateOfflineStream(state->ctx));

    SherpaOnnxAcceptWaveformOffline(state->tmpstream, static_cast<i32>(sample_rate), pcm, static_cast<i32>(frames));
    SherpaOnnxDecodeOfflineStream(state->ctx, state->tmpstream);
    return state->tmpstream != nullptr;
}


bool results_sherpa_impl(void* ptr, inferenceResultBuffer& fixedSizeOutput) {
    auto* state = static_cast<SherpaBackendState*>(ptr);
    const auto* result = SherpaOnnxGetOfflineStreamResult(state->tmpstream);
    bool status = (result != nullptr);

    fixedSizeOutput[0] = '\0';
    if (result) {
        fprintf(stdout, "[transcribe_sherpa] Transcription: %s\n", result->text);
        strncpy(fixedSizeOutput.data(), result->text, fixedSizeOutput.size() - 1);
        fixedSizeOutput.back() = '\0';
        SherpaOnnxDestroyOfflineRecognizerResult(result);
    }
    SherpaOnnxDestroyOfflineStream(state->tmpstream);
    return status;
}