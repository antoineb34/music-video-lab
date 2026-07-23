#include "preview_session.hpp"
#include "project_model.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace mvlab;

namespace {

// Helper to create a minimal valid project
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

// Helper to create an empty project
Project create_empty_project()
{
    Project proj;
    proj.name = "empty_project";
    proj.schema_version = current_project_schema_version;
    return proj;
}

// Helper to add a media track with a clip
void add_media_track(Project& proj, TrackType type, TimelineTime start, TimelineTime duration)
{
    // Generate unique track and clip IDs based on track count
    int track_num = proj.timeline.tracks.size() + 1;
    std::string track_id = type == TrackType::audio
        ? "audio-track-" + std::to_string(track_num)
        : "video-track-" + std::to_string(track_num);
    std::string clip_id = type == TrackType::audio
        ? "audio-clip-" + std::to_string(track_num)
        : "video-clip-" + std::to_string(track_num);

    Track track{
        track_id,
        type,
        type == TrackType::audio ? "Audio" : "Video",
        {},
        {}
    };
    MediaClip clip{
        clip_id,
        "asset-1",
        start,
        0,
        duration
    };
    track.media_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);
}

// Helper to add a text track with a clip
void add_text_track(Project& proj, TimelineTime start, TimelineTime duration)
{
    // Generate unique track and clip IDs based on track count
    int track_num = proj.timeline.tracks.size() + 1;
    std::string track_id = "text-track-" + std::to_string(track_num);
    std::string clip_id = "text-clip-" + std::to_string(track_num);

    Track track{
        track_id,
        TrackType::text,
        "Text",
        {},
        {}
    };
    TextClip clip{
        clip_id,
        "Lyric",
        start,
        duration
    };
    track.text_clips.push_back(clip);
    proj.timeline.tracks.push_back(track);
}

}  // namespace

// ===== Creation =====

TEST_CASE("Valid project creates stopped session at zero", "[preview_session][creation]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);

    auto session = result.value();
    CHECK(session.state() == PreviewPlaybackState::stopped);
    CHECK(session.playhead() == 0);
}

TEST_CASE("Invalid project rejected", "[preview_session][creation]")
{
    // Create a project with invalid export settings (zero width)
    auto proj = create_empty_project();
    proj.export_settings.width = 0;

    auto result = PreviewSession::create(proj);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Empty project duration is zero", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    CHECK(result.value().duration() == 0);
}

TEST_CASE("Duration uses greatest clip end across mixed tracks", "[preview_session][creation]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::audio, 0, 1000000);
    add_media_track(proj, TrackType::video, 0, 2000000);
    add_text_track(proj, 500000, 1500000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    CHECK(result.value().duration() == 2000000);
}

TEST_CASE("Invalid frame-rate numerator rejected", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 0;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative frame-rate numerator rejected", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = -30;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Invalid frame-rate denominator rejected", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    PreviewSessionSettings settings;
    settings.frame_rate.denominator = 0;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative frame-rate denominator rejected", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    PreviewSessionSettings settings;
    settings.frame_rate.denominator = -1;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(!result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Custom frame rate accepted", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 24;
    settings.frame_rate.denominator = 1;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    CHECK(result.value().settings().frame_rate.numerator == 24);
}

TEST_CASE("NTSC frame rate 30000/1001 accepted", "[preview_session][creation]")
{
    auto proj = create_empty_project();

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30000;
    settings.frame_rate.denominator = 1001;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
}

// ===== Playback state =====

TEST_CASE("Play from stopped", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto play_result = session.play();
    REQUIRE(play_result);
    CHECK(session.state() == PreviewPlaybackState::playing);
}

TEST_CASE("Play from paused", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.pause();
    auto play_result = session.play();
    REQUIRE(play_result);
    CHECK(session.state() == PreviewPlaybackState::playing);
}

TEST_CASE("Repeated play is idempotent", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    auto playhead_after_first = session.playhead();
    session.play();
    CHECK(session.playhead() == playhead_after_first);
    CHECK(session.state() == PreviewPlaybackState::playing);
}

TEST_CASE("Pause from playing", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    auto pause_result = session.pause();
    REQUIRE(pause_result);
    CHECK(session.state() == PreviewPlaybackState::paused);
}

TEST_CASE("Repeated pause is idempotent", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.pause();
    auto playhead_after_first = session.playhead();
    session.pause();
    CHECK(session.playhead() == playhead_after_first);
    CHECK(session.state() == PreviewPlaybackState::paused);
}

TEST_CASE("Stop resets playhead and state", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.seek(500000);
    session.stop();

    CHECK(session.state() == PreviewPlaybackState::stopped);
    CHECK(session.playhead() == 0);
}

TEST_CASE("Repeated stop is idempotent", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.stop();
    CHECK(session.playhead() == 0);
    session.stop();
    CHECK(session.playhead() == 0);
    CHECK(session.state() == PreviewPlaybackState::stopped);
}

TEST_CASE("Play at non-looping end restarts from zero", "[preview_session][playback_state]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.loop = false;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.seek(1000000);  // At exact end
    CHECK(session.playhead() == 1000000);
    session.play();
    CHECK(session.playhead() == 0);
}

// ===== Seeking =====

TEST_CASE("Seek within range", "[preview_session][seeking]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto seek_result = session.seek(500000);
    REQUIRE(seek_result);
    CHECK(session.playhead() == 500000);
}

TEST_CASE("Seek to exact end", "[preview_session][seeking]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto seek_result = session.seek(1000000);
    REQUIRE(seek_result);
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("Seek past end clamps", "[preview_session][seeking]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto seek_result = session.seek(2000000);
    REQUIRE(seek_result);
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("Negative seek rejected", "[preview_session][seeking]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto seek_result = session.seek(-1);
    REQUIRE(!seek_result);
    CHECK(seek_result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Seek preserves playing state", "[preview_session][seeking]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.seek(300000);
    CHECK(session.state() == PreviewPlaybackState::playing);
}

TEST_CASE("Seek preserves paused state", "[preview_session][seeking]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.pause();
    session.seek(300000);
    CHECK(session.state() == PreviewPlaybackState::paused);
}

TEST_CASE("Empty project seek remains zero", "[preview_session][seeking]")
{
    auto proj = create_empty_project();

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto seek_result = session.seek(1000000);
    REQUIRE(seek_result);
    CHECK(session.playhead() == 0);
}

// ===== Advancing =====

TEST_CASE("Stopped does not advance", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.advance(1000000);
    CHECK(session.playhead() == 0);
}

TEST_CASE("Paused does not advance", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.pause();
    session.advance(1000000);
    CHECK(session.playhead() == 0);
}

TEST_CASE("Playing advances", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.advance(1000000);
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("Zero elapsed is a no-op", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.seek(500000);
    session.advance(0);
    CHECK(session.playhead() == 500000);
}

TEST_CASE("Negative elapsed rejected", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    auto advance_result = session.advance(-1);
    REQUIRE(!advance_result);
    CHECK(advance_result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Non-looping playback stops at end", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.loop = false;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.advance(1500000);
    CHECK(session.state() == PreviewPlaybackState::stopped);
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("Playhead remains at end after completion", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.loop = false;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.advance(2000000);
    CHECK(session.playhead() == 1000000);
    session.advance(500000);  // Additional advance should not change it
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("Looping playback wraps", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.loop = true;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.advance(1500000);
    CHECK(session.playhead() == 500000);
}

TEST_CASE("Elapsed spanning multiple loops wraps correctly", "[preview_session][advancing]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.loop = true;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.advance(3500000);  // 3.5 loops
    CHECK(session.playhead() == 500000);
}

TEST_CASE("Empty project remains stable during advance", "[preview_session][advancing]")
{
    auto proj = create_empty_project();

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.advance(1000000);
    CHECK(session.playhead() == 0);
    CHECK(session.state() == PreviewPlaybackState::stopped);
}

// ===== Frame stepping =====

TEST_CASE("One frame forward at 30 fps", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    auto step_result = session.step_frames(1);
    REQUIRE(step_result);
    // Frame duration: 1_000_000 / 30 = 33333 microseconds (with integer division)
    CHECK(session.playhead() == 33333);
}

TEST_CASE("Several frames forward", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    auto step_result = session.step_frames(30);
    REQUIRE(step_result);
    // 30 frames at 30 fps = 1 second = 1_000_000 microseconds
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("One frame backward", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.seek(500000);
    auto step_result = session.step_frames(-1);
    REQUIRE(step_result);
    CHECK(session.playhead() == 500000 - 33333);
}

TEST_CASE("Clamp at zero", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;
    settings.loop = false;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.step_frames(-10);
    CHECK(session.playhead() == 0);
}

TEST_CASE("Clamp at duration", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;
    settings.loop = false;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.step_frames(1000);
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("Looping forward wrap", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;
    settings.loop = true;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.step_frames(100);
    CHECK(session.playhead() >= 0);
    CHECK(session.playhead() < 1000000);
}

TEST_CASE("Looping backward wrap", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30;
    settings.frame_rate.denominator = 1;
    settings.loop = true;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    session.step_frames(-10);
    CHECK(session.playhead() >= 0);
    CHECK(session.playhead() < 1000000);
}

TEST_CASE("24 fps frame stepping", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 24;
    settings.frame_rate.denominator = 1;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    auto step_result = session.step_frames(24);
    REQUIRE(step_result);
    // 24 frames at 24 fps = 1 second = 1_000_000 microseconds
    CHECK(session.playhead() == 1000000);
}

TEST_CASE("NTSC 30000/1001 fps frame stepping", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 10000000000);

    PreviewSessionSettings settings;
    settings.frame_rate.numerator = 30000;
    settings.frame_rate.denominator = 1001;

    auto result = PreviewSession::create(proj, settings);
    REQUIRE(result);
    auto session = result.value();

    auto step_result = session.step_frames(30000);
    REQUIRE(step_result);
    // 30000 frames at 30000/1001 fps = (30000 * 1_000_000 * 1001) / 30000 = 1_001_000_000 microseconds
    CHECK(session.playhead() == 1001000000);
}

TEST_CASE("Playback state preserved while stepping", "[preview_session][frame_stepping]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 2000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    session.step_frames(10);
    CHECK(session.state() == PreviewPlaybackState::playing);

    session.pause();
    session.step_frames(10);
    CHECK(session.state() == PreviewPlaybackState::paused);
}

TEST_CASE("Empty project remains at zero while stepping", "[preview_session][frame_stepping]")
{
    auto proj = create_empty_project();

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.step_frames(100);
    CHECK(session.playhead() == 0);
}

// ===== Frame plans =====

TEST_CASE("Plan playhead matches session playhead", "[preview_session][frame_plans]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.seek(500000);
    auto plan_result = session.current_frame_plan();
    REQUIRE(plan_result);
    CHECK(plan_result.value().playhead == 500000);
}

TEST_CASE("Seek changes active clips in returned plan", "[preview_session][frame_plans]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);
    add_media_track(proj, TrackType::video, 1000000, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.seek(500000);
    auto plan1_result = session.current_frame_plan();
    REQUIRE(plan1_result);
    auto plan1 = plan1_result.value();

    session.seek(1500000);
    auto plan2_result = session.current_frame_plan();
    REQUIRE(plan2_result);
    auto plan2 = plan2_result.value();

    // Different playheads should give different plans
    CHECK(plan1.playhead != plan2.playhead);
}

TEST_CASE("Advance changes active clips", "[preview_session][frame_plans]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);
    add_media_track(proj, TrackType::video, 1000000, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.play();
    auto plan1_result = session.current_frame_plan();
    REQUIRE(plan1_result);

    session.advance(1500000);
    auto plan2_result = session.current_frame_plan();
    REQUIRE(plan2_result);

    CHECK(plan1_result.value().playhead != plan2_result.value().playhead);
}

TEST_CASE("Mixed audio/video/text plan returned", "[preview_session][frame_plans]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::audio, 0, 1000000);
    add_media_track(proj, TrackType::video, 0, 1000000);
    add_text_track(proj, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    auto plan_result = session.current_frame_plan();
    REQUIRE(plan_result);
    auto plan = plan_result.value();

    CHECK(!plan.audio_clips.empty());
    CHECK(!plan.video_clips.empty());
    CHECK(!plan.text_clips.empty());
}

TEST_CASE("Repeated frame plan calls are deterministic", "[preview_session][frame_plans]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.seek(500000);
    auto plan1 = session.current_frame_plan();
    auto plan2 = session.current_frame_plan();

    REQUIRE(plan1);
    REQUIRE(plan2);
    CHECK(plan1.value().playhead == plan2.value().playhead);
    CHECK(plan1.value().video_clips.size() == plan2.value().video_clips.size());
}

TEST_CASE("Project content remains unchanged", "[preview_session][frame_plans]")
{
    auto proj = create_test_project();
    add_media_track(proj, TrackType::video, 0, 1000000);

    auto track_count_orig = proj.timeline.tracks.size();
    auto clip_count_orig = proj.timeline.tracks[0].media_clips.size();

    auto result = PreviewSession::create(proj);
    REQUIRE(result);
    auto session = result.value();

    session.seek(500000);
    session.current_frame_plan();

    // Verify project wasn't modified
    CHECK(proj.timeline.tracks.size() == track_count_orig);
    CHECK(proj.timeline.tracks[0].media_clips.size() == clip_count_orig);
}
