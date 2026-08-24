#pragma once
#include "patterns/gui/ICommand.hpp"

namespace patterns::gui {

// ==================================
// COMMAND BATCH BUILDER
// Fluent builder — GUI collects ICommand objects instead of sending immediately.
// Each method receives the callback + data and creates the concrete command.
// ==================================
class CommandBatchBuilder {
public:
    using AddVectorFn  = AddVectorCommand::Fn;
    using SortVectorFn = SortVectorCommand::Fn;
    using PrintDataFn  = PrintDataCommand::Fn;

    CommandBatchBuilder& addVector(AddVectorFn fn, const std::vector<int>& vec);
    CommandBatchBuilder& sortVector(SortVectorFn fn, size_t index);
    CommandBatchBuilder& printData(PrintDataFn fn);

    [[nodiscard]] CommandBatch build();  // returns finished batch and clears builder

private:
    CommandBatch commands_;
};

} // namespace patterns::gui
