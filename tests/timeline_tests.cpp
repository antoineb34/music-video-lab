#include "timeline.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace mvlab;

// Helper to create default presentation for test fixtures
inline TextPresentation default_text_presentation()
{
    return make_text_presentation_preset(TextPresentationPreset::clean_centered).value();
}

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
    , default_text_presentation()
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
    , default_text_presentation()
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
    , default_text_presentation()
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
    , default_text_presentation()
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
    , default_text_presentation()
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
    , default_text_presentation()
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
    , default_text_presentation()
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
            TextClip{"clip-1", "Verse 1", 0, 5000000, default_text_presentation()},
            TextClip{"clip-2", "Chorus", 5000000, 3000000, default_text_presentation()}
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
        {TextClip{"clip-1", "Hello", 0, 1000000, default_text_presentation()}}
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
        {TextClip{"clip-1", "Hello", 0, 1000000, default_text_presentation()}}
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
                {TextClip{"clip-3", "Verse", 0, 1000000, default_text_presentation()}}
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
                {TextClip{"clip-1", "Hello", 0, 1000000, default_text_presentation()}}
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
                {TextClip{"clip-2", "Hello", 0, 1000000, default_text_presentation()}}
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
                    TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()},
                    TextClip{"clip-2", "Chorus", 1000000, 1000000, default_text_presentation()}
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
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
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
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
            },
            {
                "track-2",
                TrackType::text,
                "T2",
                {},
                {TextClip{"clip-2", "Chorus", 0, 1000000, default_text_presentation()}}
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
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
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
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
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

// ===== Track editing operations =====

TEST_CASE("Add valid empty track", "[timeline][editing]")
{
    Timeline timeline{{}};
    Track track{
        "track-1",
        TrackType::audio,
        "A",
        {},
        {}
    };
    auto result = add_track(timeline, track);
    REQUIRE(result);
    REQUIRE(timeline.tracks.size() == 1);
    REQUIRE(timeline.tracks[0].id == "track-1");
}

TEST_CASE("Add valid populated track", "[timeline][editing]")
{
    Timeline timeline{{}};
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
    auto result = add_track(timeline, track);
    REQUIRE(result);
    REQUIRE(timeline.tracks.size() == 1);
    REQUIRE(timeline.tracks[0].media_clips.size() == 2);
}

TEST_CASE("Add track with invalid contents rejects", "[timeline][editing]")
{
    Timeline timeline{{}};
    Track track{
        "track-1",
        TrackType::audio,
        "A",
        {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
        {TextClip{"clip-2", "Hello", 0, 1000000, default_text_presentation()}}
    };
    auto result = add_track(timeline, track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks.empty());
}

TEST_CASE("Reject duplicate track ID", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    Track track{
        "track-1",
        TrackType::video,
        "B",
        {},
        {}
    };
    auto result = add_track(timeline, track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks.size() == 1);
}

TEST_CASE("Reject globally duplicate clip ID when adding track", "[timeline][editing]")
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
    Track track{
        "track-2",
        TrackType::video,
        "B",
        {MediaClip{"clip-1", "asset-2", 0, 0, 1000000}},
        {}
    };
    auto result = add_track(timeline, track);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks.size() == 1);
}

TEST_CASE("Remove existing track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    auto result = remove_track(timeline, "track-1");
    REQUIRE(result);
    REQUIRE(timeline.tracks.size() == 1);
    REQUIRE(timeline.tracks[0].id == "track-2");
}

TEST_CASE("Remove track and all its clips", "[timeline][editing]")
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
    auto result = remove_track(timeline, "track-1");
    REQUIRE(result);
    REQUIRE(timeline.tracks.empty());
}

TEST_CASE("Remove missing track rejects", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    auto result = remove_track(timeline, "track-999");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
    REQUIRE(timeline.tracks.size() == 1);
}

TEST_CASE("Failed track removal leaves timeline unchanged", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    auto original = timeline;
    remove_track(timeline, "track-999");
    REQUIRE(timeline.tracks.size() == original.tracks.size());
    REQUIRE(timeline.tracks[0].id == original.tracks[0].id);
}

// ===== Media clip insertion =====

TEST_CASE("Add media clip to audio track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    MediaClip clip{"clip-1", "asset-1", 0, 0, 1000000};
    auto result = add_media_clip(timeline, "track-1", clip);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips.size() == 1);
    REQUIRE(timeline.tracks[0].media_clips[0].id == "clip-1");
}

TEST_CASE("Add media clip to video track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::video, "V", {}, {}}
        }
    };
    MediaClip clip{"clip-1", "asset-1", 0, 0, 1000000};
    auto result = add_media_clip(timeline, "track-1", clip);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips.size() == 1);
}

TEST_CASE("Reject media clip on text track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::text, "T", {}, {}}
        }
    };
    MediaClip clip{"clip-1", "asset-1", 0, 0, 1000000};
    auto result = add_media_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips.empty());
}

TEST_CASE("Reject media clip on missing track", "[timeline][editing]")
{
    Timeline timeline{{}};
    MediaClip clip{"clip-1", "asset-1", 0, 0, 1000000};
    auto result = add_media_clip(timeline, "track-999", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Reject duplicate clip ID when adding media clip", "[timeline][editing]")
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
    MediaClip clip{"clip-1", "asset-2", 1000000, 0, 1000000};
    auto result = add_media_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips.size() == 1);
}

TEST_CASE("Reject duplicate clip ID across different tracks", "[timeline][editing]")
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
            {"track-2", TrackType::video, "V", {}, {}}
        }
    };
    MediaClip clip{"clip-1", "asset-2", 0, 0, 1000000};
    auto result = add_media_clip(timeline, "track-2", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[1].media_clips.empty());
}

TEST_CASE("Reject overlapping media clips", "[timeline][editing]")
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
    MediaClip clip{"clip-2", "asset-2", 500000, 0, 1000000};
    auto result = add_media_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Allow exact endpoint adjacency for media clips", "[timeline][editing]")
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
    MediaClip clip{"clip-2", "asset-2", 1000000, 0, 1000000};
    auto result = add_media_clip(timeline, "track-1", clip);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips.size() == 2);
}

TEST_CASE("Failed media clip insertion leaves timeline unchanged", "[timeline][editing]")
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
    auto original = timeline;
    MediaClip clip{"clip-1", "asset-2", 1000000, 0, 1000000};
    add_media_clip(timeline, "track-1", clip);
    REQUIRE(timeline.tracks[0].media_clips.size() == original.tracks[0].media_clips.size());
}

// ===== Text clip insertion =====

TEST_CASE("Add text clip to text track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::text, "T", {}, {}}
        }
    };
    TextClip clip{"clip-1", "Verse", 0, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-1", clip);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips.size() == 1);
    REQUIRE(timeline.tracks[0].text_clips[0].id == "clip-1");
}

TEST_CASE("Reject text clip on audio track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    TextClip clip{"clip-1", "Hello", 0, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].text_clips.empty());
}

TEST_CASE("Reject text clip on video track", "[timeline][editing]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::video, "V", {}, {}}
        }
    };
    TextClip clip{"clip-1", "Hello", 0, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].text_clips.empty());
}

TEST_CASE("Reject text clip on missing track", "[timeline][editing]")
{
    Timeline timeline{{}};
    TextClip clip{"clip-1", "Hello", 0, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-999", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Reject duplicate clip ID when adding text clip", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
            }
        }
    };
    TextClip clip{"clip-1", "Chorus", 1000000, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].text_clips.size() == 1);
}

TEST_CASE("Reject overlapping text clips", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
            }
        }
    };
    TextClip clip{"clip-2", "Chorus", 500000, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-1", clip);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Allow exact endpoint adjacency for text clips", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
            }
        }
    };
    TextClip clip{"clip-2", "Chorus", 1000000, 1000000, default_text_presentation()
};
    auto result = add_text_clip(timeline, "track-1", clip);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips.size() == 2);
}

TEST_CASE("Failed text clip insertion leaves timeline unchanged", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
            }
        }
    };
    auto original = timeline;
    TextClip clip{"clip-1", "Chorus", 1000000, 1000000, default_text_presentation()
};
    add_text_clip(timeline, "track-1", clip);
    REQUIRE(timeline.tracks[0].text_clips.size() == original.tracks[0].text_clips.size());
}

// ===== Clip removal =====

TEST_CASE("Remove media clip", "[timeline][editing]")
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
    auto result = remove_clip(timeline, "clip-1");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips.size() == 1);
    REQUIRE(timeline.tracks[0].media_clips[0].id == "clip-2");
}

TEST_CASE("Remove text clip", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {
                    TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()},
                    TextClip{"clip-2", "Chorus", 1000000, 1000000, default_text_presentation()}
                }
            }
        }
    };
    auto result = remove_clip(timeline, "clip-1");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips.size() == 1);
    REQUIRE(timeline.tracks[0].text_clips[0].id == "clip-2");
}

TEST_CASE("Remove clip from correct track only", "[timeline][editing]")
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
                TrackType::video,
                "V",
                {MediaClip{"clip-2", "asset-2", 0, 0, 1000000}},
                {}
            }
        }
    };
    auto result = remove_clip(timeline, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips.size() == 1);
    REQUIRE(timeline.tracks[1].media_clips.empty());
}

TEST_CASE("Remove missing clip rejects", "[timeline][editing]")
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
    auto result = remove_clip(timeline, "clip-999");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Failed clip removal leaves timeline unchanged", "[timeline][editing]")
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
    auto original = timeline;
    remove_clip(timeline, "clip-999");
    REQUIRE(timeline.tracks[0].media_clips.size() == original.tracks[0].media_clips.size());
    REQUIRE(timeline.tracks[0].media_clips[0].id == original.tracks[0].media_clips[0].id);
}

// ===== Clip movement =====

TEST_CASE("Move media clip successfully", "[timeline][editing]")
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
    auto result = move_clip(timeline, "clip-1", 5000000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == 5000000);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == 1000000);
}

TEST_CASE("Move text clip successfully", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Verse", 0, 1000000, default_text_presentation()}}
            }
        }
    };
    auto result = move_clip(timeline, "clip-1", 5000000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[0].timeline_start == 5000000);
    REQUIRE(timeline.tracks[0].text_clips[0].duration == 1000000);
}

TEST_CASE("Reject negative start when moving clip", "[timeline][editing]")
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
    auto original = timeline;
    auto result = move_clip(timeline, "clip-1", -1000000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == original.tracks[0].media_clips[0].timeline_start);
}

TEST_CASE("Reject overflow when moving clip", "[timeline][editing]")
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
    auto original = timeline;
    auto result = move_clip(timeline, "clip-1", std::numeric_limits<TimelineTime>::max() - 100);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == original.tracks[0].media_clips[0].timeline_start);
}

TEST_CASE("Reject resulting overlap when moving clip", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 2000000, 0, 1000000}
                },
                {}
            }
        }
    };
    auto original = timeline;
    auto result = move_clip(timeline, "clip-2", 500000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips[1].timeline_start == original.tracks[0].media_clips[1].timeline_start);
}

TEST_CASE("Allow adjacency when moving clip", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 2000000, 0, 1000000}
                },
                {}
            }
        }
    };
    auto result = move_clip(timeline, "clip-2", 1000000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[1].timeline_start == 1000000);
}

TEST_CASE("Failed clip move preserves original timeline state", "[timeline][editing]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 2000000, 0, 1000000}
                },
                {}
            }
        }
    };
    auto original = timeline;
    move_clip(timeline, "clip-2", 500000);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == original.tracks[0].media_clips[0].timeline_start);
    REQUIRE(timeline.tracks[0].media_clips[1].timeline_start == original.tracks[0].media_clips[1].timeline_start);
}

// ===== Track organization =====

TEST_CASE("Rename existing track", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "Old Name", {}, {}}
        }
    };
    auto result = rename_track(timeline, "track-1", "New Name");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].name == "New Name");
    REQUIRE(timeline.tracks[0].id == "track-1");
}

TEST_CASE("Rename preserves all other track fields", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::video,
                "Original",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            }
        }
    };
    auto result = rename_track(timeline, "track-1", "Renamed");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].type == TrackType::video);
    REQUIRE(timeline.tracks[0].media_clips[0].id == "clip-1");
}

TEST_CASE("Rename missing track rejects", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    auto result = rename_track(timeline, "track-999", "New");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Rename to empty name rejects", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "Old", {}, {}}
        }
    };
    auto original = timeline;
    auto result = rename_track(timeline, "track-1", "");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].name == original.tracks[0].name);
}

TEST_CASE("Failed rename leaves timeline unchanged", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "Original", {}, {}}
        }
    };
    auto original = timeline;
    rename_track(timeline, "track-999", "New");
    REQUIRE(timeline.tracks[0].name == original.tracks[0].name);
}

TEST_CASE("Move first track to end", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}},
            {"track-3", TrackType::text, "C", {}, {}}
        }
    };
    auto result = move_track(timeline, "track-1", 2);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].id == "track-2");
    REQUIRE(timeline.tracks[1].id == "track-3");
    REQUIRE(timeline.tracks[2].id == "track-1");
}

TEST_CASE("Move last track to beginning", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}},
            {"track-3", TrackType::text, "C", {}, {}}
        }
    };
    auto result = move_track(timeline, "track-3", 0);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].id == "track-3");
    REQUIRE(timeline.tracks[1].id == "track-1");
    REQUIRE(timeline.tracks[2].id == "track-2");
}

TEST_CASE("Move middle track", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}},
            {"track-3", TrackType::text, "C", {}, {}}
        }
    };
    auto result = move_track(timeline, "track-2", 2);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].id == "track-1");
    REQUIRE(timeline.tracks[1].id == "track-3");
    REQUIRE(timeline.tracks[2].id == "track-2");
}

TEST_CASE("Move track to same index is no-op", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}},
            {"track-3", TrackType::text, "C", {}, {}}
        }
    };
    auto result = move_track(timeline, "track-2", 1);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].id == "track-1");
    REQUIRE(timeline.tracks[1].id == "track-2");
    REQUIRE(timeline.tracks[2].id == "track-3");
}

TEST_CASE("Move missing track rejects", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}}
        }
    };
    auto result = move_track(timeline, "track-999", 0);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Move to out-of-range index rejects", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    auto original = timeline;
    auto result = move_track(timeline, "track-1", 5);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].id == original.tracks[0].id);
}

TEST_CASE("Move track preserves clips", "[timeline][track_organization]")
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
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    auto result = move_track(timeline, "track-1", 1);
    REQUIRE(result);
    REQUIRE(timeline.tracks[1].id == "track-1");
    REQUIRE(timeline.tracks[1].media_clips[0].id == "clip-1");
}

TEST_CASE("Failed move leaves timeline unchanged", "[timeline][track_organization]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    auto original = timeline;
    move_track(timeline, "track-999", 1);
    REQUIRE(timeline.tracks[0].id == original.tracks[0].id);
    REQUIRE(timeline.tracks[1].id == original.tracks[1].id);
}

// ===== Trim operations =====

TEST_CASE("Trim media clip start successfully", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 500000, 2000000}},
                {}
            }
        }
    };
    auto result = trim_media_clip_start(timeline, "clip-1", 1500000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == 1500000);
    REQUIRE(timeline.tracks[0].media_clips[0].source_start == 1000000);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == 1500000);
}

TEST_CASE("Trim advances source_start by exact delta", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 500000, 2000000}},
                {}
            }
        }
    };
    auto result = trim_media_clip_start(timeline, "clip-1", 1750000);
    REQUIRE(result);
    TimelineTime delta = 1750000 - 1000000;
    REQUIRE(timeline.tracks[0].media_clips[0].source_start == 500000 + delta);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == 2000000 - delta);
}

TEST_CASE("Trim preserves clip end time", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    TimelineTime original_end = 1000000 + 2000000;
    auto result = trim_media_clip_start(timeline, "clip-1", 1500000);
    REQUIRE(result);
    TimelineTime new_end = 1500000 + timeline.tracks[0].media_clips[0].duration;
    REQUIRE(new_end == original_end);
}

TEST_CASE("Reject trim at current start", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto original = timeline;
    auto result = trim_media_clip_start(timeline, "clip-1", 1000000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == original.tracks[0].media_clips[0].timeline_start);
}

TEST_CASE("Reject trim at clip end", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto original = timeline;
    auto result = trim_media_clip_start(timeline, "clip-1", 3000000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject trim beyond end", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto result = trim_media_clip_start(timeline, "clip-1", 5000000);
    REQUIRE(!result);
}

TEST_CASE("Reject trim on text clip", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 1000000, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = trim_media_clip_start(timeline, "clip-1", 1500000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Reject trim on missing clip", "[timeline][trim]")
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
    auto result = trim_media_clip_start(timeline, "clip-999", 500000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Failed media trim leaves timeline unchanged", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto original = timeline;
    trim_media_clip_start(timeline, "clip-1", 1000000);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == original.tracks[0].media_clips[0].timeline_start);
    REQUIRE(timeline.tracks[0].media_clips[0].source_start == original.tracks[0].media_clips[0].source_start);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == original.tracks[0].media_clips[0].duration);
}

TEST_CASE("Trim media clip end successfully", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto result = trim_clip_end(timeline, "clip-1", 2500000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == 1000000);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == 1500000);
}

TEST_CASE("Trim text clip end successfully", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 1000000, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = trim_clip_end(timeline, "clip-1", 2500000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[0].timeline_start == 1000000);
    REQUIRE(timeline.tracks[0].text_clips[0].duration == 1500000);
}

TEST_CASE("Trim preserves start and other fields", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 500000, 2000000}},
                {}
            }
        }
    };
    auto result = trim_clip_end(timeline, "clip-1", 2500000);
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == 1000000);
    REQUIRE(timeline.tracks[0].media_clips[0].source_start == 500000);
}

TEST_CASE("Reject trim end at start", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto result = trim_clip_end(timeline, "clip-1", 1000000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject trim end before start", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto result = trim_clip_end(timeline, "clip-1", 500000);
    REQUIRE(!result);
}

TEST_CASE("Reject trim extension beyond current end", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto result = trim_clip_end(timeline, "clip-1", 5000000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject trim on missing clip for end trim", "[timeline][trim]")
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
    auto result = trim_clip_end(timeline, "clip-999", 500000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Failed end trim leaves timeline unchanged", "[timeline][trim]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto original = timeline;
    trim_clip_end(timeline, "clip-1", 1000000);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == original.tracks[0].media_clips[0].duration);
}

// ===== Split operations =====

TEST_CASE("Split media clip successfully", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value() == "clip-2");
    REQUIRE(timeline.tracks[0].media_clips.size() == 2);
}

TEST_CASE("Split media produces exact left/right durations", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].duration == 1000000);
    REQUIRE(timeline.tracks[0].media_clips[1].duration == 1000000);
}

TEST_CASE("Split preserves timeline continuity", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 500000, 1000000, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1500000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start == 500000);
    REQUIRE(timeline.tracks[0].media_clips[1].timeline_start == 1500000);
}

TEST_CASE("Split preserves source continuity", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 1000000, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].source_start == 1000000);
    REQUIRE(timeline.tracks[0].media_clips[1].source_start == 2000000);
}

TEST_CASE("Split preserves asset ID", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].asset_id == "asset-1");
    REQUIRE(timeline.tracks[0].media_clips[1].asset_id == "asset-1");
}

TEST_CASE("Split preserves original ID on left", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].id == "clip-1");
}

TEST_CASE("Split uses caller-provided ID on right", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[1].id == "clip-2");
}

TEST_CASE("Split inserts into same track", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            },
            {"track-2", TrackType::video, "B", {}, {}}
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips.size() == 2);
    REQUIRE(timeline.tracks[1].media_clips.empty());
}

TEST_CASE("Split deterministic clip order", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].media_clips[0].timeline_start < timeline.tracks[0].media_clips[1].timeline_start);
}

TEST_CASE("Reject split at start", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto original = timeline;
    auto result = split_media_clip(timeline, "clip-1", 0, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips.size() == original.tracks[0].media_clips.size());
}

TEST_CASE("Reject split at end", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 2000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject split outside range", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 1000000, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 5000000, "clip-2");
    REQUIRE(!result);
}

TEST_CASE("Reject duplicate right ID globally", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 2000000},
                    MediaClip{"clip-2", "asset-1", 2000000, 0, 1000000}
                },
                {}
            }
        }
    };
    auto original = timeline;
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
    REQUIRE(timeline.tracks[0].media_clips.size() == original.tracks[0].media_clips.size());
}

TEST_CASE("Reject empty right ID", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject split on text clip", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Reject split on missing clip", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_media_clip(timeline, "clip-999", 1000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Failed media split leaves timeline unchanged", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto original = timeline;
    split_media_clip(timeline, "clip-1", 1000000, "");
    REQUIRE(timeline.tracks[0].media_clips.size() == original.tracks[0].media_clips.size());
    REQUIRE(timeline.tracks[0].media_clips[0].duration == original.tracks[0].media_clips[0].duration);
}

TEST_CASE("Split text clip successfully", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello World", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value() == "clip-2");
    REQUIRE(timeline.tracks[0].text_clips.size() == 2);
}

TEST_CASE("Split text produces exact durations", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[0].duration == 1000000);
    REQUIRE(timeline.tracks[0].text_clips[1].duration == 1000000);
}

TEST_CASE("Split text preserves/copies text", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello World", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[0].text == "Hello World");
    REQUIRE(timeline.tracks[0].text_clips[1].text == "Hello World");
}

TEST_CASE("Split text preserves original ID on left", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[0].id == "clip-1");
}

TEST_CASE("Split text uses supplied right ID", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[1].id == "clip-2");
}

TEST_CASE("Split text inserts into same track", "[timeline][split]")
{
    Timeline timeline{
        {
            {"track-1", TrackType::audio, "A", {}, {}},
            {
                "track-2",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[1].text_clips.size() == 2);
}

TEST_CASE("Split text deterministic ordering", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(result);
    REQUIRE(timeline.tracks[0].text_clips[0].timeline_start < timeline.tracks[0].text_clips[1].timeline_start);
}

TEST_CASE("Reject text split at start", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto original = timeline;
    auto result = split_text_clip(timeline, "clip-1", 0, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject text split at end", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 2000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject text split duplicate/empty right ID", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Reject text split on media clip", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::audio,
                "A",
                {MediaClip{"clip-1", "asset-1", 0, 0, 2000000}},
                {}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-1", 1000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Reject text split on missing clip", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto result = split_text_clip(timeline, "clip-999", 1000000, "clip-2");
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::file_not_found);
}

TEST_CASE("Failed text split leaves timeline unchanged", "[timeline][split]")
{
    Timeline timeline{
        {
            {
                "track-1",
                TrackType::text,
                "T",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_text_presentation()}}
            }
        }
    };
    auto original = timeline;
    split_text_clip(timeline, "clip-1", 1000000, "");
    REQUIRE(timeline.tracks[0].text_clips.size() == original.tracks[0].text_clips.size());
    REQUIRE(timeline.tracks[0].text_clips[0].duration == original.tracks[0].text_clips[0].duration);
}

// ===== Text presentation preservation through edits =====

// Helper to create a rich, non-default presentation for testing
TextPresentation create_test_presentation() {
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
    pres.entrance = {TextAnimationKind::slide_from_left, 400000, EasingKind::ease_in};
    pres.exit = {TextAnimationKind::slide_from_right, 300000, EasingKind::ease_out};
    return pres;
}

// Helper to verify two presentations are identical
void assert_presentations_equal(const TextPresentation& a, const TextPresentation& b) {
    REQUIRE(a.style.font_family == b.style.font_family);
    REQUIRE(a.style.font_size == b.style.font_size);
    REQUIRE(a.style.bold == b.style.bold);
    REQUIRE(a.style.italic == b.style.italic);
    REQUIRE(a.style.fill_color.red == b.style.fill_color.red);
    REQUIRE(a.style.fill_color.green == b.style.fill_color.green);
    REQUIRE(a.style.fill_color.blue == b.style.fill_color.blue);
    REQUIRE(a.style.fill_color.alpha == b.style.fill_color.alpha);
    REQUIRE(a.style.outline_color.red == b.style.outline_color.red);
    REQUIRE(a.style.outline_color.green == b.style.outline_color.green);
    REQUIRE(a.style.outline_color.blue == b.style.outline_color.blue);
    REQUIRE(a.style.outline_color.alpha == b.style.outline_color.alpha);
    REQUIRE(a.style.outline_width == b.style.outline_width);
    REQUIRE(a.style.position_x == b.style.position_x);
    REQUIRE(a.style.position_y == b.style.position_y);
    REQUIRE(a.style.horizontal_alignment == b.style.horizontal_alignment);
    REQUIRE(a.style.vertical_alignment == b.style.vertical_alignment);
    REQUIRE(a.entrance.kind == b.entrance.kind);
    REQUIRE(a.entrance.duration_us == b.entrance.duration_us);
    REQUIRE(a.entrance.easing == b.entrance.easing);
    REQUIRE(a.exit.kind == b.exit.kind);
    REQUIRE(a.exit.duration_us == b.exit.duration_us);
    REQUIRE(a.exit.easing == b.exit.easing);
}

TEST_CASE("Text presentation preserved by move_clip", "[timeline][text_presentation][editing]")
{
    auto custom_pres = create_test_presentation();
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, custom_pres}}
            }
        }
    };

    auto original_presentation = timeline.tracks[0].text_clips[0].presentation;

    // Move the clip
    auto result = move_clip(timeline, "clip-1", 5000000);
    REQUIRE(result);

    // Verify timing changed
    REQUIRE(timeline.tracks[0].text_clips[0].timeline_start == 5000000);
    REQUIRE(timeline.tracks[0].text_clips[0].duration == 2000000);

    // Verify text preserved
    REQUIRE(timeline.tracks[0].text_clips[0].text == "Hello");

    // Verify presentation preserved exactly
    assert_presentations_equal(timeline.tracks[0].text_clips[0].presentation, original_presentation);
}

TEST_CASE("Text presentation preserved by trim_clip_end", "[timeline][text_presentation][editing]")
{
    auto custom_pres = create_test_presentation();
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Hello", 0, 3000000, custom_pres}}
            }
        }
    };

    auto original_presentation = timeline.tracks[0].text_clips[0].presentation;
    auto original_text = timeline.tracks[0].text_clips[0].text;

    // Trim the clip end
    auto result = trim_clip_end(timeline, "clip-1", 1500000);
    REQUIRE(result);

    // Verify duration changed
    REQUIRE(timeline.tracks[0].text_clips[0].duration == 1500000);

    // Verify start and text unchanged
    REQUIRE(timeline.tracks[0].text_clips[0].timeline_start == 0);
    REQUIRE(timeline.tracks[0].text_clips[0].text == original_text);

    // Verify presentation preserved exactly
    assert_presentations_equal(timeline.tracks[0].text_clips[0].presentation, original_presentation);
}

TEST_CASE("Text presentation copied by split_text_clip", "[timeline][text_presentation][editing]")
{
    auto custom_pres = create_test_presentation();
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Hello world", 0, 4000000, custom_pres}}
            }
        }
    };

    auto original_presentation = timeline.tracks[0].text_clips[0].presentation;

    // Split the clip
    auto result = split_text_clip(timeline, "clip-1", 2000000, "clip-2");
    REQUIRE(result);
    REQUIRE(result.value() == "clip-2");

    // Verify we have two clips now
    REQUIRE(timeline.tracks[0].text_clips.size() == 2);

    // Verify left clip
    REQUIRE(timeline.tracks[0].text_clips[0].id == "clip-1");
    REQUIRE(timeline.tracks[0].text_clips[0].text == "Hello world");
    REQUIRE(timeline.tracks[0].text_clips[0].timeline_start == 0);
    REQUIRE(timeline.tracks[0].text_clips[0].duration == 2000000);
    assert_presentations_equal(timeline.tracks[0].text_clips[0].presentation, original_presentation);

    // Verify right clip
    REQUIRE(timeline.tracks[0].text_clips[1].id == "clip-2");
    REQUIRE(timeline.tracks[0].text_clips[1].text == "Hello world");
    REQUIRE(timeline.tracks[0].text_clips[1].timeline_start == 2000000);
    REQUIRE(timeline.tracks[0].text_clips[1].duration == 2000000);
    assert_presentations_equal(timeline.tracks[0].text_clips[1].presentation, original_presentation);

    // Verify presentations are independent copies (not aliased)
    // Modify right clip's presentation and verify left is unchanged
    timeline.tracks[0].text_clips[1].presentation.style.font_size = 12.0f;
    REQUIRE(timeline.tracks[0].text_clips[0].presentation.style.font_size == 36.0f);
}

TEST_CASE("Failed move_clip preserves presentation and leaves timeline unchanged", "[timeline][text_presentation][editing]")
{
    auto custom_pres = create_test_presentation();
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {
                    TextClip{"clip-1", "First", 0, 2000000, custom_pres},
                    TextClip{"clip-2", "Second", 2000000, 2000000, custom_pres}
                }
            }
        }
    };

    // Save original state
    auto original_timeline = timeline;

    // Try to move clip-1 to a position that would cause overlap with clip-2
    auto result = move_clip(timeline, "clip-1", 2500000);
    REQUIRE(!result);
    REQUIRE(result.error().code == ErrorCode::invalid_argument);

    // Verify timeline is completely unchanged, including presentation
    REQUIRE(timeline.tracks.size() == original_timeline.tracks.size());
    REQUIRE(timeline.tracks[0].text_clips.size() == 2);
    
    for (size_t i = 0; i < timeline.tracks[0].text_clips.size(); ++i) {
        REQUIRE(timeline.tracks[0].text_clips[i].id == original_timeline.tracks[0].text_clips[i].id);
        REQUIRE(timeline.tracks[0].text_clips[i].text == original_timeline.tracks[0].text_clips[i].text);
        REQUIRE(timeline.tracks[0].text_clips[i].timeline_start == original_timeline.tracks[0].text_clips[i].timeline_start);
        REQUIRE(timeline.tracks[0].text_clips[i].duration == original_timeline.tracks[0].text_clips[i].duration);
        assert_presentations_equal(
            timeline.tracks[0].text_clips[i].presentation,
            original_timeline.tracks[0].text_clips[i].presentation
        );
    }
}
