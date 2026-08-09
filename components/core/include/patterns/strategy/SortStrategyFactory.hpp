#pragma once
#include <expected>
#include <memory>
#include <string>
#include "ISortStrategy.hpp"

namespace patterns::strategy {

class SortStrategyFactory {
public:
    static std::expected<std::unique_ptr<ISortStrategy>, std::string> create(SortStrategyId id);
};

} // namespace patterns::strategy
