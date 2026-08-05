#pragma once
#include <vector>
#include "SortStrategyId.hpp"

namespace patterns::strategy {

class ISortStrategy {
public:
    virtual ~ISortStrategy() = default;

    virtual void operator()(std::vector<int>& data) const = 0;
    virtual const char*    name() const = 0;
    virtual SortStrategyId id()   const = 0;
};

} // namespace patterns::strategy
