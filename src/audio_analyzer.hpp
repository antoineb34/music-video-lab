#ifndef MVLAB_AUDIO_ANALYZER_HPP
#define MVLAB_AUDIO_ANALYZER_HPP

#include <string>
#include <vector>
#include <utility>
#include <cstdint>

namespace mvlab {

struct AudioAnalysisResult {
    uint64_t total_frames = 0;
    int channels = 0;
    int sample_rate = 0;
    double peak_amplitude = 0.0;
    double rms_amplitude = 0.0;
    std::vector<double> waveform_envelope;
};

// Analyze audio file using ffmpeg to decode PCM and C++ to compute metrics
// Returns result and error message (empty string if successful)
std::pair<AudioAnalysisResult, std::string> analyze_audio(
    const std::string& file_path,
    int envelope_points = 100
);

} // namespace mvlab

#endif // MVLAB_AUDIO_ANALYZER_HPP
