#ifndef MVLAB_ERROR_HPP
#define MVLAB_ERROR_HPP

#include <string>
#include <optional>

namespace mvlab {

enum class ErrorCode {
    none,
    invalid_argument,
    file_not_found,
    permission_denied,
    invalid_media,
    external_tool_unavailable,
    external_tool_failed,
    malformed_external_output,
    malformed_project,
    unsupported_format,
    unsupported_schema,
    filesystem_error,
    serialization_error,
    internal_error
};

// Stable machine-readable name for each ErrorCode (e.g. "file_not_found").
// Not a user-facing sentence; Error::message serves that purpose.
std::string to_string(ErrorCode code);

struct Error {
    ErrorCode code = ErrorCode::none;
    std::string message;                 // concise, user-facing
    std::optional<std::string> details;  // technical diagnostics (stderr, parser output, etc.)
};

// Maps an ErrorCode to a stable CLI process exit code. Kept as one
// function so the policy has a single source of truth and can be tested
// exhaustively. See AGENTS.md for the documented category ranges.
int exit_code_for(ErrorCode code);

} // namespace mvlab

#endif // MVLAB_ERROR_HPP
