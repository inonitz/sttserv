#include <sttserv/backend.hpp>
#include <parakeet.h>
#include <cstdio>
#include <cstring>


struct ParakeetBackendState {
    parakeet_context* ctx;
    parakeet_context_params ctx_params;
    parakeet_full_params full_params;
};


bool init_parakeet_impl(ModelBackend* self, const CommandLineArguments& args) {
    auto* state                  = new ParakeetBackendState{};
    state->ctx_params            = parakeet_context_default_params();
    state->ctx_params.use_gpu    = args.m_deviceID != -1;
    state->ctx_params.gpu_device = args.m_deviceID;

    state->ctx = parakeet_init_from_file_with_params(args.m_modelFullpath.c_str(), state->ctx_params);
    if (!state->ctx) { 
        delete state;
        return false;
    }

    state->full_params = parakeet_full_default_params(PARAKEET_SAMPLING_GREEDY);
    state->full_params.n_threads  = args.m_numThreads;
    state->full_params.no_context = (args.mk_prevChunkSize == 0 && args.mk_currChunkSize == 0 && args.mk_postChunkSize == 0);
    state->full_params.duration_ms = 1000;

    self->m_internalState = state;
    return true;
}


void destroy_parakeet_impl(void* ptr) {
    auto* state = static_cast<ParakeetBackendState*>(ptr);
    parakeet_free(state->ctx);
    delete state;
}


bool transcribe_parakeet_impl(void* ptr, const f32* pcm, size_t frames, u32 duration_ms) {
    auto* state = static_cast<ParakeetBackendState*>(ptr);
    state->full_params.duration_ms = static_cast<i32>(duration_ms);
    return parakeet_full(state->ctx, state->full_params, pcm, static_cast<int>(frames)) == 0;
}


bool results_parakeet_impl(void* ptr, inferenceResultBuffer& fixedSizeOutput) {
    auto* state = static_cast<ParakeetBackendState*>(ptr);
    const int kNumSegments = parakeet_full_n_segments(state->ctx);
    fixedSizeOutput[0] = '\0';

    for (int i = 0; i < kNumSegments; ++i) {
        const char* text = parakeet_full_get_segment_text(state->ctx, i);
        fprintf(stdout, "Transcription [%d]: %s\n", i, text);
        strncat(fixedSizeOutput.data(), text, fixedSizeOutput.size() - strlen(fixedSizeOutput.data()) - 1);
    }
    fixedSizeOutput.back() = '\0';
    return true;
}


void print_timings_parakeet_impl(void* ptr) { parakeet_print_timings(static_cast<ParakeetBackendState*>(ptr)->ctx); }
void reset_timings_parakeet_impl(void* ptr) { parakeet_reset_timings(static_cast<ParakeetBackendState*>(ptr)->ctx); }
