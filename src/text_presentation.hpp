#ifndef MVLAB_TEXT_PRESENTATION_HPP
#define MVLAB_TEXT_PRESENTATION_HPP

#include "result.hpp"
#include <string>
#include <cstdint>
#include <nlohmann/json.hpp>

namespace mvlab {

// A color with channels normalized to [0, 1]. Renderer-independent: no
// platform pixel format is implied.
struct RgbaColor {
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;
    float alpha = 1.0f;
};

enum class TextHorizontalAlignment {
    left,
    center,
    right
};

std::string to_string(TextHorizontalAlignment alignment);

// Parses a string to a TextHorizontalAlignment. Returns invalid_argument if
// the string is not a recognized alignment.
Result<TextHorizontalAlignment> parse_text_horizontal_alignment(const std::string& str);

enum class TextVerticalAlignment {
    top,
    center,
    bottom
};

std::string to_string(TextVerticalAlignment alignment);

// Parses a string to a TextVerticalAlignment. Returns invalid_argument if
// the string is not a recognized alignment.
Result<TextVerticalAlignment> parse_text_vertical_alignment(const std::string& str);

// Font, outline, and screen-position settings for a single text clip.
// Renderer-independent: no platform font handle or filesystem font path.
// position_x/position_y are normalized screen coordinates in [0, 1]
// (0,0 = top-left, 1,1 = bottom-right) marking the anchor point that
// horizontal_alignment/vertical_alignment are relative to.
struct TextStyle {
    std::string font_family = "Sans";
    float font_size = 48.0f;
    bool bold = false;
    bool italic = false;

    RgbaColor fill_color{1.0f, 1.0f, 1.0f, 1.0f};
    RgbaColor outline_color{0.0f, 0.0f, 0.0f, 1.0f};
    float outline_width = 2.0f;

    float position_x = 0.5f;
    float position_y = 0.85f;

    TextHorizontalAlignment horizontal_alignment = TextHorizontalAlignment::center;
    TextVerticalAlignment vertical_alignment = TextVerticalAlignment::bottom;
};

// Validates a color's channels. Requires each of red/green/blue/alpha to be
// finite and within [0, 1].
Result<void> validate_rgba_color(const RgbaColor& color);

// Validates a TextStyle. Requires:
// - font_family is not empty
// - font_size is finite and positive
// - outline_width is finite and non-negative
// - fill_color and outline_color are valid (see validate_rgba_color)
// - position_x and position_y are finite and within [0, 1]
Result<void> validate_text_style(const TextStyle& style);

enum class TextAnimationKind {
    none,
    fade,
    drop_from_top,
    rise_from_bottom,
    slide_from_left,
    slide_from_right,
    scale_in,
    scale_out
};

std::string to_string(TextAnimationKind kind);

// Parses a string to a TextAnimationKind. Returns invalid_argument if the
// string is not a recognized animation kind.
Result<TextAnimationKind> parse_text_animation_kind(const std::string& str);

enum class EasingKind {
    linear,
    ease_in,
    ease_out,
    ease_in_out
};

std::string to_string(EasingKind kind);

// Parses a string to an EasingKind. Returns invalid_argument if the string
// is not a recognized easing kind.
Result<EasingKind> parse_easing_kind(const std::string& str);

// A single entrance or exit animation applied at a text clip boundary.
struct TextAnimation {
    TextAnimationKind kind = TextAnimationKind::none;
    std::int64_t duration_us = 0;
    EasingKind easing = EasingKind::linear;
};

// Validates a TextAnimation. Requires:
// - duration_us is non-negative
// - kind == none implies duration_us == 0
// - kind != none implies duration_us > 0
Result<void> validate_text_animation(const TextAnimation& animation);

// The complete renderer-neutral presentation of a piece of on-screen text:
// static style plus entrance/exit animations.
struct TextPresentation {
    TextStyle style;
    TextAnimation entrance;
    TextAnimation exit;
};

// Validates a TextPresentation by validating style, entrance, and exit.
Result<void> validate_text_presentation(const TextPresentation& presentation);

// A sampled, renderer-neutral animation state at a single point in time.
// The steady (non-animating) state is the default: opacity 1, offsets 0,
// scale 1. Offsets are expressed in normalized screen units, added to a
// clip's position_x/position_y before rendering.
struct TextAnimationState {
    float opacity = 1.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float scale = 1.0f;
};

// Samples the entrance/exit animation state of a presentation at a given
// local time within a text clip.
//
// local_time_us is relative to the clip's own start (0 = clip start).
// Requires clip_duration_us > 0 and local_time_us within
// [0, clip_duration_us].
//
// The entrance animation drives state near the clip start; the exit
// animation drives state near the clip end. Outside their respective
// windows, an animation contributes the steady state. If the entrance and
// exit windows overlap (their durations sum to more than clip_duration_us),
// the two contributions are combined deterministically: opacity and scale
// combine multiplicatively (steady value 1), offsets combine additively
// (steady value 0). The result is always finite, opacity stays within
// [0, 1], and scale stays positive.
Result<TextAnimationState> sample_text_presentation(
    const TextPresentation& presentation,
    std::int64_t clip_duration_us,
    std::int64_t local_time_us);

inline constexpr std::uint32_t current_text_presentation_schema_version = 1;

// Serializes a TextPresentation to JSON under the current schema version.
// Validates the presentation before serializing; returns invalid_argument
// if it is invalid.
Result<nlohmann::json> text_presentation_to_json(
    const TextPresentation& presentation);

// Deserializes a TextPresentation from JSON.
//
// Rejects missing fields, wrong types, unknown enum strings, unsupported
// schema versions, and out-of-range/non-finite numeric values. Never
// silently repairs malformed input; the result is validated before being
// returned.
Result<TextPresentation> text_presentation_from_json(
    const nlohmann::json& json);

// Built-in, renderer-neutral presentation presets. Every preset is valid
// by construction.
enum class TextPresentationPreset {
    clean_centered,
    cinematic_fade,
    drop_from_top,
    rise_from_bottom
};

// Builds a TextPresentation from a built-in preset.
Result<TextPresentation> make_text_presentation_preset(
    TextPresentationPreset preset);

} // namespace mvlab

#endif // MVLAB_TEXT_PRESENTATION_HPP
