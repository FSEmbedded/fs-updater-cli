#pragma once

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>

/* Pure scanners for the parser: strict decimal uint32, single-char state
 * syntax, exact long-option match (no abbreviation). */
namespace cli
{
    /* Strict decimal std::uint32_t parse: digits only (no sign, no "0x", no
     * leading/trailing whitespace, no partial-token match), errno/range/
     * full-token checked. Rejects everything above UINT32_MAX. */
    [[nodiscard]] inline std::optional<std::uint32_t> parse_u32(const char *text) noexcept
    {
        if (text == nullptr || text[0] == '\0') return std::nullopt;

        for (const char *p = text; *p != '\0'; ++p)
        {
            if (std::isdigit(static_cast<unsigned char>(*p)) == 0) return std::nullopt;
        }

        errno = 0;
        char *end = nullptr;
        const unsigned long value = std::strtoul(text, &end, 10);

        if (errno == ERANGE || value > 0xFFFFFFFFul || *end != '\0') return std::nullopt;

        return static_cast<std::uint32_t>(value);
    }

    /* Single-character syntax check only — no A/B value constraint here:
     * a parser-level constraint on the state letter would move the handler's
     * rc 53 to the parser's rc 1, which is pinned against. */
    [[nodiscard]] inline std::optional<char> parse_state_char(const char *text) noexcept
    {
        if (text == nullptr || text[0] == '\0' || text[1] != '\0') return std::nullopt;

        return text[0];
    }

    /* True iff token is exactly "--name" or "--name=...". No abbreviation:
     * a prefix match would break silently once a later flag makes it
     * ambiguous. token must already be known to start with "--". */
    [[nodiscard]] inline bool exact_long_match(const char *token, const char *name) noexcept
    {
        const char *given = token + 2; /* past "--" */
        const std::size_t n = std::strlen(name);
        return (std::strncmp(given, name, n) == 0)
            && (given[n] == '\0' || given[n] == '=');
    }
}
