#include <sttserv/backend.hpp>
#include <whisper.h>
#include <cstdio>
#include <cstring>


struct WhisperBackendState {
    whisper_context* ctx;
    whisper_context_params ctx_params;
    whisper_full_params full_params;
};


static constexpr const char* kWhisperSystemPrompt = "\
You are listening to audio input in a noisy environment.\n\
There may be wind, industrial vehicles operating and also man-made noises.\n\
You are tasked with deciphering your operators' instructions, who will talk the closest to the microphone\n\
";


bool init_whisper_impl(ModelBackend* self, const CommandLineArguments& args) {
    auto* state = new WhisperBackendState{};
    state->ctx_params            = whisper_context_default_params();
    state->ctx_params.use_gpu    = args.m_deviceID != -1;
    state->ctx_params.flash_attn = args.mb_FlashAttention;
    state->ctx_params.gpu_device = args.m_deviceID;

    state->ctx = whisper_init_from_file_with_params(args.m_modelFullpath.c_str(), state->ctx_params);
    if (!state->ctx) { delete state; return false; }


    state->full_params = whisper_full_default_params(WHISPER_SAMPLING_BEAM_SEARCH);
    state->full_params.n_threads            = args.m_numThreads;
    state->full_params.translate            = args.mb_translateEnglish;
    state->full_params.no_timestamps        = true;
    state->full_params.single_segment       = true;
    state->full_params.initial_prompt       = kWhisperSystemPrompt;
    state->full_params.carry_initial_prompt = true;
    state->full_params.language             = args.m_lang.c_str();
    state->full_params.detect_language      = (args.m_lang == "auto");
    state->full_params.suppress_blank       = true;
    state->full_params.beam_search.beam_size = 8;

    self->m_internalState = state;
    return true;
}


void destroy_whisper_impl(void* ptr) {
    auto* state = static_cast<WhisperBackendState*>(ptr);
    whisper_free(state->ctx);
    delete state;
}


bool transcribe_whisper_impl(void* ptr, const f32* pcm, size_t frames, u32 duration_ms) {
    auto* state = static_cast<WhisperBackendState*>(ptr);
    state->full_params.duration_ms = static_cast<i32>(duration_ms);
    return whisper_full(state->ctx, state->full_params, pcm, static_cast<int>(frames)) == 0;
}


bool results_whisper_impl(void* ptr, inferenceResultBuffer& fixedSizeOutput) {
    auto* state = static_cast<WhisperBackendState*>(ptr);
    const int kNumSegments = whisper_full_n_segments(state->ctx);
    fixedSizeOutput[0] = '\0';

    for (int i = 0; i < kNumSegments; ++i) {
        const char* text = whisper_full_get_segment_text(state->ctx, i);
        fprintf(stdout, "Transcription [%d]: %s\n", i, text);
        strncat(fixedSizeOutput.data(), text, fixedSizeOutput.size() - strlen(fixedSizeOutput.data()) - 1);
    }
    fixedSizeOutput.back() = '\0';
    return true;
}


void print_timings_whisper_impl(void* ptr) { 
    whisper_print_timings(static_cast<WhisperBackendState*>(ptr)->ctx);
    return;
}
void reset_timings_whisper_impl(void* ptr) { 
    whisper_reset_timings(static_cast<WhisperBackendState*>(ptr)->ctx);
    return;
}