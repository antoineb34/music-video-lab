#ifndef MVLAB_PREVIEW_SESSION_HPP
#define MVLAB_PREVIEW_SESSION_HPP

#include "result.hpp"
#include "preview_plan.hpp"
#include "project_model.hpp"
#include <cstdint>

namespace mvlab {

enum class PreviewPlaybackState {
    stopped,
    paused,
    playing
};

struct PreviewFrameRate {
    std::int32_t numerator = 30;
    std::int32_t denominator = 1;
};

struct PreviewSessionSettings {
    PreviewFrameRate frame_rate{30, 1};
    bool loop = false;
};

// A renderer-neutral preview playback session that owns playhead state and
// produces PreviewFramePlan values. Takes explicit elapsed time from caller.
// Does not render, decode, play audio, use threads, timers, or OS clocks.
class PreviewSession {
public:
    // Create a playback session for a project with optional settings.
    // Validates project, frame rate, and initializes playhead to zero in stopped state.
    // Returns invalid_argument for invalid frame rate, invalid_project for
    // validation errors, or internal_error for arithmetic overflow.
    static Result<PreviewSession> create(
        const Project& project,
        PreviewSessionSettings settings = {});

    // Current playback state: stopped, paused, or playing.
    [[nodiscard]] PreviewPlaybackState state() const noexcept;

    // Current playhead position in microseconds.
    [[nodiscard]] TimelineTime playhead() const noexcept;

    // Project duration in microseconds (greatest clip end across all tracks).
    [[nodiscard]] TimelineTime duration() const noexcept;

    // Session settings (frame rate and loop mode).
    [[nodiscard]] const PreviewSessionSettings& settings() const noexcept;

    // Start playback. Idempotent when already playing. At the exact end of a
    // non-looping session, restarts from zero before playing.
    Result<void> play();

    // Pause playback. Idempotent when already paused. Pausing from stopped
    // transitions to paused state.
    Result<void> pause();

    // Stop playback and reset playhead to zero. Idempotent.
    Result<void> stop();

    // Seek to a playhead position. Clamps positions beyond duration to duration.
    // Rejects negative positions. Preserves playback state.
    Result<void> seek(TimelineTime playhead);

    // Advance playback by elapsed time in microseconds. Only advances while playing.
    // Rejects negative elapsed values. Clamps at duration in non-looping mode,
    // wraps in looping mode. Transitions to stopped at end of non-looping content.
    Result<void> advance(TimelineTime elapsed);

    // Step by a number of frames. Positive steps forward, negative backward.
    // Works in any playback state. Uses rational frame rate for deterministic
    // integer arithmetic. Clamps/wraps based on loop mode.
    Result<void> step_frames(std::int64_t frame_count);

    // Get the current frame plan for the session playhead. Reflects active
    // audio, video, and text clips at the current position.
    Result<PreviewFramePlan> current_frame_plan() const;

private:
    PreviewSession(Project project, PreviewSessionSettings settings, TimelineTime duration)
        : project_(std::move(project)),
          settings_(settings),
          duration_(duration),
          playhead_(0),
          state_(PreviewPlaybackState::stopped)
    {
    }

    Project project_;
    PreviewSessionSettings settings_;
    TimelineTime duration_;
    TimelineTime playhead_;
    PreviewPlaybackState state_;
};

} // namespace mvlab

#endif // MVLAB_PREVIEW_SESSION_HPP
