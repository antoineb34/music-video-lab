#include "preview_session.hpp"
#include "preview_plan.hpp"
#include "project_model.hpp"
#include "timeline.hpp"
#include <limits>

namespace mvlab {

namespace {

// Calculate the duration of the project as the greatest clip end across all tracks
Result<TimelineTime> calculate_project_duration(const Project& project)
{
    TimelineTime max_end = 0;

    for (const auto& track : project.timeline.tracks) {
        // Check media clips
        for (const auto& clip : track.media_clips) {
            // Calculate clip end: timeline_start + duration
            // Check for overflow
            if (clip.timeline_start > std::numeric_limits<TimelineTime>::max() - clip.duration) {
                return Error{
                    ErrorCode::internal_error,
                    "Arithmetic overflow: media clip end time in duration calculation",
                    std::nullopt
                };
            }
            TimelineTime clip_end = clip.timeline_start + clip.duration;
            if (clip_end > max_end) {
                max_end = clip_end;
            }
        }

        // Check text clips
        for (const auto& clip : track.text_clips) {
            // Calculate clip end: timeline_start + duration
            // Check for overflow
            if (clip.timeline_start > std::numeric_limits<TimelineTime>::max() - clip.duration) {
                return Error{
                    ErrorCode::internal_error,
                    "Arithmetic overflow: text clip end time in duration calculation",
                    std::nullopt
                };
            }
            TimelineTime clip_end = clip.timeline_start + clip.duration;
            if (clip_end > max_end) {
                max_end = clip_end;
            }
        }
    }

    return max_end;
}

// Check for overflow when adding two int64 values
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

}  // namespace

Result<PreviewSession> PreviewSession::create(
    const Project& project,
    PreviewSessionSettings settings)
{
    // Validate project
    auto project_valid = validate_project(project);
    if (!project_valid) {
        return project_valid.error();
    }

    // Validate frame rate
    if (settings.frame_rate.numerator <= 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Frame rate numerator must be positive",
            std::nullopt
        };
    }
    if (settings.frame_rate.denominator <= 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Frame rate denominator must be positive",
            std::nullopt
        };
    }

    // Calculate project duration
    auto duration_result = calculate_project_duration(project);
    if (!duration_result) {
        return duration_result.error();
    }

    PreviewSession session(project, settings, duration_result.value());
    return session;
}

PreviewPlaybackState PreviewSession::state() const noexcept
{
    return state_;
}

TimelineTime PreviewSession::playhead() const noexcept
{
    return playhead_;
}

TimelineTime PreviewSession::duration() const noexcept
{
    return duration_;
}

const PreviewSessionSettings& PreviewSession::settings() const noexcept
{
    return settings_;
}

Result<void> PreviewSession::play()
{
    // If at the end of a non-looping session, restart from zero
    if (!settings_.loop && playhead_ == duration_ && playhead_ > 0) {
        playhead_ = 0;
    }

    state_ = PreviewPlaybackState::playing;
    return Result<void>();
}

Result<void> PreviewSession::pause()
{
    state_ = PreviewPlaybackState::paused;
    return Result<void>();
}

Result<void> PreviewSession::stop()
{
    state_ = PreviewPlaybackState::stopped;
    playhead_ = 0;
    return Result<void>();
}

Result<void> PreviewSession::seek(TimelineTime target_playhead)
{
    // Reject negative positions
    if (target_playhead < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Playhead cannot be negative",
            std::nullopt
        };
    }

    // Clamp at duration if not looping
    if (target_playhead > duration_) {
        target_playhead = duration_;
    }

    playhead_ = target_playhead;
    return Result<void>();
}

Result<void> PreviewSession::advance(TimelineTime elapsed)
{
    // Reject negative elapsed
    if (elapsed < 0) {
        return Error{
            ErrorCode::invalid_argument,
            "Elapsed time cannot be negative",
            std::nullopt
        };
    }

    // Only advance while playing
    if (state_ != PreviewPlaybackState::playing) {
        return Result<void>();
    }

    // Empty project case: remain at zero but transition to stopped in non-looping mode
    if (duration_ == 0) {
        if (!settings_.loop) {
            state_ = PreviewPlaybackState::stopped;
        }
        return Result<void>();
    }

    TimelineTime new_playhead;

    if (settings_.loop) {
        // Looping: wrap around using modulo
        // To avoid overflow in intermediate calculations, use modulo at each step
        // new_position = (current + elapsed) % duration
        TimelineTime offset = elapsed % duration_;
        new_playhead = (playhead_ + offset) % duration_;
    } else {
        // Non-looping: clamp at duration
        // Check for overflow when adding
        if (would_overflow_add(playhead_, elapsed)) {
            new_playhead = duration_;
        } else {
            new_playhead = playhead_ + elapsed;
            if (new_playhead > duration_) {
                new_playhead = duration_;
            }
        }
    }

    playhead_ = new_playhead;

    // In non-looping mode, transition to stopped at the end
    if (!settings_.loop && playhead_ >= duration_) {
        state_ = PreviewPlaybackState::stopped;
    }

    return Result<void>();
}

Result<void> PreviewSession::step_frames(std::int64_t frame_count)
{
    // Zero is a no-op
    if (frame_count == 0) {
        return Result<void>();
    }

    // Empty project remains at zero
    if (duration_ == 0) {
        return Result<void>();
    }

    // Calculate the frame duration in microseconds using rational arithmetic
    // frame_duration = (1_000_000 * denominator) / numerator
    // But we calculate the total offset for all frames to avoid drift:
    // total_offset = (frame_count * 1_000_000 * denominator) / numerator

    int64_t numerator = settings_.frame_rate.numerator;
    int64_t denominator = settings_.frame_rate.denominator;

    // Calculate total offset with overflow detection
    // offset = (frame_count * 1_000_000 * denominator) / numerator

    // Check if frame_count * 1_000_000 would overflow
    if (frame_count > 0) {
        if (frame_count > std::numeric_limits<int64_t>::max() / 1000000) {
            return Error{
                ErrorCode::internal_error,
                "Arithmetic overflow: frame count too large",
                std::nullopt
            };
        }
    } else if (frame_count < 0) {
        if (frame_count < std::numeric_limits<int64_t>::min() / 1000000) {
            return Error{
                ErrorCode::internal_error,
                "Arithmetic overflow: frame count too small",
                std::nullopt
            };
        }
    }

    int64_t intermediate = frame_count * 1000000;

    // Check if intermediate * denominator would overflow
    // For positive intermediate and positive denominator:
    if (intermediate > 0 && denominator > 0) {
        if (denominator > std::numeric_limits<int64_t>::max() / intermediate) {
            return Error{
                ErrorCode::internal_error,
                "Arithmetic overflow: frame calculation overflow",
                std::nullopt
            };
        }
    }
    // For negative intermediate and positive denominator:
    // The result will be negative, which generally won't overflow downward
    // unless intermediate is very large in magnitude
    else if (intermediate < 0 && denominator > 0) {
        // min_int64 = -9223372036854775808
        // We need: intermediate * denominator >= min_int64
        // Which is: intermediate >= min_int64 / denominator (careful with division)
        // Use a simplified check: if both intermediate and result would be negative,
        // and intermediate magnitude is huge, reject
        if (intermediate < std::numeric_limits<int64_t>::min() / denominator) {
            return Error{
                ErrorCode::internal_error,
                "Arithmetic overflow: frame calculation overflow",
                std::nullopt
            };
        }
    }

    int64_t offset_numerator = intermediate * denominator;
    int64_t offset = offset_numerator / numerator;

    // Apply the offset
    TimelineTime new_playhead;

    if (offset >= 0) {
        // Stepping forward
        if (settings_.loop) {
            // Wrap in looping mode
            int64_t wrapped_offset = offset % duration_;
            new_playhead = (playhead_ + wrapped_offset) % duration_;
        } else {
            // Clamp in non-looping mode
            if (would_overflow_add(playhead_, offset)) {
                new_playhead = duration_;
            } else {
                new_playhead = playhead_ + offset;
                if (new_playhead > duration_) {
                    new_playhead = duration_;
                }
            }
        }
    } else {
        // Stepping backward
        int64_t abs_offset = -offset;
        if (settings_.loop) {
            // Wrap backward in looping mode
            // (playhead - abs_offset) % duration, handling negatives correctly
            // In C++, negative modulo can give negative results, so we handle it
            TimelineTime wrapped_offset = abs_offset % duration_;
            if (playhead_ >= wrapped_offset) {
                new_playhead = playhead_ - wrapped_offset;
            } else {
                // Wrap around
                new_playhead = duration_ - (wrapped_offset - playhead_);
            }
        } else {
            // Clamp backward in non-looping mode
            if (playhead_ >= abs_offset) {
                new_playhead = playhead_ - abs_offset;
            } else {
                new_playhead = 0;
            }
        }
    }

    playhead_ = new_playhead;

    // Transition to stopped if we hit the end in non-looping mode and we're playing
    if (!settings_.loop && playhead_ >= duration_ && state_ == PreviewPlaybackState::playing) {
        state_ = PreviewPlaybackState::stopped;
    }

    return Result<void>();
}

Result<PreviewFramePlan> PreviewSession::current_frame_plan() const
{
    return build_preview_frame_plan(project_, playhead_);
}

}  // namespace mvlab
