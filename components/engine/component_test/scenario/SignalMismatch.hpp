#pragma once
#include <optional>
#include <string>
#include <vector>
#include "PayloadMismatch.hpp"

// ─── SignalMeta ───────────────────────────────────────────────────────────────
// Pre-formatted signal identity (from/to/name as strings).
// Built by compareSignals() via endpointName() — no Endpoint enum here.
// Stored in SignalMismatch so the formatter needs no access to Signal types.
struct SignalMeta {
    std::string from;
    std::string to;
    std::string name;
};

// ─── SignalFieldMismatch ──────────────────────────────────────────────────────
// One mismatching metadata field of a signal (from / to / name).
struct SignalFieldMismatch {
    std::string path;      // "from", "to", "name"
    std::string expected;
    std::string actual;
};

// ─── SignalMismatchKind ───────────────────────────────────────────────────────
enum class SignalMismatchKind {
    MetadataMismatch,   // from/to/name differs from expected
    PayloadMismatch,    // metadata OK, payload differs
    UnexpectedExtra     // actual arrived with no expected counterpart
};

// ─── SignalMismatch ───────────────────────────────────────────────────────────
// Built by compareSignals() or makeUnexpectedExtra().
// Consumed by SignalMismatchFormatter::format().
// Never visible in public component tests.
struct SignalMismatch {
    SignalMismatchKind kind;

    // Expected signal identity — nullopt for UnexpectedExtra (no expected exists).
    std::optional<SignalMeta> expected;

    // Actual signal identity — always present.
    SignalMeta actual;

    // For MetadataMismatch: which metadata fields differ.
    std::vector<SignalFieldMismatch> metadata;

    // For PayloadMismatch: structured payload diff.
    std::optional<PayloadMismatch> payload;
};
