#ifndef MVLAB_PREVIEW_PLAN_HPP
#define MVLAB_PREVIEW_PLAN_HPP

#include "result.hpp"
#include "timeline.hpp"
#include "text_presentation.hpp"
#include <vector>

namespace mvlab {

struct Project;  // Forward declaration

// A media clip that is active at a specific playhead position.
// All fields are owned values, independent of the Project.
struct ActiveMediaClip {
    TrackId track_id;
    ClipId clip_id;
    AssetId asset_id;
    TrackType track_type;

    TimelineTime timeline_start;      // clip's start position on timeline
    TimelineTime source_time;         // playhead time within the source media
    TimelineTime remaining_duration;  // time until clip ends
};

// A text clip that is active at a specific playhead position.
// All fields are owned values, independent of the Project.
struct ActiveTextClip {
    TrackId track_id;
    ClipId clip_id;
    std::string text;

    TimelineTime timeline_start;      // clip's start position on timeline
    TimelineTime local_time;          // playhead time within the text clip
    TimelineTime remaining_duration;  // time until clip ends

    TextPresentation presentation;
    TextAnimationState animation_state;
};

// A deterministic frame plan for a future renderer.
// Returns active clips in track order, with clip order preserved within each track.
struct PreviewFramePlan {
    TimelineTime playhead;
    std::vector<ActiveMediaClip> audio_clips;
    std::vector<ActiveMediaClip> video_clips;
    std::vector<ActiveTextClip> text_clips;
};

// Build a frame plan for a validated project at a given playhead position.
// Returns invalid_argument if the playhead is negative or if the project is invalid.
// Returns invalid_argument if any media asset reference cannot be resolved.
// Returns invalid_argument if text animation sampling fails.
// Returns internal_error if arithmetic overflow is detected.
// Does not mutate the project.
Result<PreviewFramePlan> build_preview_frame_plan(
    const Project& project,
    TimelineTime playhead);

} // namespace mvlab

#endif // MVLAB_PREVIEW_PLAN_HPP
