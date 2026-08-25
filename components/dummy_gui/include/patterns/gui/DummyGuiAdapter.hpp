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
// IGui speaks:  click*, queue*, flushBatch
// DummyGui speaks: on*Clicked, schedule*, dispatchScheduled
//
// Application holds unique_ptr<IGui> and never sees DummyGui.
// Configurator wires callbacks via register*Handler() before the adapter
// is handed to Application as IGui.
// ==================================
class DummyGuiAdapter : public IGui {
public:
    explicit DummyGuiAdapter(std::filesystem::path manifestPath = {});

    // ── Wiring API (called by Configurator) ───────────────────────────────
    void registerAddVectorHandler  (DummyGui::AddVectorFunc       f);
    void registerSortVectorHandler (DummyGui::SortVectorFunc      f);
    void registerPrintDataHandler  (DummyGui::PrintDataFunc       f);
    void registerStrategyHandler   (DummyGui::SetSortStrategyFunc f);

    // ── IGui interface — translated to DummyGui's event/schedule API ──────
    void  clickAddVector      (const std::vector<int>& vec)           override;
    void  clickSortVector     (size_t index)                          override;
    void  clickPrintData      ()                                      override;
    void  clickSetSortStrategy(patterns::strategy::SortStrategyId id) override;
    IGui& queueAddVector      (const std::vector<int>& vec)           override;
    IGui& queueSortVector     (size_t index)                          override;
    IGui& queuePrintData      ()                                      override;
    void  flushBatch          ()                                      override;

    // ── Extended API (not in IGui, used by tests) ─────────────────────────
    [[nodiscard]] CommandBatch buildBatch();

private:
    std::unique_ptr<DummyGui> gui_;  // default_delete<DummyGui> calls deleteGUI
};

} // namespace patterns::gui
