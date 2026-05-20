#pragma once
#include <cstdint>
#include <string>
#include <vector>


#ifdef __cplusplus
extern "C" {
#endif


typedef struct __copied_from_crispasr_c_api_cpp_open_params_v1 {
    int abi_version; // = 1 or 2
    int n_threads;
    int use_gpu;   // 0 = CPU only, non-zero = GPU when available
    int verbosity; // 0 = silent, 1+ = chatty
    // ── v2 (0.6.2) additions ───────────────────────────────────────
    // Set abi_version >= 2 to opt into these fields. v1 callers
    // get the historical defaults.
    int flash_attn;   // 0 = off, non-zero = on (default on)
    int n_gpu_layers; // -1 = max, 0 = CPU-only LLM, >0 = bound
    int reserved[6];  // future-compat padding (was 8 in v1; -2 here)
} crispasr_open_params_v1;


typedef struct __copied_from_crispasr_c_api_cpp_session_segment {
    std::string text;
    int64_t t0 = 0; // centiseconds absolute
    int64_t t1 = 0;
    struct word {
        std::string text;
        int64_t t0 = 0; // centiseconds absolute
        int64_t t1 = 0;
        float p = 1.0f;
    };
    std::vector<word> words;
} crispasr_session_seg;


typedef struct crispasr_session crispasr_session;


typedef struct __copied_from_crispasr_c_api_cpp_session_result {
    std::vector<crispasr_session_seg> segments;
    std::string backend;
} crispasr_session_result;


crispasr_session* crispasr_session_open_with_params(
	const char* model_path, 
	const char* backend_name,
	const crispasr_open_params_v1* params
);


crispasr_session* crispasr_session_open(
    const char* model_path, 
    int n_threads
);


void crispasr_session_close(crispasr_session* s);


crispasr_session_result* crispasr_session_transcribe_lang(
	crispasr_session* s, 
	const float* pcm,
	int n_samples, 
	const char* language
);


} /* extern "C" */