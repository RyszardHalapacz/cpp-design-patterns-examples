#include "patterns/strategy/DescendingSortStrategy.hpp"
#include <algorithm>
#include <functional>

namespace patterns::strategy {

void DescendingSortStrategy::operator()(std::vector<int>& data) const {
    std::sort(data.begin(), data.end(), std::greater<int>());
}

const char*    DescendingSortStrategy::name() const { return "DescendingSort (std::sort, greater)"; }
SortStrategyId DescendingSortStrategy::id()   const { return SortStrategyId::Descending; }

} // namespace patterns::strategy
