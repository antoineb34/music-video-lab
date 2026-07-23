#ifndef MVLAB_TIMELINE_HPP
#define MVLAB_TIMELINE_HPP

#include "result.hpp"
#include "media_asset.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace mvlab {

// TimelineTime represents a point or duration in the timeline using integer
// microseconds. This avoids floating-point precision issues for timeline
// positions and durations.
using TimelineTime = std::int64_t;

// Stable track and clip identity types.
using TrackId = std::string;
using ClipId = std::string;

enum class TrackType {
    audio,
    video,
    text
};

// Converts a TrackType enum value to its string representation.
std::string to_string(TrackType type);

// Parses a string to a TrackType. Returns invalid_argument if the string is
// not a recognized track type.
Result<TrackType> parse_track_type(const std::string& type_str);

// A clip on an audio or video track that references an existing media asset.
struct MediaClip {
    ClipId id;
    AssetId asset_id;
    TimelineTime timeline_start;   // where this clip appears on the track
    TimelineTime source_start;     // where in the source media to start playing
    TimelineTime duration;         // how long to play from source_start

    // timeline occupies [timeline_start, timeline_start + duration)
};

// A clip on a text track containing editable text.
struct TextClip {
    ClipId id;
    std::string text;
    TimelineTime timeline_start;   // where this clip appears on the track
    TimelineTime duration;         // how long to display the text
};

// A track is a sequence of clips (either media or text).
struct Track {
    TrackId id;
    TrackType type;
    std::string name;
    std::vector<MediaClip> media_clips;
    std::vector<TextClip> text_clips;
};

// A timeline is a collection of tracks representing the overall structure.
struct Timeline {
    std::vector<Track> tracks;
};

// Validation functions return invalid_argument for validation failures.

// Validates a MediaClip. Requires:
// - id is not empty
// - asset_id is not empty
// - timeline_start >= 0
// - source_start >= 0
// - duration > 0
Result<void> validate_media_clip(const MediaClip& clip);

// Validates a TextClip. Requires:
// - id is not empty
// - text is not empty
// - timeline_start >= 0
// - duration > 0
Result<void> validate_text_clip(const TextClip& clip);

// Validates a Track. Requires:
// - id is not empty
// - name is not empty
// - type is consistent with clip contents (audio/video tracks contain
//   media clips only; text tracks contain text clips only)
// - no duplicate clip IDs within this track
// - clips do not overlap (allow exact endpoint adjacency)
Result<void> validate_track(const Track& track);

// Validates a Timeline. Requires:
// - every track is valid
// - no duplicate track IDs
// - no duplicate clip IDs across all tracks
Result<void> validate_timeline(const Timeline& timeline);

// ID generation functions find the first available positive integer and
// return stable, readable IDs (track-1, track-2, etc.).

// Generates a unique track ID for the given timeline.
TrackId generate_track_id(const Timeline& timeline);

// Generates a unique clip ID for the given timeline.
ClipId generate_clip_id(const Timeline& timeline);

// Lookup functions return file_not_found for unknown IDs and invalid_argument
// for empty IDs.

// Find a mutable track by ID in the timeline.
Result<Track*> find_track(Timeline& timeline, const TrackId& id);

// Find a const track by ID in the timeline.
Result<const Track*> find_track(const Timeline& timeline, const TrackId& id);

// Find a media clip by ID anywhere in the timeline (searches all tracks).
// Returns a pointer to the clip and the track it belongs to.
Result<MediaClip*> find_media_clip(Timeline& timeline, const ClipId& id);

// Find a const media clip by ID anywhere in the timeline.
Result<const MediaClip*> find_media_clip(const Timeline& timeline, const ClipId& id);

// Find a text clip by ID anywhere in the timeline (searches all tracks).
Result<TextClip*> find_text_clip(Timeline& timeline, const ClipId& id);

// Find a const text clip by ID anywhere in the timeline.
Result<const TextClip*> find_text_clip(const Timeline& timeline, const ClipId& id);

} // namespace mvlab

#endif // MVLAB_TIMELINE_HPP
