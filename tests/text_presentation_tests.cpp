#include "text_presentation.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>
#include <cmath>
#include <vector>

using namespace mvlab;
using Catch::Approx;

namespace {
constexpr float kMargin = 0.0001f;
}

// ===== Validation: styling =====

TEST_CASE("Default TextStyle validates", "[text_presentation][validation]")
{
    TextStyle style{};
    auto result = validate_text_style(style);
    REQUIRE(result);
}

TEST_CASE("Default TextPresentation validates", "[text_presentation][validation]")
{
    TextPresentation presentation{};
    auto result = validate_text_presentation(presentation);
    REQUIRE(result);
}

TEST_CASE("Empty font family rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.font_family = "";
    auto result = validate_text_style(style);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Zero font size rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.font_size = 0.0f;
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("Negative font size rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.font_size = -10.0f;
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("NaN font size rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.font_size = std::numeric_limits<float>::quiet_NaN();
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("Infinite font size rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.font_size = std::numeric_limits<float>::infinity();
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("Negative outline width rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.outline_width = -1.0f;
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("NaN outline width rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.outline_width = std::numeric_limits<float>::quiet_NaN();
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("Infinite outline width rejected", "[text_presentation][validation]")
{
    TextStyle style{};
    style.outline_width = std::numeric_limits<float>::infinity();
    auto result = validate_text_style(style);
    REQUIRE(!result);
}

TEST_CASE("Zero outline width accepted (non-negative boundary)", "[text_presentation][validation]")
{
    TextStyle style{};
    style.outline_width = 0.0f;
    auto result = validate_text_style(style);
    REQUIRE(result);
}

TEST_CASE("Each invalid fill_color channel rejected", "[text_presentation][validation]")
{
    TextStyle base{};

    TextStyle red_low = base; red_low.fill_color.red = -0.1f;
    CHECK(!validate_text_style(red_low));

    TextStyle red_high = base; red_high.fill_color.red = 1.1f;
    CHECK(!validate_text_style(red_high));

    TextStyle green_low = base; green_low.fill_color.green = -0.1f;
    CHECK(!validate_text_style(green_low));

    TextStyle green_high = base; green_high.fill_color.green = 1.1f;
    CHECK(!validate_text_style(green_high));

    TextStyle blue_low = base; blue_low.fill_color.blue = -0.1f;
    CHECK(!validate_text_style(blue_low));

    TextStyle blue_high = base; blue_high.fill_color.blue = 1.1f;
    CHECK(!validate_text_style(blue_high));

    TextStyle alpha_low = base; alpha_low.fill_color.alpha = -0.1f;
    CHECK(!validate_text_style(alpha_low));

    TextStyle alpha_high = base; alpha_high.fill_color.alpha = 1.1f;
    CHECK(!validate_text_style(alpha_high));

    TextStyle nan_channel = base; nan_channel.fill_color.red = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validate_text_style(nan_channel));
}

TEST_CASE("Each invalid outline_color channel rejected", "[text_presentation][validation]")
{
    TextStyle base{};

    TextStyle red = base; red.outline_color.red = -1.0f;
    CHECK(!validate_text_style(red));

    TextStyle green = base; green.outline_color.green = 2.0f;
    CHECK(!validate_text_style(green));

    TextStyle blue = base; blue.outline_color.blue = std::numeric_limits<float>::infinity();
    CHECK(!validate_text_style(blue));

    TextStyle alpha = base; alpha.outline_color.alpha = -0.5f;
    CHECK(!validate_text_style(alpha));
}

TEST_CASE("Color channel boundaries 0 and 1 are valid", "[text_presentation][validation]")
{
    TextStyle style{};
    style.fill_color = RgbaColor{0.0f, 0.0f, 0.0f, 0.0f};
    CHECK(validate_text_style(style));

    style.fill_color = RgbaColor{1.0f, 1.0f, 1.0f, 1.0f};
    CHECK(validate_text_style(style));
}

TEST_CASE("Invalid positions rejected", "[text_presentation][validation]")
{
    TextStyle base{};

    TextStyle neg_x = base; neg_x.position_x = -0.01f;
    CHECK(!validate_text_style(neg_x));

    TextStyle over_x = base; over_x.position_x = 1.01f;
    CHECK(!validate_text_style(over_x));

    TextStyle neg_y = base; neg_y.position_y = -0.01f;
    CHECK(!validate_text_style(neg_y));

    TextStyle over_y = base; over_y.position_y = 1.01f;
    CHECK(!validate_text_style(over_y));

    TextStyle nan_x = base; nan_x.position_x = std::numeric_limits<float>::quiet_NaN();
    CHECK(!validate_text_style(nan_x));

    TextStyle inf_y = base; inf_y.position_y = std::numeric_limits<float>::infinity();
    CHECK(!validate_text_style(inf_y));
}

TEST_CASE("Position boundaries 0 and 1 are valid", "[text_presentation][validation]")
{
    TextStyle style{};
    style.position_x = 0.0f;
    style.position_y = 1.0f;
    CHECK(validate_text_style(style));
}

TEST_CASE("Style validation does not mutate input", "[text_presentation][validation]")
{
    TextStyle style{};
    style.font_family = "Serif";
    style.font_size = 32.0f;
    TextStyle copy = style;

    auto result = validate_text_style(style);
    REQUIRE(result);

    CHECK(style.font_family == copy.font_family);
    CHECK(style.font_size == copy.font_size);
}

// ===== Validation: animation =====

TEST_CASE("Negative animation duration rejected", "[text_presentation][validation]")
{
    TextAnimation animation{TextAnimationKind::fade, -1, EasingKind::linear};
    auto result = validate_text_animation(animation);
    REQUIRE(!result);
}

TEST_CASE("None kind with nonzero duration rejected", "[text_presentation][validation]")
{
    TextAnimation animation{TextAnimationKind::none, 100, EasingKind::linear};
    auto result = validate_text_animation(animation);
    REQUIRE(!result);
}

TEST_CASE("Non-none kind with zero duration rejected", "[text_presentation][validation]")
{
    TextAnimation animation{TextAnimationKind::fade, 0, EasingKind::linear};
    auto result = validate_text_animation(animation);
    REQUIRE(!result);
}

TEST_CASE("None kind with zero duration accepted", "[text_presentation][validation]")
{
    TextAnimation animation{TextAnimationKind::none, 0, EasingKind::linear};
    auto result = validate_text_animation(animation);
    REQUIRE(result);
}

TEST_CASE("Non-none kind with positive duration accepted", "[text_presentation][validation]")
{
    TextAnimation animation{TextAnimationKind::fade, 500000, EasingKind::linear};
    auto result = validate_text_animation(animation);
    REQUIRE(result);
}

TEST_CASE("Invalid entrance animation rejects whole presentation", "[text_presentation][validation]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 0, EasingKind::linear};
    auto result = validate_text_presentation(presentation);
    REQUIRE(!result);
}

TEST_CASE("Invalid exit animation rejects whole presentation", "[text_presentation][validation]")
{
    TextPresentation presentation{};
    presentation.exit = TextAnimation{TextAnimationKind::none, 1, EasingKind::linear};
    auto result = validate_text_presentation(presentation);
    REQUIRE(!result);
}

TEST_CASE("Presentation validation does not mutate input", "[text_presentation][validation]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 300000, EasingKind::ease_in};
    TextPresentation copy = presentation;

    auto result = validate_text_presentation(presentation);
    REQUIRE(result);

    CHECK(presentation.entrance.kind == copy.entrance.kind);
    CHECK(presentation.entrance.duration_us == copy.entrance.duration_us);
    CHECK(presentation.entrance.easing == copy.entrance.easing);
}

// ===== Enum conversion =====

TEST_CASE("Every animation kind round-trips through string conversion", "[text_presentation][enum]")
{
    std::vector<TextAnimationKind> kinds{
        TextAnimationKind::none,
        TextAnimationKind::fade,
        TextAnimationKind::drop_from_top,
        TextAnimationKind::rise_from_bottom,
        TextAnimationKind::slide_from_left,
        TextAnimationKind::slide_from_right,
        TextAnimationKind::scale_in,
        TextAnimationKind::scale_out
    };

    for (auto kind : kinds) {
        std::string s = to_string(kind);
        CHECK(!s.empty());
        auto parsed = parse_text_animation_kind(s);
        REQUIRE(parsed);
        CHECK(parsed.value() == kind);
    }
}

TEST_CASE("Every easing kind round-trips through string conversion", "[text_presentation][enum]")
{
    std::vector<EasingKind> kinds{
        EasingKind::linear,
        EasingKind::ease_in,
        EasingKind::ease_out,
        EasingKind::ease_in_out
    };

    for (auto kind : kinds) {
        std::string s = to_string(kind);
        auto parsed = parse_easing_kind(s);
        REQUIRE(parsed);
        CHECK(parsed.value() == kind);
    }
}

TEST_CASE("Every horizontal alignment round-trips through string conversion", "[text_presentation][enum]")
{
    std::vector<TextHorizontalAlignment> alignments{
        TextHorizontalAlignment::left,
        TextHorizontalAlignment::center,
        TextHorizontalAlignment::right
    };

    for (auto alignment : alignments) {
        std::string s = to_string(alignment);
        auto parsed = parse_text_horizontal_alignment(s);
        REQUIRE(parsed);
        CHECK(parsed.value() == alignment);
    }
}

TEST_CASE("Every vertical alignment round-trips through string conversion", "[text_presentation][enum]")
{
    std::vector<TextVerticalAlignment> alignments{
        TextVerticalAlignment::top,
        TextVerticalAlignment::center,
        TextVerticalAlignment::bottom
    };

    for (auto alignment : alignments) {
        std::string s = to_string(alignment);
        auto parsed = parse_text_vertical_alignment(s);
        REQUIRE(parsed);
        CHECK(parsed.value() == alignment);
    }
}

TEST_CASE("Unknown animation kind string rejected", "[text_presentation][enum]")
{
    auto result = parse_text_animation_kind("explode");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Unknown easing kind string rejected", "[text_presentation][enum]")
{
    auto result = parse_easing_kind("bounce");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Unknown horizontal alignment string rejected", "[text_presentation][enum]")
{
    auto result = parse_text_horizontal_alignment("diagonal");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Unknown vertical alignment string rejected", "[text_presentation][enum]")
{
    auto result = parse_text_vertical_alignment("middle-ish");
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

// ===== Sampling =====

TEST_CASE("No-animation state is steady", "[text_presentation][sampling]")
{
    TextPresentation presentation{};  // entrance/exit both none

    auto result = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(result);

    CHECK(result.value().opacity == Approx(1.0f).margin(kMargin));
    CHECK(result.value().offset_x == Approx(0.0f).margin(kMargin));
    CHECK(result.value().offset_y == Approx(0.0f).margin(kMargin));
    CHECK(result.value().scale == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Entrance beginning, midpoint, and end", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 1000000, EasingKind::linear};
    presentation.exit = TextAnimation{TextAnimationKind::none, 0, EasingKind::linear};

    auto begin = sample_text_presentation(presentation, 2000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().opacity == Approx(0.0f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 2000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().opacity == Approx(0.5f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 2000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().opacity == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Exit beginning, midpoint, and end", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::none, 0, EasingKind::linear};
    presentation.exit = TextAnimation{TextAnimationKind::fade, 1000000, EasingKind::linear};

    // clip_duration = 2,000,000; exit_start = 1,000,000
    auto begin = sample_text_presentation(presentation, 2000000, 1000000);
    REQUIRE(begin);
    CHECK(begin.value().opacity == Approx(1.0f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 2000000, 1500000);
    REQUIRE(mid);
    CHECK(mid.value().opacity == Approx(0.5f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 2000000, 2000000);
    REQUIRE(end);
    CHECK(end.value().opacity == Approx(0.0f).margin(kMargin));
}

TEST_CASE("Fade kind interpolates opacity only", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 1000000, EasingKind::linear};

    auto result = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(result);
    CHECK(result.value().opacity == Approx(0.5f).margin(kMargin));
    CHECK(result.value().offset_x == Approx(0.0f).margin(kMargin));
    CHECK(result.value().offset_y == Approx(0.0f).margin(kMargin));
    CHECK(result.value().scale == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Drop from top interpolates offset_y from negative", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::drop_from_top, 1000000, EasingKind::linear};

    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().offset_y == Approx(-0.3f).margin(kMargin));
    CHECK(begin.value().opacity == Approx(1.0f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().offset_y == Approx(-0.15f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().offset_y == Approx(0.0f).margin(kMargin));
}

TEST_CASE("Rise from bottom interpolates offset_y from positive", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::rise_from_bottom, 1000000, EasingKind::linear};

    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().offset_y == Approx(0.3f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().offset_y == Approx(0.15f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().offset_y == Approx(0.0f).margin(kMargin));
}

TEST_CASE("Slide from left interpolates offset_x from negative", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::slide_from_left, 1000000, EasingKind::linear};

    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().offset_x == Approx(-0.3f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().offset_x == Approx(-0.15f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().offset_x == Approx(0.0f).margin(kMargin));
}

TEST_CASE("Slide from right interpolates offset_x from positive", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::slide_from_right, 1000000, EasingKind::linear};

    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().offset_x == Approx(0.3f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().offset_x == Approx(0.15f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().offset_x == Approx(0.0f).margin(kMargin));
}

TEST_CASE("Scale in interpolates scale from a smaller value", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::scale_in, 1000000, EasingKind::linear};

    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().scale == Approx(0.5f).margin(kMargin));

    auto mid = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().scale == Approx(0.75f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().scale == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Scale out interpolates scale toward a smaller value", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.exit = TextAnimation{TextAnimationKind::scale_out, 1000000, EasingKind::linear};

    // clip_duration == exit.duration_us, so exit_start == 0: the whole clip is the exit window.
    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().scale == Approx(1.0f).margin(kMargin));

    auto three_quarters = sample_text_presentation(presentation, 1000000, 750000);
    REQUIRE(three_quarters);
    CHECK(three_quarters.value().scale == Approx(0.625f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().scale == Approx(0.5f).margin(kMargin));
}

TEST_CASE("All easing kinds produce exact expected progress", "[text_presentation][sampling]")
{
    // t/duration = 0.25 for each case below.
    auto sample_at_quarter = [](EasingKind easing) -> float {
        TextPresentation presentation{};
        presentation.entrance = TextAnimation{TextAnimationKind::fade, 1000000, easing};
        auto result = sample_text_presentation(presentation, 1000000, 250000);
        REQUIRE(result);
        return result.value().opacity;
    };

    CHECK(sample_at_quarter(EasingKind::linear) == Approx(0.25f).margin(kMargin));
    CHECK(sample_at_quarter(EasingKind::ease_in) == Approx(0.0625f).margin(kMargin));
    CHECK(sample_at_quarter(EasingKind::ease_out) == Approx(0.4375f).margin(kMargin));
    CHECK(sample_at_quarter(EasingKind::ease_in_out) == Approx(0.125f).margin(kMargin));
}

TEST_CASE("Ease-in-out is continuous and exact at its midpoint", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 1000000, EasingKind::ease_in_out};

    auto mid = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(mid);
    CHECK(mid.value().opacity == Approx(0.5f).margin(kMargin));

    auto three_quarters = sample_text_presentation(presentation, 1000000, 750000);
    REQUIRE(three_quarters);
    CHECK(three_quarters.value().opacity == Approx(0.875f).margin(kMargin));
}

TEST_CASE("Steady middle state between non-overlapping entrance and exit", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 500000, EasingKind::linear};
    presentation.exit = TextAnimation{TextAnimationKind::fade, 500000, EasingKind::linear};

    auto result = sample_text_presentation(presentation, 2000000, 1000000);
    REQUIRE(result);
    CHECK(result.value().opacity == Approx(1.0f).margin(kMargin));
    CHECK(result.value().offset_x == Approx(0.0f).margin(kMargin));
    CHECK(result.value().offset_y == Approx(0.0f).margin(kMargin));
    CHECK(result.value().scale == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Overlapping entrance and exit fades combine multiplicatively", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 400000, EasingKind::linear};
    presentation.exit = TextAnimation{TextAnimationKind::fade, 400000, EasingKind::linear};

    // clip_duration = 500,000 < 400,000 + 400,000: windows overlap.
    auto result = sample_text_presentation(presentation, 500000, 250000);
    REQUIRE(result);

    // p_entrance = 250000/400000 = 0.625; p_exit = 1 - (150000/400000) = 0.625
    // combined opacity = 0.625 * 0.625 = 0.390625
    CHECK(result.value().opacity == Approx(0.390625f).margin(kMargin));
    CHECK(result.value().opacity >= 0.0f);
    CHECK(result.value().opacity <= 1.0f);
}

TEST_CASE("Overlapping opposite-direction offsets combine additively", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::drop_from_top, 400000, EasingKind::linear};
    presentation.exit = TextAnimation{TextAnimationKind::rise_from_bottom, 400000, EasingKind::linear};

    auto result = sample_text_presentation(presentation, 500000, 250000);
    REQUIRE(result);

    // entrance offset_y = -0.1125, exit offset_y = +0.1125: symmetric overlap cancels exactly.
    CHECK(result.value().offset_y == Approx(0.0f).margin(kMargin));
    CHECK(result.value().opacity == Approx(1.0f).margin(kMargin));
    CHECK(result.value().scale == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Zero clip duration rejected", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    auto result = sample_text_presentation(presentation, 0, 0);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative clip duration rejected", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    auto result = sample_text_presentation(presentation, -1000, 0);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative local time rejected", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    auto result = sample_text_presentation(presentation, 1000000, -1);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Local time beyond clip duration rejected", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    auto result = sample_text_presentation(presentation, 1000000, 1000001);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Local time exactly at clip duration is accepted", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    auto result = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(result);
}

TEST_CASE("Sampling rejects an invalid presentation", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.style.font_family = "";  // invalid

    auto result = sample_text_presentation(presentation, 1000000, 500000);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Sampled output remains finite and bounded across a full clip", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::scale_in, 300000, EasingKind::ease_in_out};
    presentation.exit = TextAnimation{TextAnimationKind::rise_from_bottom, 300000, EasingKind::ease_out};

    std::int64_t clip_duration = 1000000;
    for (std::int64_t t = 0; t <= clip_duration; t += 50000) {
        auto result = sample_text_presentation(presentation, clip_duration, t);
        REQUIRE(result);
        const auto& state = result.value();
        CHECK(std::isfinite(state.opacity));
        CHECK(std::isfinite(state.offset_x));
        CHECK(std::isfinite(state.offset_y));
        CHECK(std::isfinite(state.scale));
        CHECK(state.opacity >= 0.0f);
        CHECK(state.opacity <= 1.0f);
        CHECK(state.scale > 0.0f);
    }
}

TEST_CASE("Exact endpoint values for drop_from_top at clip start", "[text_presentation][sampling]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::drop_from_top, 200000, EasingKind::linear};

    auto result = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(result);
    CHECK(result.value().offset_y == Approx(-0.3f).margin(kMargin));
    CHECK(result.value().opacity == Approx(1.0f).margin(kMargin));
    CHECK(result.value().scale == Approx(1.0f).margin(kMargin));
}

TEST_CASE("Exact endpoint values for no-op animation at clip start and end", "[text_presentation][sampling]")
{
    TextPresentation presentation{};  // entrance/exit both none

    auto begin = sample_text_presentation(presentation, 1000000, 0);
    REQUIRE(begin);
    CHECK(begin.value().opacity == Approx(1.0f).margin(kMargin));
    CHECK(begin.value().offset_x == Approx(0.0f).margin(kMargin));
    CHECK(begin.value().offset_y == Approx(0.0f).margin(kMargin));
    CHECK(begin.value().scale == Approx(1.0f).margin(kMargin));

    auto end = sample_text_presentation(presentation, 1000000, 1000000);
    REQUIRE(end);
    CHECK(end.value().opacity == Approx(1.0f).margin(kMargin));
}

// ===== JSON codec =====

TEST_CASE("Valid presentation round-trips through JSON", "[text_presentation][json]")
{
    TextPresentation presentation{};
    presentation.style.font_family = "Serif";
    presentation.style.font_size = 60.0f;
    presentation.style.bold = true;
    presentation.style.italic = true;
    presentation.style.fill_color = RgbaColor{0.2f, 0.4f, 0.6f, 0.8f};
    presentation.style.outline_color = RgbaColor{0.9f, 0.1f, 0.1f, 1.0f};
    presentation.style.outline_width = 3.5f;
    presentation.style.position_x = 0.25f;
    presentation.style.position_y = 0.75f;
    presentation.style.horizontal_alignment = TextHorizontalAlignment::left;
    presentation.style.vertical_alignment = TextVerticalAlignment::top;
    presentation.entrance = TextAnimation{TextAnimationKind::slide_from_left, 300000, EasingKind::ease_in};
    presentation.exit = TextAnimation{TextAnimationKind::scale_out, 250000, EasingKind::ease_out};

    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    auto parsed_result = text_presentation_from_json(json_result.value());
    REQUIRE(parsed_result);

    const auto& parsed = parsed_result.value();
    CHECK(parsed.style.font_family == presentation.style.font_family);
    CHECK(parsed.style.font_size == Approx(presentation.style.font_size).margin(kMargin));
    CHECK(parsed.style.bold == presentation.style.bold);
    CHECK(parsed.style.italic == presentation.style.italic);
    CHECK(parsed.style.fill_color.red == Approx(presentation.style.fill_color.red).margin(kMargin));
    CHECK(parsed.style.fill_color.green == Approx(presentation.style.fill_color.green).margin(kMargin));
    CHECK(parsed.style.fill_color.blue == Approx(presentation.style.fill_color.blue).margin(kMargin));
    CHECK(parsed.style.fill_color.alpha == Approx(presentation.style.fill_color.alpha).margin(kMargin));
    CHECK(parsed.style.outline_color.red == Approx(presentation.style.outline_color.red).margin(kMargin));
    CHECK(parsed.style.outline_width == Approx(presentation.style.outline_width).margin(kMargin));
    CHECK(parsed.style.position_x == Approx(presentation.style.position_x).margin(kMargin));
    CHECK(parsed.style.position_y == Approx(presentation.style.position_y).margin(kMargin));
    CHECK(parsed.style.horizontal_alignment == presentation.style.horizontal_alignment);
    CHECK(parsed.style.vertical_alignment == presentation.style.vertical_alignment);
    CHECK(parsed.entrance.kind == presentation.entrance.kind);
    CHECK(parsed.entrance.duration_us == presentation.entrance.duration_us);
    CHECK(parsed.entrance.easing == presentation.entrance.easing);
    CHECK(parsed.exit.kind == presentation.exit.kind);
    CHECK(parsed.exit.duration_us == presentation.exit.duration_us);
    CHECK(parsed.exit.easing == presentation.exit.easing);
}

TEST_CASE("Every animation kind round-trips through JSON", "[text_presentation][json]")
{
    std::vector<TextAnimationKind> kinds{
        TextAnimationKind::none,
        TextAnimationKind::fade,
        TextAnimationKind::drop_from_top,
        TextAnimationKind::rise_from_bottom,
        TextAnimationKind::slide_from_left,
        TextAnimationKind::slide_from_right,
        TextAnimationKind::scale_in,
        TextAnimationKind::scale_out
    };

    for (auto kind : kinds) {
        TextPresentation presentation{};
        if (kind == TextAnimationKind::none) {
            presentation.entrance = TextAnimation{kind, 0, EasingKind::linear};
        } else {
            presentation.entrance = TextAnimation{kind, 200000, EasingKind::linear};
        }

        auto json_result = text_presentation_to_json(presentation);
        REQUIRE(json_result);

        auto parsed_result = text_presentation_from_json(json_result.value());
        REQUIRE(parsed_result);
        CHECK(parsed_result.value().entrance.kind == kind);
    }
}

TEST_CASE("Every easing kind round-trips through JSON", "[text_presentation][json]")
{
    std::vector<EasingKind> easings{
        EasingKind::linear,
        EasingKind::ease_in,
        EasingKind::ease_out,
        EasingKind::ease_in_out
    };

    for (auto easing : easings) {
        TextPresentation presentation{};
        presentation.entrance = TextAnimation{TextAnimationKind::fade, 200000, easing};

        auto json_result = text_presentation_to_json(presentation);
        REQUIRE(json_result);

        auto parsed_result = text_presentation_from_json(json_result.value());
        REQUIRE(parsed_result);
        CHECK(parsed_result.value().entrance.easing == easing);
    }
}

TEST_CASE("Every alignment round-trips through JSON", "[text_presentation][json]")
{
    std::vector<TextHorizontalAlignment> h_alignments{
        TextHorizontalAlignment::left,
        TextHorizontalAlignment::center,
        TextHorizontalAlignment::right
    };
    for (auto alignment : h_alignments) {
        TextPresentation presentation{};
        presentation.style.horizontal_alignment = alignment;

        auto json_result = text_presentation_to_json(presentation);
        REQUIRE(json_result);
        auto parsed_result = text_presentation_from_json(json_result.value());
        REQUIRE(parsed_result);
        CHECK(parsed_result.value().style.horizontal_alignment == alignment);
    }

    std::vector<TextVerticalAlignment> v_alignments{
        TextVerticalAlignment::top,
        TextVerticalAlignment::center,
        TextVerticalAlignment::bottom
    };
    for (auto alignment : v_alignments) {
        TextPresentation presentation{};
        presentation.style.vertical_alignment = alignment;

        auto json_result = text_presentation_to_json(presentation);
        REQUIRE(json_result);
        auto parsed_result = text_presentation_from_json(json_result.value());
        REQUIRE(parsed_result);
        CHECK(parsed_result.value().style.vertical_alignment == alignment);
    }
}

TEST_CASE("Repeated serialization of the same input is deterministic", "[text_presentation][json]")
{
    TextPresentation presentation{};
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 300000, EasingKind::ease_in};

    auto first = text_presentation_to_json(presentation);
    auto second = text_presentation_to_json(presentation);
    REQUIRE(first);
    REQUIRE(second);

    CHECK(first.value() == second.value());
    CHECK(first.value().dump() == second.value().dump());
}

TEST_CASE("Unsupported schema version rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["schema_version"] = 999;

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::unsupported_schema);
}

TEST_CASE("Missing top-level schema_version rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j.erase("schema_version");

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Missing style object rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j.erase("style");

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Missing nested font_family rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"].erase("font_family");

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Missing entrance object rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j.erase("entrance");

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Missing exit.kind rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["exit"].erase("kind");

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Wrong type for font_size rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["font_size"] = "huge";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Wrong type for bold rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["bold"] = 42;

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Wrong type for entrance.duration_us rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["entrance"]["duration_us"] = "soon";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Unknown horizontal_alignment enum string rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["horizontal_alignment"] = "diagonal";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Unknown entrance.kind enum string rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["entrance"]["kind"] = "explode";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Unknown exit.easing enum string rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["exit"]["easing"] = "bounce";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("NaN position rejected during deserialization", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["position_x"] = std::numeric_limits<float>::quiet_NaN();

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative font_size rejected during deserialization", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["font_size"] = -5.0f;

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Out-of-range color channel rejected during deserialization", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["fill_color"]["alpha"] = 3.0f;

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Malformed nested fill_color rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["style"]["fill_color"] = "not-a-color";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Malformed nested entrance animation rejected", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json j = json_result.value();
    j["entrance"] = "not-an-animation";

    auto parsed = text_presentation_from_json(j);
    REQUIRE(!parsed);
    CHECK(parsed.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("to_json rejects an invalid presentation instead of writing it", "[text_presentation][json]")
{
    TextPresentation presentation{};
    presentation.style.font_family = "";  // invalid

    auto result = text_presentation_to_json(presentation);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("to_json does not mutate its input", "[text_presentation][json]")
{
    TextPresentation presentation{};
    presentation.style.font_family = "Monospace";
    presentation.entrance = TextAnimation{TextAnimationKind::fade, 250000, EasingKind::ease_out};

    TextPresentation copy = presentation;
    auto result = text_presentation_to_json(presentation);
    REQUIRE(result);

    CHECK(presentation.style.font_family == copy.style.font_family);
    CHECK(presentation.entrance.kind == copy.entrance.kind);
    CHECK(presentation.entrance.duration_us == copy.entrance.duration_us);
}

TEST_CASE("from_json does not mutate its input", "[text_presentation][json]")
{
    TextPresentation presentation{};
    auto json_result = text_presentation_to_json(presentation);
    REQUIRE(json_result);

    nlohmann::json original = json_result.value();
    nlohmann::json copy = original;

    auto parsed = text_presentation_from_json(original);
    REQUIRE(parsed);

    CHECK(original == copy);
}

// ===== Presets =====

TEST_CASE("All presets validate", "[text_presentation][presets]")
{
    std::vector<TextPresentationPreset> presets{
        TextPresentationPreset::clean_centered,
        TextPresentationPreset::cinematic_fade,
        TextPresentationPreset::drop_from_top,
        TextPresentationPreset::rise_from_bottom
    };

    for (auto preset : presets) {
        auto result = make_text_presentation_preset(preset);
        REQUIRE(result);
        CHECK(validate_text_presentation(result.value()));
    }
}

TEST_CASE("clean_centered preset has expected style and no animation", "[text_presentation][presets]")
{
    auto result = make_text_presentation_preset(TextPresentationPreset::clean_centered);
    REQUIRE(result);

    const auto& presentation = result.value();
    CHECK(presentation.style.horizontal_alignment == TextHorizontalAlignment::center);
    CHECK(presentation.style.vertical_alignment == TextVerticalAlignment::center);
    CHECK(presentation.style.position_x == Approx(0.5f).margin(kMargin));
    CHECK(presentation.style.position_y == Approx(0.5f).margin(kMargin));
    CHECK(presentation.entrance.kind == TextAnimationKind::none);
    CHECK(presentation.entrance.duration_us == 0);
    CHECK(presentation.exit.kind == TextAnimationKind::none);
    CHECK(presentation.exit.duration_us == 0);
}

TEST_CASE("cinematic_fade preset has fade entrance and exit", "[text_presentation][presets]")
{
    auto result = make_text_presentation_preset(TextPresentationPreset::cinematic_fade);
    REQUIRE(result);

    const auto& presentation = result.value();
    CHECK(presentation.entrance.kind == TextAnimationKind::fade);
    CHECK(presentation.entrance.duration_us == 500000);
    CHECK(presentation.entrance.easing == EasingKind::ease_out);
    CHECK(presentation.exit.kind == TextAnimationKind::fade);
    CHECK(presentation.exit.duration_us == 500000);
    CHECK(presentation.exit.easing == EasingKind::ease_in);
}

TEST_CASE("drop_from_top preset has drop entrance and fade exit", "[text_presentation][presets]")
{
    auto result = make_text_presentation_preset(TextPresentationPreset::drop_from_top);
    REQUIRE(result);

    const auto& presentation = result.value();
    CHECK(presentation.entrance.kind == TextAnimationKind::drop_from_top);
    CHECK(presentation.entrance.duration_us == 500000);
    CHECK(presentation.entrance.easing == EasingKind::ease_out);
    CHECK(presentation.exit.kind == TextAnimationKind::fade);
    CHECK(presentation.exit.duration_us == 400000);
    CHECK(presentation.exit.easing == EasingKind::ease_in);
}

TEST_CASE("rise_from_bottom preset has rise entrance and fade exit", "[text_presentation][presets]")
{
    auto result = make_text_presentation_preset(TextPresentationPreset::rise_from_bottom);
    REQUIRE(result);

    const auto& presentation = result.value();
    CHECK(presentation.entrance.kind == TextAnimationKind::rise_from_bottom);
    CHECK(presentation.entrance.duration_us == 500000);
    CHECK(presentation.entrance.easing == EasingKind::ease_out);
    CHECK(presentation.exit.kind == TextAnimationKind::fade);
    CHECK(presentation.exit.duration_us == 400000);
    CHECK(presentation.exit.easing == EasingKind::ease_in);
}

TEST_CASE("Repeated preset creation is deterministic", "[text_presentation][presets]")
{
    auto first = make_text_presentation_preset(TextPresentationPreset::drop_from_top);
    auto second = make_text_presentation_preset(TextPresentationPreset::drop_from_top);
    REQUIRE(first);
    REQUIRE(second);

    const auto& a = first.value();
    const auto& b = second.value();

    CHECK(a.style.font_family == b.style.font_family);
    CHECK(a.style.position_x == Approx(b.style.position_x).margin(kMargin));
    CHECK(a.style.position_y == Approx(b.style.position_y).margin(kMargin));
    CHECK(a.style.horizontal_alignment == b.style.horizontal_alignment);
    CHECK(a.style.vertical_alignment == b.style.vertical_alignment);
    CHECK(a.entrance.kind == b.entrance.kind);
    CHECK(a.entrance.duration_us == b.entrance.duration_us);
    CHECK(a.entrance.easing == b.entrance.easing);
    CHECK(a.exit.kind == b.exit.kind);
    CHECK(a.exit.duration_us == b.exit.duration_us);
    CHECK(a.exit.easing == b.exit.easing);
}
