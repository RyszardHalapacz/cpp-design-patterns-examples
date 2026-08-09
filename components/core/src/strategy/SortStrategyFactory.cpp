#include "patterns/strategy/SortStrategyFactory.hpp"
#include "patterns/strategy/AscendingSortStrategy.hpp"
#include "patterns/strategy/DescendingSortStrategy.hpp"
#include "patterns/strategy/BubbleSortStrategy.hpp"
#include <expected>

namespace patterns::strategy {

std::expected<std::unique_ptr<ISortStrategy>, std::string> SortStrategyFactory::create(SortStrategyId id) {
    switch (id) {
        case SortStrategyId::Ascending:
            return std::make_unique<AscendingSortStrategy>();
        case SortStrategyId::Descending:
            return std::make_unique<DescendingSortStrategy>();
        case SortStrategyId::Bubble:
            return std::make_unique<BubbleSortStrategy>();
    }
    return std::unexpected("SortStrategyFactory: unknown SortStrategyId");
}

} // namespace patterns::strategy
