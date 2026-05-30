#pragma once
#include <gtest/gtest.h>
#include <sttserv/backend.hpp>
#include <string>
#include <vector>
#include <memory>


extern CommandLineArguments g_args;


enum class NoiseLevel { QUIET, MID, NOISY };

struct AudioTestParam {
    std::string filepath;
    i32         sentence_idx;
    NoiseLevel  noise;
};

struct TestResultMetric {
    i8    sentence_idx;
    bool  passed;
    bool  reserved[2];
    float accuracy;
};

class ASRModelTest : public testing::TestWithParam<AudioTestParam> {
protected:
    static std::unique_ptr<ModelBackend> sh_backend;
    static std::vector<TestResultMetric> s_stats;

    static void SetUpTestSuite();
    static void TearDownTestSuite();
};


bool                        discover_sentences(std::string const& txtFileFullPath, std::vector<std::string>& outSentences); 
bool                        discover_audio_files(std::string const& audio_base_dir, std::vector<AudioTestParam>& outFiles);
std::vector<AudioTestParam> discover_relevant_data(std::string const& base_dir, std::string const& sentenceTxtFileName);
std::string                 normalize_text(std::string const& s);
float                       calculate_accuracy(std::string const& ref, std::string const& hyp);
f32                         get_percentile(std::vector<f32> const& sorted_vec, f32 percentile);