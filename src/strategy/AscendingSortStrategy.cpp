#include "patterns/strategy/AscendingSortStrategy.hpp"
#include <algorithm>

namespace patterns::strategy {

void AscendingSortStrategy::operator()(std::vector<int>& data) const {
    std::sort(data.begin(), data.end());
}

const char*    AscendingSortStrategy::name() const { return "AscendingSort (std::sort)"; }
SortStrategyId AscendingSortStrategy::id()   const { return SortStrategyId::Ascending; }

} // namespace patterns::strategy
