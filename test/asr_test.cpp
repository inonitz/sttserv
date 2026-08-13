#include "asr_test.hpp"
#include <cmath>
#include <string>
#include <util2/C/print2.h>
#include <miniaudio.h>
#include <algorithm>
#include <cctype>
#include <regex>
#include <filesystem>
#include <fstream>
#include <map>
#include <limits>
#include "snr_mix_core.h"


namespace fs = std::filesystem;


static constexpr const char*                kAudioFileDirectory = "../../../../dependencies/recordings";
static const std::string                    kAudioFileSentences = "sentences.txt"; 
// --- SNR robustness sweep: gunfire/explosion beds mixed into the clean clips ---
static const std::string              kNoiseBedDir = "../../../../dependencies/noise_beds";
static const std::vector<std::string> kNoiseBeds   = { "battle_0.wav", "battle_1.wav", "battle_2.wav", "battle_3.wav" }; // firefight, explosion, artillery, rifle
static const std::vector<float>       kSnrGrid     = { 20,18,16,14,12,10,8,6,4,2,0,-2,-4,-6,-8,-10 };
static std::vector<std::string>             gs_testSentences;
std::unique_ptr<ModelBackend> ASRModelTest::sh_backend;
std::vector<TestResultMetric> ASRModelTest::s_stats;


static ma_result createAudioDecoder(std::string const& filepath, ma_decoder* outDecoder);
static ma_result readToVector(ma_decoder* inDecoder, std::vector<f32>& outPcmData);
static ma_result destroyAudioDecoder(ma_decoder* in);


void ASRModelTest::SetUpTestSuite() {
    sh_backend = std::make_unique<ModelBackend>();

    ASSERT_TRUE(sh_backend->create(g_args, BackendType::BACKEND_MAX));
    s_stats.clear();

    if(gs_testSentences.empty()) {
        GTEST_SKIP() << "Sentence File is empty. Skipping All Tests\n";
    }
    return;
}

void ASRModelTest::TearDownTestSuite() {
    /* First destroy the backend as it is not required for stats. */
    if (sh_backend.get()) {
        sh_backend->destroy();
        sh_backend.reset();
    }

    /* Early exit if no relevant data collected */
    if (s_stats.empty()) {
        return;
    }


    std::vector<f32>                allAccuracy;
    std::array<std::vector<f32>, 5> sentenceAccuracy;
    u16 testsPassed = 0;
    u16 testsTotal  = 0;
    f32 min_val  = 0;
    f32 max_val  = 0;
    f32 median   = 0;
    f32 p99      = 0;
    f32 p999     = 0;

    for (auto const& m : s_stats) {
        allAccuracy.push_back(m.accuracy);
        sentenceAccuracy[static_cast<size_t>(m.sentence_idx)].push_back(m.accuracy);
        testsPassed += m.passed;
        ++testsTotal;
    }


    std::sort(allAccuracy.begin(), allAccuracy.end());
    min_val  = allAccuracy.front();
    max_val  = allAccuracy.back();
    median   = get_percentile(allAccuracy, 0.50f);
    p99      = get_percentile(allAccuracy, 0.99f);
    p999     = get_percentile(allAccuracy, 0.999f);
    util2_fprintf(stdout, "\n==================================================\n");
    util2_fprintf(stdout, "   AGGREGATED MODEL ACCURACY PERFORMANCE REPORT   \n");
    util2_fprintf(stdout, "==================================================\n");
    // util2_fprintf(stderr, "Model %s\n", sh_backend->);
    util2_fprintf(stdout, "Global Stats across %zu files:\n", allAccuracy.size());
    util2_fprintf(stdout, "  Tests Passed:  %u/%u (%3.3f%%)\n", 
        testsPassed, 
        testsTotal, 
        static_cast<f32>(testsPassed) / static_cast<f32>(testsTotal)
    );
    util2_fprintf(stdout, "  Accuracy\n");
    util2_fprintf(stdout, "    Min:   %3.2f%%\n", min_val);
    util2_fprintf(stdout, "    p50:   %3.2f%%\n", median);
    util2_fprintf(stdout, "    p99:   %3.2f%%\n", p99);
    util2_fprintf(stdout, "    p99.9: %3.2f%%\n", p999);
    util2_fprintf(stdout, "    Max:   %3.2f%%\n", max_val);
    util2_fprintf(stdout, "--------------------------------------------------\n");
    util2_fprintf(stdout, "Per-Sentence Median & Min Accuracies:\n");

    for (u32 i = 0; i < sentenceAccuracy.size(); ++i) {
        auto& accAt = sentenceAccuracy[i];
        std::sort(accAt.begin(), accAt.end());
        f32 sent_median = get_percentile(accAt, 0.50f);
        util2_fprintf(stdout, 
            "  Sentence [%2u]: p50=%3.2f%%, min=%3.2f%% (across %zu variations)\n", 
            i, 
            sent_median,
            accAt[0], /* Sorted buffer is in ascending order. Min will be @start */
            accAt.size()
        );
    }
    /* ---- Accuracy-vs-SNR curve (the robustness deliverable) ---- */
    std::map<int, std::vector<f32>> bySnr;
    for (auto const& m : s_stats) {
        if (std::isfinite(m.snr_db)) {
            bySnr[static_cast<int>(std::lround(m.snr_db))].push_back(m.accuracy);
        }
    }
    if (!bySnr.empty()) {
        util2_fprintf(stdout, "--------------------------------------------------\n");
        util2_fprintf(stdout, "Accuracy vs SNR (gunfire/explosion noise):\n");
        util2_fprintf(stdout, "   SNR(dB)    n   mean_acc   median   pass>=75%%\n");
        for (auto it = bySnr.rbegin(); it != bySnr.rend(); ++it) {   /* easy -> hard */
            std::vector<f32> v = it->second;
            std::sort(v.begin(), v.end());
            double sum = 0; int pass = 0;
            for (f32 a : v) { sum += a; if (a >= 75.0f) ++pass; }
            util2_fprintf(stdout, "   %+5d   %4zu   %6.2f%%   %6.2f%%   %3d/%zu\n",
                it->first, v.size(), sum / v.size(), get_percentile(v, 0.50f), pass, v.size());
        }
    }
    util2_fprintf(stdout, "==================================================\n\n");
    return;
}


TEST_P(ASRModelTest, TranscribeAndVerify) {
    AudioTestParam         param = GetParam();
    bool                   status = true;
    ma_result              mastatus = MA_SUCCESS;
    ma_decoder             audioReader;
    std::vector<f32>       audioData;
    std::array<char, 1024> fixedSizeOutput{};
    u32                    duration_ms = 0;
    std::string            expected_text, actual_text;


    if( ( mastatus = createAudioDecoder(param.filepath, &audioReader) ) != MA_SUCCESS) {
        fprintf(stderr, "Audio-Decoder Creation Failed -> %s ( audiofile=[%s] )\n", 
            ma_result_description(mastatus),
            param.filepath.c_str()
        );
        ASSERT_TRUE(false);
    }
    if( ( mastatus = readToVector(&audioReader, audioData) ) != MA_SUCCESS) {
        fprintf(stderr, "Audio Decoder Read Failed -> %s\n", ma_result_description(mastatus));
        ASSERT_TRUE(false);
    }
    ASSERT_EQ(MA_SUCCESS, destroyAudioDecoder(&audioReader));


    /* SNR robustness sweep: mix a gunfire/explosion bed into the clean speech */
    if (std::isfinite(param.snr_db) && !param.noise_path.empty()) {
        ma_decoder       noiseReader;
        std::vector<f32> noiseData;
        if (createAudioDecoder(param.noise_path, &noiseReader) != MA_SUCCESS) {
            fprintf(stderr, "Noise bed open failed: %s\n", param.noise_path.c_str());
            ASSERT_TRUE(false);
        }
        if (readToVector(&noiseReader, noiseData) != MA_SUCCESS) {
            fprintf(stderr, "Noise bed read failed: %s\n", param.noise_path.c_str());
            ASSERT_TRUE(false);
        }
        destroyAudioDecoder(&noiseReader);
        std::vector<f32> mixed = snrmix::mix_at_snr(
            audioData, CommandLineArguments::kInferenceSampleRate,
            noiseData, CommandLineArguments::kInferenceSampleRate,
            static_cast<double>(param.snr_db), static_cast<double>(param.noise_offset_sec));
        if (!mixed.empty()) audioData.swap(mixed);
    }


    /* Transcribe and get result */
    duration_ms = static_cast<u32>(
        (audioData.size() * CommandLineArguments::kMillisecondsIn1Second) 
            / CommandLineArguments::kInferenceSampleRate
    );

    status = sh_backend->transcribe(
        audioData.data(), 
        audioData.size(), 
        duration_ms, 
        CommandLineArguments::kInferenceSampleRate
    );
    ASSERT_TRUE(status);

    status = sh_backend->result(fixedSizeOutput);
    ASSERT_TRUE(status);


    expected_text = normalize_text(gs_testSentences[__scast(u32, param.sentence_idx)]);
    actual_text   = normalize_text(fixedSizeOutput.data());
    
    f32 accuracy = calculate_accuracy(expected_text, actual_text);
    f32 threshold = (param.noise == NoiseLevel::QUIET) ? 95.0f 
        : (param.noise == NoiseLevel::MID) ? 85.0f 
        : 75.0f; /* Noisy */


    util2_fprintf(stderr, "=== TEST RESULTS ===\n");
    util2_fprintf(stderr, "File:     %s\n", param.filepath.c_str());
    util2_fprintf(stderr, "Accuracy: %3.5f (>%3.5f)\n", accuracy, threshold);
    util2_fprintf(stderr, "Expected: %s\n", expected_text.c_str());
    util2_fprintf(stderr, "Actual:   %s\n", actual_text.c_str());
    util2_fprintf(stderr, "=== TEST RESULTS ===\n");
    s_stats.push_back({
        __scast(i8, param.sentence_idx), 
        accuracy >= threshold,
        {},
        accuracy,
        param.snr_db
    });
    

    // {
    //     int _ntok = 0; float _meanp = 0.0f;
    //     extern float confidence_parakeet_impl(void*, int*, float*);
    //     float _cf = confidence_parakeet_impl(sh_backend->m_internalState, &_ntok, &_meanp);
    //     util2_fprintf(stderr, "CFDUMP file=%s acc=%.3f pass=%d geomean=%.4f meanp=%.4f ntok=%d\n",
    //         param.filepath.c_str(), accuracy, (int)(accuracy>=threshold), _cf, _meanp, _ntok);
    // }

    EXPECT_GE(accuracy, threshold);
    return;
}
// -----------------------------------------------------------------------------
// Dynamic Test Registration
// -----------------------------------------------------------------------------
static std::vector<AudioTestParam> build_all_params() {
    std::vector<AudioTestParam> base = discover_relevant_data(kAudioFileDirectory, kAudioFileSentences);
    std::vector<AudioTestParam> all;
    for (auto const& p : base) {
        all.push_back(p);                              /* original clip, snr=INF (no mix) */
        if (p.noise != NoiseLevel::QUIET) continue;    /* only re-mix genuinely clean clips */
        for (size_t b = 0; b < kNoiseBeds.size(); ++b) {
            for (float snr : kSnrGrid) {
                AudioTestParam v = p;
                v.snr_db           = snr;
                v.noise_path       = kNoiseBedDir + "/" + kNoiseBeds[b];
                v.noise_offset_sec = static_cast<float>(p.sentence_idx) * 2.3f + static_cast<float>(b) * 5.7f;
                all.push_back(v);
            }
        }
    }
    fprintf(stderr, "[SNR-SWEEP] base=%zu total=%zu (beds=%zu grid=%zu)\n",
        base.size(), all.size(), kNoiseBeds.size(), kSnrGrid.size());
    return all;
}

INSTANTIATE_TEST_SUITE_P(
    DynamicAudioFolder,
    ASRModelTest,
    testing::ValuesIn(build_all_params())
);


bool discover_sentences(
    std::string const&        fullPathToTxtFile,
    std::vector<std::string>& outSentences
) {
    if (!fs::exists(fullPathToTxtFile) || !fs::is_regular_file(fullPathToTxtFile)) {
        fprintf(stderr, "[Warning] Sentence File Not found [file=%s] [CWD is [%s] ]\n", 
            fullPathToTxtFile.c_str(), 
            fs::current_path().c_str()
        );
        return false;
    }


    std::ifstream is(fullPathToTxtFile);
    std::string str;
    while(getline(is, str)) {
        outSentences.push_back(str);
    }
    return true;
}

bool discover_audio_files(
    std::string const&           audio_base_dir, 
    std::vector<AudioTestParam>& outFiles
) {
    if (!fs::exists(audio_base_dir) || !fs::is_directory(audio_base_dir)) {
        std::cerr << "[Warning] Audio directory not found: " << audio_base_dir << " (CWD is [" << fs::current_path() << "])\n";
        return false;
    }


    /* Regex to find digits representing sentence index */
    std::regex index_regex(R"(\d+)");
    std::string filename, filepath;

    fprintf(stderr, "----------------------------------------------------------------\n");
    for (const auto& entry : fs::recursive_directory_iterator(audio_base_dir)) {
        fprintf(stderr, "[BGN] Entry is %s\n", entry.path().c_str());
        if (!entry.is_regular_file()) continue;

        fprintf(stderr, "[CON] Entry is %s\n", entry.path().c_str());
        filename = entry.path().filename().string();
        filepath = entry.path().string();


        /* 1. Determine Noise Level via string tags */
        NoiseLevel noise = NoiseLevel::QUIET; // default
        noise = filename.find("mid")   != std::string::npos ? NoiseLevel::MID : 
                filename.find("noisy") != std::string::npos ? NoiseLevel::NOISY : 
                noise;


        /* 2. Determine Sentence Index via regex */
        int sentence_idx = -1;
        std::smatch match;
        if (std::regex_search(filename, match, index_regex)) {
            sentence_idx = std::stoi(match.str());
        }

        /* 3. Validate index bounds against dictionary before adding */
        if (sentence_idx >= 0 && sentence_idx < static_cast<int>(gs_testSentences.size())) {
            outFiles.push_back({
                filepath, 
                static_cast<i8>(sentence_idx), 
                noise
            });

            // auto noiseLevelString = outFiles.back().noise == NoiseLevel::QUIET ? "Quiet" 
            //     : outFiles.back().noise == NoiseLevel::MID ? " Mid "
            //     : outFiles.back().noise == NoiseLevel::NOISY ? "Noisy"
            //     : "Unknown";
            // ;
            // fprintf(stderr, "Sentence-Index %u [%s] in File %s\n", 
            //     outFiles.back().sentence_idx,
            //     noiseLevelString,
            //     outFiles.back().filepath.c_str()
            // );
        }
    }
    /* stdout is swallowed by something, probably ctest/gtest (it is ctest) */
    // fprintf(stderr, "----------------------------------------------------------------\n");


    return true;
}

std::vector<AudioTestParam> discover_relevant_data(
    std::string const& base_dir,
    std::string const& sentenceTxtFileName
)
{
    std::vector<AudioTestParam> discovered_audio;
    std::error_code ec;

    if (!fs::exists(base_dir) || !fs::is_directory(base_dir)) {
        fprintf(stderr, "Audio directory not found: %s (CWD is [%s])\n", 
            base_dir.c_str(), 
            fs::current_path().c_str()
        );
        return discovered_audio;
    }


    auto iterator = fs::recursive_directory_iterator(
        base_dir, 
        fs::directory_options::skip_permission_denied, 
        ec
    );
    if (ec) {
        fprintf(stderr, "Error initializing directory iterator: %s\n", ec.message().c_str());
        return discovered_audio;
    }

    /* Find the sentence.txt file and populate the global variable */
    for (const auto& entry : iterator) 
    {
        fprintf(stderr, "%s | %s\n", entry.path().filename().c_str(), sentenceTxtFileName.c_str());
        if(entry.path().filename() == sentenceTxtFileName) {
            if(discover_sentences(entry.path(), gs_testSentences)) {
                fprintf(stderr, "Could not retrieve Sentences from %s\n", 
                    entry.path().c_str()
                );
            }
            break;
        }

        if(ec) {
            fprintf(stderr, "Error for %s ==> %s\n", 
                entry.path().c_str(),
                ec.message().c_str()
            );
            ec.clear();
        }
    }


    if(!discover_audio_files(base_dir, discovered_audio)) {
        fprintf(stderr, "Could not retrieve Audio Files from %s\n", 
            base_dir.c_str()
        );
    }

    // for(u32 i = 0; i < gs_testSentences.size(); ++i) {
    //     fprintf(stderr, "Sentence %u -> %s\n", i, gs_testSentences[i].c_str());
    // }
    // for(u32 i = 0; i < discovered_audio.size(); ++i) {
    //     fprintf(stderr, "Audio File %u -> %s\n", i, discovered_audio[i].filepath.c_str());
    // }
    return discovered_audio;
}





std::string normalize_text(std::string const& s) {
    std::string sOut = s;
    sOut.erase(
        std::remove_if(sOut.begin(), sOut.end(), 
            [](char c) { return std::ispunct(c); }
        ), 
        sOut.end()
    );
    std::transform(sOut.begin(), sOut.end(), sOut.begin(), 
        [](unsigned char c) { return std::tolower(c); }
    );
    return s;
}

f32 calculate_accuracy(
	const std::string& ref, 
	const std::string& hyp
) {
    /* Levenshtein Distance between 2 strings */
    size_t m = ref.length();
    size_t n = hyp.length();

    if (m == 0) {
        return n == 0 ? 100.0f : 0.0f;
    }

    std::vector<std::vector<size_t>> d(m + 1, std::vector<size_t>(n + 1));
    for (size_t i = 0; i <= m; ++i) { 
        d[i][0] = i;
    }
    for (size_t j = 0; j <= n; ++j) {
        d[0][j] = j;
    }
    for (size_t i = 1; i <= m; ++i) 
    {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (ref[i - 1] == hyp[j - 1]) ? 0 : 1;
            d[i][j] = std::min({ 
                d[i - 1][j] + 1, 
                d[i][j - 1] + 1, 
                d[i - 1][j - 1] + cost 
            });
        }
    }


    return 100.0f * (  1.0f - ( static_cast<f32>(d[m][n]) / static_cast<f32>(std::max(m, n)) )  );
}

f32 get_percentile(std::vector<f32> const& sorted_vec, f32 percentile) {
    if (sorted_vec.empty()) {
        return 0.0f;
    }
    
    size_t idx = static_cast<size_t>(
        std::ceil( percentile * static_cast<f32>(sorted_vec.size()) )
    ) - 1;
    idx = std::clamp(idx, size_t{0}, sorted_vec.size() - 1);
    return sorted_vec[idx];
}




static ma_result createAudioDecoder(std::string const& filepath, ma_decoder* outDecoder) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, CommandLineArguments::kInferenceSampleRate);
    
    return ma_decoder_init_file(filepath.c_str(), &config, outDecoder);
}

static ma_result readToVector(ma_decoder* inDecoder, std::vector<f32>& outPcmData) {
    ma_uint64 framesToRead = 0;
    ma_result status = MA_SUCCESS;

    ma_decoder_get_length_in_pcm_frames(inDecoder, &framesToRead);
    outPcmData.resize(framesToRead);
    status = ma_decoder_read_pcm_frames(inDecoder, outPcmData.data(), framesToRead, nullptr);

    return status;
}

static ma_result destroyAudioDecoder(ma_decoder* in) {
    return ma_decoder_uninit(in);
}