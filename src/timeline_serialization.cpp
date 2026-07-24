#include "timeline_serialization.hpp"
#include "text_presentation.hpp"
#include <stdexcept>
#include <string>

namespace mvlab {

namespace {

// ===== Serialization (Timeline -> JSON) =====

nlohmann::json media_clip_to_json(const MediaClip& clip)
{
    return nlohmann::json{
        {"id", clip.id},
        {"asset_id", clip.asset_id},
        {"timeline_start_us", clip.timeline_start},
        {"source_start_us", clip.source_start},
        {"duration_us", clip.duration}
    };
}

nlohmann::json text_clip_to_json(const TextClip& clip)
{
    auto presentation_json = text_presentation_to_json(clip.presentation);
    if (!presentation_json) {
        throw std::runtime_error(
            "Failed to serialize text presentation: " + presentation_json.error().message);
    }

    return nlohmann::json{
        {"id", clip.id},
        {"text", clip.text},
        {"timeline_start_us", clip.timeline_start},
        {"duration_us", clip.duration},
        {"presentation", presentation_json.value()}
    };
}

nlohmann::json track_to_json(const Track& track)
{
    nlohmann::json media_clips = nlohmann::json::array();
    for (const auto& clip : track.media_clips) {
        media_clips.push_back(media_clip_to_json(clip));
    }

    nlohmann::json text_clips = nlohmann::json::array();
    for (const auto& clip : track.text_clips) {
        text_clips.push_back(text_clip_to_json(clip));
    }

    return nlohmann::json{
        {"id", track.id},
        {"type", to_string(track.type)},
        {"name", track.name},
        {"media_clips", media_clips},
        {"text_clips", text_clips}
    };
}

// ===== Deserialization (JSON -> Timeline) =====
//
// Every helper below throws std::runtime_error with a short, specific
// message on failure. Callers add positional context (tracks[i],
// media_clips[j], ...) as the exception unwinds, so a single failure deep
// inside a clip surfaces as a full field path by the time it reaches
// timeline_from_json(). Nothing here silently defaults or repairs bad data.

std::string require_string_field(const nlohmann::json& j, const std::string& field)
{
    if (!j.contains(field)) {
        throw std::runtime_error("missing required field '" + field + "'");
    }
    const auto& value = j.at(field);
    if (!value.is_string()) {
        throw std::runtime_error("field '" + field + "' must be a string");
    }
    return value.get<std::string>();
}

TimelineTime require_time_field(const nlohmann::json& j, const std::string& field)
{
    if (!j.contains(field)) {
        throw std::runtime_error("missing required field '" + field + "'");
    }
    const auto& value = j.at(field);
    if (!value.is_number_integer()) {
        throw std::runtime_error("field '" + field + "' must be an integer");
    }
    try {
        return value.get<TimelineTime>();
    } catch (const nlohmann::json::exception&) {
        throw std::runtime_error("field '" + field + "' is out of range");
    }
}

MediaClip media_clip_from_json(const nlohmann::json& j)
{
    if (!j.is_object()) {
        throw std::runtime_error("media clip must be an object");
    }

    MediaClip clip;
    clip.id = require_string_field(j, "id");
    clip.asset_id = require_string_field(j, "asset_id");
    clip.timeline_start = require_time_field(j, "timeline_start_us");
    clip.source_start = require_time_field(j, "source_start_us");
    clip.duration = require_time_field(j, "duration_us");
    return clip;
}

TextClip text_clip_from_json_v1(const nlohmann::json& j)
{
    if (!j.is_object()) {
        throw std::runtime_error("text clip must be an object");
    }

    TextClip clip;
    clip.id = require_string_field(j, "id");
    clip.text = require_string_field(j, "text");
    clip.timeline_start = require_time_field(j, "timeline_start_us");
    clip.duration = require_time_field(j, "duration_us");

    // Use default clean_centered presentation for migrated v1 clips
    auto default_presentation = make_text_presentation_preset(
        TextPresentationPreset::clean_centered);
    if (!default_presentation) {
        throw std::runtime_error(
            "Failed to create default presentation: " + default_presentation.error().message);
    }
    clip.presentation = default_presentation.value();

    return clip;
}

TextClip text_clip_from_json_v2(const nlohmann::json& j)
{
    if (!j.is_object()) {
        throw std::runtime_error("text clip must be an object");
    }

    TextClip clip;
    clip.id = require_string_field(j, "id");
    clip.text = require_string_field(j, "text");
    clip.timeline_start = require_time_field(j, "timeline_start_us");
    clip.duration = require_time_field(j, "duration_us");

    if (!j.contains("presentation")) {
        throw std::runtime_error("missing required field 'presentation'");
    }

    auto presentation_result = text_presentation_from_json(j.at("presentation"));
    if (!presentation_result) {
        throw std::runtime_error(
            "Failed to parse presentation: " + presentation_result.error().message);
    }

    clip.presentation = presentation_result.value();
    return clip;
}

Track track_from_json_base(
    const nlohmann::json& j,
    std::size_t track_index,
    bool expect_presentation)
{
    try {
        if (!j.is_object()) {
            throw std::runtime_error("must be an object");
        }

        Track track;
        track.id = require_string_field(j, "id");
        track.name = require_string_field(j, "name");

        std::string type_str = require_string_field(j, "type");
        auto type_result = parse_track_type(type_str);
        if (!type_result) {
            throw std::runtime_error("unknown track type '" + type_str + "'");
        }
        track.type = type_result.value();

        if (!j.contains("media_clips") || !j["media_clips"].is_array()) {
            throw std::runtime_error("field 'media_clips' must be an array");
        }
        std::size_t clip_index = 0;
        for (const auto& clip_json : j["media_clips"]) {
            try {
                track.media_clips.push_back(media_clip_from_json(clip_json));
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    "media_clips[" + std::to_string(clip_index) + "]: " + e.what());
            }
            ++clip_index;
        }

        if (!j.contains("text_clips") || !j["text_clips"].is_array()) {
            throw std::runtime_error("field 'text_clips' must be an array");
        }
        clip_index = 0;
        for (const auto& clip_json : j["text_clips"]) {
            try {
                if (expect_presentation) {
                    track.text_clips.push_back(text_clip_from_json_v2(clip_json));
                } else {
                    track.text_clips.push_back(text_clip_from_json_v1(clip_json));
                }
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    "text_clips[" + std::to_string(clip_index) + "]: " + e.what());
            }
            ++clip_index;
        }

        return track;
    } catch (const std::exception& e) {
        throw std::runtime_error(
            "tracks[" + std::to_string(track_index) + "]: " + std::string(e.what()));
    }
}

// Parses the schema-version-1 timeline body. Text clips receive the default
// clean_centered presentation during migration.
Result<Timeline> parse_timeline_v1(const nlohmann::json& json)
{
    if (!json.contains("tracks") || !json["tracks"].is_array()) {
        return Error{
            ErrorCode::serialization_error,
            "Malformed timeline JSON",
            std::string("field 'tracks' must be an array")
        };
    }

    Timeline timeline;
    std::size_t track_index = 0;
    for (const auto& track_json : json["tracks"]) {
        try {
            timeline.tracks.push_back(track_from_json_base(track_json, track_index, false));
        } catch (const std::exception& e) {
            return Error{
                ErrorCode::serialization_error,
                "Malformed timeline JSON",
                std::string(e.what())
            };
        }
        ++track_index;
    }

    auto validation = validate_timeline(timeline);
    if (!validation) {
        return validation.error();
    }

    return timeline;
}

// Parses the schema-version-2 timeline body. Text clips must include
// a valid presentation object.
Result<Timeline> parse_timeline_v2(const nlohmann::json& json)
{
    if (!json.contains("tracks") || !json["tracks"].is_array()) {
        return Error{
            ErrorCode::serialization_error,
            "Malformed timeline JSON",
            std::string("field 'tracks' must be an array")
        };
    }

    Timeline timeline;
    std::size_t track_index = 0;
    for (const auto& track_json : json["tracks"]) {
        try {
            timeline.tracks.push_back(track_from_json_base(track_json, track_index, true));
        } catch (const std::exception& e) {
            return Error{
                ErrorCode::serialization_error,
                "Malformed timeline JSON",
                std::string(e.what())
            };
        }
        ++track_index;
    }

    auto validation = validate_timeline(timeline);
    if (!validation) {
        return validation.error();
    }

    return timeline;
}

} // namespace

Result<nlohmann::json> timeline_to_json(const Timeline& timeline)
{
    auto validation = validate_timeline(timeline);
    if (!validation) {
        return validation.error();
    }

    nlohmann::json tracks = nlohmann::json::array();
    for (const auto& track : timeline.tracks) {
        tracks.push_back(track_to_json(track));
    }

    return nlohmann::json{
        {"schema_version", current_timeline_schema_version},
        {"tracks", tracks}
    };
}

Result<Timeline> timeline_from_json(const nlohmann::json& json)
{
    if (!json.is_object()) {
        return Error{
            ErrorCode::serialization_error,
            "Malformed timeline JSON",
            std::string("root must be an object")
        };
    }

    if (!json.contains("schema_version")) {
        return Error{
            ErrorCode::serialization_error,
            "Malformed timeline JSON",
            std::string("missing required field 'schema_version'")
        };
    }

    if (!json["schema_version"].is_number_integer()) {
        return Error{
            ErrorCode::serialization_error,
            "Malformed timeline JSON",
            std::string("field 'schema_version' must be an integer")
        };
    }

    std::uint32_t schema_version = 0;
    try {
        schema_version = json.at("schema_version").get<std::uint32_t>();
    } catch (const nlohmann::json::exception& e) {
        return Error{
            ErrorCode::serialization_error,
            "Malformed timeline JSON",
            std::string("field 'schema_version' is out of range: ") + e.what()
        };
    }

    // Version dispatch: add a case per supported schema version so old
    // readers keep working as the format evolves.
    switch (schema_version) {
        case 1:
            return parse_timeline_v1(json);
        case 2:
            return parse_timeline_v2(json);
        default:
            return Error{
                ErrorCode::unsupported_schema,
                "Unsupported timeline schema version: " + std::to_string(schema_version) +
                    " (expected " + std::to_string(current_timeline_schema_version) + ")",
                std::nullopt
            };
    }
}

} // namespace mvlab
