#include "lyrics.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>

namespace mvlab {

namespace {

// Helper: Trim leading and trailing whitespace from a string
std::string trim(const std::string& str)
{
    auto start = str.begin();
    while (start != str.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }

    auto end = str.end();
    do {
        --end;
    } while (std::distance(start, end) > 0 && std::isspace(static_cast<unsigned char>(*end)));

    return std::string(start, end + 1);
}

// Helper: Check if string is all whitespace
bool is_whitespace_only(const std::string& str)
{
    return std::all_of(str.begin(), str.end(), [](unsigned char c) {
        return std::isspace(c);
    });
}

// Helper: Convert to lowercase
std::string to_lowercase(const std::string& str)
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

// Helper: Split string by Unix or Windows line endings
std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string line;

    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                lines.push_back(line);
                line.clear();
                ++i;
            } else {
                lines.push_back(line);
                line.clear();
            }
        } else if (text[i] == '\n') {
            lines.push_back(line);
            line.clear();
        } else {
            line += text[i];
        }
    }

    if (!line.empty()) {
        lines.push_back(line);
    }

    return lines;
}

// Helper: Parse a timestamp like [mm:ss], [mm:ss.xx], or [mm:ss.xxx]
// Returns milliseconds as int64_t, or -1 on parse error
LyricTime parse_timestamp(const std::string& timestamp_str)
{
    if (timestamp_str.size() < 6) return -1;
    if (timestamp_str[0] != '[' || timestamp_str[timestamp_str.size() - 1] != ']') return -1;

    std::string inner = timestamp_str.substr(1, timestamp_str.size() - 2);
    size_t colon_pos = inner.find(':');
    if (colon_pos == std::string::npos) return -1;

    std::string min_str = inner.substr(0, colon_pos);
    std::string sec_frac_str = inner.substr(colon_pos + 1);

    if (min_str.empty() || sec_frac_str.empty()) return -1;
    if (!std::all_of(min_str.begin(), min_str.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return -1;
    }

    size_t dot_pos = sec_frac_str.find('.');
    std::string sec_str, frac_str;

    if (dot_pos == std::string::npos) {
        sec_str = sec_frac_str;
        frac_str = "";
    } else {
        sec_str = sec_frac_str.substr(0, dot_pos);
        frac_str = sec_frac_str.substr(dot_pos + 1);
    }

    if (sec_str.size() != 2) return -1;
    if (!std::all_of(sec_str.begin(), sec_str.end(), [](unsigned char c) { return std::isdigit(c); })) {
        return -1;
    }

    if (!frac_str.empty() && !std::all_of(frac_str.begin(), frac_str.end(), [](unsigned char c) {
        return std::isdigit(c);
    })) {
        return -1;
    }

    if (frac_str.size() != 2 && frac_str.size() != 3 && !frac_str.empty()) {
        return -1;
    }

    try {
        long minutes = std::stol(min_str);
        int seconds = std::stoi(sec_str);
        int frac = 0;

        if (seconds < 0 || seconds > 59) return -1;
        if (minutes < 0 || minutes > std::numeric_limits<long>::max() / 60000) return -1;

        if (!frac_str.empty()) {
            frac = std::stoi(frac_str);
            if (frac_str.size() == 2) {
                frac = frac * 10;
            }
        }

        if (frac < 0 || frac > 999) return -1;

        LyricTime total_ms = minutes * 60000LL + seconds * 1000LL + frac;
        if (total_ms < 0) return -1;

        return total_ms;
    } catch (...) {
        return -1;
    }
}

// Helper: Extract all timestamps from the start of a line
// Returns vector of timestamps and the remaining line text
struct ExtractResult {
    std::vector<LyricTime> timestamps;
    std::string remaining_text;
};

ExtractResult extract_timestamps(const std::string& line)
{
    ExtractResult result;
    size_t pos = 0;

    while (pos < line.size() && line[pos] == '[') {
        size_t close = line.find(']', pos);
        if (close == std::string::npos) break;

        std::string bracket_content = line.substr(pos, close - pos + 1);

        if (bracket_content.size() >= 6 && bracket_content[0] == '[' && bracket_content[bracket_content.size() - 1] == ']') {
            LyricTime ts = parse_timestamp(bracket_content);
            if (ts >= 0) {
                result.timestamps.push_back(ts);
                pos = close + 1;
                continue;
            }
        }

        break;
    }

    result.remaining_text = line.substr(pos);
    return result;
}

// Helper: Check if a line is a recognized metadata tag
bool is_recognized_metadata(const std::string& line)
{
    if (line.size() < 4 || line[0] != '[' || line[line.size() - 1] != ']') return false;

    std::string inner = line.substr(1, line.size() - 2);
    size_t colon_pos = inner.find(':');
    if (colon_pos == std::string::npos) return false;

    std::string tag = inner.substr(0, colon_pos);
    return tag == "ar" || tag == "ti" || tag == "al" || tag == "by" || tag == "offset";
}

// Helper: Check if a line contains an unknown bracket tag (not a timestamp)
bool contains_unknown_bracket_tag(const std::string& line)
{
    if (line.empty() || line[0] != '[') return false;

    size_t close = line.find(']');
    if (close == std::string::npos) return false;

    std::string bracket_part = line.substr(0, close + 1);

    if (bracket_part.size() < 4) return false;

    std::string inner = bracket_part.substr(1, bracket_part.size() - 2);
    size_t colon_pos = inner.find(':');

    if (colon_pos == std::string::npos) {
        return true;
    }

    std::string potential_tag = inner.substr(0, colon_pos);

    if (potential_tag == "ar" || potential_tag == "ti" || potential_tag == "al" ||
        potential_tag == "by" || potential_tag == "offset") {
        return false;
    }

    if (std::all_of(potential_tag.begin(), potential_tag.end(), [](unsigned char c) {
        return std::isdigit(c);
    })) {
        return false;
    }

    return true;
}

} // anonymous namespace

Result<ParsedLyrics> parse_plain_text_lyrics(const std::string& contents)
{
    ParsedLyrics result;
    result.format = LyricsFormat::plain_text;

    std::vector<std::string> lines = split_lines(contents);

    for (const auto& line : lines) {
        std::string trimmed = trim(line);
        if (!trimmed.empty() && !is_whitespace_only(trimmed)) {
            LyricLine lyric_line;
            lyric_line.start_time = std::nullopt;
            lyric_line.text = trimmed;
            result.lines.push_back(lyric_line);
        }
    }

    if (result.lines.empty()) {
        return Error{
            ErrorCode::malformed_external_output,
            "Plain-text lyrics must contain at least one non-empty line",
            std::nullopt
        };
    }

    return result;
}

Result<ParsedLyrics> parse_lrc_lyrics(const std::string& contents)
{
    ParsedLyrics result;
    result.format = LyricsFormat::lrc;

    std::vector<std::string> lines = split_lines(contents);

    for (const auto& raw_line : lines) {
        std::string line = trim(raw_line);

        if (line.empty()) continue;

        if (line[0] == '[') {
            if (is_recognized_metadata(line)) {
                continue;
            }

            if (contains_unknown_bracket_tag(line)) {
                return Error{
                    ErrorCode::malformed_external_output,
                    "Unknown metadata tag in LRC file",
                    std::nullopt
                };
            }
        }

        ExtractResult extracted = extract_timestamps(line);
        std::string lyric_text = trim(extracted.remaining_text);

        if (!extracted.timestamps.empty()) {
            if (lyric_text.empty()) {
                return Error{
                    ErrorCode::malformed_external_output,
                    "LRC entry with timestamp must have non-empty lyric text",
                    std::nullopt
                };
            }

            for (LyricTime ts : extracted.timestamps) {
                LyricLine lyric_line;
                lyric_line.start_time = ts;
                lyric_line.text = lyric_text;
                result.lines.push_back(lyric_line);
            }
        }
    }

    if (result.lines.empty()) {
        return Error{
            ErrorCode::malformed_external_output,
            "LRC lyrics must contain at least one timestamped entry",
            std::nullopt
        };
    }

    return result;
}

Result<LyricsFormat> detect_lyrics_format(const std::filesystem::path& path)
{
    if (path.empty()) {
        return Error{
            ErrorCode::invalid_argument,
            "File path must not be empty",
            std::nullopt
        };
    }

    if (!std::filesystem::exists(path)) {
        return Error{
            ErrorCode::file_not_found,
            "Lyrics file not found",
            std::nullopt
        };
    }

    if (!std::filesystem::is_regular_file(path)) {
        return Error{
            ErrorCode::invalid_argument,
            "Path must be a regular file",
            std::nullopt
        };
    }

    std::string extension = to_lowercase(path.extension().string());

    if (extension == ".txt") {
        return LyricsFormat::plain_text;
    } else if (extension == ".lrc") {
        return LyricsFormat::lrc;
    } else {
        return Error{
            ErrorCode::unsupported_format,
            "Unsupported lyrics file extension: " + path.extension().string(),
            std::nullopt
        };
    }
}

Result<ParsedLyrics> parse_lyrics_file(const std::filesystem::path& path)
{
    auto format_result = detect_lyrics_format(path);
    if (!format_result) {
        return format_result.error();
    }

    LyricsFormat format = format_result.value();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return Error{
            ErrorCode::filesystem_error,
            "Failed to open lyrics file",
            "Could not open file for reading"
        };
    }

    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    if (format == LyricsFormat::plain_text) {
        return parse_plain_text_lyrics(contents);
    } else {
        return parse_lrc_lyrics(contents);
    }
}

} // namespace mvlab
