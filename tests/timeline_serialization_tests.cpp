#include <catch2/catch_all.hpp>
#include "timeline_serialization.hpp"
#include <limits>

using namespace mvlab;

// ===== Round trips =====

TEST_CASE("Timeline serialization: empty timeline round trip", "[timeline_serialization]")
{
    Timeline timeline{};

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);
    CHECK(json_result.value()["schema_version"] == 2);
    CHECK(json_result.value()["tracks"].is_array());
    CHECK(json_result.value()["tracks"].empty());

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    CHECK(round_trip.value().tracks.empty());
}

TEST_CASE("Timeline serialization: audio track round trip", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Main Audio",
                {MediaClip{"clip-1", "asset-1", 0, 0, 5000000}},
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    REQUIRE(round_trip.value().tracks.size() == 1);
    const auto& track = round_trip.value().tracks[0];
    CHECK(track.id == "track-1");
    CHECK(track.type == TrackType::audio);
    CHECK(track.name == "Main Audio");
    REQUIRE(track.media_clips.size() == 1);
    CHECK(track.media_clips[0].id == "clip-1");
    CHECK(track.media_clips[0].asset_id == "asset-1");
    CHECK(track.media_clips[0].timeline_start == 0);
    CHECK(track.media_clips[0].source_start == 0);
    CHECK(track.media_clips[0].duration == 5000000);
}

TEST_CASE("Timeline serialization: video track round trip", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::video,
                "Background",
                {MediaClip{"clip-1", "asset-2", 1000000, 500000, 2000000}},
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    CHECK(round_trip.value().tracks[0].type == TrackType::video);
    CHECK(round_trip.value().tracks[0].media_clips[0].source_start == 500000);
}

TEST_CASE("Timeline serialization: text track round trip", "[timeline_serialization]")
{
    auto default_presentation = make_text_presentation_preset(TextPresentationPreset::clean_centered).value();
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Hello world", 0, 3000000, default_presentation}}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    REQUIRE(round_trip.value().tracks[0].text_clips.size() == 1);
    CHECK(round_trip.value().tracks[0].text_clips[0].text == "Hello world");
    CHECK(round_trip.value().tracks[0].text_clips[0].duration == 3000000);
}

TEST_CASE("Timeline serialization: mixed timeline round trip", "[timeline_serialization]")
{
    auto default_presentation = make_text_presentation_preset(TextPresentationPreset::clean_centered).value();
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Audio",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            },
            Track{
                "track-2",
                TrackType::video,
                "Video",
                {MediaClip{"clip-2", "asset-2", 0, 0, 1000000}},
                {}
            },
            Track{
                "track-3",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-3", "Verse", 0, 1000000, default_presentation}}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    REQUIRE(round_trip.value().tracks.size() == 3);
    CHECK(round_trip.value().tracks[0].type == TrackType::audio);
    CHECK(round_trip.value().tracks[1].type == TrackType::video);
    CHECK(round_trip.value().tracks[2].type == TrackType::text);
}

TEST_CASE("Timeline serialization: multiple clips round trip", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Audio",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 1000000, 0, 1000000},
                    MediaClip{"clip-3", "asset-3", 2000000, 0, 1000000}
                },
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    REQUIRE(round_trip.value().tracks[0].media_clips.size() == 3);
}

TEST_CASE("Timeline serialization: zero timestamps round trip", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Audio",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);
    CHECK(json_result.value()["tracks"][0]["media_clips"][0]["timeline_start_us"] == 0);
    CHECK(json_result.value()["tracks"][0]["media_clips"][0]["source_start_us"] == 0);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    CHECK(round_trip.value().tracks[0].media_clips[0].timeline_start == 0);
    CHECK(round_trip.value().tracks[0].media_clips[0].source_start == 0);
}

TEST_CASE("Timeline serialization: large valid integer timestamps round trip", "[timeline_serialization]")
{
    TimelineTime large_start = std::numeric_limits<TimelineTime>::max() - 10000000;
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Audio",
                {MediaClip{"clip-1", "asset-1", large_start, 0, 5000000}},
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    CHECK(round_trip.value().tracks[0].media_clips[0].timeline_start == large_start);
}

TEST_CASE("Timeline serialization: exact IDs and ordering preserved", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-b",
                TrackType::audio,
                "B",
                {
                    MediaClip{"clip-y", "asset-1", 2000000, 0, 1000000},
                    MediaClip{"clip-x", "asset-2", 0, 0, 1000000}
                },
                {}
            },
            Track{
                "track-a",
                TrackType::video,
                "A",
                {MediaClip{"clip-z", "asset-3", 0, 0, 1000000}},
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    CHECK(round_trip.value().tracks[0].id == "track-b");
    CHECK(round_trip.value().tracks[1].id == "track-a");
    CHECK(round_trip.value().tracks[0].media_clips[0].id == "clip-y");
    CHECK(round_trip.value().tracks[0].media_clips[1].id == "clip-x");
}

TEST_CASE("Timeline serialization: deterministic repeated serialization", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Audio",
                {MediaClip{"clip-1", "asset-1", 0, 0, 1000000}},
                {}
            }
        }
    };

    auto first = timeline_to_json(timeline);
    auto second = timeline_to_json(timeline);
    REQUIRE(first);
    REQUIRE(second);
    CHECK(first.value().dump() == second.value().dump());
}

TEST_CASE("Timeline serialization: empty clip arrays included explicitly", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{"track-1", TrackType::audio, "Audio", {}, {}}
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);
    CHECK(json_result.value()["tracks"][0].contains("media_clips"));
    CHECK(json_result.value()["tracks"][0]["media_clips"].is_array());
    CHECK(json_result.value()["tracks"][0]["media_clips"].empty());
    CHECK(json_result.value()["tracks"][0].contains("text_clips"));
    CHECK(json_result.value()["tracks"][0]["text_clips"].empty());
}

// ===== Serialization rejects invalid in-memory timelines =====

TEST_CASE("Timeline serialization: invalid timeline rejected before serialization", "[timeline_serialization]")
{
    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::audio,
                "Audio",
                {
                    MediaClip{"clip-1", "asset-1", 0, 0, 1000000},
                    MediaClip{"clip-2", "asset-2", 500000, 0, 1000000}
                },
                {}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE_FALSE(json_result);
    CHECK(json_result.error().code == ErrorCode::invalid_argument);
}

// ===== Deserialization rejects malformed JSON =====

TEST_CASE("Timeline serialization: non-object root rejected", "[timeline_serialization]")
{
    nlohmann::json json = nlohmann::json::array();
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: absent schema_version rejected", "[timeline_serialization]")
{
    nlohmann::json json{{"tracks", nlohmann::json::array()}};
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: wrong-type schema_version rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", "1"},
        {"tracks", nlohmann::json::array()}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: unsupported schema version rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 999},
        {"tracks", nlohmann::json::array()}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::unsupported_schema);
    CHECK(result.error().message.find("999") != std::string::npos);
}

TEST_CASE("Timeline serialization: missing tracks field rejected", "[timeline_serialization]")
{
    nlohmann::json json{{"schema_version", 1}};
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: wrong-type tracks field rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", "not an array"}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: malformed track missing id rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
    REQUIRE(result.error().details.has_value());
    CHECK(result.error().details.value().find("tracks[0]") != std::string::npos);
}

TEST_CASE("Timeline serialization: unknown track type rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "unknown"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: malformed media clip missing asset_id rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"timeline_start_us", 0},
                        {"source_start_us", 0},
                        {"duration_us", 1000000}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
    REQUIRE(result.error().details.has_value());
    CHECK(result.error().details.value().find("media_clips[0]") != std::string::npos);
}

TEST_CASE("Timeline serialization: wrong field type for duration rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-1"},
                        {"timeline_start_us", 0},
                        {"source_start_us", 0},
                        {"duration_us", "not a number"}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: fractional duration value rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-1"},
                        {"timeline_start_us", 0},
                        {"source_start_us", 0},
                        {"duration_us", 1000000.5}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: negative time value rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-1"},
                        {"timeline_start_us", -1},
                        {"source_start_us", 0},
                        {"duration_us", 1000000}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Timeline serialization: overflowing time values rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-1"},
                        {"timeline_start_us", std::numeric_limits<TimelineTime>::max() - 100},
                        {"source_start_us", 0},
                        {"duration_us", 1000}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Timeline serialization: duplicate clip IDs rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-1"},
                        {"timeline_start_us", 0},
                        {"source_start_us", 0},
                        {"duration_us", 1000000}
                    },
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-2"},
                        {"timeline_start_us", 1000000},
                        {"source_start_us", 0},
                        {"duration_us", 1000000}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Timeline serialization: overlapping clips rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"asset_id", "asset-1"},
                        {"timeline_start_us", 0},
                        {"source_start_us", 0},
                        {"duration_us", 1000000}
                    },
                    nlohmann::json{
                        {"id", "clip-2"},
                        {"asset_id", "asset-2"},
                        {"timeline_start_us", 500000},
                        {"source_start_us", 0},
                        {"duration_us", 1000000}
                    }
                })},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Timeline serialization: wrong clip type for track rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "Audio"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"text", "Hello"},
                        {"timeline_start_us", 0},
                        {"duration_us", 1000000}
                    }
                })}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

TEST_CASE("Timeline serialization: duplicate track IDs rejected", "[timeline_serialization]")
{
    nlohmann::json json{
        {"schema_version", 1},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "audio"},
                {"name", "A"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array()}
            },
            nlohmann::json{
                {"id", "track-1"},
                {"type", "video"},
                {"name", "B"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array()}
            }
        })}
    };
    auto result = timeline_from_json(json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::invalid_argument);
}

// ===== Text presentation tests =====

TEST_CASE("Timeline serialization: text clip with default presentation", "[timeline_serialization][text_presentation]")
{
    auto default_preset = make_text_presentation_preset(TextPresentationPreset::clean_centered).value();

    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Hello", 0, 2000000, default_preset}}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto json = json_result.value();
    REQUIRE(json["tracks"].size() == 1);
    REQUIRE(json["tracks"][0]["text_clips"].size() == 1);
    const auto& clip_json = json["tracks"][0]["text_clips"][0];
    REQUIRE(clip_json.contains("presentation"));
    REQUIRE(clip_json["presentation"].contains("schema_version"));
    CHECK(clip_json["presentation"]["schema_version"] == 1);
    REQUIRE(clip_json["presentation"].contains("style"));
    REQUIRE(clip_json["presentation"].contains("entrance"));
    REQUIRE(clip_json["presentation"].contains("exit"));

    auto round_trip = timeline_from_json(json);
    REQUIRE(round_trip);
    REQUIRE(round_trip.value().tracks[0].text_clips.size() == 1);
    const auto& loaded_clip = round_trip.value().tracks[0].text_clips[0];
    CHECK(loaded_clip.text == "Hello");
    CHECK(loaded_clip.presentation.style.font_size == default_preset.style.font_size);
    CHECK(loaded_clip.presentation.entrance.kind == TextAnimationKind::none);
    CHECK(loaded_clip.presentation.exit.kind == TextAnimationKind::none);
}

TEST_CASE("Timeline serialization: text clip with custom presentation", "[timeline_serialization][text_presentation]")
{
    auto custom_preset = make_text_presentation_preset(TextPresentationPreset::cinematic_fade).value();

    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Fade in", 0, 3000000, custom_preset}}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);
    REQUIRE(round_trip.value().tracks[0].text_clips.size() == 1);
    const auto& loaded_clip = round_trip.value().tracks[0].text_clips[0];
    CHECK(loaded_clip.text == "Fade in");
    CHECK(loaded_clip.presentation.entrance.kind == TextAnimationKind::fade);
    CHECK(loaded_clip.presentation.entrance.duration_us == 500000);
    CHECK(loaded_clip.presentation.exit.kind == TextAnimationKind::fade);
    CHECK(loaded_clip.presentation.exit.duration_us == 500000);
}

TEST_CASE("Timeline serialization: schema v1 migration assigns default presentation", "[timeline_serialization][text_presentation]")
{
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

    auto result = timeline_from_json(v1_json);
    REQUIRE(result);
    REQUIRE(result.value().tracks[0].text_clips.size() == 1);

    const auto& clip = result.value().tracks[0].text_clips[0];
    CHECK(clip.text == "Migrated text");
    CHECK(clip.presentation.entrance.kind == TextAnimationKind::none);
    CHECK(clip.presentation.exit.kind == TextAnimationKind::none);
    CHECK(clip.presentation.style.horizontal_alignment == TextHorizontalAlignment::center);
    CHECK(clip.presentation.style.vertical_alignment == TextVerticalAlignment::center);
}

TEST_CASE("Timeline serialization: schema v2 rejects missing presentation", "[timeline_serialization][text_presentation]")
{
    nlohmann::json v2_json{
        {"schema_version", 2},
        {"tracks", nlohmann::json::array({
            nlohmann::json{
                {"id", "track-1"},
                {"type", "text"},
                {"name", "Lyrics"},
                {"media_clips", nlohmann::json::array()},
                {"text_clips", nlohmann::json::array({
                    nlohmann::json{
                        {"id", "clip-1"},
                        {"text", "Missing presentation"},
                        {"timeline_start_us", 0},
                        {"duration_us", 2000000}
                    }
                })}
            }
        })}
    };

    auto result = timeline_from_json(v2_json);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == ErrorCode::serialization_error);
}

TEST_CASE("Timeline serialization: presentation round trip preserves all fields", "[timeline_serialization][text_presentation]")
{
    TextPresentation custom_presentation;
    custom_presentation.style.font_family = "Serif";
    custom_presentation.style.font_size = 36.0f;
    custom_presentation.style.bold = true;
    custom_presentation.style.italic = true;
    custom_presentation.style.fill_color = {1.0f, 0.5f, 0.2f, 1.0f};
    custom_presentation.style.outline_color = {0.1f, 0.1f, 0.1f, 0.5f};
    custom_presentation.style.outline_width = 3.5f;
    custom_presentation.style.position_x = 0.3f;
    custom_presentation.style.position_y = 0.7f;
    custom_presentation.style.horizontal_alignment = TextHorizontalAlignment::left;
    custom_presentation.style.vertical_alignment = TextVerticalAlignment::top;
    custom_presentation.entrance = {TextAnimationKind::slide_from_left, 400000, EasingKind::ease_in};
    custom_presentation.exit = {TextAnimationKind::slide_from_right, 300000, EasingKind::ease_out};

    Timeline timeline{
        {
            Track{
                "track-1",
                TrackType::text,
                "Lyrics",
                {},
                {TextClip{"clip-1", "Styled text", 0, 2000000, custom_presentation}}
            }
        }
    };

    auto json_result = timeline_to_json(timeline);
    REQUIRE(json_result);

    auto round_trip = timeline_from_json(json_result.value());
    REQUIRE(round_trip);

    const auto& loaded = round_trip.value().tracks[0].text_clips[0];
    CHECK(loaded.presentation.style.font_family == "Serif");
    CHECK(loaded.presentation.style.font_size == 36.0f);
    CHECK(loaded.presentation.style.bold == true);
    CHECK(loaded.presentation.style.italic == true);
    CHECK(loaded.presentation.style.fill_color.red == 1.0f);
    CHECK(loaded.presentation.style.fill_color.green == 0.5f);
    CHECK(loaded.presentation.style.fill_color.blue == 0.2f);
    CHECK(loaded.presentation.style.outline_color.alpha == 0.5f);
    CHECK(loaded.presentation.style.outline_width == 3.5f);
    CHECK(loaded.presentation.style.position_x == 0.3f);
    CHECK(loaded.presentation.style.position_y == 0.7f);
    CHECK(loaded.presentation.style.horizontal_alignment == TextHorizontalAlignment::left);
    CHECK(loaded.presentation.style.vertical_alignment == TextVerticalAlignment::top);
    CHECK(loaded.presentation.entrance.kind == TextAnimationKind::slide_from_left);
    CHECK(loaded.presentation.entrance.duration_us == 400000);
    CHECK(loaded.presentation.entrance.easing == EasingKind::ease_in);
    CHECK(loaded.presentation.exit.kind == TextAnimationKind::slide_from_right);
    CHECK(loaded.presentation.exit.duration_us == 300000);
    CHECK(loaded.presentation.exit.easing == EasingKind::ease_out);
}
