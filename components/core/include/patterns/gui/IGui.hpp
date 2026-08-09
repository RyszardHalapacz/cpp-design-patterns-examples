#pragma once
#include <vector>
#include "patterns/strategy/SortStrategyId.hpp"

namespace patterns::gui {

// ==================================
// IGUI — Adapter target interface
// Application communicates only through this interface;
// it does not know which concrete GUI library is underneath.
// ==================================
class IGui {
public:
    virtual ~IGui() = default;

    virtual void  clickAddVector      (const std::vector<int>& vec) = 0;
    virtual void  clickSortVector     (size_t index)                = 0;
    virtual void  clickPrintData      ()                            = 0;
    virtual void  clickSetSortStrategy(patterns::strategy::SortStrategyId id) = 0;

    virtual IGui& queueAddVector (const std::vector<int>& vec) = 0;
    virtual IGui& queueSortVector(size_t index)                = 0;
    virtual IGui& queuePrintData ()                            = 0;
    virtual void  flushBatch     ()                            = 0;
};

} // namespace patterns::gui
