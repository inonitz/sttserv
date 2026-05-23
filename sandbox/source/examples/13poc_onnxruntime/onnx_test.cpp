#include <cstdio>
#include <vector>
#include <sherpa-onnx/c-api/cxx-api.h>
#include <util2/time.hpp>
#include <util2/C/marker4.h>


using namespace sherpa_onnx::cxx;


int main() {
    OfflineRecognizerConfig            config;
    std::unique_ptr<OfflineRecognizer> recognizer;
    std::unique_ptr<OfflineStream>     stream;

    util2::Time::Timer<> m_loggingTime;

    // std::string modelFolderPath = "dependencies/models/distil-whisper-v3.5-fp16-onnx/onnx/";
    // config.model_config.whisper.encoder = modelFolderPath + "encoder_model_fp16.onnx";
    // config.model_config.whisper.decoder = modelFolderPath + "decoder_model_merged_fp16.onnx";
    // config.model_config.tokens          = modelFolderPath + "../distil-large-v3.5-tokens.txt";
    // config.model_config.provider        = "auto";

    std::string modelFolderPath = "dependencies/models/sherpa-onnx/sherpa-onnx-whisper-distil-large-v3.5/";
    // config.model_config.whisper.encoder = modelFolderPath + "distil-large-v3.5-decoder.int8.onnx";
    // config.model_config.whisper.decoder = modelFolderPath + "distil-large-v3.5-decoder.int8.onnx";
    config.model_config.whisper.encoder = modelFolderPath + "whisper-encoder.onnx";
    config.model_config.whisper.encoder = modelFolderPath + "whisper-decoder.onnx";
    config.model_config.tokens          = modelFolderPath + "distil-large-v3.5-tokens.txt";
    // config.model_config.provider        = "auto";
    // config.model_config.graph_optimization_level = ORT_ENABLE_BASIC;

    // auto lm_sessionCfg = sherpa_onnx::GetSessionOptions(config.lm_config);
    // config.lm_config. = lm_sessionCfg;

    // fprintf(stdout, "[INFO] Current ONNX Optimization Level: %d (1 = BASIC)\n", 
    //         config.model_config.session_options.graph_optimization_level);


    // 2. Initialize Engine & Stream
    std::fputs("Loading model\n", stdout);
    recognizer = std::make_unique<OfflineRecognizer>(OfflineRecognizer::Create(config));
    if(recognizer == nullptr) {
        fprintf(stderr, "Error Loading Model\n");
        return -1;
    }
    std::fputs("Loading model Done\n", stdout);



    mark();
    // 3. Load Raw PCM Data (16kHz, float32, range [-1.0, 1.0])
    // Using 2 seconds of silence/zeros for this example
    std::vector<float> pcm_data(16000 * 2, 0.0f); 
    int sample_rate = 16000;
    
    mark();
    // 4. Feed & Decode
    m_loggingTime.tick();
    mark();
    stream->AcceptWaveform(sample_rate, pcm_data.data(), pcm_data.size());
    mark();
    recognizer->Decode(stream.get());
    mark();
    m_loggingTime.tock();
    mark();

    mark();
    OfflineRecognizerResult result = recognizer->GetResult(stream.get());
    // 5. Fetch Result
    const auto elapsedTimeNs = m_loggingTime.duration().count();
    fprintf(stdout, "Whisper Result: %s\n", result.text.c_str());
    fprintf(stdout, "Took %llu ns (%llu Microseconds) (%llu Milliseconds)\n",
                    __scast(unsigned long long, elapsedTimeNs),
                    __scast(unsigned long long, (elapsedTimeNs+999) / 1000),
                    __scast(unsigned long long, (elapsedTimeNs+1000000-1) / 1000000)
                );
    return 0;
}