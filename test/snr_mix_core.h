// snr_mix_core — additive noise mixing at a controlled signal-to-noise ratio.
//
// Header-only, zero dependencies, operates on plain float PCM buffers so any
// project can include it (the noisefilter benchmark and the sttserv ASR test
// both do). A library function, not a tool.
//
// It scales a noise buffer so that, added to speech, the mixture sits at a
// requested SNR (in dB), and returns the sum.
//
//   SNR(dB) = 20 * log10(rms_speech / rms_noise_after_scaling)
//   gain    = (rms_speech / rms_noise) / 10^(target_dB / 20)
//
// The noise is linearly resampled to the speech rate and tiled if shorter than
// the speech. If the sum would clip past +/-1, the whole mixture is scaled down
// and the attenuation is reported through out_clip_atten.

#pragma once
#ifndef SNR_MIX_CORE_H
#define SNR_MIX_CORE_H

#include <cmath>
#include <vector>

namespace snrmix {

inline double rms(const std::vector<float>& x) {
    if (x.empty()) return 0.0;
    double acc = 0.0;
    for (float v : x) acc += static_cast<double>(v) * v;
    return std::sqrt(acc / x.size());
}

// Linear resample from src_rate to dst_rate. Good enough for a noise bed; the
// speech is never resampled, so no artefact touches the signal under test.
inline std::vector<float> resample(const std::vector<float>& in, int src_rate, int dst_rate) {
    if (src_rate == dst_rate || in.empty()) return in;
    const double ratio = static_cast<double>(dst_rate) / src_rate;
    const size_t out_n = static_cast<size_t>(in.size() * ratio);
    std::vector<float> out(out_n);
    for (size_t i = 0; i < out_n; ++i) {
        const double sp = i / ratio;
        const size_t i0 = static_cast<size_t>(sp);
        const size_t i1 = i0 + 1 < in.size() ? i0 + 1 : i0;
        const double frac = sp - i0;
        out[i] = static_cast<float>(in[i0] * (1.0 - frac) + in[i1] * frac);
    }
    return out;
}

// Mix `noise` into `speech` at `snr_db`. Both buffers are mono. The result is at
// the speech sample rate and the speech length. On bad input (empty or silent)
// returns an empty vector. out_gain / out_clip_atten are optional.
inline std::vector<float> mix_at_snr(const std::vector<float>& speech, int speech_rate,
                                     const std::vector<float>& noise,  int noise_rate,
                                     double snr_db, double noise_offset_sec = 0.0,
                                     double* out_gain = nullptr, double* out_clip_atten = nullptr) {
    if (out_gain)       *out_gain = 0.0;
    if (out_clip_atten) *out_clip_atten = 1.0;

    std::vector<float> nz = resample(noise, noise_rate, speech_rate);
    if (speech.empty() || nz.empty()) return {};

    const double rs = rms(speech);
    const double rn = rms(nz);
    if (rs == 0.0 || rn == 0.0) return {};

    const double gain = (rs / rn) / std::pow(10.0, snr_db / 20.0);
    if (out_gain) *out_gain = gain;

    const size_t off = static_cast<size_t>(noise_offset_sec * speech_rate) % nz.size();
    std::vector<float> mix(speech.size());
    float peak = 0.f;
    for (size_t i = 0; i < speech.size(); ++i) {
        const float n = nz[(off + i) % nz.size()];
        const float v = speech[i] + static_cast<float>(gain * n);
        mix[i] = v;
        const float a = std::fabs(v);
        if (a > peak) peak = a;
    }

    if (peak > 1.0f) {
        const double atten = 1.0 / peak;
        for (float& v : mix) v = static_cast<float>(v * atten);
        if (out_clip_atten) *out_clip_atten = atten;
    }
    return mix;
}

} // namespace snrmix

#endif
