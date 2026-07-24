#ifndef MVLAB_TIMELINE_HPP
#define MVLAB_TIMELINE_HPP

#include "result.hpp"
#include "media_asset.hpp"
#include "text_presentation.hpp"
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
    TextPresentation presentation; // visual style and animations
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

// Editing operations return invalid_argument for validation failures,
// file_not_found for missing targets, and invalid_argument for duplicates.

// Add a track to the timeline. On failure, timeline is unchanged.
// Validates the complete timeline and rejects duplicate track IDs or
// globally duplicate clip IDs.
Result<void> add_track(Timeline& timeline, Track track);

// Remove a track and all its clips from the timeline. On failure, timeline
// is unchanged. Returns file_not_found if track does not exist.
Result<void> remove_track(Timeline& timeline, const TrackId& track_id);

// Add a media clip to an existing audio or video track. On failure, timeline
// is unchanged. Rejects insertion into text tracks, missing tracks, invalid
// clips, globally duplicate clip IDs, and overlaps (allows exact adjacency).
Result<void> add_media_clip(
    Timeline& timeline,
    const TrackId& track_id,
    MediaClip clip);

// Add a text clip to an existing text track. On failure, timeline is
// unchanged. Rejects insertion into media tracks, missing tracks, invalid
// clips, globally duplicate clip IDs, and overlaps (allows exact adjacency).
Result<void> add_text_clip(
    Timeline& timeline,
    const TrackId& track_id,
    TextClip clip);

// Remove a clip (media or text) from its track. On failure, timeline is
// unchanged. Searches globally and returns file_not_found if absent.
Result<void> remove_clip(Timeline& timeline, const ClipId& clip_id);

// Move a clip (media or text) to a new timeline start time. On failure,
// timeline is unchanged. Preserves the clip's duration and all other fields.
// Rejects negative start times, overflow, and resulting overlaps.
Result<void> move_clip(
    Timeline& timeline,
    const ClipId& clip_id,
    TimelineTime new_timeline_start);

// Track organization operations

// Rename a track. On failure, timeline is unchanged. Rejects empty names.
Result<void> rename_track(
    Timeline& timeline,
    const TrackId& track_id,
    std::string new_name);

// Move a track to a new zero-based index position. On failure, timeline is
// unchanged. Allows moving to current index as a no-op. Rejects out-of-range
// indices. Preserves all tracks and clips exactly.
Result<void> move_track(
    Timeline& timeline,
    const TrackId& track_id,
    std::size_t new_index);

// Trim operations

// Trim the start of a media clip by adjusting timeline_start and source_start
// while reducing duration. new_timeline_start must be strictly after the
// current start and strictly before the clip end. On failure, timeline is
// unchanged. Rejects text clips and resulting overlap.
Result<void> trim_media_clip_start(
    Timeline& timeline,
    const ClipId& clip_id,
    TimelineTime new_timeline_start);

// Trim the end of a clip (media or text) to a new timeline end position.
// new_timeline_end must be strictly after the clip start and at or before
// the current end. On failure, timeline is unchanged. Does not extend clips.
Result<void> trim_clip_end(
    Timeline& timeline,
    const ClipId& clip_id,
    TimelineTime new_timeline_end);

// Split operations

// Split a media clip into left and right clips at an absolute timeline position.
// Splits must occur strictly inside the clip. Preserves the original clip ID
// for the left part and uses the caller-provided right_clip_id for the right part.
// Returns the newly created right clip ID on success. On failure, timeline is
// unchanged and no new ID is created. Rejects duplicate right clip IDs.
Result<ClipId> split_media_clip(
    Timeline& timeline,
    const ClipId& clip_id,
    TimelineTime split_time,
    ClipId right_clip_id);

// Split a text clip into left and right clips at an absolute timeline position.
// Both clips share the same text. Splits must occur strictly inside the clip.
// Returns the newly created right clip ID on success. On failure, timeline is
// unchanged and no new ID is created. Rejects duplicate right clip IDs.
Result<ClipId> split_text_clip(
    Timeline& timeline,
    const ClipId& clip_id,
    TimelineTime split_time,
    ClipId right_clip_id);

} // namespace mvlab

#endif // MVLAB_TIMELINE_HPP
