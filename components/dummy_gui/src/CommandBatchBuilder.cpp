#include "patterns/gui/CommandBatchBuilder.hpp"

namespace patterns::gui {

CommandBatchBuilder& CommandBatchBuilder::addVector(const std::vector<int>& vec) {
    commands_.push_back(Command{CommandType::AddVector, vec, 0});
    return *this;
}

CommandBatchBuilder& CommandBatchBuilder::sortVector(size_t index) {
    commands_.push_back(Command{CommandType::SortVector, {}, index});
    return *this;
}

CommandBatchBuilder& CommandBatchBuilder::printData() {
    commands_.push_back(Command{CommandType::PrintData, {}, 0});
    return *this;
}

CommandBatch CommandBatchBuilder::build() {
    CommandBatch result = std::move(commands_);
    commands_.clear();
    return result;
}

} // namespace patterns::gui
