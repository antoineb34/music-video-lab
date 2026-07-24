#include "preview_plan.hpp"
#include "project_model.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mvlab;

namespace {

// Helper to create a test project with predefined structure
Project create_test_project()
{
    Project proj;
    proj.name = "test_project";
    proj.schema_version = current_project_schema_version;

    // Add a test asset
    MediaAsset asset{
        "asset-1",
        MediaAssetType::video,
        "test_video.mp4",
        "media/test_video.mp4",
        1024000
    };
    proj.assets.push_back(asset);

    return proj;
}

// Helper to create a project with empty timeline
Project create_empty_project()
{
    Project proj;
    proj.name = "empty_project";
    proj.schema_version = current_project_schema_version;
    return proj;
}

// Helper to create default presentation for test fixtures
TextPresentation default_text_presentation()
{
    return make_text_presentation_preset(TextPresentationPreset::clean_centered).value();
}

// Helper to create a non-default presentation, distinct in every field from
// both the TextStyle defaults and the clean_centered preset.
TextPresentation create_custom_text_presentation()
{
    TextPresentation pres;
    pres.style.font_family = "Serif";
    pres.style.font_size = 36.0f;
    pres.style.bold = true;
    pres.style.italic = true;
    pres.style.fill_color = {0.8f, 0.2f, 0.9f, 0.95f};
    pres.style.outline_color = {0.1f, 0.8f, 0.3f, 0.7f};
    pres.style.outline_width = 2.5f;
    pres.style.position_x = 0.25f;
    pres.style.position_y = 0.75f;
    pres.style.horizontal_alignment = TextHorizontalAlignment::left;
    pres.style.vertical_alignment = TextVerticalAlignment::top;
    pres.entrance = {TextAnimationKind::fade, 500000, EasingKind::ease_out};
    pres.exit = {TextAnimationKind::slide_from_right, 400000, EasingKind::ease_in};
    return pres;
}

}  // namespace

// ===== General cases =====

TEST_CASE("Empty project returns empty plan", "[preview_plan][general]")
{
    auto proj = create_empty_project();
    auto result = build_preview_frame_plan(proj, 0);
    REQUIRE(result);

    auto plan = result.value();
    CHECK(plan.playhead == 0);
    CHECK(plan.audio_clips.empty());
    CHECK(plan.video_clips.empty());
    CHECK(plan.text_clips.empty());
}

TEST_CASE("Negative playhead rejected", "[preview_plan][general]")
{
    auto proj = create_empty_project();
    auto result = build_preview_frame_plan(proj, -1);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Playhead preserved in plan", "[preview_plan][general]")
{
    auto proj = create_empty_project();
    TimelineTime test_time = 5000000;  // 5 seconds
    auto result = build_preview_frame_plan(proj, test_time);
    REQUIRE(result);
    CHECK(result.value().playhead == test_time);
}

TEST_CASE("Project not mutated by planning", "[preview_plan][general]")
{
    auto proj = create_test_project();
    Track audio_track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    audio_track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(audio_track);

    auto orig_track_count = proj.timeline.tracks.size();
    auto orig_clip_count = proj.timeline.tracks[0].media_clips.size();
    auto result = build_preview_frame_plan(proj, 500000);

    REQUIRE(result);
    CHECK(proj.timeline.tracks.size() == orig_track_count);
    CHECK(proj.timeline.tracks[0].media_clips.size() == orig_clip_count);
}

TEST_CASE("Repeated planning is deterministic", "[preview_plan][general]")
{
    auto proj = create_test_project();
    Track video_track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000,
        0,
        2000000
    };
    video_track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(video_track);

    auto result1 = build_preview_frame_plan(proj, 1500000);
    auto result2 = build_preview_frame_plan(proj, 1500000);

    REQUIRE(result1);
    REQUIRE(result2);
    auto plan1 = result1.value();
    auto plan2 = result2.value();

    REQUIRE(plan1.video_clips.size() == plan2.video_clips.size());
    if (!plan1.video_clips.empty()) {
        CHECK(plan1.video_clips[0].clip_id == plan2.video_clips[0].clip_id);
        CHECK(plan1.video_clips[0].source_time == plan2.video_clips[0].source_time);
    }
}

// ===== Boundary cases =====

TEST_CASE("Clip active at exact start", "[preview_plan][boundaries]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    CHECK(result.value().audio_clips.size() == 1);
}

TEST_CASE("Clip inactive at exact end", "[preview_plan][boundaries]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 2000000);
    REQUIRE(result);
    CHECK(result.value().audio_clips.empty());
}

TEST_CASE("Clip active one microsecond before end", "[preview_plan][boundaries]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 1999999);
    REQUIRE(result);
    CHECK(result.value().audio_clips.size() == 1);
}

TEST_CASE("Clip inactive before start", "[preview_plan][boundaries]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 999999);
    REQUIRE(result);
    CHECK(result.value().video_clips.empty());
}

TEST_CASE("Adjacent clips switch at shared endpoint", "[preview_plan][boundaries]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip1{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    MediaClip clip2{
        "clip-2",
        "asset-1",
        1000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip1);
    track.media_clips.push_back(clip2);
    proj.timeline.tracks.push_back(track);

    auto result_at_boundary = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result_at_boundary);
    auto plan = result_at_boundary.value();

    REQUIRE(plan.audio_clips.size() == 1);
    CHECK(plan.audio_clips[0].clip_id == "clip-2");  // Only clip2 active
}

// ===== Audio and video =====

TEST_CASE("Active audio clip included", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.audio_clips.size() == 1);
    CHECK(plan.audio_clips[0].track_id == "track-1");
    CHECK(plan.audio_clips[0].clip_id == "clip-1");
}

TEST_CASE("Active video clip included", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.video_clips.size() == 1);
    CHECK(plan.video_clips[0].track_type == TrackType::video);
}

TEST_CASE("Source time calculation with zero offset", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000,  // timeline start
        0,        // source start
        2000000   // duration
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 2500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.audio_clips.size() == 1);
    CHECK(plan.audio_clips[0].source_time == 1500000);  // 2500000 - 1000000
}

TEST_CASE("Source time calculation with nonzero offset", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,        // timeline start
        500000,   // source start
        2000000   // duration
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.video_clips.size() == 1);
    CHECK(plan.video_clips[0].source_time == 1500000);  // 500000 + 1000000
}

TEST_CASE("Remaining duration calculation", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,
        0,
        3000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.audio_clips.size() == 1);
    CHECK(plan.audio_clips[0].remaining_duration == 2000000);
}

TEST_CASE("Inactive clips omitted", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip1{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    MediaClip clip2{
        "clip-2",
        "asset-1",
        2000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip1);
    track.media_clips.push_back(clip2);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 1500000);
    REQUIRE(result);
    CHECK(result.value().video_clips.empty());
}

TEST_CASE("Multiple active clips on separate tracks", "[preview_plan][media]")
{
    auto proj = create_test_project();

    Track audio_track{
        "audio-track",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip audio_clip{
        "audio-clip-1",
        "asset-1",
        0,
        0,
        2000000
    };
    audio_track.media_clips.push_back(audio_clip);

    Track video_track{
        "video-track",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip video_clip{
        "video-clip-1",
        "asset-1",
        0,
        0,
        2000000
    };
    video_track.media_clips.push_back(video_clip);

    proj.timeline.tracks.push_back(audio_track);
    proj.timeline.tracks.push_back(video_track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    auto plan = result.value();

    CHECK(plan.audio_clips.size() == 1);
    CHECK(plan.video_clips.size() == 1);
}

TEST_CASE("Track order preserved in output", "[preview_plan][media]")
{
    auto proj = create_test_project();

    Track video_track{
        "video-track",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip video_clip{
        "video-clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    video_track.media_clips.push_back(video_clip);

    Track audio_track{
        "audio-track",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip audio_clip{
        "audio-clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    audio_track.media_clips.push_back(audio_clip);

    proj.timeline.tracks.push_back(video_track);
    proj.timeline.tracks.push_back(audio_track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.video_clips.size() == 1);
    REQUIRE(plan.audio_clips.size() == 1);
    CHECK(plan.video_clips[0].track_id == "video-track");
    CHECK(plan.audio_clips[0].track_id == "audio-track");
}

TEST_CASE("Clip order within track preserved", "[preview_plan][media]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip1{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    MediaClip clip2{
        "clip-2",
        "asset-1",
        1000000,
        0,
        1000000
    };
    track.media_clips.push_back(clip1);
    track.media_clips.push_back(clip2);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 1500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.audio_clips.size() == 1);
    CHECK(plan.audio_clips[0].clip_id == "clip-2");
}

// ===== Text clips =====

TEST_CASE("Active text clip included", "[preview_plan][text]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Hello, World!",
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.text_clips.size() == 1);
    CHECK(plan.text_clips[0].clip_id == "text-clip-1");
}

TEST_CASE("Text content preserved exactly", "[preview_plan][text]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    std::string test_text = "🎵 Music Video Lab";
    TextClip clip{
        "text-clip-1",
        test_text,
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    CHECK(result.value().text_clips[0].text == test_text);
}

TEST_CASE("Text local time calculation", "[preview_plan][text]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        500000,
        2000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.text_clips.size() == 1);
    CHECK(plan.text_clips[0].local_time == 500000);
}

TEST_CASE("Text remaining duration calculation", "[preview_plan][text]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        0,
        3000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    CHECK(result.value().text_clips[0].remaining_duration == 2000000);
}

TEST_CASE("Steady-state animation sampled", "[preview_plan][text]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto state = result.value().text_clips[0].animation_state;

    // Steady state should have neutral animation values
    CHECK(state.opacity == 1.0f);
    CHECK(state.offset_x == 0.0f);
    CHECK(state.offset_y == 0.0f);
    CHECK(state.scale == 1.0f);
}

TEST_CASE("Text animation state is valid", "[preview_plan][text]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 250000);
    REQUIRE(result);
    auto state = result.value().text_clips[0].animation_state;

    // Opacity should be in [0, 1] and scale should be positive
    CHECK(state.opacity >= 0.0f);
    CHECK(state.opacity <= 1.0f);
    CHECK(state.scale > 0.0f);
}

TEST_CASE("Multiple text tracks preserve ordering", "[preview_plan][text]")
{
    auto proj = create_empty_project();

    Track text_track1{
        "text-track-1",
        TrackType::text,
        "Text 1",
        {},
        {}
    };
    TextClip clip1{
        "clip-1",
        "First",
        0,
        1000000,
        default_text_presentation()
    };
    text_track1.text_clips.push_back(clip1);

    Track text_track2{
        "text-track-2",
        TrackType::text,
        "Text 2",
        {},
        {}
    };
    TextClip clip2{
        "clip-2",
        "Second",
        0,
        1000000,
        default_text_presentation()
    };
    text_track2.text_clips.push_back(clip2);

    proj.timeline.tracks.push_back(text_track1);
    proj.timeline.tracks.push_back(text_track2);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto plan = result.value();

    REQUIRE(plan.text_clips.size() == 2);
    CHECK(plan.text_clips[0].track_id == "text-track-1");
    CHECK(plan.text_clips[1].track_id == "text-track-2");
}

// ===== Mixed projects =====

TEST_CASE("Simultaneous audio, video, and text clips", "[preview_plan][mixed]")
{
    auto proj = create_test_project();

    Track audio_track{
        "audio-track",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip audio_clip{
        "audio-clip-1",
        "asset-1",
        0,
        0,
        2000000
    };
    audio_track.media_clips.push_back(audio_clip);

    Track video_track{
        "video-track",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip video_clip{
        "video-clip-1",
        "asset-1",
        0,
        0,
        2000000
    };
    video_track.media_clips.push_back(video_clip);

    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip text_clip{
        "text-clip-1",
        "Lyric",
        0,
        2000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(text_clip);

    proj.timeline.tracks.push_back(audio_track);
    proj.timeline.tracks.push_back(video_track);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    auto plan = result.value();

    CHECK(plan.audio_clips.size() == 1);
    CHECK(plan.video_clips.size() == 1);
    CHECK(plan.text_clips.size() == 1);
}

TEST_CASE("Text tracks do not appear in media lists", "[preview_plan][mixed]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    auto plan = result.value();

    CHECK(plan.audio_clips.empty());
    CHECK(plan.video_clips.empty());
    REQUIRE(plan.text_clips.size() == 1);
}

TEST_CASE("Deterministic complete frame plan", "[preview_plan][mixed]")
{
    auto proj = create_test_project();

    Track track1{
        "track-1",
        TrackType::audio,
        "Audio",
        {},
        {}
    };
    MediaClip clip1{
        "clip-1",
        "asset-1",
        0,
        500000,
        2000000
    };
    track1.media_clips.push_back(clip1);

    Track track2{
        "track-2",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip2{
        "clip-2",
        "asset-1",
        0,
        0,
        2000000
    };
    track2.media_clips.push_back(clip2);

    Track track3{
        "track-3",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip3{
        "clip-3",
        "Lyric",
        0,
        2000000,
        default_text_presentation()
    };
    track3.text_clips.push_back(clip3);

    proj.timeline.tracks.push_back(track1);
    proj.timeline.tracks.push_back(track2);
    proj.timeline.tracks.push_back(track3);

    auto result1 = build_preview_frame_plan(proj, 1000000);
    auto result2 = build_preview_frame_plan(proj, 1000000);

    REQUIRE(result1);
    REQUIRE(result2);
    auto plan1 = result1.value();
    auto plan2 = result2.value();

    CHECK(plan1.audio_clips.size() == plan2.audio_clips.size());
    CHECK(plan1.video_clips.size() == plan2.video_clips.size());
    CHECK(plan1.text_clips.size() == plan2.text_clips.size());
}

// ===== Asset validation =====

TEST_CASE("Valid media asset succeeds", "[preview_plan][assets]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
}

TEST_CASE("Missing asset reference rejected", "[preview_plan][assets]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    MediaClip clip{
        "clip-1",
        "nonexistent-asset",  // This asset doesn't exist
        0,
        0,
        1000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Text-only project needs no media asset", "[preview_plan][assets]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
}

// ===== Edge cases for arithmetic =====

TEST_CASE("Very large clip duration values handled", "[preview_plan][overflow]")
{
    auto proj = create_test_project();
    Track track{
        "track-1",
        TrackType::video,
        "Video",
        {},
        {}
    };
    // Use large but valid values that fit in int64_t
    MediaClip clip{
        "clip-1",
        "asset-1",
        1000000000000000,
        0,
        1000000000000000
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);

    // Playhead outside this clip - should return empty plan
    auto result = build_preview_frame_plan(proj, 500000);
    REQUIRE(result);
    CHECK(result.value().video_clips.empty());
}

TEST_CASE("Playhead zero with zero-duration clip edge case", "[preview_plan][boundaries]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        "text-clip-1",
        "Text",
        0,
        1000000,
        default_text_presentation()
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 0);
    REQUIRE(result);
    REQUIRE(result.value().text_clips.size() == 1);
}

// ===== Cross-feature: TextClip::presentation consumed by preview planning =====
//
// These tests prove that preview planning uses the exact TextPresentation
// stored on TextClip (attached by the timeline-presentation feature), with
// no production fallback substituting a default preset for current-schema
// clips. Persistence-schema migration (old timeline JSON -> clean_centered)
// belongs to timeline deserialization, not preview planning; the last test
// below confirms preview planning consumes that already-migrated value.

TEST_CASE("Custom text presentation fields are preserved exactly in preview frame plan",
    "[preview_plan][text_presentation]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    auto custom = create_custom_text_presentation();
    TextClip clip{
        "text-clip-1",
        "Custom styled text",
        0,
        2000000,
        custom
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    // A playhead outside both the entrance and exit windows, so only the
    // static style/animation-definition fields are under test here.
    auto result = build_preview_frame_plan(proj, 1000000);
    REQUIRE(result);
    REQUIRE(result.value().text_clips.size() == 1);

    const auto& active = result.value().text_clips[0].presentation;
    CHECK(active.style.font_family == custom.style.font_family);
    CHECK(active.style.font_size == custom.style.font_size);
    CHECK(active.style.fill_color.red == custom.style.fill_color.red);
    CHECK(active.style.fill_color.green == custom.style.fill_color.green);
    CHECK(active.style.fill_color.blue == custom.style.fill_color.blue);
    CHECK(active.style.fill_color.alpha == custom.style.fill_color.alpha);
    CHECK(active.style.outline_color.red == custom.style.outline_color.red);
    CHECK(active.style.outline_color.green == custom.style.outline_color.green);
    CHECK(active.style.outline_color.blue == custom.style.outline_color.blue);
    CHECK(active.style.outline_color.alpha == custom.style.outline_color.alpha);
    CHECK(active.style.outline_width == custom.style.outline_width);
    CHECK(active.style.position_x == custom.style.position_x);
    CHECK(active.style.position_y == custom.style.position_y);
    CHECK(active.style.horizontal_alignment == custom.style.horizontal_alignment);
    CHECK(active.style.vertical_alignment == custom.style.vertical_alignment);
    CHECK(active.entrance.kind == custom.entrance.kind);
    CHECK(active.entrance.duration_us == custom.entrance.duration_us);
    CHECK(active.entrance.easing == custom.entrance.easing);
    CHECK(active.exit.kind == custom.exit.kind);
    CHECK(active.exit.duration_us == custom.exit.duration_us);
    CHECK(active.exit.easing == custom.exit.easing);

    // Sanity check: none of these match the clean_centered fallback preset,
    // so this is not passing by coincidence with a substituted default.
    auto fallback = default_text_presentation();
    CHECK(active.style.font_family != fallback.style.font_family);
    CHECK(active.style.vertical_alignment != fallback.style.vertical_alignment);
}

TEST_CASE("Preview animation state is sampled from the clip's own custom presentation",
    "[preview_plan][text_presentation]")
{
    auto proj = create_empty_project();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    // clean_centered has entrance.kind == none, which always yields steady
    // state (opacity 1.0). A custom fade entrance sampled at clip start
    // must yield opacity 0.0 -- this only holds if the real presentation
    // (not a clean_centered fallback) drove the sampling.
    auto custom = create_custom_text_presentation();
    TextClip clip{
        "text-clip-1",
        "Fading in",
        0,
        2000000,
        custom
    };
    text_track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(text_track);

    auto result = build_preview_frame_plan(proj, 0);
    REQUIRE(result);
    REQUIRE(result.value().text_clips.size() == 1);
    CHECK(result.value().text_clips[0].animation_state.opacity == 0.0f);
}

TEST_CASE("Split text clip preserves presentation on both sides in preview plan",
    "[preview_plan][text_presentation][split]")
{
    auto proj = create_empty_project();
    auto custom = create_custom_text_presentation();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    text_track.text_clips.push_back(TextClip{
        "clip-1",
        "Hello World",
        0,
        2000000,
        custom
    });
    proj.timeline.tracks.push_back(text_track);

    auto split_result = split_text_clip(proj.timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(split_result);

    auto left_plan = build_preview_frame_plan(proj, 500000);
    REQUIRE(left_plan);
    REQUIRE(left_plan.value().text_clips.size() == 1);
    CHECK(left_plan.value().text_clips[0].clip_id == "clip-1");
    CHECK(left_plan.value().text_clips[0].presentation.style.font_family == custom.style.font_family);
    CHECK(left_plan.value().text_clips[0].presentation.entrance.kind == custom.entrance.kind);

    auto right_plan = build_preview_frame_plan(proj, 1500000);
    REQUIRE(right_plan);
    REQUIRE(right_plan.value().text_clips.size() == 1);
    CHECK(right_plan.value().text_clips[0].clip_id == "clip-2");
    CHECK(right_plan.value().text_clips[0].presentation.style.font_family == custom.style.font_family);
    CHECK(right_plan.value().text_clips[0].presentation.entrance.kind == custom.entrance.kind);
}

TEST_CASE("Moved and trimmed text clip still exposes its presentation through preview planning",
    "[preview_plan][text_presentation][editing]")
{
    auto proj = create_empty_project();
    auto custom = create_custom_text_presentation();
    Track text_track{
        "text-track",
        TrackType::text,
        "Text",
        {},
        {}
    };
    text_track.text_clips.push_back(TextClip{
        "clip-1",
        "Moving text",
        0,
        2000000,
        custom
    });
    proj.timeline.tracks.push_back(text_track);

    auto move_result = move_clip(proj.timeline, "clip-1", 1000000);
    REQUIRE(move_result);
    auto trim_result = trim_clip_end(proj.timeline, "clip-1", 2500000);
    REQUIRE(trim_result);

    auto plan = build_preview_frame_plan(proj, 2000000);
    REQUIRE(plan);
    REQUIRE(plan.value().text_clips.size() == 1);
    CHECK(plan.value().text_clips[0].presentation.style.font_family == custom.style.font_family);
    CHECK(plan.value().text_clips[0].presentation.style.outline_width == custom.style.outline_width);
    CHECK(plan.value().text_clips[0].presentation.entrance.kind == custom.entrance.kind);
    CHECK(plan.value().text_clips[0].presentation.exit.kind == custom.exit.kind);
}

TEST_CASE("Timeline-v1 migrated text clip presentation flows into preview planning unmodified",
    "[preview_plan][text_presentation][migration]")
{
    // A v1 timeline document predates TextClip::presentation; deserialization
    // migrates it to clean_centered. Preview planning must consume that
    // already-migrated value, not apply a fallback of its own.
    nlohmann::json v1_json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "text"},
                {"name", "Lyrics"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"text", "Migrated text"},
                        {"timeline_start_us", 0},
                        {"duration_us", 2000000}
                    }
                })}
            }
        })}
    };

    auto timeline_result = timeline_from_json(v1_json);
    REQUIRE(timeline_result);

    auto proj = create_empty_project();
    proj.timeline = timeline_result.value();

    auto plan = build_preview_frame_plan(proj, 1000000);
    REQUIRE(plan);
    REQUIRE(plan.value().text_clips.size() == 1);

    auto expected = default_text_presentation();  // clean_centered
    const auto& active = plan.value().text_clips[0].presentation;
    CHECK(active.style.horizontal_alignment == expected.style.horizontal_alignment);
    CHECK(active.style.vertical_alignment == expected.style.vertical_alignment);
    CHECK(active.style.position_x == expected.style.position_x);
    CHECK(active.style.position_y == expected.style.position_y);
    CHECK(active.entrance.kind == expected.entrance.kind);
    CHECK(active.exit.kind == expected.exit.kind);
}
