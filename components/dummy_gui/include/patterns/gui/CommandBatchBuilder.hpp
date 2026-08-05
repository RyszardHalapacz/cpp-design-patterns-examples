#pragma once
#include "patterns/gui/Command.hpp"

namespace patterns::gui {

// ==================================
// COMMAND BATCH BUILDER
// Fluent builder — GUI collects commands instead of sending them immediately.
// ==================================
class CommandBatchBuilder {
public:
    CommandBatchBuilder& addVector(const std::vector<int>& vec);
    CommandBatchBuilder& sortVector(size_t index);
    CommandBatchBuilder& printData();

    CommandBatch build();  // returns finished batch and clears builder

private:
    CommandBatch commands_;
};

} // namespace patterns::gui
