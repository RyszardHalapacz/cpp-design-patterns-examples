# PROPOSAL: Structured Signal & Payload Mismatch Diagnostics (v3)

> v3 delta from previous draft: unified Expected / Actual / Mismatches output model;
> `SignalMeta` struct replaces flat label strings in `SignalMismatch`; `finalizeStep()`
> updated to use formatter; `SignalMismatchFormatter` uses private helpers for each
> section; `expectHistorianSnapshot` / `historian.publishSnapshot` gain full
> `ExpectedEngineSnapshot` overload; `Scenarios.hpp` gains `ExpectedEngineSnapshot`
> overload for `expectHistorianSnapshot`; dependency graph re-verified against actual
> repo file list; test plan GMock-free throughout; `CMakeLists.txt` unchanged.

---

## 1. Scope

Two distinct diagnostic gaps:

**Gap A — metadata mismatch (wrong from / to / name):**
```
// Current output
Unexpected signal:
  received: Engine -> Factory : create
  expected: Engine -> Historian : recordCommand

// Target output
Unexpected signal

Expected signal:
  from: Engine
  to:   Historian
  name: recordCommand

Actual signal:
  from: Engine
  to:   Factory
  name: create

Mismatches:
  field [to]:
    expected: Historian
    actual:   Factory
  field [name]:
    expected: recordCommand
    actual:   create
```

**Gap B — payload mismatch:**
```
// Current output
Signal payload mismatch:
  signal: Engine -> Historian : recordCommand

// Target output
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Actual signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Mismatches:
  payload.commandName:
    expected: "addVector"
    actual:   "sortVector"
  payload.data.size:
    expected: 4
    actual:   3
  payload.data[2]:
    expected: 3
    actual:   99
  payload.data[3]:
    expected: 40
    actual:   <missing>
```

**Primary constraint:** the public component test DSL does not change. No new types
appear in normal `TEST_F` bodies. All diagnostics happen automatically.

---

## 2. Public Test API — Unchanged

This is the public DSL. It must remain exactly as it is today.

```cpp
// AddVector
engine.receive(engine.addVector({1, 2, 3}));
historian.receive(historian.addVector({1, 2, 3}));

// SortVector (engine must already have a vector)
engine.receive(engine.sortVector(0));
historian.receive(historian.sortVector());

// SetStrategy — two collaborators, one step, strict order
engine.receive(engine.strategyChange(SortStrategyId::Descending));
factory.receive(factory.create(SortStrategyId::Descending));
historian.receive(historian.setSortStrategy());

// PublishSnapshot — vectorCount only (existing call sites preserved)
engine.receive(engine.publishSnapshot());
historian.receive(historian.publishSnapshot(1));

// PublishSnapshot — full partial expectation (new overload, backward compatible)
historian.receive(historian.publishSnapshot({
    .running     = true,
    .strategy    = SortStrategyId::Descending,
    .vectorCount = 3
}));
```

Pre-built scenario API (used via `EngineDriver`):

```cpp
EngineDriver driver(*engine_, verifier_, channels_);
driver.run(Scenarios::AddVector({1, 2, 3}));
driver.run(Scenarios::SetStrategy(SortStrategyId::Descending));
driver.run(Scenarios::PublishSnapshot(1));
driver.run(Scenarios::FullEngineFlow());
```

A test author:
- Does **not** call `compareSignals()`
- Does **not** inspect `std::expected` or `SignalMismatch`
- Does **not** call `captureNextFailure()`
- Does **not** build expected/actual manually

All structured diagnostics happen automatically inside `historian.receive()` and
`factory.receive()`, which delegate through `ScenarioExecutor` → `ScenarioVerifier`
→ `compareSignals()` → `SignalMismatchFormatter` → `ADD_FAILURE()`.

---

## 3. What Remains Internal

These types and functions **never appear in a normal `TEST_F` body**:

| Symbol | Location |
|---|---|
| `PayloadMatchResult` | `PayloadMismatch.hpp` |
| `FieldMismatch` | `PayloadMismatch.hpp` |
| `PayloadMismatch` | `PayloadMismatch.hpp` |
| `SignalMeta` | `SignalMismatch.hpp` |
| `SignalFieldMismatch` | `SignalMismatch.hpp` |
| `SignalMismatchKind` | `SignalMismatch.hpp` |
| `SignalMismatch` | `SignalMismatch.hpp` |
| `SignalMatchResult` | `SignalComparator.hpp` |
| `compareSignals()` | `SignalComparator.hpp` |
| `makeUnexpectedExtra()` | `SignalComparator.hpp` |
| `SignalMismatchFormatter` | `SignalMismatchFormatter.hpp` |
| `detail::matchCommandHistory` | `MatcherHelpers.hpp` |
| `detail::matchEngineSnapshot` | `MatcherHelpers.hpp` |
| `detail::matchSortStrategyId` | `MatcherHelpers.hpp` |

They appear only in new infrastructure headers and in the new test fixtures
(`SignalComparatorTest`, `SignalMismatchFormatterTest`, `ScenarioVerifierTest`).

---

## 4. Internal Architecture

```
PUBLIC TEST

  historian.receive(historian.addVector({1, 2, 3}))
           │
           │  HistorianEndpoint::receive()
           │  builds Signal{role=Expectation, name="recordCommand",
           │    from=Engine, to=Historian,
           │    payloadMatcher = detail::matchCommandHistory("addVector", [1,2,3])}
           │
           ▼
   ScenarioExecutor::declareExpectation(Signal)
           │
           ▼
   ScenarioVerifier::matchExpectation(Signal exp)
           │
           │  stepActuals_[nextActualInStep_] → SignalDescriptor actual
           │
           ▼
   compareSignals(exp, actual)                      ← SignalComparator.hpp
           │
           │  returns SignalMatchResult
           │    = std::expected<void, SignalMismatch>
           │
           ├── success → advance, pendingRows_.emplace_back(), SequenceLog
           │
           └── mismatch
                  │
                  ▼
         SignalMismatchFormatter::format(mismatch)  ← SignalMismatchFormatter.hpp
                  │
                  ▼
             ADD_FAILURE()

────────────────────────────────────────────────────────────────
FRAMEWORK INTERNALS — never visible in normal TEST_F bodies
────────────────────────────────────────────────────────────────
```

`ScenarioVerifier` also handles two additional cases directly:

- **"Signal not received"** — expected signal never arrived; produced inline in
  `verifyComplete()` and `matchExpectation()`. Not going through the formatter.
  Unchanged from current implementation.

- **"Unexpected extra" signal** — no expected signal remaining in this step;
  `ScenarioVerifier` constructs `SignalMismatch{kind=UnexpectedExtra}` via
  `makeUnexpectedExtra()` and passes it to the formatter.

---

## 5. New Types

### 5.1 File: `scenario/PayloadMismatch.hpp` (new)

Generic diagnostic types — **no domain headers**.

```cpp
#pragma once
#include <algorithm>    // std::min
#include <any>
#include <cstdlib>      // free()
#include <cxxabi.h>     // abi::__cxa_demangle
#include <expected>
#include <string>
#include <string_view>
#include <vector>

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
        std::string name(demangled); free(demangled); return name;
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
    for (size_t i = 0; i < v.size(); ++i) {
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

    const size_t common = std::min(expected.size(), actual.size());
    for (size_t i = 0; i < common; ++i) {
        if (expected[i] != actual[i])
            diffs.push_back({p + "[" + std::to_string(i) + "]",
                std::to_string(expected[i]), std::to_string(actual[i])});
    }
    for (size_t i = common; i < expected.size(); ++i)
        diffs.push_back({p + "[" + std::to_string(i) + "]",
            std::to_string(expected[i]), "<missing>"});
    for (size_t i = common; i < actual.size(); ++i)
        diffs.push_back({p + "[" + std::to_string(i) + "]",
            "<missing>", std::to_string(actual[i])});

    return diffs;
}
```

`PayloadMismatch.hpp` includes **no project headers** — only stdlib and `<cxxabi.h>`.

---

### 5.2 File: `scenario/SignalMismatch.hpp` (new)

Top-level mismatch model. Depends only on `PayloadMismatch.hpp`.

```cpp
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
    std::string from;   // "Engine", "Driver", etc.
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
```

---

### 5.3 File: `scenario/ExpectationSpecs.hpp` (new)

Domain-level partial expectation descriptor. Depends only on `SortStrategyId.hpp`.
Placed in a separate header so `MatcherHelpers.hpp` can use `ExpectedEngineSnapshot`
without including `HistorianEndpoint.hpp` — preventing a circular dependency.

```cpp
#pragma once
#include <cstddef>
#include <optional>
#include "patterns/strategy/SortStrategyId.hpp"

// ─── ExpectedEngineSnapshot ───────────────────────────────────────────────────
// Partial expectation for historian.publishSnapshot() / expectHistorianSnapshot().
// Each optional field:
//   has_value() → field is checked; mismatch reported if wrong
//   nullopt     → don't-care; field absent from diagnostic output
//
// Usage examples:
//   historian.publishSnapshot()                         — all don't-care
//   historian.publishSnapshot(3)                        — vectorCount == 3
//   historian.publishSnapshot({.vectorCount = 3})       — same
//   historian.publishSnapshot({
//       .running     = true,
//       .strategy    = SortStrategyId::Descending,
//       .vectorCount = 3
//   })                                                  — all three checked
struct ExpectedEngineSnapshot {
    std::optional<bool>                               running;
    std::optional<patterns::strategy::SortStrategyId> strategy;
    std::optional<std::size_t>                        vectorCount;
};
```

---

### 5.4 File: `scenario/MatcherHelpers.hpp` (new)

Domain matching logic — the **only** place in the scenario layer that includes
`IHistorian.hpp` and knows about `CommandHistory` / `EngineSnapshot` fields.
Uses `sortStrategyIdName()` from `SortStrategyId.hpp` — the existing project function
that covers `Ascending`, `Descending`, `Bubble`; `-Wswitch` fires if a new enumerator
is added without updating it.

```cpp
#pragma once
#include <any>
#include <optional>
#include <string>
#include <vector>
#include "PayloadMismatch.hpp"
#include "ExpectationSpecs.hpp"
#include "patterns/historian/IHistorian.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

namespace detail {

// ── CommandHistory matcher ────────────────────────────────────────────────────
// Checks commandName. If expectedData is provided, also diffs data per element.
// All mismatching fields collected — no early exit.
inline PayloadMatchResult matchCommandHistory(
        const std::string&                     expectedName,
        const std::optional<std::vector<int>>& expectedData,
        const std::any&                        payload)
{
    const auto* cmd = std::any_cast<patterns::historian::CommandHistory>(&payload);
    if (!cmd) return payloadWrongType("CommandHistory", anyTypeName(payload));

    std::vector<FieldMismatch> fields;

    if (cmd->commandName != expectedName)
        fields.push_back({"commandName",
            "\"" + expectedName + "\"", "\"" + cmd->commandName + "\""});

    if (expectedData) {
        auto diffs = diffVector("data", *expectedData, cmd->data);
        fields.insert(fields.end(), diffs.begin(), diffs.end());
    }

    if (!fields.empty())
        return payloadFieldMismatch("CommandHistory", std::move(fields));
    return payloadOk();
}

// ── EngineSnapshot matcher ────────────────────────────────────────────────────
// Checks each field present in spec. Uses sortStrategyIdName() — existing project
// function in SortStrategyId.hpp. All mismatching fields collected.
inline PayloadMatchResult matchEngineSnapshot(
        const ExpectedEngineSnapshot& spec,
        const std::any&               payload)
{
    const auto* snap = std::any_cast<patterns::historian::EngineSnapshot>(&payload);
    if (!snap) return payloadWrongType("EngineSnapshot", anyTypeName(payload));

    std::vector<FieldMismatch> fields;

    if (spec.running && snap->running != *spec.running)
        fields.push_back({"running",
            *spec.running ? "true" : "false",
            snap->running  ? "true" : "false"});

    if (spec.strategy && snap->strategy != *spec.strategy)
        fields.push_back({"strategy",
            patterns::strategy::sortStrategyIdName(*spec.strategy),
            patterns::strategy::sortStrategyIdName(snap->strategy)});

    if (spec.vectorCount && snap->vectorCount != *spec.vectorCount)
        fields.push_back({"vectorCount",
            std::to_string(*spec.vectorCount),
            std::to_string(snap->vectorCount)});

    if (!fields.empty())
        return payloadFieldMismatch("EngineSnapshot", std::move(fields));
    return payloadOk();
}

// ── SortStrategyId matcher ────────────────────────────────────────────────────
// Uses sortStrategyIdName() — covers Ascending, Descending, Bubble.
inline PayloadMatchResult matchSortStrategyId(
        patterns::strategy::SortStrategyId expected,
        const std::any&                    payload)
{
    const auto* rid = std::any_cast<patterns::strategy::SortStrategyId>(&payload);
    if (!rid) return payloadWrongType("SortStrategyId", anyTypeName(payload));
    if (*rid != expected)
        return payloadFieldMismatch("SortStrategyId", {
            {"value",
             patterns::strategy::sortStrategyIdName(expected),
             patterns::strategy::sortStrategyIdName(*rid)}
        });
    return payloadOk();
}

} // namespace detail
```

---

### 5.5 File: `scenario/SignalComparator.hpp` (new)

Pure comparison function — no `ADD_FAILURE`, no GoogleTest dependency.
Directly testable in `SignalComparatorTest` without any failure capture.
Uses `endpointName()` from `Signal.hpp` to pre-format `SignalMeta` strings.

```cpp
#pragma once
#include <expected>
#include "Signal.hpp"         // Signal, SignalDescriptor, Endpoint, endpointName
#include "SignalMismatch.hpp" // SignalMismatch, SignalMeta, SignalMismatchKind

using SignalMatchResult = std::expected<void, SignalMismatch>;

// ─── compareSignals() ─────────────────────────────────────────────────────────
// Compares expected Signal with actual SignalDescriptor.
// Returns success if all metadata and payload match.
//
// Comparison order:
//   1. from, to, name — if any differ → MetadataMismatch (all diffs reported)
//   2. payloadMatcher (if non-null)  — if fails → PayloadMismatch
//
// Step 2 is only attempted if step 1 succeeds.
// Within each step, all differences are collected before returning.
inline SignalMatchResult compareSignals(const Signal&           expected,
                                        const SignalDescriptor&  actual)
{
    SignalMeta expMeta{
        endpointName(expected.from), endpointName(expected.to), expected.name
    };
    SignalMeta actMeta{
        endpointName(actual.from), endpointName(actual.to), actual.name
    };

    // Step 1 — metadata
    std::vector<SignalFieldMismatch> metaDiffs;
    if (actual.from != expected.from)
        metaDiffs.push_back({"from", expMeta.from, actMeta.from});
    if (actual.to != expected.to)
        metaDiffs.push_back({"to", expMeta.to, actMeta.to});
    if (actual.name != expected.name)
        metaDiffs.push_back({"name", expMeta.name, actMeta.name});

    if (!metaDiffs.empty())
        return std::unexpected(SignalMismatch{
            .kind     = SignalMismatchKind::MetadataMismatch,
            .expected = expMeta,
            .actual   = actMeta,
            .metadata = std::move(metaDiffs),
            .payload  = std::nullopt
        });

    // Step 2 — payload
    if (expected.payloadMatcher) {
        auto result = expected.payloadMatcher(actual.payload);
        if (!result)
            return std::unexpected(SignalMismatch{
                .kind     = SignalMismatchKind::PayloadMismatch,
                .expected = expMeta,
                .actual   = actMeta,
                .metadata = {},
                .payload  = result.error()
            });
    }

    return {};  // success
}

// ─── makeUnexpectedExtra() ────────────────────────────────────────────────────
// Builds SignalMismatch for an actual signal that has no expected counterpart.
// Used by ScenarioVerifier when nextExpected_ >= expectations_.size() (Mode 1)
// and in finalizeStep() for unmatched actuals (Mode 2).
inline SignalMismatch makeUnexpectedExtra(const SignalDescriptor& actual)
{
    return {
        .kind     = SignalMismatchKind::UnexpectedExtra,
        .expected = std::nullopt,
        .actual   = {endpointName(actual.from), endpointName(actual.to), actual.name},
        .metadata = {},
        .payload  = std::nullopt
    };
}
```

---

### 5.6 File: `scenario/SignalMismatchFormatter.hpp` (new)

Pure formatting — no domain types, no GoogleTest dependency.
`SignalMismatch.hpp` is the only project header included.

Output format uses the unified Expected / Actual / Mismatches model.
The leading keyword on each path preserves the substrings checked by existing tests:
- `"Unexpected signal"` — MetadataMismatch and UnexpectedExtra paths
- `"payload mismatch"` — PayloadMismatch path (via `"Signal payload mismatch"`)
- `"Signal not received"` — not produced here; ScenarioVerifier emits it directly

```cpp
#pragma once
#include <string>
#include "SignalMismatch.hpp"

// ─── SignalMismatchFormatter ──────────────────────────────────────────────────
// Converts a SignalMismatch to a human-readable multi-line string for ADD_FAILURE().
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
    //
    // Example:
    //   Unexpected signal
    //
    //   Expected signal:
    //     from: Engine
    //     to:   Historian
    //     name: recordCommand
    //
    //   Actual signal:
    //     from: Engine
    //     to:   Factory
    //     name: create
    //
    //   Mismatches:
    //     field [to]:
    //       expected: Historian
    //       actual:   Factory
    //     field [name]:
    //       expected: recordCommand
    //       actual:   create
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
    //
    // Type mismatch example:
    //   Signal payload mismatch
    //
    //   Expected signal:
    //     from:         Engine
    //     to:           Historian
    //     name:         recordCommand
    //     payload type: CommandHistory
    //
    //   Actual signal:
    //     from:         Engine
    //     to:           Historian
    //     name:         recordCommand
    //     payload type: EngineSnapshot
    //
    //   Mismatches:
    //     payload.type:
    //       expected: CommandHistory
    //       actual:   EngineSnapshot
    //
    // Field mismatch example:
    //   Signal payload mismatch
    //
    //   Expected signal:
    //     from:         Engine
    //     to:           Historian
    //     name:         recordCommand
    //     payload type: CommandHistory
    //
    //   Actual signal:
    //     from:         Engine
    //     to:           Historian
    //     name:         recordCommand
    //     payload type: CommandHistory
    //
    //   Mismatches:
    //     payload.commandName:
    //       expected: "addVector"
    //       actual:   "sortVector"
    //     payload.data[2]:
    //       expected: 3
    //       actual:   99
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
                 + "    expected: " + p.expectedType + "\n"
                 + "    actual:   " + p.actualType   + "\n";
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
    //
    // Example:
    //   Unexpected signal
    //
    //   Expected signal:
    //     (none — no expectation for this step)
    //
    //   Actual signal:
    //     from: Engine
    //     to:   Historian
    //     name: recordCommand
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
```

---

## 6. `scenario/Signal.hpp` — Modified

Two changes only:

```cpp
// Add at top (after existing stdlib includes):
#include "PayloadMismatch.hpp"

// Change payloadMatcher field type (was bool, now PayloadMatchResult):
// Before:
std::function<bool(const std::any&)> payloadMatcher;
// After:
std::function<PayloadMatchResult(const std::any&)> payloadMatcher;
```

Null-matcher semantic preserved: `payloadMatcher == nullptr` → any payload accepted.
All existing call sites check `if (expected.payloadMatcher)` — `std::function`
`operator bool()` — unchanged.

---

## 7. `scenario/ScenarioVerifier.hpp` — Modified

### 7.1 New includes

```cpp
#include "SignalComparator.hpp"         // compareSignals(), makeUnexpectedExtra()
#include "SignalMismatchFormatter.hpp"  // SignalMismatchFormatter::format()
```

`ScenarioVerifier` has **no domain headers** after this change. It sees only
`Signal`, `SignalDescriptor`, `SignalComparator`, and `SignalMismatchFormatter`.

### 7.2 Mode 1 `report()` — replace both mismatch blocks

```cpp
void report(const SignalDescriptor& actual) {
    if (collectingActuals_) { stepActuals_.push_back(actual); return; }
    if (!armed_ || confused_) return;

    // No expectation remaining in this step → unexpected extra signal
    if (nextExpected_ >= expectations_.size()) {
        ADD_FAILURE() << SignalMismatchFormatter::format(makeUnexpectedExtra(actual));
        confused_ = true;
        return;
    }

    const Signal& expected = expectations_[nextExpected_];
    auto result = compareSignals(expected, actual);

    if (!result) {
        ADD_FAILURE() << SignalMismatchFormatter::format(result.error());
        if (result.error().kind == SignalMismatchKind::MetadataMismatch) {
            confused_ = true;
            return;  // don't advance — signal was not consumed
        }
        // PayloadMismatch: signal was received, advance even on payload failure
    }

    pendingRows_.emplace_back(expected.from, expected.to, expected.name);
    ++nextExpected_;
}
```

### 7.3 Mode 2 `matchExpectation()` — replace both mismatch blocks

```cpp
void matchExpectation(const Signal& exp) {
    if (stepConfused_) return;

    if (nextActualInStep_ >= stepActuals_.size()) {
        ADD_FAILURE()
            << "Signal not received:\n"
            << "  from:   " << endpointName(exp.from) << "\n"
            << "  to:     " << endpointName(exp.to)   << "\n"
            << "  signal: " << exp.name;
        return;
    }

    const SignalDescriptor& actual = stepActuals_[nextActualInStep_];
    auto result = compareSignals(exp, actual);

    if (!result) {
        ADD_FAILURE() << SignalMismatchFormatter::format(result.error());
        if (result.error().kind == SignalMismatchKind::MetadataMismatch) {
            stepConfused_ = true;
            ++nextActualInStep_;
            return;
        }
        // PayloadMismatch: advance
    }

    pendingRows_.emplace_back(exp.from, exp.to, exp.name);
    ++nextActualInStep_;
}
```

### 7.4 `finalizeStep()` — replace inline "Unexpected signal" block

```cpp
void finalizeStep() {
    if (!stepConfused_) {
        for (size_t i = nextActualInStep_; i < stepActuals_.size(); ++i)
            ADD_FAILURE() << SignalMismatchFormatter::format(
                              makeUnexpectedExtra(stepActuals_[i]));
    }
    stepActuals_.clear();
    nextActualInStep_  = 0;
    stepConfused_      = false;
    collectingActuals_ = false;
}
```

### 7.5 Unchanged methods

`setExpected()`, `beginStep()`, `endStepCollection()`, `verifyComplete()`,
`flushDiagramRows()` — unchanged. `verifyComplete()` still produces "Signal not
received" inline; this message is not routed through the formatter.

---

## 8. `scenario/HistorianEndpoint.hpp` — Modified

Replace old inner-lambda approach with delegation to `detail::` helpers.
Add full `ExpectedEngineSnapshot` overload for `publishSnapshot`.

### 8.1 New includes

```cpp
#include "MatcherHelpers.hpp"   // detail::matchCommandHistory, matchEngineSnapshot
#include "ExpectationSpecs.hpp" // ExpectedEngineSnapshot
```

Remove the old `#include "patterns/historian/IHistorian.hpp"` — now pulled in
transitively via `MatcherHelpers.hpp`.

### 8.2 New `makeRecordCommand` and public command methods

```cpp
// Internal helper: builds a recordCommand expectation signal.
Signal makeRecordCommand(const std::string&              commandName,
                         std::optional<std::vector<int>> data = {}) const
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "recordCommand",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action = {},
        .payloadMatcher = [commandName, data]
                          (const std::any& payload) -> PayloadMatchResult {
            return detail::matchCommandHistory(commandName, data, payload);
        }
    };
}

[[nodiscard]] Signal addVector(std::vector<int> data) const {
    return makeRecordCommand("addVector", std::move(data));
}
[[nodiscard]] Signal sortVector()       const { return makeRecordCommand("sortVector");       }
[[nodiscard]] Signal setSortStrategy()  const { return makeRecordCommand("setSortStrategy");  }
```

### 8.3 `publishSnapshot()` overloads

```cpp
// Primary overload: full partial expectation.
[[nodiscard]] Signal publishSnapshot(ExpectedEngineSnapshot spec = {}) const {
    return {
        .role   = SignalRole::Expectation,
        .name   = "publishSnapshot",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action = {},
        .payloadMatcher = [spec](const std::any& payload) -> PayloadMatchResult {
            return detail::matchEngineSnapshot(spec, payload);
        }
    };
}

// Convenience overload: vectorCount-only check.
// Preserves existing call sites: historian.receive(historian.publishSnapshot(1))
[[nodiscard]] Signal publishSnapshot(std::size_t vectorCount) const {
    return publishSnapshot(ExpectedEngineSnapshot{ .vectorCount = vectorCount });
}
```

Overload resolution:
- `historian.publishSnapshot()` → primary overload (default `{}`) ✓
- `historian.publishSnapshot(1)` → `int` → `size_t` → convenience overload ✓
- `historian.publishSnapshot({.vectorCount=3})` → designated init → primary overload ✓
- `historian.publishSnapshot({.running=true, ...})` → primary overload ✓

---

## 9. `scenario/FactoryEndpoint.hpp` — Modified

```cpp
#include "MatcherHelpers.hpp"

[[nodiscard]] Signal create(patterns::strategy::SortStrategyId id) const {
    return {
        .role   = SignalRole::Expectation,
        .name   = "create",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Factory,
        .action = {},
        .payloadMatcher = [id](const std::any& payload) -> PayloadMatchResult {
            return detail::matchSortStrategyId(id, payload);
        }
    };
}
```

Remove old `#include "patterns/strategy/SortStrategyId.hpp"` — pulled in via
`MatcherHelpers.hpp` → `ExpectationSpecs.hpp` → `SortStrategyId.hpp`.

---

## 10. `scenario/Scenarios.hpp` — Modified

All three expectation builders delegate to `detail::` helpers and return
`PayloadMatchResult`. `expectHistorianSnapshot` gains an `ExpectedEngineSnapshot`
overload while keeping backward-compatible `std::optional<size_t>` overload.

### 10.1 New includes (in addition to existing)

```cpp
#include "MatcherHelpers.hpp"    // detail::match*
#include "ExpectationSpecs.hpp"  // ExpectedEngineSnapshot
```

### 10.2 Updated `expectHistorianCommand`

```cpp
inline Signal expectHistorianCommand(
        std::string                     commandName,
        std::optional<std::vector<int>> data = std::nullopt)
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "recordCommand",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action         = {},
        .payloadMatcher = [n = std::move(commandName), data]
                          (const std::any& payload) -> PayloadMatchResult {
            return detail::matchCommandHistory(n, data, payload);
        }
    };
}
```

### 10.3 Updated `expectHistorianSnapshot` (two overloads)

```cpp
// Primary overload: full partial expectation.
inline Signal expectHistorianSnapshot(ExpectedEngineSnapshot spec = {})
{
    return {
        .role   = SignalRole::Expectation,
        .name   = "publishSnapshot",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Historian,
        .action         = {},
        .payloadMatcher = [spec](const std::any& payload) -> PayloadMatchResult {
            return detail::matchEngineSnapshot(spec, payload);
        }
    };
}

// Convenience overload: vectorCount-only check.
// Preserves: expectHistorianSnapshot(std::nullopt) and Scenarios::PublishSnapshot(vc).
inline Signal expectHistorianSnapshot(std::optional<std::size_t> vectorCount)
{
    return expectHistorianSnapshot(ExpectedEngineSnapshot{ .vectorCount = vectorCount });
}
```

Existing `Scenarios::PublishSnapshot(std::optional<size_t> vectorCount)` calls
`expectHistorianSnapshot(vectorCount)` → `std::optional<size_t>` overload → backward
compatible. ✓

### 10.4 Updated `expectFactoryCreate`

```cpp
inline Signal expectFactoryCreate(patterns::strategy::SortStrategyId id) {
    return {
        .role   = SignalRole::Expectation,
        .name   = "create",
        .from   = Endpoint::Engine,
        .to     = Endpoint::Factory,
        .action         = {},
        .payloadMatcher = [id](const std::any& payload) -> PayloadMatchResult {
            return detail::matchSortStrategyId(id, payload);
        }
    };
}
```

---

## 11. Dependency Graph

```
stdlib / cxxabi.h
        │
        ▼
PayloadMismatch.hpp          ← no project deps
        │
        ├─────────────────────────────────────────┐
        │                                         │
        ▼                                         ▼
SignalMismatch.hpp            ExpectationSpecs.hpp ← SortStrategyId.hpp
        │                             │
        │                             ▼
        │                    MatcherHelpers.hpp ← IHistorian.hpp
        │                             │           SortStrategyId.hpp
        │                             │           ExpectationSpecs.hpp
        │                             │           PayloadMismatch.hpp
        │                    ┌────────┤
        │                    │        │
Signal.hpp                   │   HistorianEndpoint.hpp ← MatcherHelpers.hpp
(payloadMatcher:             │   FactoryEndpoint.hpp   ← MatcherHelpers.hpp
 PayloadMatchResult)         │   Scenarios.hpp         ← MatcherHelpers.hpp
        │                    │
        ▼                    │
SignalComparator.hpp         │
(compareSignals,             │
 makeUnexpectedExtra)        │
        │                    │
        ▼                    ▼
SignalMismatchFormatter.hpp (SignalMismatch.hpp only)
        │                    │
        └──────────┬──────────┘
                   ▼
        ScenarioVerifier.hpp ← SequenceLog.hpp
```

**Proof of acyclicity:** follow any path — no header re-encounters itself.

- `PayloadMismatch.hpp` → stdlib only. Trivially acyclic.
- `SignalMismatch.hpp` → `PayloadMismatch.hpp` only.
- `ExpectationSpecs.hpp` → `SortStrategyId.hpp` only.
- `MatcherHelpers.hpp` → `PayloadMismatch.hpp`, `ExpectationSpecs.hpp`,
  `IHistorian.hpp`, `SortStrategyId.hpp` — none include `MatcherHelpers.hpp`.
- `Signal.hpp` → `PayloadMismatch.hpp`, `Engine.hpp` — neither includes `Signal.hpp`.
- `SignalComparator.hpp` → `Signal.hpp`, `SignalMismatch.hpp` — neither includes
  `SignalComparator.hpp`.
- `SignalMismatchFormatter.hpp` → `SignalMismatch.hpp` only.
- `ScenarioVerifier.hpp` → `SignalComparator.hpp`, `SignalMismatchFormatter.hpp`,
  `Signal.hpp`, `SequenceLog.hpp` — none include `ScenarioVerifier.hpp`.
- `HistorianEndpoint.hpp` → `MatcherHelpers.hpp`, `Signal.hpp`, `ExpectationSpecs.hpp`,
  `ScenarioExecutor.hpp` — `MatcherHelpers.hpp` does **not** include
  `HistorianEndpoint.hpp`. The cycle present in naïve designs is absent because
  `ExpectedEngineSnapshot` was extracted to `ExpectationSpecs.hpp`. ✓

---

## 12. Formatter Output Examples

### 12.1 Metadata mismatch — wrong `to` and `name`

```
Unexpected signal

Expected signal:
  from: Engine
  to:   Historian
  name: recordCommand

Actual signal:
  from: Engine
  to:   Factory
  name: create

Mismatches:
  field [to]:
    expected: Historian
    actual:   Factory
  field [name]:
    expected: recordCommand
    actual:   create
```

Substring `"Unexpected signal"` present — existing tests pass. ✓

### 12.2 Payload mismatch — commandName + vector diff

```
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Actual signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Mismatches:
  payload.commandName:
    expected: "addVector"
    actual:   "sortVector"
  payload.data.size:
    expected: 4
    actual:   3
  payload.data[2]:
    expected: 3
    actual:   99
  payload.data[3]:
    expected: 40
    actual:   <missing>
```

Substring `"payload mismatch"` present — existing tests pass. ✓

### 12.3 Payload type mismatch

```
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Actual signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: patterns::historian::EngineSnapshot

Mismatches:
  payload.type:
    expected: CommandHistory
    actual:   patterns::historian::EngineSnapshot
```

`anyTypeName()` demangles the actual type. Expected type is always the logical name
because the matcher knows it (`"CommandHistory"` string literal). ✓

### 12.4 Unexpected extra signal

```
Unexpected signal

Expected signal:
  (none — no expectation for this step)

Actual signal:
  from: Engine
  to:   Historian
  name: recordCommand
```

Substring `"Unexpected signal"` present — existing tests pass. ✓

### 12.5 EngineSnapshot — partial spec, two fields wrong

```
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Historian
  name:         publishSnapshot
  payload type: EngineSnapshot

Actual signal:
  from:         Engine
  to:           Historian
  name:         publishSnapshot
  payload type: EngineSnapshot

Mismatches:
  payload.strategy:
    expected: Descending
    actual:   Ascending
  payload.vectorCount:
    expected: 3
    actual:   7
```

`running` not in spec → not reported. ✓

---

## 13. File Summary

### New files (`scenario/`)

| File | Role | Dependencies |
|---|---|---|
| `PayloadMismatch.hpp` | Generic diff types, `diffVector`, helpers | stdlib, `<cxxabi.h>` |
| `SignalMismatch.hpp` | `SignalMeta`, top-level mismatch model, `SignalMismatchKind` | `PayloadMismatch.hpp` |
| `ExpectationSpecs.hpp` | `ExpectedEngineSnapshot` | `SortStrategyId.hpp` |
| `MatcherHelpers.hpp` | `detail::match*` — only place with domain knowledge | `PayloadMismatch.hpp`, `ExpectationSpecs.hpp`, `IHistorian.hpp`, `SortStrategyId.hpp` |
| `SignalComparator.hpp` | `compareSignals()`, `makeUnexpectedExtra()` | `Signal.hpp`, `SignalMismatch.hpp` |
| `SignalMismatchFormatter.hpp` | `format(SignalMismatch)` → `std::string` | `SignalMismatch.hpp` |

### Modified files

| File | Change |
|---|---|
| `Signal.hpp` | Include `PayloadMismatch.hpp`; `payloadMatcher` return type `bool` → `PayloadMatchResult` |
| `ScenarioVerifier.hpp` | Include `SignalComparator.hpp`, `SignalMismatchFormatter.hpp`; replace mismatch blocks in `report()`, `matchExpectation()`, `finalizeStep()` |
| `HistorianEndpoint.hpp` | `makeRecordCommand` delegates to `detail::matchCommandHistory`; `publishSnapshot` gains `ExpectedEngineSnapshot` overload |
| `FactoryEndpoint.hpp` | `create()` delegates to `detail::matchSortStrategyId` |
| `Scenarios.hpp` | 3 expectation builders via `detail::` helpers; `expectHistorianSnapshot` gains `ExpectedEngineSnapshot` overload |

### Not modified

`EngineDriver.hpp`, `ScenarioExecutor.hpp`, `ScenarioExecutor.hpp`,
`SequenceLog.hpp`, `Spies.hpp`, `EngineEndpoint.hpp`,
`EngineComponentTest.cpp` (component tests), `CMakeLists.txt`.

---

## 14. Implementation Order

### D1 — `PayloadMismatch.hpp`

Create. No project dependencies.

Build gate: `g++ -std=c++23 -c PayloadMismatch.hpp` (no errors, no warnings).

### D2 — `SignalMismatch.hpp`

Create. Depends only on `PayloadMismatch.hpp`.

Build gate: standalone compilation.

### D3 — `ExpectationSpecs.hpp`

Create. Depends only on `SortStrategyId.hpp`.

Build gate: standalone compilation.

### D4 — `MatcherHelpers.hpp`

Create. Depends on D1, D3, `IHistorian.hpp`, `SortStrategyId.hpp`.

Build gate: standalone compilation.

### D5 — `SignalMismatchFormatter.hpp`

Create. Depends only on D2. No GoogleTest dependency — pure `std::string` output.

Build gate: standalone compilation.

### D6-D9 — Atomic type migration commit

All four steps must land together: after D6 the old `bool` lambdas no longer compile;
D7–D9 fix them. Do not commit D6 alone.

**D6** `Signal.hpp` — add `#include "PayloadMismatch.hpp"`, change `payloadMatcher`
type. All existing `-> bool` lambdas now fail to compile.

**D7** `SignalComparator.hpp` — create. Depends on D6 `Signal.hpp` (now carrying
`PayloadMatchResult`) and D2 `SignalMismatch.hpp`.

**D8** `ScenarioVerifier.hpp` — add includes for D7 and D5; replace three mismatch
blocks (`report()`, `matchExpectation()`, `finalizeStep()`).

**D9** Endpoints + Scenarios migration:
- `HistorianEndpoint.hpp` — add `MatcherHelpers.hpp` / `ExpectationSpecs.hpp` includes;
  new `makeRecordCommand`; two `publishSnapshot` overloads.
- `FactoryEndpoint.hpp` — add `MatcherHelpers.hpp` include; update `create()`.
- `Scenarios.hpp` — add `MatcherHelpers.hpp` / `ExpectationSpecs.hpp` includes;
  update three expectation builders; add `expectHistorianSnapshot(ExpectedEngineSnapshot)`.

D9 completes the migration.

Build gate: `cmake --build` green; all existing tests pass.

### D10 — `SignalComparatorTest` (new fixture in `EngineComponentTest.cpp`)

Tests added to `EngineComponentTest.cpp` (no CMakeLists change needed). See §15.A.

Build gate: new tests compile and pass.

### D11 — `SignalMismatchFormatterTest` (new fixture)

See §15.B.

Build gate: new tests compile and pass.

### D12 — `ScenarioVerifierTest` — integration tests (new fixture)

See §15.C. Uses `ScopedFakeTestPartResultReporter` from `<gtest/gtest-spi.h>`
(already included in `EngineComponentTest.cpp`).

Build gate: all tests pass.

### D13 — Documentation

Update inline comments in modified files to reflect new types. No code changes.

---

## 15. Test Plan

Three new test fixture classes added to `EngineComponentTest.cpp`. No new source files —
no CMakeLists change. No GoogleMock — `CMakeLists.txt` links only `GTest::gtest_main`
and remains unchanged.

### A. `SignalComparatorTest` — structured assertions on `SignalMismatch`

Tests call `compareSignals()` directly and assert on the returned `SignalMismatch`
structure. No `ADD_FAILURE`. No string capture.

```cpp
class SignalComparatorTest : public ::testing::Test {
protected:
    static Signal expectCommand(std::string name,
                                std::optional<std::vector<int>> data = {}) {
        return expectHistorianCommand(std::move(name), data);
    }
    static SignalDescriptor actualCommand(std::string name,
                                         std::vector<int> data = {}) {
        return {Endpoint::Engine, Endpoint::Historian, "recordCommand",
                std::any{CommandHistory{std::move(name), std::move(data)}}};
    }
    static SignalDescriptor actualSnapshot(bool running = false,
                                           SortStrategyId strategy = SortStrategyId::Ascending,
                                           std::size_t vectorCount = 0) {
        EngineSnapshot s; s.running=running; s.strategy=strategy; s.vectorCount=vectorCount;
        return {Endpoint::Engine, Endpoint::Historian, "publishSnapshot", std::any{s}};
    }
    static SignalDescriptor actualFactoryCreate(SortStrategyId id) {
        return {Endpoint::Engine, Endpoint::Factory, "create", std::any{id}};
    }
};

// All metadata and payload match → success.
TEST_F(SignalComparatorTest, Match_AllOk) {
    auto r = compareSignals(expectCommand("addVector", {{1,2,3}}),
                            actualCommand("addVector", {1,2,3}));
    EXPECT_TRUE(r.has_value());
}

// Wrong 'to' endpoint → MetadataMismatch, one field diff.
TEST_F(SignalComparatorTest, MetadataMismatch_WrongTo) {
    Signal exp = expectHistorianCommand("addVector");
    SignalDescriptor act{Endpoint::Engine, Endpoint::Factory, "recordCommand", {}};
    auto r = compareSignals(exp, act);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, SignalMismatchKind::MetadataMismatch);
    ASSERT_EQ(r.error().metadata.size(), 1u);
    EXPECT_EQ(r.error().metadata[0].path,     "to");
    EXPECT_EQ(r.error().metadata[0].expected, "Historian");
    EXPECT_EQ(r.error().metadata[0].actual,   "Factory");
}

// Wrong 'to' and wrong 'name' → MetadataMismatch, two field diffs.
TEST_F(SignalComparatorTest, MetadataMismatch_WrongToAndName) {
    Signal exp = expectHistorianCommand("addVector");
    SignalDescriptor act{Endpoint::Engine, Endpoint::Factory, "create",
                         std::any{SortStrategyId::Ascending}};
    auto r = compareSignals(exp, act);
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error().metadata.size(), 2u);
    EXPECT_EQ(r.error().metadata[0].path, "to");
    EXPECT_EQ(r.error().metadata[1].path, "name");
}

// Wrong commandName → PayloadMismatch, commandName field.
TEST_F(SignalComparatorTest, PayloadMismatch_WrongCommandName) {
    auto r = compareSignals(expectCommand("addVector"),
                            actualCommand("sortVector"));
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().kind, SignalMismatchKind::PayloadMismatch);
    ASSERT_TRUE(r.error().payload.has_value());
    ASSERT_EQ(r.error().payload->fields.size(), 1u);
    EXPECT_EQ(r.error().payload->fields[0].path,     "commandName");
    EXPECT_EQ(r.error().payload->fields[0].expected, "\"addVector\"");
    EXPECT_EQ(r.error().payload->fields[0].actual,   "\"sortVector\"");
}

// commandName wrong + data wrong → two fields reported simultaneously.
TEST_F(SignalComparatorTest, PayloadMismatch_MultipleFields) {
    auto r = compareSignals(expectCommand("addVector", {{1,2,3}}),
                            actualCommand("sortVector", {9,9}));
    ASSERT_FALSE(r.has_value());
    const auto& fields = r.error().payload->fields;
    bool hasCmd = false, hasSize = false;
    for (const auto& f : fields) {
        if (f.path == "commandName") hasCmd  = true;
        if (f.path == "data.size")   hasSize = true;
    }
    EXPECT_TRUE(hasCmd);
    EXPECT_TRUE(hasSize);
}

// Vector diff: size + element mismatch + <missing>.
TEST_F(SignalComparatorTest, PayloadMismatch_VectorDiff) {
    auto r = compareSignals(expectCommand("addVector", {{1,2,3,40}}),
                            actualCommand("addVector", {1,2,99}));
    ASSERT_FALSE(r.has_value());
    const auto& fields = r.error().payload->fields;
    bool hasSize=false, hasIdx2=false, hasIdx3=false;
    for (const auto& f : fields) {
        if (f.path == "data.size") hasSize = true;
        if (f.path == "data[2]")  hasIdx2 = true;
        if (f.path == "data[3]") {
            hasIdx3 = true;
            EXPECT_EQ(f.expected, "40");
            EXPECT_EQ(f.actual,   "<missing>");
        }
    }
    EXPECT_TRUE(hasSize); EXPECT_TRUE(hasIdx2); EXPECT_TRUE(hasIdx3);
}

// Actual vector longer than expected → <missing> on expected side.
TEST_F(SignalComparatorTest, PayloadMismatch_VectorActualLonger) {
    auto r = compareSignals(expectCommand("addVector", {{1,2}}),
                            actualCommand("addVector", {1,2,99}));
    ASSERT_FALSE(r.has_value());
    const auto& fields = r.error().payload->fields;
    bool hasExtra = false;
    for (const auto& f : fields)
        if (f.path == "data[2]" && f.expected == "<missing>") hasExtra = true;
    EXPECT_TRUE(hasExtra);
}

// Wrong payload type → PayloadMismatch, type names differ.
TEST_F(SignalComparatorTest, PayloadMismatch_WrongType) {
    Signal exp = expectHistorianCommand("addVector");
    auto r = compareSignals(exp, actualSnapshot());
    ASSERT_FALSE(r.has_value());
    ASSERT_TRUE(r.error().payload.has_value());
    const auto& p = r.error().payload.value();
    EXPECT_NE(p.expectedType, p.actualType);
    EXPECT_EQ(p.expectedType, "CommandHistory");
}

// Empty std::any → PayloadMismatch, actualType == "(empty)".
TEST_F(SignalComparatorTest, PayloadMismatch_EmptyPayload) {
    SignalDescriptor act{Endpoint::Engine, Endpoint::Historian, "recordCommand", {}};
    auto r = compareSignals(expectCommand("addVector"), act);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().payload->actualType, "(empty)");
}

// Don't-care: no data constraint → any data accepted.
TEST_F(SignalComparatorTest, DontCare_NoDataConstraint) {
    auto r = compareSignals(expectCommand("addVector"),
                            actualCommand("addVector", {99,99}));
    EXPECT_TRUE(r.has_value());
}

// Null payloadMatcher → any payload accepted.
TEST_F(SignalComparatorTest, DontCare_NullMatcher) {
    Signal exp = expectHistorianCommand("recordCommand");
    exp.payloadMatcher = nullptr;
    auto r = compareSignals(exp, actualCommand("recordCommand", {1,2,3}));
    EXPECT_TRUE(r.has_value());
}

// EngineSnapshot: two fields wrong, running don't-care.
TEST_F(SignalComparatorTest, PayloadMismatch_SnapshotTwoFields) {
    Signal exp = expectHistorianSnapshot({
        .strategy    = SortStrategyId::Descending,
        .vectorCount = 3
    });
    auto r = compareSignals(exp,
        actualSnapshot(true, SortStrategyId::Ascending, 7));
    ASSERT_FALSE(r.has_value());
    const auto& fields = r.error().payload->fields;
    ASSERT_EQ(fields.size(), 2u);
    EXPECT_EQ(fields[0].path, "strategy");
    EXPECT_EQ(fields[1].path, "vectorCount");
    for (const auto& f : fields)
        EXPECT_NE(f.path, "running");  // don't-care: absent
}

// SortStrategyId mismatch → symbolic names via sortStrategyIdName().
TEST_F(SignalComparatorTest, PayloadMismatch_SortStrategyId) {
    Signal exp = expectFactoryCreate(SortStrategyId::Descending);
    auto r = compareSignals(exp, actualFactoryCreate(SortStrategyId::Ascending));
    ASSERT_FALSE(r.has_value());
    ASSERT_EQ(r.error().payload->fields.size(), 1u);
    EXPECT_EQ(r.error().payload->fields[0].expected, "Descending");
    EXPECT_EQ(r.error().payload->fields[0].actual,   "Ascending");
}

// makeUnexpectedExtra — kind and actual set, expected is nullopt.
TEST_F(SignalComparatorTest, UnexpectedExtra) {
    auto m = makeUnexpectedExtra(actualCommand("addVector"));
    EXPECT_EQ(m.kind, SignalMismatchKind::UnexpectedExtra);
    EXPECT_FALSE(m.expected.has_value());
    EXPECT_EQ(m.actual.to, "Historian");
    EXPECT_EQ(m.actual.name, "recordCommand");
}
```

### B. `SignalMismatchFormatterTest` — string assertions on formatter output

Tests call `SignalMismatchFormatter::format()` directly. No `ADD_FAILURE`.
No GoogleMock — uses `expectContains` / `expectAbsent` helpers with `std::string::find`.

```cpp
class SignalMismatchFormatterTest : public ::testing::Test {
protected:
    static void expectContains(const std::string& text, std::string_view sub) {
        EXPECT_NE(text.find(sub), std::string::npos)
            << "Expected to find: \"" << sub << "\"\nIn:\n" << text;
    }
    static void expectAbsent(const std::string& text, std::string_view sub) {
        EXPECT_EQ(text.find(sub), std::string::npos)
            << "Expected NOT to find: \"" << sub << "\"\nIn:\n" << text;
    }
};

TEST_F(SignalMismatchFormatterTest, MetadataMismatch_ContainsUnexpectedSignal) {
    SignalMismatch m{
        .kind     = SignalMismatchKind::MetadataMismatch,
        .expected = SignalMeta{"Engine", "Historian", "recordCommand"},
        .actual   = SignalMeta{"Engine", "Factory",   "create"},
        .metadata = {{"to",   "Historian", "Factory"},
                     {"name", "recordCommand", "create"}},
        .payload  = {}
    };
    auto text = SignalMismatchFormatter::format(m);
    expectContains(text, "Unexpected signal");
    expectContains(text, "Expected signal");
    expectContains(text, "Actual signal");
    expectContains(text, "field [to]");
    expectContains(text, "field [name]");
    expectContains(text, "Historian");
    expectContains(text, "Factory");
}

TEST_F(SignalMismatchFormatterTest, PayloadMismatch_ContainsPayloadMismatch) {
    PayloadMismatch p{"CommandHistory", "CommandHistory",
        {{"commandName", "\"addVector\"", "\"sortVector\""}}};
    SignalMismatch m{
        .kind     = SignalMismatchKind::PayloadMismatch,
        .expected = SignalMeta{"Engine", "Historian", "recordCommand"},
        .actual   = SignalMeta{"Engine", "Historian", "recordCommand"},
        .payload  = p
    };
    auto text = SignalMismatchFormatter::format(m);
    expectContains(text, "payload mismatch");
    expectContains(text, "payload type");
    expectContains(text, "CommandHistory");
    expectContains(text, "payload.commandName");
    expectContains(text, "\"addVector\"");
    expectContains(text, "\"sortVector\"");
}

TEST_F(SignalMismatchFormatterTest, PayloadMismatch_VectorIndexedDiff) {
    PayloadMismatch p{"CommandHistory", "CommandHistory",
        {{"data.size","4","3"},{"data[2]","3","99"},{"data[3]","40","<missing>"}}};
    SignalMismatch m{
        .kind    = SignalMismatchKind::PayloadMismatch,
        .actual  = SignalMeta{"Engine", "Historian", "recordCommand"},
        .payload = p
    };
    auto text = SignalMismatchFormatter::format(m);
    expectContains(text, "payload.data.size");
    expectContains(text, "payload.data[2]");
    expectContains(text, "payload.data[3]");
    expectContains(text, "<missing>");
}

TEST_F(SignalMismatchFormatterTest, TypeMismatch_ShowsBothTypes) {
    PayloadMismatch p{"CommandHistory", "EngineSnapshot", {}};
    SignalMismatch m{
        .kind    = SignalMismatchKind::PayloadMismatch,
        .actual  = SignalMeta{"Engine", "Historian", "recordCommand"},
        .payload = p
    };
    auto text = SignalMismatchFormatter::format(m);
    expectContains(text, "CommandHistory");
    expectContains(text, "EngineSnapshot");
    expectContains(text, "payload.type");
    expectAbsent(text, "payload.commandName");  // no per-field entries for type mismatch
}

TEST_F(SignalMismatchFormatterTest, UnexpectedExtra_ContainsUnexpectedSignal) {
    SignalMismatch m{
        .kind     = SignalMismatchKind::UnexpectedExtra,
        .expected = std::nullopt,
        .actual   = SignalMeta{"Engine", "Historian", "recordCommand"},
    };
    auto text = SignalMismatchFormatter::format(m);
    expectContains(text, "Unexpected signal");
    expectContains(text, "(none");
    expectContains(text, "recordCommand");
    expectAbsent(text, "Expected signal:\n  from");  // "none" section, not full meta
}

TEST_F(SignalMismatchFormatterTest, PayloadMismatch_EmptyFields_NoPayloadLines) {
    PayloadMismatch p{"EngineSnapshot", "EngineSnapshot", {}};
    SignalMismatch m{
        .kind    = SignalMismatchKind::PayloadMismatch,
        .actual  = SignalMeta{"Engine", "Historian", "publishSnapshot"},
        .payload = p
    };
    auto text = SignalMismatchFormatter::format(m);
    expectAbsent(text, "payload.running");
    expectAbsent(text, "payload.strategy");
}
```

### C. `ScenarioVerifierTest` — integration (few tests)

Only this fixture uses `ScopedFakeTestPartResultReporter`. Confirms that
`compareSignals` → `ScenarioVerifier` → formatter → `ADD_FAILURE` pipeline works
end-to-end. Verifies the substrings that existing tests rely on.

```cpp
class ScenarioVerifierTest : public ::testing::Test {
protected:
    ScenarioVerifier verifier_;

    std::string captureFailure(std::function<void()> action) {
        testing::TestPartResultArray results;
        {
            testing::ScopedFakeTestPartResultReporter reporter(
                testing::ScopedFakeTestPartResultReporter::INTERCEPT_ALL_THREADS,
                &results);
            action();
        }
        if (results.size() == 0) {
            ADD_FAILURE() << "Expected a failure but none was reported";
            return {};
        }
        return results.GetTestPartResult(0).message();
    }

    static void expectContains(const std::string& text, std::string_view sub) {
        EXPECT_NE(text.find(sub), std::string::npos)
            << "Expected: \"" << sub << "\"\nIn:\n" << text;
    }

    static SignalDescriptor historianCommand(std::string name,
                                             std::vector<int> data = {}) {
        return {Endpoint::Engine, Endpoint::Historian, "recordCommand",
                std::any{CommandHistory{std::move(name), std::move(data)}}};
    }
    static SignalDescriptor factoryCreate(SortStrategyId id) {
        return {Endpoint::Engine, Endpoint::Factory, "create", std::any{id}};
    }
};

// Mode 1: metadata mismatch → "Unexpected signal" + field diff in ADD_FAILURE.
TEST_F(ScenarioVerifierTest, Integration_MetadataMismatch_ReportsFields) {
    verifier_.setExpected({expectHistorianCommand("addVector")});
    auto msg = captureFailure([&]{
        verifier_.report(factoryCreate(SortStrategyId::Ascending));
    });
    expectContains(msg, "Unexpected signal");
    expectContains(msg, "field [to]");
    expectContains(msg, "Historian");
    expectContains(msg, "Factory");
}

// Mode 1: payload mismatch → "payload mismatch" + vector element diff.
TEST_F(ScenarioVerifierTest, Integration_PayloadMismatch_VectorDiff) {
    verifier_.setExpected({expectHistorianCommand("addVector", {{1,2,3}})});
    auto msg = captureFailure([&]{
        verifier_.report(historianCommand("addVector", {1,2,99}));
    });
    expectContains(msg, "payload mismatch");
    expectContains(msg, "data[2]");
    expectContains(msg, "99");
}

// Mode 1: unexpected extra → "Unexpected signal" + "(none".
TEST_F(ScenarioVerifierTest, Integration_UnexpectedExtra) {
    verifier_.setExpected({});
    auto msg = captureFailure([&]{
        verifier_.report(historianCommand("addVector"));
    });
    expectContains(msg, "Unexpected signal");
    expectContains(msg, "(none");
}

// Mode 2: payload mismatch in matchExpectation → "payload mismatch" + field diff.
TEST_F(ScenarioVerifierTest, Integration_Mode2_PayloadDiff) {
    verifier_.beginStep();
    verifier_.report(historianCommand("addVector", {9,9,9}));
    verifier_.endStepCollection();
    auto msg = captureFailure([&]{
        verifier_.matchExpectation(expectHistorianCommand("addVector", {{1,2,3}}));
    });
    expectContains(msg, "payload mismatch");
    expectContains(msg, "data[0]");
}
```

### D. Existing tests — NO changes

`ScenarioFrameworkTest`, `EndpointApiTest`, `EngineComponentTest`,
`HistorianOnlyTest`, `FactoryOnlyTest` — all unchanged.

Their `EXPECT_NONFATAL_FAILURE` assertions check for `"Unexpected signal"`,
`"payload mismatch"`, and `"Signal not received"`. The new formatter output preserves
all three substrings. All existing tests pass after D9.

---

## 16. Backward Compatibility

| Concern | Resolution |
|---|---|
| `payloadMatcher` type change | Compiler error at all old lambda sites; D6–D9 atomic commit fixes all |
| `"Unexpected signal"` substring | Preserved: `formatMetadata()` and `formatExtra()` both start with `"Unexpected signal"` |
| `"payload mismatch"` substring | Preserved: `formatPayload()` starts with `"Signal payload mismatch"` |
| `"Signal not received"` | Unchanged — produced by `ScenarioVerifier` directly, not the formatter |
| `"Expectation ... before any stimulus"` | Unchanged — produced by `ScenarioExecutor` directly |
| `historian.publishSnapshot(1)` | `size_t` convenience overload resolves correctly |
| `historian.publishSnapshot()` no-arg | Calls `publishSnapshot(ExpectedEngineSnapshot{})` — all nullopt, any snapshot accepted |
| `expectHistorianSnapshot()` | Calls primary overload with default `ExpectedEngineSnapshot{}` — all don't-care |
| `expectHistorianSnapshot(nullopt)` | `std::optional<size_t>` overload → vectorCount nullopt → don't-care |
| `Scenarios::PublishSnapshot(vc)` | Calls `expectHistorianSnapshot(vc)` where `vc` is `std::optional<size_t>` → convenience overload ✓ |
| `EngineDriver.hpp` | Unchanged |
| `ScenarioExecutor.hpp` | Unchanged |
| `CMakeLists.txt` | Unchanged — no new link libraries, no GMock |

---

## 17. Acceptance Criteria

1. Normal component tests remain declarative:
   ```cpp
   engine.receive(engine.addVector({1, 2, 3}));
   historian.receive(historian.addVector({1, 2, 3}));
   ```
   No `compareSignals`, `SignalMismatch`, `captureNextFailure`, formatter in `TEST_F`.

2. `payloadMatcher` returns `PayloadMatchResult`, not `bool`.

3. Metadata mismatch shows which fields (`from`, `to`, `name`) differ with
   expected/actual values.

4. Payload mismatch shows concrete field values including per-element vector diffs
   (`data.size`, `data[i]`, `<missing>`).

5. `CommandHistory` reports multiple field mismatches simultaneously.

6. `EngineSnapshot` supports partial expectation (`running` / `strategy` /
   `vectorCount`); unchecked fields are absent from diagnostic output.

7. Wrong payload type shows expected type name and actual type name.

8. Extra signal (no expectation) uses `SignalMismatchKind::UnexpectedExtra` —
   no `"(extra)"` hack or fake field injection.

9. `ScenarioVerifier` includes no domain headers (`IHistorian.hpp`,
   `ISortStrategyFactory.hpp`, `SortStrategyId.hpp`).

10. `PayloadMismatch.hpp` and `SignalMismatch.hpp` include no project domain headers.

11. `SignalMismatchFormatter.hpp` includes no domain headers.

12. Dependency graph is acyclic — proven in §11.

13. Existing `sortStrategyIdName()` from `SortStrategyId.hpp` is used; no duplicate
    helper function. Covers `Ascending`, `Descending`, `Bubble`.

14. `SignalComparatorTest` tests structured data directly without string parsing.

15. `SignalMismatchFormatterTest` tests formatted strings without `ADD_FAILURE` capture.

16. Only `ScenarioVerifierTest` (4 tests) uses `ScopedFakeTestPartResultReporter`.

17. No GMock dependency — `CMakeLists.txt` unchanged.

18. All existing tests pass after D9. New tests (D10–D12) pass additionally.

19. Zero new warnings at current project warning flags.

20. Output uses unified Expected / Actual / Mismatches model across all mismatch kinds.

---

## 18. Final Public API Examples

These are the actual test scenarios a developer writes. No internal types appear.

### Example 1 — AddVector

```cpp
TEST_F(EngineComponentTest, AddVector) {
    engine.receive(engine.addVector({1, 2, 3}));
    historian.receive(historian.addVector({1, 2, 3}));
}
```

If Engine sends `data = {1, 2, 99}`, the framework automatically reports:
```
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Actual signal:
  from:         Engine
  to:           Historian
  name:         recordCommand
  payload type: CommandHistory

Mismatches:
  payload.data[2]:
    expected: 3
    actual:   99
```

No manual diagnostic code in the test.

### Example 2 — SetStrategy with factory and historian

```cpp
TEST_F(EngineComponentTest, SetStrategy) {
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());
}
```

If `factory.create()` is called with `Ascending` instead of `Descending`:
```
Signal payload mismatch

Expected signal:
  from:         Engine
  to:           Factory
  name:         create
  payload type: SortStrategyId

Actual signal:
  from:         Engine
  to:           Factory
  name:         create
  payload type: SortStrategyId

Mismatches:
  payload.value:
    expected: Descending
    actual:   Ascending
```

### Example 3 — PublishSnapshot with full partial expectation

```cpp
TEST_F(EngineComponentTest, PublishSnapshot_FullSpec) {
    engine_->addVector({1, 2, 3});
    engine.receive(engine.strategyChange(SortStrategyId::Descending));
    factory.receive(factory.create(SortStrategyId::Descending));
    historian.receive(historian.setSortStrategy());

    engine.receive(engine.publishSnapshot());
    historian.receive(historian.publishSnapshot({
        .running     = true,
        .strategy    = SortStrategyId::Descending,
        .vectorCount = 1
    }));
}
```

If `vectorCount` is wrong (e.g., Engine reports `0`), only that field appears:
```
Mismatches:
  payload.vectorCount:
    expected: 1
    actual:   0
```

If `running` is wrong but not in the spec, it is not reported.
All diagnostic logic runs automatically — the test body stays declarative.
