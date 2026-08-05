#include "patterns/strategy/SortStrategyFactory.hpp"
#include "patterns/strategy/AscendingSortStrategy.hpp"
#include "patterns/strategy/DescendingSortStrategy.hpp"
#include "patterns/strategy/BubbleSortStrategy.hpp"
#include <stdexcept>

namespace patterns::strategy {

std::unique_ptr<ISortStrategy> SortStrategyFactory::create(SortStrategyId id) {
    switch (id) {
        case SortStrategyId::Ascending:
            return std::make_unique<AscendingSortStrategy>();
        case SortStrategyId::Descending:
            return std::make_unique<DescendingSortStrategy>();
        case SortStrategyId::Bubble:
            return std::make_unique<BubbleSortStrategy>();
    }
    throw std::runtime_error("SortStrategyFactory: unknown SortStrategyId");
}

} // namespace patterns::strategy
