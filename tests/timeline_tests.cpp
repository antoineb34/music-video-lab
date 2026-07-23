#include "timeline.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace mvlab;

// ===== Time and clip validation =====

TEST_CASE("Valid media clip", "[timeline][validation]")
{
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,           // timeline_start
        0,           // source_start
        1000000      // duration (1 second in microseconds)
    };
    REQUIRE(validate_media_clip(clip));
}

TEST_CASE("Valid text clip", "[timeline][validation]")
{
    TextClip clip{
        "clip-1",
        "Hello world",
        0,           // timeline_start
        1000000      // duration (1 second in microseconds)
    };
    REQUIRE(validate_text_clip(clip));
}

TEST_CASE("Empty clip ID rejected for media clip", "[timeline][validation]")
{
    MediaClip clip{
        "",
        "asset-1",
        0,
        0,
        1000000
    };
    auto result = validate_media_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Empty clip ID rejected for text clip", "[timeline][validation]")
{
    TextClip clip{
        "",
        "Hello",
        0,
        1000000
    };
    auto result = validate_text_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Empty asset ID rejected for media clip", "[timeline][validation]")
{
    MediaClip clip{
        "clip-1",
        "",
        0,
        0,
        1000000
    };
    auto result = validate_media_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Empty text rejected for text clip", "[timeline][validation]")
{
    TextClip clip{
        "clip-1",
        "",
        0,
        1000000
    };
    auto result = validate_text_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative timeline start rejected for media clip", "[timeline][validation]")
{
    MediaClip clip{
        "clip-1",
        "asset-1",
        -1,
        0,
        1000000
    };
    auto result = validate_media_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative timeline start rejected for text clip", "[timeline][validation]")
{
    TextClip clip{
        "clip-1",
        "Hello",
        -1,
        1000000
    };
    auto result = validate_text_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative source start rejected for media clip", "[timeline][validation]")
{
    MediaClip clip{
        "clip-1",
        "asset-1",
        0,
        -1,
        1000000
    };
    auto result = validate_media_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Zero duration rejected", "[timeline][validation]")
{
    MediaClip media_clip{
        "clip-1",
        "asset-1",
        0,
        0,
        0
    };
    auto result = validate_media_clip(media_clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);

    TextClip text_clip{
        "clip-1",
        "Hello",
        0,
        0
    };
    result = validate_text_clip(text_clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Negative duration rejected", "[timeline][validation]")
{
    MediaClip media_clip{
        "clip-1",
        "asset-1",
        0,
        0,
        -1
    };
    auto result = validate_media_clip(media_clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);

    TextClip text_clip{
        "clip-1",
        "Hello",
        0,
        -1
    };
    result = validate_text_clip(text_clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Endpoint overflow detected for media clip", "[timeline][validation]")
{
    MediaClip clip{
        "clip-1",
        "asset-1",
        std::numeric_limits<TimelineTime>::max() - 100,
        0,
        1000  // Exceeds max when added to timeline_start
    };
    auto result = validate_media_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Endpoint overflow detected for text clip", "[timeline][validation]")
{
    TextClip clip{
        "clip-1",
        "Hello",
        std::numeric_limits<TimelineTime>::max() - 100,
        1000  // Exceeds max when added to timeline_start
    };
    auto result = validate_text_clip(clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

// ===== Track validation =====

TEST_CASE("Valid audio track", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {
            MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            MediaClip{"clip-2", "asset-2", 1000000, 0, 1000000}
        },
        {}
    };
    REQUIRE(validate_track(track));
}

TEST_CASE("Valid video track", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::video,
        "Background",
        {
            MediaClip{"clip-1", "asset-1", 0, 0, 2000000}
        },
        {}
    };
    REQUIRE(validate_track(track));
}

TEST_CASE("Valid text track", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::text,
        "Lyrics",
        {},
        {
            TextClip{"clip-1", "Verse 1", 0, 5000000},
            TextClip{"clip-2", "Chorus", 5000000, 3000000}
        }
    };
    REQUIRE(validate_track(track));
}

TEST_CASE("Empty track ID rejected", "[timeline][validation]")
{
    Track track{
        "",
        TrackType::audio,
        "Backing",
        {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
        {}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Empty track name rejected", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "",
        {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
        {}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Text clip on audio track rejected", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {},
        {TextClip{"clip-1", "Hello", 0, 1000000}}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Text clip on video track rejected", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::video,
        "Background",
        {},
        {TextClip{"clip-1", "Hello", 0, 1000000}}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Media clip on text track rejected", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::text,
        "Lyrics",
        {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
        {}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Duplicate clip IDs in one track rejected", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {
            MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            MediaClip{"clip-1", "asset-2", 1000000, 0, 1000000}
        },
        {}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Overlapping clips rejected", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {
            MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            MediaClip{"clip-2", "asset-2", 500000, 0, 1000000}
        },
        {}
    };
    auto result = validate_track(track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Exact endpoint adjacency allowed", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {
            MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            MediaClip{"clip-2", "asset-2", 1000000, 0, 1000000}
        },
        {}
    };
    REQUIRE(validate_track(track));
}

TEST_CASE("Non-overlapping clips allowed", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {
            MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            MediaClip{"clip-2", "asset-2", 2000000, 0, 1000000}
        },
        {}
    };
    REQUIRE(validate_track(track));
}

TEST_CASE("Track order is preserved", "[timeline][validation]")
{
    Track track{
        "track-1",
        TrackType::audio,
        "Backing",
        {
            MediaClip{"clip-3", "asset-3", 2000000, 0, 1000000},
            MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
            MediaClip{"clip-2", "asset-2", 1000000, 0, 1000000}
        },
        {}
    };
    REQUIRE(validate_track(track));
    // Verify order is unchanged
    REQUIRE(track.media_clips[0].id == "clip-3");
    REQUIRE(track.media_clips[1].id == "clip-1");
    REQUIRE(track.media_clips[2].id == "clip-2");
}

// ===== Timeline validation =====

TEST_CASE("Empty timeline is valid", "[timeline][validation]")
{
    Timeline timeline{{}};
    REQUIRE(validate_timeline(timeline));
}

TEST_CASE("Multiple valid tracks accepted", "[timeline][validation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "Backing",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            },
            {
                "track-2",
                TrackType::video,
                "Background",
                {MediaClip{"clip-2", "asset-2", 0, 0, 1000000}},
                {}
            },
            {
                "track-3",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-3", "Verse", 0, 1000000}}
            }
        }
    };
    REQUIRE(validate_timeline(timeline));
}

TEST_CASE("Duplicate track IDs rejected", "[timeline][validation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "Track A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            },
            {
                "track-1",
                TrackType::video,
                "Track B",
                {MediaClip{"clip-2", "asset-2", 0, 0, 1000000}},
                {}
            }
        }
    };
    auto result = validate_timeline(timeline);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Duplicate clip IDs across different tracks rejected", "[timeline][validation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "Backing",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            },
            {
                "track-2",
                TrackType::video,
                "Background",
                {MediaClip{"clip-1", "asset-2", 0, 0, 1000000}},
                {}
            }
        }
    };
    auto result = validate_timeline(timeline);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Mixed clip types with duplicate IDs rejected", "[timeline][validation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "Backing",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            },
            {
                "track-2",
                TrackType::text,
                "Text",
                {},
                {TextClip{"clip-1", "Hello", 0, 1000000}}
            }
        }
    };
    auto result = validate_timeline(timeline);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Timeline order is preserved", "[timeline][validation]")
{
    Timeline timeline{
        {
            {"track-3", TrackType::audio, "C", {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}}, {}},
            {"track-1", TrackType::audio, "A", {MediaClip{"clip-2", "asset-2", 0, 0, 1000000}}, {}},
            {"track-2", TrackType::audio, "B", {MediaClip{"clip-3", "asset-3", 0, 0, 1000000}}, {}}
        }
    };
    REQUIRE(validate_timeline(timeline));
    // Verify order is unchanged
    REQUIRE(timeline.tracks[0].id == "track-3");
    REQUIRE(timeline.tracks[1].id == "track-1");
    REQUIRE(timeline.tracks[2].id == "track-2");
}

// ===== Track type conversion =====

TEST_CASE("Track type to_string", "[timeline][types]")
{
    REQUIRE(to_string(TrackType::audio) == "audio");
    REQUIRE(to_string(TrackType::video) == "video");
    REQUIRE(to_string(TrackType::text) == "text");
}

TEST_CASE("Parse valid track type", "[timeline][types]")
{
    auto result = parse_track_type("audio");
    REQUIRE(result);
    REQUIRE(result.value() == TrackType::audio);

    result = parse_track_type("video");
    REQUIRE(result);
    REQUIRE(result.value() == TrackType::video);

    result = parse_track_type("text");
    REQUIRE(result);
    REQUIRE(result.value() == TrackType::text);
}

TEST_CASE("Parse invalid track type", "[timeline][types]")
{
    auto result = parse_track_type("unknown");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

// ===== ID generation =====

TEST_CASE("Empty timeline generates track-1 and clip-1", "[timeline][id_generation]")
{
    Timeline timeline{{}};
    auto track_id = generate_track_id(timeline);
    auto clip_id = generate_clip_id(timeline);
    REQUIRE(track_id == "track-1");
    REQUIRE(clip_id == "clip-1");
}

TEST_CASE("Generate sequential track IDs", "[timeline][id_generation]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::audio, "B", {}, {}}
        }
    };
    auto track_id = generate_track_id(timeline);
    REQUIRE(track_id == "track-3");
}

TEST_CASE("Generate sequential clip IDs", "[timeline][id_generation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 1000000, 0, 1000000}
                },
                {}
            }
        }
    };
    auto clip_id = generate_clip_id(timeline);
    REQUIRE(clip_id == "clip-3");
}

TEST_CASE("ID generation ignores gaps", "[timeline][id_generation]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-3", TrackType::audio, "B", {}, {}}
        }
    };
    auto track_id = generate_track_id(timeline);
    REQUIRE(track_id == "track-2");
}

TEST_CASE("ID generation ignores malformed IDs", "[timeline][id_generation]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-xyz", TrackType::audio, "B", {}, {}},
            {"custom-id", TrackType::audio, "C", {}, {}}
        }
    };
    auto track_id = generate_track_id(timeline);
    REQUIRE(track_id == "track-2");
}

TEST_CASE("Clip IDs are unique across media and text clips", "[timeline][id_generation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            },
            {
                "track-2",
                TrackType::text,
                "B",
                {},
                {TextClip{"clip-2", "Hello", 0, 1000000}}
            }
        }
    };
    auto clip_id = generate_clip_id(timeline);
    REQUIRE(clip_id == "clip-3");
}

TEST_CASE("Clip IDs are unique across all tracks", "[timeline][id_generation]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-3", "asset-2", 1000000, 0, 1000000}
                },
                {}
            },
            {
                "track-2",
                TrackType::video,
                "B",
                {MediaClip{"clip-2", "asset-3", 0, 0, 1000000}},
                {}
            }
        }
    };
    auto clip_id = generate_clip_id(timeline);
    REQUIRE(clip_id == "clip-4");
}

// ===== Lookup functions =====

TEST_CASE("Find mutable track by ID", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    auto result = find_track(timeline, "track-2");
    REQUIRE(result);
    REQUIRE(result.value()->id == "track-2");
}

TEST_CASE("Find const track by ID", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    const Timeline& const_timeline = timeline;
    auto result = find_track(const_timeline, "track-2");
    REQUIRE(result);
    REQUIRE(result.value()->id == "track-2");
}

TEST_CASE("Track lookup returns unknown ID error", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    auto result = find_track(timeline, "track-999");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Track lookup returns empty ID error", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    auto result = find_track(timeline, "");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Find mutable media clip by ID", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 1000000, 0, 1000000}
                },
                {}
            }
        }
    };
    auto result = find_media_clip(timeline, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value()->id == "clip-2");
    REQUIRE(result.value()->asset_id == "asset-2");
}

TEST_CASE("Find const media clip by ID", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            }
        }
    };
    const Timeline& const_timeline = timeline;
    auto result = find_media_clip(const_timeline, "clip-1");
    REQUIRE(result);
    REQUIRE(result.value()->id == "clip-1");
}

TEST_CASE("Media clip lookup searches all tracks", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}}, {}},
            {"track-2", TrackType::video, "B", {MediaClip{"clip-2", "asset-2", 0, 0, 1000000}}, {}}
        }
    };
    auto result = find_media_clip(timeline, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value()->id == "clip-2");
}

TEST_CASE("Media clip lookup returns unknown ID error", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}}, {}}
        }
    };
    auto result = find_media_clip(timeline, "clip-999");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Media clip lookup returns empty ID error", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}}, {}}
        }
    };
    auto result = find_media_clip(timeline, "");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Find mutable text clip by ID", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {
                    TextClip{"clip-1", "Verse", 0, 1000000},
                    TextClip{"clip-2", "Chorus", 1000000, 1000000}
                }
            }
        }
    };
    auto result = find_text_clip(timeline, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value()->id == "clip-2");
    REQUIRE(result.value()->text == "Chorus");
}

TEST_CASE("Find const text clip by ID", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000}}
            }
        }
    };
    const Timeline& const_timeline = timeline;
    auto result = find_text_clip(const_timeline, "clip-1");
    REQUIRE(result);
    REQUIRE(result.value()->id == "clip-1");
}

TEST_CASE("Text clip lookup searches all tracks", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T1",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000}}
            },
            {
                "track-2",
                TrackType::text,
                "T2",
                {},
                {TextClip{"clip-2", "Chorus", 0, 1000000}}
            }
        }
    };
    auto result = find_text_clip(timeline, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value()->id == "clip-2");
}

TEST_CASE("Text clip lookup returns unknown ID error", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000}}
            }
        }
    };
    auto result = find_text_clip(timeline, "clip-999");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Text clip lookup returns empty ID error", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000}}
            }
        }
    };
    auto result = find_text_clip(timeline, "");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Lookup does not mutate ordering", "[timeline][lookup]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-3", "asset-1", 2000000, 0, 1000000},
                    MediaClip{"clip-1", "asset-2", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-3", 1000000, 0, 1000000}
                },
                {}
            }
        }
    };
    find_media_clip(timeline, "clip-1");
    find_media_clip(timeline, "clip-2");
    find_media_clip(timeline, "clip-3");
    REQUIRE(timeline.tracks[0].media_clips[0].id == "clip-3");
    REQUIRE(timeline.tracks[0].media_clips[1].id == "clip-1");
    REQUIRE(timeline.tracks[0].media_clips[2].id == "clip-2");
}
