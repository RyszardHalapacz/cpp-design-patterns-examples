#pragma once
#include <memory>
#include "ISortStrategy.hpp"

namespace patterns::strategy {

class SortStrategyFactory {
public:
    static std::unique_ptr<ISortStrategy> create(SortStrategyId id);
};

} // namespace patterns::strategy
