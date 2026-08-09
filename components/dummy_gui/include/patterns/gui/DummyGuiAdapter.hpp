#pragma once
#include <filesystem>
#include <vector>
#include "patterns/gui/IGui.hpp"
#include "patterns/gui/DummyGui.hpp"

namespace patterns::gui {

// ==================================
// DUMMYGUIADAPTER — Adapter
// Target interface : IGui           (what Application knows)
// Adaptee          : DummyGui       (C-style API: makeGUI / deleteGUI)
// Adapter          : DummyGuiAdapter (bridges the two)
//
// Application holds unique_ptr<IGui> and never sees DummyGui.
// Configurator wires callbacks via connect*() before the adapter
// is handed to Application as IGui.
// ==================================
class DummyGuiAdapter : public IGui {
public:
    explicit DummyGuiAdapter(std::filesystem::path manifestPath = {});

    // ── Wiring API (called by Configurator) ───────────────────────────────
    void connectAddVector      (DummyGui::AddVectorFunc       f);
    void connectSortVector     (DummyGui::SortVectorFunc      f);
    void connectPrintData      (DummyGui::PrintDataFunc       f);
    void connectSetSortStrategy(DummyGui::SetSortStrategyFunc f);

    // ── IGui interface ────────────────────────────────────────────────────
    void  clickAddVector      (const std::vector<int>& vec)           override;
    void  clickSortVector     (size_t index)                          override;
    void  clickPrintData      ()                                      override;
    void  clickSetSortStrategy(patterns::strategy::SortStrategyId id) override;
    IGui& queueAddVector      (const std::vector<int>& vec)           override;
    IGui& queueSortVector     (size_t index)                          override;
    IGui& queuePrintData      ()                                      override;
    void  flushBatch          ()                                      override;

    // ── Extended API (not in IGui, used by tests) ─────────────────────────
    CommandBatch buildBatch();

private:
    std::unique_ptr<DummyGui> gui_;  // default_delete<DummyGui> calls deleteGUI
};

} // namespace patterns::gui
