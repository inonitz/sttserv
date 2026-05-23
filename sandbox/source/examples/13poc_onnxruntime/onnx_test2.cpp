#include <chrono> // NOLINT
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include "sherpa-onnx/c-api/cxx-api.h"


// #define MODEL_WHISPER
#define MODEL_PARAKEET 2


int32_t main() {

	using namespace sherpa_onnx::cxx; // NOLINT
	OfflineRecognizerConfig config{};
	std::string modelFolderPath = "";
	std::string wave_filename = "";


	fprintf(stdout, "CWD=[%s]", std::filesystem::current_path().c_str());


#ifdef MODEL_WHISPER
    modelFolderPath = "dependencies/models/sherpa-onnx/sherpa-onnx-whisper-distil-large-v3.5/";
	// config.model_config.whisper.encoder = "./" + modelFolderPath +
	//     "distil-large-v3.5-decoder.int8.onnx";
	// config.model_config.whisper.decoder = "./" + modelFolderPath +
	//     "distil-large-v3.5-encoder.int8.onnx";
	// config.model_config.tokens = "./" + modelFolderPath +
	//     "distil-large-v3.5-tokens.txt";

    // config.model_config.whisper.encoder = modelFolderPath + "distil-large-v3.5-decoder.int8.onnx";
    // config.model_config.whisper.decoder = modelFolderPath + "distil-large-v3.5-decoder.int8.onnx";
    config.model_config.whisper.encoder = "./" + modelFolderPath + "whisper-encoder.onnx";
    config.model_config.whisper.decoder = "./" + modelFolderPath + "whisper-decoder.onnx";
    config.model_config.tokens          = "./" + modelFolderPath + "distil-large-v3.5-tokens.txt";
	config.model_config.num_threads = 1;


	std::cout << "Loading model\n";
	OfflineRecognizer recognizer = OfflineRecognizer::Create(config);
	if (!recognizer.Get()) {
		std::cerr << "Please check your config\n";
		return -1;
	}
	std::cout << "Loading model done\n";

#elif defined MODEL_PARAKEET && MODEL_PARAKEET == 3
    modelFolderPath = "../../../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v3-int8/";
    config.model_config.transducer.encoder = "./" + modelFolderPath + "encoder.int8.onnx";
    config.model_config.transducer.decoder = "./" + modelFolderPath + "decoder.int8.onnx";
    config.model_config.transducer.joiner  = "./" + modelFolderPath + "joiner.int8.onnx";
	config.model_config.tokens             = "./" + modelFolderPath + "tokens.txt";
	config.model_config.debug 		       = true;
	// config.model_config.debug 			   = false;
    config.model_config.num_threads 	   = 4;
	config.model_config.provider 		   = "cpu";
	// config.decoding_method 				   = "greedy_search";
	// config.max_active_paths				   = 1;
	// wave_filename = "./" + modelFolderPath + "test_wavs/en.wav";
	wave_filename = "./../../../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-fp16/test_wavs/0.wav";


    std::cout << "Loading model\n";
    OfflineRecognizer recognizer = OfflineRecognizer::Create(config);

    if (!recognizer.Get()) {
        std::cerr << "Please check your config\n";
        return -1;
    }
    std::cout << "Loading model done\n";

#elif defined MODEL_PARAKEET && MODEL_PARAKEET == 2
    modelFolderPath = "../../../../dependencies/models/sherpa-onnx/sherpa-onnx-nemo-parakeet-tdt-0.6b-v2-fp16/";
    config.model_config.transducer.encoder = "./" + modelFolderPath + "encoder.fp16.onnx";
    config.model_config.transducer.decoder = "./" + modelFolderPath + "decoder.fp16.onnx";
    config.model_config.transducer.joiner  = "./" + modelFolderPath + "joiner.fp16.onnx";
	config.model_config.debug 		       = true;
    config.model_config.tokens             = "./" + modelFolderPath + "tokens.txt";
    config.model_config.num_threads 	   = 4;
	config.model_config.debug 			   = true;
	config.model_config.provider 		   = "cpu";
	wave_filename = "./" + modelFolderPath + "test_wavs/0.wav";


    std::cout << "Loading model\n";
    OfflineRecognizer recognizer = OfflineRecognizer::Create(config);

    if (!recognizer.Get()) {
        std::cerr << "Please check your config\n";
        return -1;
    }
    std::cout << "Loading model done\n";
#endif


	std::cout << "\n=============================================" << std::endl;
	std::cout << "[BACKEND] Active Device Provider: " 
			<< (config.model_config.provider.empty() ? "cpu" : config.model_config.provider) 
			<< std::endl;
	std::cout << "[BACKEND] Thread Pool Count: " 
			<< config.model_config.num_threads << std::endl;
	std::cout << "=============================================\n" << std::endl;

	Wave wave = ReadWave(wave_filename);
	if (wave.samples.empty()) {
		std::cerr << "Failed to read: '" << wave_filename << "'\n";
		return -1;
	}


	int32_t sampleRateIn = 24000;
	int32_t sampleRateOut = 16000;

	// Filter cutoff rule of thumb: 0.99 * 0.5 * min(sampleRateIn, sampleRateOut) -> 0.99 * 8000 = 7920.0f
	float filterCutoff = 7920.0f; 
	int32_t numZeros = 6; // Standard Kaldi/sherpa default window width
	// Call the static factory to initialize the resampler object
	LinearResampler resampler = LinearResampler::Create(
		sampleRateIn, 
		sampleRateOut, 
		filterCutoff, 
		numZeros
	);

	// Resample the raw data array
	std::vector<float> samples16k = resampler.Resample(
		wave.samples.data(), 
		static_cast<int32_t>(wave.samples.size()), 
		true
	);

	std::cout << "Start recognition\n";
	OfflineRecognizerResult result;


	for(uint32_t i = 0; i < 1000; ++i) {
		const auto begin = std::chrono::high_resolution_clock::now();
		
		OfflineStream stream = recognizer.CreateStream();
		stream.AcceptWaveform(16000, samples16k.data(), samples16k.size());
		recognizer.Decode(&stream);
		result = recognizer.GetResult(&stream);

		const auto end = std::chrono::high_resolution_clock::now();

	
		const float kAudioDuration = wave.samples.size() / static_cast<float>(wave.sample_rate);
		const float kInferElapsed_millisecond = std::chrono::duration_cast<
			std::chrono::nanoseconds
			>(end - begin).count() * 1e-6;
		
		const float kRealTimeFactor = kAudioDuration * 1000 / kInferElapsed_millisecond;

		// std::cout << "text: " << result.text << "\n";
		fprintf(stdout, "[%s] (%.3f Seconds) Transcribed in %3.3fms [RTF=%3.3f]\n", 
			wave_filename.c_str(), 
			kAudioDuration, 
			kInferElapsed_millisecond,
			kInferElapsed_millisecond / (kAudioDuration * 1000)
		);
	}


	return 0;
}