#pragma once
#include "ISortStrategyFactory.hpp"

namespace patterns::strategy {

class SortStrategyFactory : public ISortStrategyFactory {
public:
    [[nodiscard]] std::expected<std::unique_ptr<ISortStrategy>, std::string>
    create(SortStrategyId id) override;
};

} // namespace patterns::strategy
