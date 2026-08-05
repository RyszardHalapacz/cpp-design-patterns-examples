#pragma once

namespace patterns::strategy {

enum class SortStrategyId {
    Ascending,
    Descending,
    Bubble
};

inline const char* sortStrategyIdName(SortStrategyId id) {
    switch (id) {
        case SortStrategyId::Ascending:  return "Ascending";
        case SortStrategyId::Descending: return "Descending";
        case SortStrategyId::Bubble:     return "Bubble";
    }
    return "Unknown";
}

} // namespace patterns::strategy
