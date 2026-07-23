#include "preview_plan.hpp"
#include "project_model.hpp"
#include "media_asset.hpp"
#include "text_presentation.hpp"
#include <limits>

namespace mvlab {

namespace {

// Check if adding two int64_t values would overflow
bool would_overflow_add(int64_t a, int64_t b)
{
    if (b > 0 && a > std::numeric_limits<int64_t>::max() - b) {
        return true;
    }
    if (b < 0 && a < std::numeric_limits<int64_t>::min() - b) {
        return true;
    }
    return false;
}

// Check if subtracting two int64_t values would overflow
bool would_overflow_sub(int64_t a, int64_t b)
{
    if (b < 0 && a > std::numeric_limits<int64_t>::max() + b) {
        return true;
    }
    if (b > 0 && a < std::numeric_limits<int64_t>::min() + b) {
        return true;
    }
    return false;
}

}  // namespace

Result<PreviewFramePlan> build_preview_frame_plan(
    const Project& project,
    TimelineTime playhead)
{
    // Validate project first
    auto project_valid = validate_project(project);
    if (!project_valid) {
        return project_valid.error();
    }

    // Check for negative playhead
    if (playhead < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Playhead cannot be negative",
            std::nullopt
        };
    }

    PreviewFramePlan plan;
    plan.playhead = playhead;

    // Iterate through tracks in order
    for (const auto& track : project.timeline.tracks) {
        // Process media clips (audio and video tracks)
        if (track.type == TrackType::audio || track.type == TrackType::video) {
            for (const auto& clip : track.media_clips) {
                TimelineTime clip_end;

                // Calculate clip end time, checking for overflow
                if (would_overflow_add(clip.timeline_start, clip.duration)) {
                    return Error{
                        ErrorCode::internal_error,
                        "Arithmetic overflow: clip end time",
                        std::nullopt
                    };
                }
                clip_end = clip.timeline_start + clip.duration;

                // Check if clip is active: [start, end)
                if (playhead >= clip.timeline_start && playhead < clip_end) {
                    // Calculate local time (time from clip start to playhead)
                    if (would_overflow_sub(playhead, clip.timeline_start)) {
                        return Error{
                            ErrorCode::internal_error,
                            "Arithmetic overflow: calculating local time",
                            std::nullopt
                        };
                    }
                    TimelineTime local_time = playhead - clip.timeline_start;

                    // Calculate source time
                    if (would_overflow_add(clip.source_start, local_time)) {
                        return Error{
                            ErrorCode::internal_error,
                            "Arithmetic overflow: calculating source time",
                            std::nullopt
                        };
                    }
                    TimelineTime source_time = clip.source_start + local_time;

                    // Calculate remaining duration
                    if (would_overflow_sub(clip_end, playhead)) {
                        return Error{
                            ErrorCode::internal_error,
                            "Arithmetic overflow: calculating remaining duration",
                            std::nullopt
                        };
                    }
                    TimelineTime remaining_duration = clip_end - playhead;

                    // Validate that the asset exists
                    auto asset_lookup = find_media_asset(project, clip.asset_id);
                    if (!asset_lookup) {
                        return Error{
                            ErrorCode::invalid_argument,
                            "Media asset not found: " + clip.asset_id,
                            std::nullopt
                        };
                    }

                    ActiveMediaClip active_clip{
                        track.id,
                        clip.id,
                        clip.asset_id,
                        track.type,
                        clip.timeline_start,
                        source_time,
                        remaining_duration
                    };

                    if (track.type == TrackType::audio) {
                        plan.audio_clips.push_back(active_clip);
                    } else {
                        plan.video_clips.push_back(active_clip);
                    }
                }
            }
        }

        // Process text clips (text tracks only)
        if (track.type == TrackType::text) {
            for (const auto& clip : track.text_clips) {
                TimelineTime clip_end;

                // Calculate clip end time, checking for overflow
                if (would_overflow_add(clip.timeline_start, clip.duration)) {
                    return Error{
                        ErrorCode::internal_error,
                        "Arithmetic overflow: text clip end time",
                        std::nullopt
                    };
                }
                clip_end = clip.timeline_start + clip.duration;

                // Check if clip is active: [start, end)
                if (playhead >= clip.timeline_start && playhead < clip_end) {
                    // Calculate local time
                    if (would_overflow_sub(playhead, clip.timeline_start)) {
                        return Error{
                            ErrorCode::internal_error,
                            "Arithmetic overflow: text local time",
                            std::nullopt
                        };
                    }
                    TimelineTime local_time = playhead - clip.timeline_start;

                    // Calculate remaining duration
                    if (would_overflow_sub(clip_end, playhead)) {
                        return Error{
                            ErrorCode::internal_error,
                            "Arithmetic overflow: text remaining duration",
                            std::nullopt
                        };
                    }
                    TimelineTime remaining_duration = clip_end - playhead;

                    // Sample the text presentation and animation state
                    // For now, TextClip doesn't store presentation directly,
                    // so we use the clean_centered preset as the default
                    auto preset_result = make_text_presentation_preset(
                        TextPresentationPreset::clean_centered);
                    if (!preset_result) {
                        return Error{
                            ErrorCode::invalid_argument,
                            "Failed to load default text presentation",
                            std::nullopt
                        };
                    }
                    TextPresentation presentation = preset_result.value();

                    auto animation_result = sample_text_presentation(
                        presentation,
                        clip.duration,
                        local_time);
                    if (!animation_result) {
                        return animation_result.error();
                    }

                    ActiveTextClip active_clip{
                        track.id,
                        clip.id,
                        clip.text,
                        clip.timeline_start,
                        local_time,
                        remaining_duration,
                        presentation,
                        animation_result.value()
                    };

                    plan.text_clips.push_back(active_clip);
                }
            }
        }
    }

    return plan;
}

}  // namespace mvlab
