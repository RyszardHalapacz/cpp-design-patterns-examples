#pragma once
#include "Command.hpp"
#include "CommandBatchBuilder.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

namespace patterns::gui {

// ==================================
// DUMMY GUI
// Simulates a GUI; holds pointers to SessionManagement methods
// granted by the Configurator — only what Configurator connected works.
// ==================================
class DummyGui {
public:
    using AddVectorFunc       = void (patterns::session::SessionManagement::*)(const std::vector<int>&);
    using SortVectorFunc      = void (patterns::session::SessionManagement::*)(size_t);
    using PrintDataFunc       = void (patterns::session::SessionManagement::*)();
    using ExecuteBatchFunc    = void (patterns::session::SessionManagement::*)(const CommandBatch&);
    using SetSortStrategyFunc = void (patterns::session::SessionManagement::*)(patterns::strategy::SortStrategyId);

    void connectAddVector      (patterns::session::SessionManagement* session, AddVectorFunc       func);
    void connectSortVector     (patterns::session::SessionManagement* session, SortVectorFunc      func);
    void connectPrintData      (patterns::session::SessionManagement* session, PrintDataFunc       func);
    void connectExecuteBatch   (patterns::session::SessionManagement* session, ExecuteBatchFunc    func);
    void connectSetSortStrategy(patterns::session::SessionManagement* session, SetSortStrategyFunc func);

    void clickAddVector      (const std::vector<int>& vec);
    void clickSortVector     (size_t index);
    void clickPrintData      ();
    void clickSetSortStrategy(patterns::strategy::SortStrategyId id);

    // Builder — queuing commands instead of sending them immediately
    DummyGui&    queueAddVector (const std::vector<int>& vec);
    DummyGui&    queueSortVector(size_t index);
    DummyGui&    queuePrintData ();
    CommandBatch buildBatch     ();
    void         flushBatch     ();

private:
    patterns::session::SessionManagement* session_ = nullptr;

    AddVectorFunc       addVectorFunc_       = nullptr;
    SortVectorFunc      sortVectorFunc_      = nullptr;
    PrintDataFunc       printDataFunc_       = nullptr;
    ExecuteBatchFunc    executeBatchFunc_    = nullptr;
    SetSortStrategyFunc setSortStrategyFunc_ = nullptr;

    CommandBatchBuilder batchBuilder_;
};

} // namespace patterns::gui
