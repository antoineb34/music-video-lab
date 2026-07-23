#ifndef MVLAB_AUDIO_INSPECTOR_HPP
#define MVLAB_AUDIO_INSPECTOR_HPP

#include <string>
#include <utility>

namespace mvlab {

struct AudioInspectResult {
    std::string codec;
    std::string sample_rate;
    std::string channels;
    std::string stream_bitrate;
    std::string duration;
    std::string format_bitrate;
};

std::pair<AudioInspectResult, std::string> inspect_audio(const std::string& file_path);

} // namespace mvlab

#endif // MVLAB_AUDIO_INSPECTOR_HPP
