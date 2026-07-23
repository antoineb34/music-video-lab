#ifndef MVLAB_TIMELINE_SERIALIZATION_HPP
#define MVLAB_TIMELINE_SERIALIZATION_HPP

#include "result.hpp"
#include "timeline.hpp"
#include <nlohmann/json.hpp>
#include <cstdint>

namespace mvlab {

// Current schema version written by timeline_to_json() and the newest
// version accepted by timeline_from_json(). Bump this and add a parallel
// parse_timeline_vN() dispatch case when the on-disk shape changes.
inline constexpr std::uint32_t current_timeline_schema_version = 1;

// Serializes a Timeline into its versioned JSON representation (see
// timeline_serialization.cpp for the exact shape). Validates the timeline
// first via validate_timeline(); returns its error (invalid_argument) if
// the timeline itself is not internally valid.
Result<nlohmann::json> timeline_to_json(const Timeline& timeline);

// Parses a versioned JSON representation into a Timeline.
// Returns:
// - serialization_error for structurally malformed JSON (non-object root,
//   missing fields, wrong field types, unknown track type)
// - unsupported_schema for a schema_version this build does not recognize
// - invalid_argument if the parsed timeline fails validate_timeline()
//   (duplicate IDs, overlaps, negative/overflowing times, type mismatches)
// Never partially repairs malformed data.
Result<Timeline> timeline_from_json(const nlohmann::json& json);

} // namespace mvlab

#endif // MVLAB_TIMELINE_SERIALIZATION_HPP
