#include "patterns/strategy/BubbleSortStrategy.hpp"

namespace patterns::strategy {

void BubbleSortStrategy::operator()(std::vector<int>& data) const {
    if (data.empty()) return;

    for (size_t i = 0; i < data.size() - 1; ++i) {
        for (size_t j = 0; j < data.size() - 1 - i; ++j) {
            if (data[j] > data[j + 1]) {
                std::swap(data[j], data[j + 1]);
            }
        }
    }
}

const char*    BubbleSortStrategy::name() const { return "BubbleSort (manual)"; }
SortStrategyId BubbleSortStrategy::id()   const { return SortStrategyId::Bubble; }

} // namespace patterns::strategy
