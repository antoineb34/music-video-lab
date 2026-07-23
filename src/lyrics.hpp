#ifndef MVLAB_LYRICS_HPP
#define MVLAB_LYRICS_HPP

#include "result.hpp"
#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <filesystem>

namespace mvlab {

using LyricTime = std::int64_t;

struct LyricLine {
    std::optional<LyricTime> start_time;
    std::string text;
};

enum class LyricsFormat {
    plain_text,
    lrc
};

struct ParsedLyrics {
    LyricsFormat format;
    std::vector<LyricLine> lines;
};

// Parse plain-text lyrics from string contents.
// Splits on Unix or Windows line endings, trims whitespace,
// skips empty lines, and preserves order.
// Rejects input with no usable lyric lines.
Result<ParsedLyrics> parse_plain_text_lyrics(const std::string& contents);

// Parse LRC-format lyrics from string contents.
// Supports [mm:ss], [mm:ss.xx], and [mm:ss.xxx] timestamp forms.
// Allows multiple timestamps before one lyric line.
// Recognizes and ignores specific metadata tags (ar, ti, al, by, offset).
// Rejects malformed syntax, unknown tags, and files with no usable lines.
Result<ParsedLyrics> parse_lrc_lyrics(const std::string& contents);

// Detect lyrics file format by file extension.
// Supports .txt and .lrc (case-insensitive).
// Rejects non-existent files, directories, and unsupported extensions.
Result<LyricsFormat> detect_lyrics_format(const std::filesystem::path& path);

// Parse a lyrics file by detecting format and reading contents.
// Validates file existence and type, reads contents, and calls matching parser.
// Preserves technical errors in Error::details.
Result<ParsedLyrics> parse_lyrics_file(const std::filesystem::path& path);

} // namespace mvlab

#endif // MVLAB_LYRICS_HPP
