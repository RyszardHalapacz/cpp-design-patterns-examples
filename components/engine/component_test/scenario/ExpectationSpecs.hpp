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
