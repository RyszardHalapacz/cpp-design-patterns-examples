#pragma once
#include <expected>
#include <memory>
#include <string>
#include "ISortStrategy.hpp"

namespace patterns::strategy {

class ISortStrategyFactory {
public:
    virtual ~ISortStrategyFactory() = default;

    [[nodiscard]] virtual std::expected<std::unique_ptr<ISortStrategy>, std::string>
    create(SortStrategyId id) = 0;
};

} // namespace patterns::strategy
