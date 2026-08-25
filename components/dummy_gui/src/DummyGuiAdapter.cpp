#include "patterns/gui/DummyGuiAdapter.hpp"

namespace patterns::gui {

DummyGuiAdapter::DummyGuiAdapter(std::filesystem::path manifestPath)
    : gui_(makeGUI<>(std::move(manifestPath))) {}

// ── Wiring: delegate to DummyGui's register*Handler API ──────────────────────

void DummyGuiAdapter::registerAddVectorHandler(DummyGui::AddVectorFunc f) {
    gui_->registerAddVectorHandler(std::move(f));
}
void DummyGuiAdapter::registerSortVectorHandler(DummyGui::SortVectorFunc f) {
    gui_->registerSortVectorHandler(std::move(f));
}
void DummyGuiAdapter::registerPrintDataHandler(DummyGui::PrintDataFunc f) {
    gui_->registerPrintDataHandler(std::move(f));
}
void DummyGuiAdapter::registerStrategyHandler(DummyGui::SetSortStrategyFunc f) {
    gui_->registerStrategyHandler(std::move(f));
}

// ── IGui → DummyGui translation ───────────────────────────────────────────────

void DummyGuiAdapter::clickAddVector(const std::vector<int>& vec) {
    gui_->onAddVectorClicked(vec);
}
void DummyGuiAdapter::clickSortVector(size_t index) {
    gui_->onSortVectorClicked(index);
}
void DummyGuiAdapter::clickPrintData() {
    gui_->onPrintDataClicked();
}
void DummyGuiAdapter::clickSetSortStrategy(patterns::strategy::SortStrategyId id) {
    gui_->onStrategySelected(id);
}

IGui& DummyGuiAdapter::queueAddVector(const std::vector<int>& vec) {
    gui_->scheduleAddVector(vec);
    return *this;
}
IGui& DummyGuiAdapter::queueSortVector(size_t index) {
    gui_->scheduleSortVector(index);
    return *this;
}
IGui& DummyGuiAdapter::queuePrintData() {
    gui_->schedulePrint();
    return *this;
}
void DummyGuiAdapter::flushBatch() {
    gui_->dispatchScheduled();
}
CommandBatch DummyGuiAdapter::buildBatch() {
    return gui_->collectBatch();
}

} // namespace patterns::gui
