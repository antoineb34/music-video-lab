#include "text_presentation.hpp"
#include <cmath>
#include <algorithm>

namespace mvlab {

namespace {

// Extreme-state offsets/scale used at the "fully hidden" end of an
// animation (progress == 0). The steady state (progress == 1) is always
// TextAnimationState{}'s default: opacity 1, offsets 0, scale 1.
constexpr float kFadeExtremeOpacity = 0.0f;
constexpr float kDropFromTopOffsetY = -0.3f;    // above the frame
constexpr float kRiseFromBottomOffsetY = 0.3f;  // below the frame
constexpr float kSlideFromLeftOffsetX = -0.3f;  // left of the frame
constexpr float kSlideFromRightOffsetX = 0.3f;  // right of the frame
constexpr float kScaleInExtreme = 0.5f;
constexpr float kScaleOutExtreme = 0.5f;

TextAnimationState extreme_state_for_kind(TextAnimationKind kind)
{
    TextAnimationState state;  // defaults to the steady state
    switch (kind) {
        case TextAnimationKind::none:
            break;
        case TextAnimationKind::fade:
            state.opacity = kFadeExtremeOpacity;
            break;
        case TextAnimationKind::drop_from_top:
            state.offset_y = kDropFromTopOffsetY;
            break;
        case TextAnimationKind::rise_from_bottom:
            state.offset_y = kRiseFromBottomOffsetY;
            break;
        case TextAnimationKind::slide_from_left:
            state.offset_x = kSlideFromLeftOffsetX;
            break;
        case TextAnimationKind::slide_from_right:
            state.offset_x = kSlideFromRightOffsetX;
            break;
        case TextAnimationKind::scale_in:
            state.scale = kScaleInExtreme;
            break;
        case TextAnimationKind::scale_out:
            state.scale = kScaleOutExtreme;
            break;
    }
    return state;
}

// Interpolates from an extreme state (p == 0) to the steady state (p == 1).
TextAnimationState lerp_to_steady(const TextAnimationState& extreme, double p)
{
    TextAnimationState steady{};
    TextAnimationState result;
    float fp = static_cast<float>(p);
    result.opacity = extreme.opacity + (steady.opacity - extreme.opacity) * fp;
    result.offset_x = extreme.offset_x + (steady.offset_x - extreme.offset_x) * fp;
    result.offset_y = extreme.offset_y + (steady.offset_y - extreme.offset_y) * fp;
    result.scale = extreme.scale + (steady.scale - extreme.scale) * fp;
    return result;
}

// Deterministic quadratic easing curves, evaluated in double precision.
// Input is clamped to [0, 1]; every curve satisfies f(0) == 0, f(1) == 1.
double apply_easing(EasingKind easing, double t)
{
    t = std::clamp(t, 0.0, 1.0);
    switch (easing) {
        case EasingKind::linear:
            return t;
        case EasingKind::ease_in:
            return t * t;
        case EasingKind::ease_out:
            return t * (2.0 - t);
        case EasingKind::ease_in_out:
            if (t < 0.5) {
                return 2.0 * t * t;
            }
            {
                double u = -2.0 * t + 2.0;
                return 1.0 - (u * u) / 2.0;
            }
    }
    return t;
}

// Progress toward the steady state driven by the entrance animation:
// 0 at clip start (fully at the extreme state), 1 once the entrance
// animation has completed (or immediately if kind == none).
double compute_entrance_progress(const TextAnimation& entrance, std::int64_t local_time_us)
{
    if (entrance.duration_us <= 0) {
        return 1.0;
    }
    if (local_time_us >= entrance.duration_us) {
        return 1.0;
    }
    double linear_t = static_cast<double>(local_time_us) / static_cast<double>(entrance.duration_us);
    return apply_easing(entrance.easing, linear_t);
}

// Progress toward the steady state driven by the exit animation: 1 before
// the exit window begins, falling to 0 at the very end of the clip (or
// staying 1 throughout if kind == none).
double compute_exit_progress(const TextAnimation& exit, std::int64_t clip_duration_us, std::int64_t local_time_us)
{
    if (exit.duration_us <= 0) {
        return 1.0;
    }
    std::int64_t exit_start = clip_duration_us - exit.duration_us;
    if (local_time_us <= exit_start) {
        return 1.0;
    }
    double elapsed = static_cast<double>(local_time_us - exit_start);
    double linear_t = std::clamp(elapsed / static_cast<double>(exit.duration_us), 0.0, 1.0);
    double eased_into_exit = apply_easing(exit.easing, linear_t);
    return 1.0 - eased_into_exit;
}

Result<RgbaColor> color_from_json(const nlohmann::json& j)
{
    RgbaColor color;
    color.red = j.at("red").get<float>();
    color.green = j.at("green").get<float>();
    color.blue = j.at("blue").get<float>();
    color.alpha = j.at("alpha").get<float>();
    return color;
}

nlohmann::json color_to_json(const RgbaColor& color)
{
    return nlohmann::json{
        {"red", color.red},
        {"green", color.green},
        {"blue", color.blue},
        {"alpha", color.alpha}
    };
}

} // namespace

std::string to_string(TextHorizontalAlignment alignment)
{
    switch (alignment) {
        case TextHorizontalAlignment::left:
            return "left";
        case TextHorizontalAlignment::center:
            return "center";
        case TextHorizontalAlignment::right:
            return "right";
    }
    return "unknown";
}

Result<TextHorizontalAlignment> parse_text_horizontal_alignment(const std::string& str)
{
    if (str == "left") return TextHorizontalAlignment::left;
    if (str == "center") return TextHorizontalAlignment::center;
    if (str == "right") return TextHorizontalAlignment::right;

    return Error{
        ErrorCode::invalid_argument,
        "Unknown horizontal alignment: " + str,
        std::nullopt
    };
}

std::string to_string(TextVerticalAlignment alignment)
{
    switch (alignment) {
        case TextVerticalAlignment::top:
            return "top";
        case TextVerticalAlignment::center:
            return "center";
        case TextVerticalAlignment::bottom:
            return "bottom";
    }
    return "unknown";
}

Result<TextVerticalAlignment> parse_text_vertical_alignment(const std::string& str)
{
    if (str == "top") return TextVerticalAlignment::top;
    if (str == "center") return TextVerticalAlignment::center;
    if (str == "bottom") return TextVerticalAlignment::bottom;

    return Error{
        ErrorCode::invalid_argument,
        "Unknown vertical alignment: " + str,
        std::nullopt
    };
}

std::string to_string(TextAnimationKind kind)
{
    switch (kind) {
        case TextAnimationKind::none:
            return "none";
        case TextAnimationKind::fade:
            return "fade";
        case TextAnimationKind::drop_from_top:
            return "drop_from_top";
        case TextAnimationKind::rise_from_bottom:
            return "rise_from_bottom";
        case TextAnimationKind::slide_from_left:
            return "slide_from_left";
        case TextAnimationKind::slide_from_right:
            return "slide_from_right";
        case TextAnimationKind::scale_in:
            return "scale_in";
        case TextAnimationKind::scale_out:
            return "scale_out";
    }
    return "unknown";
}

Result<TextAnimationKind> parse_text_animation_kind(const std::string& str)
{
    if (str == "none") return TextAnimationKind::none;
    if (str == "fade") return TextAnimationKind::fade;
    if (str == "drop_from_top") return TextAnimationKind::drop_from_top;
    if (str == "rise_from_bottom") return TextAnimationKind::rise_from_bottom;
    if (str == "slide_from_left") return TextAnimationKind::slide_from_left;
    if (str == "slide_from_right") return TextAnimationKind::slide_from_right;
    if (str == "scale_in") return TextAnimationKind::scale_in;
    if (str == "scale_out") return TextAnimationKind::scale_out;

    return Error{
        ErrorCode::invalid_argument,
        "Unknown animation kind: " + str,
        std::nullopt
    };
}

std::string to_string(EasingKind kind)
{
    switch (kind) {
        case EasingKind::linear:
            return "linear";
        case EasingKind::ease_in:
            return "ease_in";
        case EasingKind::ease_out:
            return "ease_out";
        case EasingKind::ease_in_out:
            return "ease_in_out";
    }
    return "unknown";
}

Result<EasingKind> parse_easing_kind(const std::string& str)
{
    if (str == "linear") return EasingKind::linear;
    if (str == "ease_in") return EasingKind::ease_in;
    if (str == "ease_out") return EasingKind::ease_out;
    if (str == "ease_in_out") return EasingKind::ease_in_out;

    return Error{
        ErrorCode::invalid_argument,
        "Unknown easing kind: " + str,
        std::nullopt
    };
}

Result<void> validate_rgba_color(const RgbaColor& color)
{
    if (!std::isfinite(color.red) || color.red < 0.0f || color.red > 1.0f) {
        return Error{ErrorCode::invalid_argument, "Color red channel must be a finite value within [0, 1]", std::nullopt};
    }
    if (!std::isfinite(color.green) || color.green < 0.0f || color.green > 1.0f) {
        return Error{ErrorCode::invalid_argument, "Color green channel must be a finite value within [0, 1]", std::nullopt};
    }
    if (!std::isfinite(color.blue) || color.blue < 0.0f || color.blue > 1.0f) {
        return Error{ErrorCode::invalid_argument, "Color blue channel must be a finite value within [0, 1]", std::nullopt};
    }
    if (!std::isfinite(color.alpha) || color.alpha < 0.0f || color.alpha > 1.0f) {
        return Error{ErrorCode::invalid_argument, "Color alpha channel must be a finite value within [0, 1]", std::nullopt};
    }
    return Result<void>{};
}

Result<void> validate_text_style(const TextStyle& style)
{
    if (style.font_family.empty()) {
        return Error{ErrorCode::invalid_argument, "font_family cannot be empty", std::nullopt};
    }

    if (!std::isfinite(style.font_size) || style.font_size <= 0.0f) {
        return Error{ErrorCode::invalid_argument, "font_size must be a positive finite value", std::nullopt};
    }

    if (!std::isfinite(style.outline_width) || style.outline_width < 0.0f) {
        return Error{ErrorCode::invalid_argument, "outline_width must be a non-negative finite value", std::nullopt};
    }

    auto fill_validation = validate_rgba_color(style.fill_color);
    if (!fill_validation) {
        return Error{
            ErrorCode::invalid_argument,
            "Invalid fill_color: " + fill_validation.error().message,
            std::nullopt
        };
    }

    auto outline_validation = validate_rgba_color(style.outline_color);
    if (!outline_validation) {
        return Error{
            ErrorCode::invalid_argument,
            "Invalid outline_color: " + outline_validation.error().message,
            std::nullopt
        };
    }

    if (!std::isfinite(style.position_x) || style.position_x < 0.0f || style.position_x > 1.0f) {
        return Error{ErrorCode::invalid_argument, "position_x must be a finite value within [0, 1]", std::nullopt};
    }

    if (!std::isfinite(style.position_y) || style.position_y < 0.0f || style.position_y > 1.0f) {
        return Error{ErrorCode::invalid_argument, "position_y must be a finite value within [0, 1]", std::nullopt};
    }

    return Result<void>{};
}

Result<void> validate_text_animation(const TextAnimation& animation)
{
    if (animation.duration_us < 0) {
        return Error{ErrorCode::invalid_argument, "Animation duration_us cannot be negative", std::nullopt};
    }

    if (animation.kind == TextAnimationKind::none) {
        if (animation.duration_us != 0) {
            return Error{
                ErrorCode::invalid_argument,
                "\"none\" animation must have duration_us == 0",
                std::nullopt
            };
        }
    } else {
        if (animation.duration_us <= 0) {
            return Error{
                ErrorCode::invalid_argument,
                "Non-\"none\" animation must have a positive duration_us",
                std::nullopt
            };
        }
    }

    return Result<void>{};
}

Result<void> validate_text_presentation(const TextPresentation& presentation)
{
    auto style_validation = validate_text_style(presentation.style);
    if (!style_validation) {
        return style_validation.error();
    }

    auto entrance_validation = validate_text_animation(presentation.entrance);
    if (!entrance_validation) {
        return Error{
            ErrorCode::invalid_argument,
            "Invalid entrance animation: " + entrance_validation.error().message,
            std::nullopt
        };
    }

    auto exit_validation = validate_text_animation(presentation.exit);
    if (!exit_validation) {
        return Error{
            ErrorCode::invalid_argument,
            "Invalid exit animation: " + exit_validation.error().message,
            std::nullopt
        };
    }

    return Result<void>{};
}

Result<TextAnimationState> sample_text_presentation(
    const TextPresentation& presentation,
    std::int64_t clip_duration_us,
    std::int64_t local_time_us)
{
    auto validation = validate_text_presentation(presentation);
    if (!validation) {
        return validation.error();
    }

    if (clip_duration_us <= 0) {
        return Error{
            ErrorCode::invalid_argument,
            "clip_duration_us must be positive",
            std::nullopt
        };
    }

    if (local_time_us < 0 || local_time_us > clip_duration_us) {
        return Error{
            ErrorCode::invalid_argument,
            "local_time_us must be within [0, clip_duration_us]",
            std::nullopt
        };
    }

    double p_entrance = compute_entrance_progress(presentation.entrance, local_time_us);
    double p_exit = compute_exit_progress(presentation.exit, clip_duration_us, local_time_us);

    TextAnimationState entrance_state = lerp_to_steady(
        extreme_state_for_kind(presentation.entrance.kind), p_entrance);
    TextAnimationState exit_state = lerp_to_steady(
        extreme_state_for_kind(presentation.exit.kind), p_exit);

    TextAnimationState combined;
    combined.opacity = entrance_state.opacity * exit_state.opacity;
    combined.offset_x = entrance_state.offset_x + exit_state.offset_x;
    combined.offset_y = entrance_state.offset_y + exit_state.offset_y;
    combined.scale = entrance_state.scale * exit_state.scale;

    combined.opacity = std::clamp(combined.opacity, 0.0f, 1.0f);

    if (!std::isfinite(combined.opacity) || !std::isfinite(combined.offset_x) ||
        !std::isfinite(combined.offset_y) || !std::isfinite(combined.scale) ||
        combined.scale <= 0.0f) {
        return Error{
            ErrorCode::internal_error,
            "Sampled animation state is not finite and bounded",
            std::nullopt
        };
    }

    return combined;
}

Result<nlohmann::json> text_presentation_to_json(
    const TextPresentation& presentation)
{
    auto validation = validate_text_presentation(presentation);
    if (!validation) {
        return validation.error();
    }

    try {
        nlohmann::json style_json{
            {"font_family", presentation.style.font_family},
            {"font_size", presentation.style.font_size},
            {"bold", presentation.style.bold},
            {"italic", presentation.style.italic},
            {"fill_color", color_to_json(presentation.style.fill_color)},
            {"outline_color", color_to_json(presentation.style.outline_color)},
            {"outline_width", presentation.style.outline_width},
            {"position_x", presentation.style.position_x},
            {"position_y", presentation.style.position_y},
            {"horizontal_alignment", to_string(presentation.style.horizontal_alignment)},
            {"vertical_alignment", to_string(presentation.style.vertical_alignment)}
        };

        nlohmann::json entrance_json{
            {"kind", to_string(presentation.entrance.kind)},
            {"duration_us", presentation.entrance.duration_us},
            {"easing", to_string(presentation.entrance.easing)}
        };

        nlohmann::json exit_json{
            {"kind", to_string(presentation.exit.kind)},
            {"duration_us", presentation.exit.duration_us},
            {"easing", to_string(presentation.exit.easing)}
        };

        nlohmann::json j{
            {"schema_version", current_text_presentation_schema_version},
            {"style", style_json},
            {"entrance", entrance_json},
            {"exit", exit_json}
        };

        return j;
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::serialization_error,
            "Failed to serialize text presentation",
            std::string(e.what())
        };
    }
}

Result<TextPresentation> text_presentation_from_json(
    const nlohmann::json& json)
{
    TextPresentation presentation;

    try {
        std::uint32_t schema_version = json.at("schema_version").get<std::uint32_t>();
        if (schema_version != current_text_presentation_schema_version) {
            return Error{
                ErrorCode::unsupported_schema,
                "Unsupported text presentation schema version: " + std::to_string(schema_version),
                std::nullopt
            };
        }

        const auto& style_json = json.at("style");
        presentation.style.font_family = style_json.at("font_family").get<std::string>();
        presentation.style.font_size = style_json.at("font_size").get<float>();
        presentation.style.bold = style_json.at("bold").get<bool>();
        presentation.style.italic = style_json.at("italic").get<bool>();

        auto fill_result = color_from_json(style_json.at("fill_color"));
        if (!fill_result) {
            return fill_result.error();
        }
        presentation.style.fill_color = fill_result.value();

        auto outline_result = color_from_json(style_json.at("outline_color"));
        if (!outline_result) {
            return outline_result.error();
        }
        presentation.style.outline_color = outline_result.value();

        presentation.style.outline_width = style_json.at("outline_width").get<float>();
        presentation.style.position_x = style_json.at("position_x").get<float>();
        presentation.style.position_y = style_json.at("position_y").get<float>();

        auto h_align_result = parse_text_horizontal_alignment(
            style_json.at("horizontal_alignment").get<std::string>());
        if (!h_align_result) {
            return h_align_result.error();
        }
        presentation.style.horizontal_alignment = h_align_result.value();

        auto v_align_result = parse_text_vertical_alignment(
            style_json.at("vertical_alignment").get<std::string>());
        if (!v_align_result) {
            return v_align_result.error();
        }
        presentation.style.vertical_alignment = v_align_result.value();

        const auto& entrance_json = json.at("entrance");
        auto entrance_kind_result = parse_text_animation_kind(
            entrance_json.at("kind").get<std::string>());
        if (!entrance_kind_result) {
            return entrance_kind_result.error();
        }
        presentation.entrance.kind = entrance_kind_result.value();
        presentation.entrance.duration_us = entrance_json.at("duration_us").get<std::int64_t>();

        auto entrance_easing_result = parse_easing_kind(
            entrance_json.at("easing").get<std::string>());
        if (!entrance_easing_result) {
            return entrance_easing_result.error();
        }
        presentation.entrance.easing = entrance_easing_result.value();

        const auto& exit_json = json.at("exit");
        auto exit_kind_result = parse_text_animation_kind(
            exit_json.at("kind").get<std::string>());
        if (!exit_kind_result) {
            return exit_kind_result.error();
        }
        presentation.exit.kind = exit_kind_result.value();
        presentation.exit.duration_us = exit_json.at("duration_us").get<std::int64_t>();

        auto exit_easing_result = parse_easing_kind(
            exit_json.at("easing").get<std::string>());
        if (!exit_easing_result) {
            return exit_easing_result.error();
        }
        presentation.exit.easing = exit_easing_result.value();
    } catch (const std::exception& e) {
        return Error{
            ErrorCode::invalid_argument,
            "Malformed text presentation JSON",
            std::string(e.what())
        };
    }

    auto validation = validate_text_presentation(presentation);
    if (!validation) {
        return validation.error();
    }

    return presentation;
}

Result<TextPresentation> make_text_presentation_preset(
    TextPresentationPreset preset)
{
    TextPresentation presentation;
    presentation.style.position_x = 0.5f;
    presentation.style.horizontal_alignment = TextHorizontalAlignment::center;

    switch (preset) {
        case TextPresentationPreset::clean_centered:
            presentation.style.position_y = 0.5f;
            presentation.style.vertical_alignment = TextVerticalAlignment::center;
            presentation.entrance = TextAnimation{TextAnimationKind::none, 0, EasingKind::linear};
            presentation.exit = TextAnimation{TextAnimationKind::none, 0, EasingKind::linear};
            break;

        case TextPresentationPreset::cinematic_fade:
            presentation.style.position_y = 0.5f;
            presentation.style.vertical_alignment = TextVerticalAlignment::center;
            presentation.entrance = TextAnimation{TextAnimationKind::fade, 500000, EasingKind::ease_out};
            presentation.exit = TextAnimation{TextAnimationKind::fade, 500000, EasingKind::ease_in};
            break;

        case TextPresentationPreset::drop_from_top:
            presentation.style.position_y = 0.5f;
            presentation.style.vertical_alignment = TextVerticalAlignment::center;
            presentation.entrance = TextAnimation{TextAnimationKind::drop_from_top, 500000, EasingKind::ease_out};
            presentation.exit = TextAnimation{TextAnimationKind::fade, 400000, EasingKind::ease_in};
            break;

        case TextPresentationPreset::rise_from_bottom:
            presentation.style.position_y = 0.5f;
            presentation.style.vertical_alignment = TextVerticalAlignment::center;
            presentation.entrance = TextAnimation{TextAnimationKind::rise_from_bottom, 500000, EasingKind::ease_out};
            presentation.exit = TextAnimation{TextAnimationKind::fade, 400000, EasingKind::ease_in};
            break;
    }

    auto validation = validate_text_presentation(presentation);
    if (!validation) {
        return validation.error();
    }

    return presentation;
}

} // namespace mvlab
