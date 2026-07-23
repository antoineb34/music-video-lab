#include "error.hpp"

namespace mvlab {

std::string to_string(ErrorCode code)
{
    switch (code) {
        case ErrorCode::none:                       return "none";
        case ErrorCode::invalid_argument:            return "invalid_argument";
        case ErrorCode::file_not_found:              return "file_not_found";
        case ErrorCode::permission_denied:           return "permission_denied";
        case ErrorCode::invalid_media:                return "invalid_media";
        case ErrorCode::external_tool_unavailable:   return "external_tool_unavailable";
        case ErrorCode::external_tool_failed:        return "external_tool_failed";
        case ErrorCode::malformed_external_output:   return "malformed_external_output";
        case ErrorCode::malformed_project:           return "malformed_project";
        case ErrorCode::unsupported_format:          return "unsupported_format";
        case ErrorCode::unsupported_schema:          return "unsupported_schema";
        case ErrorCode::filesystem_error:            return "filesystem_error";
        case ErrorCode::serialization_error:         return "serialization_error";
        case ErrorCode::internal_error:              return "internal_error";
    }
    return "unknown_error";
}

int exit_code_for(ErrorCode code)
{
    // Exit code policy (documented in AGENTS.md):
    //   0  success
    //   2  invalid command or argument
    //   3  file, path, or permission problem
    //   4  external dependency or process failure
    //   5  malformed or unsupported media/project data
    //  10  internal failure
    //
    // filesystem_error is grouped with the "file/path/permission" category
    // (3) since it always originates from a filesystem operation on a path.
    // serialization_error is grouped with "malformed/unsupported data" (5)
    // since it reflects a problem producing/consuming project data, the
    // same user-facing category as malformed_project.
    switch (code) {
        case ErrorCode::none:
            return 0;
        case ErrorCode::invalid_argument:
            return 2;
        case ErrorCode::file_not_found:
        case ErrorCode::permission_denied:
        case ErrorCode::filesystem_error:
            return 3;
        case ErrorCode::external_tool_unavailable:
        case ErrorCode::external_tool_failed:
            return 4;
        case ErrorCode::invalid_media:
        case ErrorCode::malformed_external_output:
        case ErrorCode::malformed_project:
        case ErrorCode::unsupported_format:
        case ErrorCode::unsupported_schema:
        case ErrorCode::serialization_error:
            return 5;
        case ErrorCode::internal_error:
            return 10;
    }
    return 10;
}

} // namespace mvlab
