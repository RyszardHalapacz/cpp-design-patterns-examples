#include "patterns/gui/DummyGuiAdapter.hpp"

namespace patterns::gui {

DummyGuiAdapter::DummyGuiAdapter(std::filesystem::path manifestPath)
    : gui_(makeGUI(std::move(manifestPath))) {}

DummyGuiAdapter::~DummyGuiAdapter() {
    deleteGUI(gui_);
}

void DummyGuiAdapter::connectAddVector(DummyGui::AddVectorFunc f) {
    gui_->connectAddVector(std::move(f));
}
void DummyGuiAdapter::connectSortVector(DummyGui::SortVectorFunc f) {
    gui_->connectSortVector(std::move(f));
}
void DummyGuiAdapter::connectPrintData(DummyGui::PrintDataFunc f) {
    gui_->connectPrintData(std::move(f));
}
void DummyGuiAdapter::connectSetSortStrategy(DummyGui::SetSortStrategyFunc f) {
    gui_->connectSetSortStrategy(std::move(f));
}

void DummyGuiAdapter::clickAddVector(const std::vector<int>& vec) {
    gui_->clickAddVector(vec);
}
void DummyGuiAdapter::clickSortVector(size_t index) {
    gui_->clickSortVector(index);
}
void DummyGuiAdapter::clickPrintData() {
    gui_->clickPrintData();
}
void DummyGuiAdapter::clickSetSortStrategy(patterns::strategy::SortStrategyId id) {
    gui_->clickSetSortStrategy(id);
}

IGui& DummyGuiAdapter::queueAddVector(const std::vector<int>& vec) {
    gui_->queueAddVector(vec);
    return *this;
}
IGui& DummyGuiAdapter::queueSortVector(size_t index) {
    gui_->queueSortVector(index);
    return *this;
}
IGui& DummyGuiAdapter::queuePrintData() {
    gui_->queuePrintData();
    return *this;
}
void DummyGuiAdapter::flushBatch() {
    gui_->flushBatch();
}
CommandBatch DummyGuiAdapter::buildBatch() {
    return gui_->buildBatch();
}

} // namespace patterns::gui
