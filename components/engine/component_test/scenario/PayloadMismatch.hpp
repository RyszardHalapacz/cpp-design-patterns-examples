#pragma once
#include <algorithm>    // std::min
#include <any>
#include <cstddef>      // std::size_t
#include <cstdlib>      // std::free()
#include <expected>
#include <string>
#include <string_view>
#include <utility>      // std::move
#include <vector>
#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#endif

// ─── FieldMismatch ────────────────────────────────────────────────────────────
// One mismatching field inside a payload struct.
// Both values are pre-formatted strings — no domain type leaks out.
struct FieldMismatch {
    std::string path;      // "commandName", "data[2]", "data.size", "vectorCount"
    std::string expected;  // formatted expected value
    std::string actual;    // formatted actual value
};

// ─── PayloadMismatch ──────────────────────────────────────────────────────────
// Returned by a payloadMatcher on failure.
// expectedType == actualType → correct type, field-level diffs in `fields`
// expectedType != actualType → type mismatch; fields is empty
struct PayloadMismatch {
    std::string                expectedType;  // "CommandHistory"
    std::string                actualType;    // "EngineSnapshot" or "(empty)"
    std::vector<FieldMismatch> fields;
};

// ─── PayloadMatchResult ───────────────────────────────────────────────────────
using PayloadMatchResult = std::expected<void, PayloadMismatch>;

// ─── Convenience factories ────────────────────────────────────────────────────

inline PayloadMatchResult payloadOk() { return {}; }

inline PayloadMatchResult payloadWrongType(std::string expectedType,
                                           std::string actualType)
{
    return std::unexpected(PayloadMismatch{
        .expectedType = std::move(expectedType),
        .actualType   = std::move(actualType),
        .fields       = {}
    });
}

// Note: expectedType passed by const ref — copied into both struct fields.
// Avoids use-after-move bug.
inline PayloadMatchResult payloadFieldMismatch(const std::string&         expectedType,
                                               std::vector<FieldMismatch> fields)
{
    return std::unexpected(PayloadMismatch{
        .expectedType = expectedType,       // copy 1
        .actualType   = expectedType,       // copy 2 — same type, field-level diff
        .fields       = std::move(fields)
    });
}

// ─── anyTypeName() ────────────────────────────────────────────────────────────
// Returns a human-readable type name of what a std::any holds.
// Demangles with abi::__cxa_demangle on GCC/Clang.
// Returns "(empty)" if not engaged.
inline std::string anyTypeName(const std::any& a)
{
    if (!a.has_value()) return "(empty)";
#if defined(__GNUC__) || defined(__clang__)
    int   status    = 0;
    char* demangled = abi::__cxa_demangle(a.type().name(), nullptr, nullptr, &status);
    if (status == 0 && demangled) {
        std::string name(demangled); std::free(demangled); return name;
    }
#endif
    return a.type().name();
}

// ─── formatVector() ───────────────────────────────────────────────────────────
// Formats std::vector<int> as "[1, 2, 3]".
inline std::string formatVector(const std::vector<int>& v)
{
    if (v.empty()) return "[]";
    std::string out = "[";
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (i) out += ", ";
        out += std::to_string(v[i]);
    }
    return out + "]";
}

// ─── diffVector() ─────────────────────────────────────────────────────────────
// Per-element diff of two int vectors. Produces one FieldMismatch per difference:
//   path.size   — when sizes differ
//   path[i]     — when element i differs (within common length)
//   path[i]     — expected N, actual <missing>   (actual is shorter)
//   path[i]     — expected <missing>, actual N   (expected is shorter)
//
// Caller provides path prefix, e.g., "data" produces "data.size", "data[0]", ...
// All differences collected — no early exit on first mismatch.
inline std::vector<FieldMismatch> diffVector(std::string_view            path,
                                             const std::vector<int>&     expected,
                                             const std::vector<int>&     actual)
{
    std::vector<FieldMismatch> diffs;
    const std::string p{path};

    if (expected.size() != actual.size())
        diffs.push_back({p + ".size",
            std::to_string(expected.size()), std::to_string(actual.size())});

    const std::size_t common = std::min(expected.size(), actual.size());
    for (std::size_t i = 0; i < common; ++i) {
        if (expected[i] != actual[i])
            diffs.push_back({p + "[" + std::to_string(i) + "]",
                std::to_string(expected[i]), std::to_string(actual[i])});
    }
    for (std::size_t i = common; i < expected.size(); ++i)
        diffs.push_back({p + "[" + std::to_string(i) + "]",
            std::to_string(expected[i]), "<missing>"});
    for (std::size_t i = common; i < actual.size(); ++i)
        diffs.push_back({p + "[" + std::to_string(i) + "]",
            "<missing>", std::to_string(actual[i])});

    return diffs;
}
