#ifndef __LLM_ASR_MODEL_UNIFIED_INTERFACE_DEFINITION_HEADER__
#define __LLM_ASR_MODEL_UNIFIED_INTERFACE_DEFINITION_HEADER__
#include <sttserv/cmdline.hpp>
#include <util2/C/macro.h>
#include <array>


using inferenceResultBuffer = std::array<char, 1024>;


struct STTSERVER_API ModelBackend {
    BackendType m_type;
    void*       m_internalState = nullptr;

    bool create(
        const CommandLineArguments& args, 
        BackendType backend_type = BackendType::BACKEND_MAX
    );
    void destroy();
    
    __hot bool transcribe(
        const f32* pcm, 
        size_t     frames, 
        u32        duration_ms, 
        u32        sample_rate
    );

    __hot bool result(inferenceResultBuffer& fixedSizeOutput);
    
    void print_timings();
    void reset_timings();
};

#endif /* __LLM_ASR_MODEL_UNIFIED_INTERFACE_DEFINITION_HEADER__ */
