#include <sttserv/backend.hpp>
#include <cstdio>


// Forward declarations of internal init helpers defined in the split files
extern bool init_whisper_impl(ModelBackend* self, const CommandLineArguments& args);
extern bool init_parakeet_impl(ModelBackend* self, const CommandLineArguments& args);
extern bool init_sherpa_impl(ModelBackend* self, const CommandLineArguments& args);

extern void destroy_whisper_impl(void* state);
extern void destroy_parakeet_impl(void* state);
extern void destroy_sherpa_impl(void* state);

extern bool transcribe_whisper_impl(void* state, const f32* pcm, size_t frames, u32 duration_ms);
extern bool transcribe_parakeet_impl(void* state, const f32* pcm, size_t frames, u32 duration_ms);
extern bool transcribe_sherpa_impl(void* state, const f32* pcm, size_t frames, u32 sample_rate);

extern bool results_whisper_impl(void* state, inferenceResultBuffer& output);
extern bool results_parakeet_impl(void* state, inferenceResultBuffer& output);
extern bool results_sherpa_impl(void* state, inferenceResultBuffer& output);

extern void print_timings_whisper_impl(void* state);
extern void print_timings_parakeet_impl(void* state);
extern void reset_timings_whisper_impl(void* state);
extern void reset_timings_parakeet_impl(void* state);


bool ModelBackend::create(const CommandLineArguments& args, BackendType backend_type) {
    if (backend_type == BackendType::BACKEND_MAX 
        && 
        args.m_chosenBackendType == BackendType::BACKEND_MAX
    ) {
        fprintf(stderr, "Invalid Backend Supplied\n");
        return false;
    }

    m_type = (backend_type == BackendType::BACKEND_MAX) ? args.m_chosenBackendType : backend_type;
    m_internalState = nullptr;

    switch (m_type) {
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER
        case BackendType::WHISPER:
        return init_whisper_impl(this, args);
        break;
        case BackendType::PARAKEET:
        return init_parakeet_impl(this, args);
        break;
#endif
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX
        case BackendType::SHERPA_ONNX:
        return init_sherpa_impl(this, args);
        break;
#endif
        default: 
        return false;
        break;
    }
    return false;
}

void ModelBackend::destroy() {
    if (!m_internalState) return;
    switch (m_type) {
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER
        case BackendType::WHISPER:
        destroy_whisper_impl(m_internalState);
        break;
        case BackendType::PARAKEET:
        destroy_parakeet_impl(m_internalState);
        break;
#endif
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX
        case BackendType::SHERPA_ONNX:
        destroy_sherpa_impl(m_internalState);
        break;
#endif
        default: break;
    }
    m_internalState = nullptr;
}

bool ModelBackend::transcribe(const f32* pcm, size_t frames, u32 duration_ms, u32 sample_rate) {
    switch (m_type) {
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER
        case BackendType::WHISPER:
        return transcribe_whisper_impl(m_internalState, pcm, frames, duration_ms);
        break;
        case BackendType::PARAKEET:
        return transcribe_parakeet_impl(m_internalState, pcm, frames, duration_ms);
        break;
#endif
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX
        case BackendType::SHERPA_ONNX:
        return transcribe_sherpa_impl(m_internalState, pcm, frames, sample_rate);
        break;
#endif
        default: return false;
    }
}


bool ModelBackend::result(inferenceResultBuffer& fixedSizeOutput) {
    switch (m_type) {
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER
        case BackendType::WHISPER:
        return results_whisper_impl(m_internalState, fixedSizeOutput);
        break;
        case BackendType::PARAKEET:
        return results_parakeet_impl(m_internalState, fixedSizeOutput);
        break;
#endif
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_SHERPA_ONNX
        case BackendType::SHERPA_ONNX:
        return results_sherpa_impl(m_internalState, fixedSizeOutput);
        break;
#endif
        default: 
        return false;
        break;
    }
    return false;
}

void ModelBackend::print_timings() {
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER
    if (m_type == BackendType::WHISPER) print_timings_whisper_impl(m_internalState);
#endif
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_PARAKEET
    if (m_type == BackendType::PARAKEET) print_timings_parakeet_impl(m_internalState);
#endif
}

void ModelBackend::reset_timings() {
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_WHISPER
    if (m_type == BackendType::WHISPER) reset_timings_whisper_impl(m_internalState);
#endif
#ifdef STTSERVER_BUILD_LIBRARY_BACKEND_PARAKEET
    if (m_type == BackendType::PARAKEET) reset_timings_parakeet_impl(m_internalState);
#endif
}