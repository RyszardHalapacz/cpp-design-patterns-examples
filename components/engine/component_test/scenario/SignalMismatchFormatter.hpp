#pragma once
#include <string>
#include "SignalMismatch.hpp"

// ─── SignalMismatchFormatter ──────────────────────────────────────────────────
// Converts a SignalMismatch to a human-readable multi-line string for ADD_FAILURE().
// Pure formatting — no domain types, no GoogleTest dependency.
//
// Output format uses the unified Expected / Actual / Mismatches model.
// The leading keyword on each path preserves the substrings checked by existing tests:
//   "Unexpected signal"  — MetadataMismatch and UnexpectedExtra paths
//   "payload mismatch"   — PayloadMismatch path (via "Signal payload mismatch")
//   "Signal not received" — not produced here; ScenarioVerifier emits it directly
class SignalMismatchFormatter {
public:
    static std::string format(const SignalMismatch& m)
    {
        switch (m.kind) {
            case SignalMismatchKind::MetadataMismatch: return formatMetadata(m);
            case SignalMismatchKind::PayloadMismatch:  return formatPayload(m);
            case SignalMismatchKind::UnexpectedExtra:  return formatExtra(m);
        }
        return {};
    }

private:
    // ── MetadataMismatch ─────────────────────────────────────────────────────
    // Leading "Unexpected signal" preserves existing test substring.
    static std::string formatMetadata(const SignalMismatch& m)
    {
        std::string out = "Unexpected signal\n\n";
        out += expectedSection(m.expected);
        out += actualSection(m.actual);
        out += "Mismatches:\n";
        for (const auto& f : m.metadata)
            out += "  field [" + f.path + "]:\n"
                 + "    expected: " + f.expected + "\n"
                 + "    actual:   " + f.actual   + "\n";
        return out;
    }

    // ── PayloadMismatch ──────────────────────────────────────────────────────
    // Leading "Signal payload mismatch" — contains "payload mismatch" substring.
    static std::string formatPayload(const SignalMismatch& m)
    {
        std::string out = "Signal payload mismatch\n\n";
        if (!m.payload.has_value()) return out;
        const auto& p = *m.payload;

        out += expectedSectionWithPayloadType(m.expected, p.expectedType);
        out += actualSectionWithPayloadType(m.actual,     p.actualType);

        out += "Mismatches:\n";
        if (p.expectedType != p.actualType) {
            out += "  payload.type:\n"
                   "    expected: " + p.expectedType + "\n";
            out += "    actual:   " + p.actualType   + "\n";
        } else {
            for (const auto& f : p.fields)
                out += "  payload." + f.path + ":\n"
                     + "    expected: " + f.expected + "\n"
                     + "    actual:   " + f.actual   + "\n";
        }
        return out;
    }

    // ── UnexpectedExtra ──────────────────────────────────────────────────────
    // Leading "Unexpected signal" preserves existing test substring.
    static std::string formatExtra(const SignalMismatch& m)
    {
        std::string out = "Unexpected signal\n\n";
        out += "Expected signal:\n  (none — no expectation for this step)\n\n";
        out += actualSection(m.actual);
        return out;
    }

    // ── Section helpers ───────────────────────────────────────────────────────

    static std::string expectedSection(const std::optional<SignalMeta>& exp)
    {
        if (!exp.has_value())
            return "Expected signal:\n  (none)\n\n";
        return "Expected signal:\n"
             + ("  from: " + exp->from + "\n")
             + ("  to:   " + exp->to   + "\n")
             + ("  name: " + exp->name + "\n\n");
    }

    static std::string expectedSectionWithPayloadType(const std::optional<SignalMeta>& exp,
                                                      const std::string& payloadType)
    {
        if (!exp.has_value())
            return "Expected signal:\n  (none)\n\n";
        return "Expected signal:\n"
             + ("  from:         " + exp->from    + "\n")
             + ("  to:           " + exp->to       + "\n")
             + ("  name:         " + exp->name     + "\n")
             + ("  payload type: " + payloadType   + "\n\n");
    }

    static std::string actualSection(const SignalMeta& act)
    {
        return "Actual signal:\n"
             + ("  from: " + act.from + "\n")
             + ("  to:   " + act.to   + "\n")
             + ("  name: " + act.name + "\n\n");
    }

    static std::string actualSectionWithPayloadType(const SignalMeta& act,
                                                    const std::string& payloadType)
    {
        return "Actual signal:\n"
             + ("  from:         " + act.from     + "\n")
             + ("  to:           " + act.to        + "\n")
             + ("  name:         " + act.name      + "\n")
             + ("  payload type: " + payloadType   + "\n\n");
    }
};
