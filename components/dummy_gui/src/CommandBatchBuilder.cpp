#include "patterns/gui/CommandBatchBuilder.hpp"

namespace patterns::gui {

CommandBatchBuilder& CommandBatchBuilder::addVector(AddVectorFn fn, const std::vector<int>& vec) {
    commands_.push_back(std::make_unique<AddVectorCommand>(std::move(fn), vec));
    return *this;
}

CommandBatchBuilder& CommandBatchBuilder::sortVector(SortVectorFn fn, size_t index) {
    commands_.push_back(std::make_unique<SortVectorCommand>(std::move(fn), index));
    return *this;
}

CommandBatchBuilder& CommandBatchBuilder::printData(PrintDataFn fn) {
    commands_.push_back(std::make_unique<PrintDataCommand>(std::move(fn)));
    return *this;
}

CommandBatch CommandBatchBuilder::build() {
    return std::move(commands_);
}

} // namespace patterns::gui
