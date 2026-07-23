#include "timeline.hpp"
#include <regex>
#include <set>
#include <limits>

namespace mvlab {

std::string to_string(TrackType type)
{
    switch (type) {
        case TrackType::audio:
            return "audio";
        case TrackType::video:
            return "video";
        case TrackType::text:
            return "text";
    }
    return "unknown";
}

Result<TrackType> parse_track_type(const std::string& type_str)
{
    if (type_str == "audio") {
        return TrackType::audio;
    } else if (type_str == "video") {
        return TrackType::video;
    } else if (type_str == "text") {
        return TrackType::text;
    }

    return Error{
        ErrorCode::invalid_argument,
        "Unknown track type: " + type_str,
        std::nullopt
    };
}

Result<void> validate_media_clip(const MediaClip& clip)
{
    if (clip.id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Clip ID cannot be empty",
            std::nullopt
        };
    }

    if (clip.asset_id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Asset ID cannot be empty",
            std::nullopt
        };
    }

    if (clip.timeline_start < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Timeline start cannot be negative",
            std::nullopt
        };
    }

    if (clip.source_start < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Source start cannot be negative",
            std::nullopt
        };
    }

    if (clip.duration <= 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Duration must be greater than zero",
            std::nullopt
        };
    }

    // Check for overflow when calculating end time
    // Avoid calculating timeline_start + duration if it would overflow
    if (clip.timeline_start > std::numeric_limits<TimelineTime>::max() - clip.duration) {
        return Error{
            ErrorCode::invalid_argument,
            "Timeline start and duration exceed maximum value",
            std::nullopt
        };
    }

    return Result<void>{};
}

Result<void> validate_text_clip(const TextClip& clip)
{
    if (clip.id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Clip ID cannot be empty",
            std::nullopt
        };
    }

    if (clip.text.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Text cannot be empty",
            std::nullopt
        };
    }

    if (clip.timeline_start < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Timeline start cannot be negative",
            std::nullopt
        };
    }

    if (clip.duration <= 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Duration must be greater than zero",
            std::nullopt
        };
    }

    // Check for overflow when calculating end time
    if (clip.timeline_start > std::numeric_limits<TimelineTime>::max() - clip.duration) {
        return Error{
            ErrorCode::invalid_argument,
            "Timeline start and duration exceed maximum value",
            std::nullopt
        };
    }

    return Result<void>{};
}

Result<void> validate_track(const Track& track)
{
    if (track.id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Track ID cannot be empty",
            std::nullopt
        };
    }

    if (track.name.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Track name cannot be empty",
            std::nullopt
        };
    }

    // Check type consistency: audio/video tracks must have only media clips
    if (track.type == TrackType::audio || track.type == TrackType::video) {
        if (!track.text_clips.empty()) {
            return Error{
                ErrorCode::invalid_argument,
                "Audio and video tracks cannot contain text clips",
                std::nullopt
            };
        }
    }

    // Check type consistency: text tracks must have only text clips
    if (track.type == TrackType::text) {
        if (!track.media_clips.empty()) {
            return Error{
                ErrorCode::invalid_argument,
                "Text tracks cannot contain media clips",
                std::nullopt
            };
        }
    }

    // Validate all media clips and check for duplicates
    std::set<ClipId> media_clip_ids;
    for (const auto& clip : track.media_clips) {
        auto validation = validate_media_clip(clip);
        if (!validation) {
            return validation;
        }
        if (media_clip_ids.count(clip.id) > 0) {
            return Error{
                ErrorCode::invalid_argument,
                "Duplicate clip ID within track: " + clip.id,
                std::nullopt
            };
        }
        media_clip_ids.insert(clip.id);
    }

    // Validate all text clips and check for duplicates
    std::set<ClipId> text_clip_ids;
    for (const auto& clip : track.text_clips) {
        auto validation = validate_text_clip(clip);
        if (!validation) {
            return validation;
        }
        if (text_clip_ids.count(clip.id) > 0) {
            return Error{
                ErrorCode::invalid_argument,
                "Duplicate clip ID within track: " + clip.id,
                std::nullopt
            };
        }
        text_clip_ids.insert(clip.id);
    }

    // Check for overlaps in media clips
    for (size_t i = 0; i < track.media_clips.size(); ++i) {
        const auto& clip_a = track.media_clips[i];
        TimelineTime end_a = clip_a.timeline_start + clip_a.duration;

        for (size_t j = i + 1; j < track.media_clips.size(); ++j) {
            const auto& clip_b = track.media_clips[j];
            TimelineTime end_b = clip_b.timeline_start + clip_b.duration;

            // Clips overlap if one starts before the other ends
            // Allow exact endpoint adjacency: A ends at time T, B can start at time T
            bool overlaps = (clip_a.timeline_start < end_b) && (clip_b.timeline_start < end_a);

            if (overlaps) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Clips overlap on track: " + clip_a.id + " and " + clip_b.id,
                    std::nullopt
                };
            }
        }
    }

    // Check for overlaps in text clips
    for (size_t i = 0; i < track.text_clips.size(); ++i) {
        const auto& clip_a = track.text_clips[i];
        TimelineTime end_a = clip_a.timeline_start + clip_a.duration;

        for (size_t j = i + 1; j < track.text_clips.size(); ++j) {
            const auto& clip_b = track.text_clips[j];
            TimelineTime end_b = clip_b.timeline_start + clip_b.duration;

            bool overlaps = (clip_a.timeline_start < end_b) && (clip_b.timeline_start < end_a);

            if (overlaps) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Clips overlap on track: " + clip_a.id + " and " + clip_b.id,
                    std::nullopt
                };
            }
        }
    }

    return Result<void>{};
}

Result<void> validate_timeline(const Timeline& timeline)
{
    // Validate all tracks
    std::set<TrackId> track_ids;
    for (const auto& track : timeline.tracks) {
        auto validation = validate_track(track);
        if (!validation) {
            return validation;
        }
        if (track_ids.count(track.id) > 0) {
            return Error{
                ErrorCode::invalid_argument,
                "Duplicate track ID: " + track.id,
                std::nullopt
            };
        }
        track_ids.insert(track.id);
    }

    // Collect all clip IDs across all tracks and check for duplicates
    std::set<ClipId> all_clip_ids;
    for (const auto& track : timeline.tracks) {
        for (const auto& clip : track.media_clips) {
            if (all_clip_ids.count(clip.id) > 0) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Duplicate clip ID across timeline: " + clip.id,
                    std::nullopt
                };
            }
            all_clip_ids.insert(clip.id);
        }
        for (const auto& clip : track.text_clips) {
            if (all_clip_ids.count(clip.id) > 0) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Duplicate clip ID across timeline: " + clip.id,
                    std::nullopt
                };
            }
            all_clip_ids.insert(clip.id);
        }
    }

    return Result<void>{};
}

TrackId generate_track_id(const Timeline& timeline)
{
    std::set<int> used_numbers;

    // Extract numbers from existing IDs matching the "track-N" format
    std::regex pattern("^track-(\\d+)$");
    for (const auto& track : timeline.tracks) {
        std::smatch match;
        if (std::regex_match(track.id, match, pattern)) {
            try {
                int num = std::stoi(match[1].str());
                used_numbers.insert(num);
            } catch (...) {
                // Ignore invalid numbers
            }
        }
    }

    // Find the next unused number starting from 1
    int next_number = 1;
    while (used_numbers.count(next_number)) {
        ++next_number;
    }

    return "track-" + std::to_string(next_number);
}

ClipId generate_clip_id(const Timeline& timeline)
{
    std::set<int> used_numbers;

    // Extract numbers from existing IDs matching the "clip-N" format
    // across all tracks and clip types
    std::regex pattern("^clip-(\\d+)$");
    for (const auto& track : timeline.tracks) {
        for (const auto& clip : track.media_clips) {
            std::smatch match;
            if (std::regex_match(clip.id, match, pattern)) {
                try {
                    int num = std::stoi(match[1].str());
                    used_numbers.insert(num);
                } catch (...) {
                    // Ignore invalid numbers
                }
            }
        }
        for (const auto& clip : track.text_clips) {
            std::smatch match;
            if (std::regex_match(clip.id, match, pattern)) {
                try {
                    int num = std::stoi(match[1].str());
                    used_numbers.insert(num);
                } catch (...) {
                    // Ignore invalid numbers
                }
            }
        }
    }

    // Find the next unused number starting from 1
    int next_number = 1;
    while (used_numbers.count(next_number)) {
        ++next_number;
    }

    return "clip-" + std::to_string(next_number);
}

Result<Track*> find_track(Timeline& timeline, const TrackId& id)
{
    if (id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Track ID cannot be empty",
            std::nullopt
        };
    }

    for (auto& track : timeline.tracks) {
        if (track.id == id) {
            return &track;
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Track not found: " + id,
        std::nullopt
    };
}

Result<const Track*> find_track(const Timeline& timeline, const TrackId& id)
{
    if (id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Track ID cannot be empty",
            std::nullopt
        };
    }

    for (const auto& track : timeline.tracks) {
        if (track.id == id) {
            return &track;
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Track not found: " + id,
        std::nullopt
    };
}

Result<MediaClip*> find_media_clip(Timeline& timeline, const ClipId& id)
{
    if (id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Clip ID cannot be empty",
            std::nullopt
        };
    }

    for (auto& track : timeline.tracks) {
        for (auto& clip : track.media_clips) {
            if (clip.id == id) {
                return &clip;
            }
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Media clip not found: " + id,
        std::nullopt
    };
}

Result<const MediaClip*> find_media_clip(const Timeline& timeline, const ClipId& id)
{
    if (id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Clip ID cannot be empty",
            std::nullopt
        };
    }

    for (const auto& track : timeline.tracks) {
        for (const auto& clip : track.media_clips) {
            if (clip.id == id) {
                return &clip;
            }
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Media clip not found: " + id,
        std::nullopt
    };
}

Result<TextClip*> find_text_clip(Timeline& timeline, const ClipId& id)
{
    if (id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Clip ID cannot be empty",
            std::nullopt
        };
    }

    for (auto& track : timeline.tracks) {
        for (auto& clip : track.text_clips) {
            if (clip.id == id) {
                return &clip;
            }
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Text clip not found: " + id,
        std::nullopt
    };
}

Result<const TextClip*> find_text_clip(const Timeline& timeline, const ClipId& id)
{
    if (id.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "Clip ID cannot be empty",
            std::nullopt
        };
    }

    for (const auto& track : timeline.tracks) {
        for (const auto& clip : track.text_clips) {
            if (clip.id == id) {
                return &clip;
            }
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Text clip not found: " + id,
        std::nullopt
    };
}

Result<void> add_track(Timeline& timeline, Track track)
{
    auto validation = validate_track(track);
    if (!validation) {
        return validation;
    }

    for (const auto& existing_track : timeline.tracks) {
        if (existing_track.id == track.id) {
            return Error{
                ErrorCode::invalid_argument,
                "Duplicate track ID: " + track.id,
                std::nullopt
            };
        }
    }

    Timeline candidate = timeline;
    candidate.tracks.push_back(track);

    auto timeline_validation = validate_timeline(candidate);
    if (!timeline_validation) {
        return timeline_validation;
    }

    timeline = candidate;
    return Result<void>{};
}

Result<void> remove_track(Timeline& timeline, const TrackId& track_id)
{
    auto track_result = find_track(timeline, track_id);
    if (!track_result) {
        return track_result.error();
    }

    Timeline candidate = timeline;

    auto it = candidate.tracks.begin();
    while (it != candidate.tracks.end()) {
        if (it->id == track_id) {
            it = candidate.tracks.erase(it);
            break;
        } else {
            ++it;
        }
    }

    timeline = candidate;
    return Result<void>{};
}

Result<void> add_media_clip(
    Timeline& timeline,
    const TrackId& track_id,
    MediaClip clip)
{
    auto track_result = find_track(timeline, track_id);
    if (!track_result) {
        return track_result.error();
    }

    if (track_result.value()->type == TrackType::text) {
        return Error{
            ErrorCode::invalid_argument,
            "Cannot add media clip to text track: " + track_id,
            std::nullopt
        };
    }

    auto clip_validation = validate_media_clip(clip);
    if (!clip_validation) {
        return clip_validation;
    }

    for (const auto& track : timeline.tracks) {
        for (const auto& existing_clip : track.media_clips) {
            if (existing_clip.id == clip.id) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Duplicate clip ID: " + clip.id,
                    std::nullopt
                };
            }
        }
        for (const auto& existing_clip : track.text_clips) {
            if (existing_clip.id == clip.id) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Duplicate clip ID: " + clip.id,
                    std::nullopt
                };
            }
        }
    }

    Timeline candidate = timeline;

    auto mutable_track = find_track(candidate, track_id);
    if (!mutable_track) {
        return mutable_track.error();
    }

    mutable_track.value()->media_clips.push_back(clip);

    auto timeline_validation = validate_timeline(candidate);
    if (!timeline_validation) {
        return timeline_validation;
    }

    timeline = candidate;
    return Result<void>{};
}

Result<void> add_text_clip(
    Timeline& timeline,
    const TrackId& track_id,
    TextClip clip)
{
    auto track_result = find_track(timeline, track_id);
    if (!track_result) {
        return track_result.error();
    }

    if (track_result.value()->type != TrackType::text) {
        return Error{
            ErrorCode::invalid_argument,
            "Cannot add text clip to non-text track: " + track_id,
            std::nullopt
        };
    }

    auto clip_validation = validate_text_clip(clip);
    if (!clip_validation) {
        return clip_validation;
    }

    for (const auto& track : timeline.tracks) {
        for (const auto& existing_clip : track.media_clips) {
            if (existing_clip.id == clip.id) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Duplicate clip ID: " + clip.id,
                    std::nullopt
                };
            }
        }
        for (const auto& existing_clip : track.text_clips) {
            if (existing_clip.id == clip.id) {
                return Error{
                    ErrorCode::invalid_argument,
                    "Duplicate clip ID: " + clip.id,
                    std::nullopt
                };
            }
        }
    }

    Timeline candidate = timeline;

    auto mutable_track = find_track(candidate, track_id);
    if (!mutable_track) {
        return mutable_track.error();
    }

    mutable_track.value()->text_clips.push_back(clip);

    auto timeline_validation = validate_timeline(candidate);
    if (!timeline_validation) {
        return timeline_validation;
    }

    timeline = candidate;
    return Result<void>{};
}

Result<void> remove_clip(Timeline& timeline, const ClipId& clip_id)
{
    auto media_clip_result = find_media_clip(timeline, clip_id);
    auto text_clip_result = find_text_clip(timeline, clip_id);

    if (!media_clip_result && !text_clip_result) {
        return Error{
            ErrorCode::file_not_found,
            "Clip not found: " + clip_id,
            std::nullopt
        };
    }

    Timeline candidate = timeline;

    for (auto& track : candidate.tracks) {
        auto it = track.media_clips.begin();
        while (it != track.media_clips.end()) {
            if (it->id == clip_id) {
                it = track.media_clips.erase(it);
                timeline = candidate;
                return Result<void>{};
            } else {
                ++it;
            }
        }

        auto text_it = track.text_clips.begin();
        while (text_it != track.text_clips.end()) {
            if (text_it->id == clip_id) {
                text_it = track.text_clips.erase(text_it);
                timeline = candidate;
                return Result<void>{};
            } else {
                ++text_it;
            }
        }
    }

    return Error{
        ErrorCode::file_not_found,
        "Clip not found: " + clip_id,
        std::nullopt
    };
}

Result<void> move_clip(
    Timeline& timeline,
    const ClipId& clip_id,
    TimelineTime new_timeline_start)
{
    if (new_timeline_start < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "New timeline start cannot be negative",
            std::nullopt
        };
    }

    auto media_clip_result = find_media_clip(timeline, clip_id);
    bool is_media = media_clip_result.operator bool();

    if (!is_media) {
        auto text_clip_result = find_text_clip(timeline, clip_id);
        if (!text_clip_result) {
            return Error{
                ErrorCode::file_not_found,
                "Clip not found: " + clip_id,
                std::nullopt
            };
        }
    }

    Timeline candidate = timeline;

    if (is_media) {
        auto mutable_clip = find_media_clip(candidate, clip_id);
        if (!mutable_clip) {
            return mutable_clip.error();
        }

        if (new_timeline_start > std::numeric_limits<TimelineTime>::max() - mutable_clip.value()->duration) {
            return Error{
                ErrorCode::invalid_argument,
                "New timeline start and duration exceed maximum value",
                std::nullopt
            };
        }

        mutable_clip.value()->timeline_start = new_timeline_start;
    } else {
        auto mutable_clip = find_text_clip(candidate, clip_id);
        if (!mutable_clip) {
            return mutable_clip.error();
        }

        if (new_timeline_start > std::numeric_limits<TimelineTime>::max() - mutable_clip.value()->duration) {
            return Error{
                ErrorCode::invalid_argument,
                "New timeline start and duration exceed maximum value",
                std::nullopt
            };
        }

        mutable_clip.value()->timeline_start = new_timeline_start;
    }

    auto timeline_validation = validate_timeline(candidate);
    if (!timeline_validation) {
        return timeline_validation;
    }

    timeline = candidate;
    return Result<void>{};
}

} // namespace mvlab
