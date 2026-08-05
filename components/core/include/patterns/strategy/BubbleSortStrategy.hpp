#pragma once
#include "ISortStrategy.hpp"

namespace patterns::strategy {

class BubbleSortStrategy : public ISortStrategy {
public:
    void           operator()(std::vector<int>& data) const override;
    const char*    name() const override;
    SortStrategyId id()   const override;
};

} // namespace patterns::strategy
