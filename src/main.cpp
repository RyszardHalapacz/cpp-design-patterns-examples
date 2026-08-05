#include "patterns/services/ServiceLocator.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/session/SessionEstablisher.hpp"
#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/gui/DummyGui.hpp"
#include "patterns/config/Configurator.hpp"
#include "patterns/strategy/SortStrategyId.hpp"

int main() {
    using namespace patterns::services;
    using patterns::strategy::SortStrategyId;

    // SERVICE LOCATOR — register services once at startup
    ServiceLocator::instance().provide<Logger>    (std::make_shared<Logger>());
    ServiceLocator::instance().provide<FileLogger>(std::make_shared<FileLogger>("engine_log.txt"));
    ServiceLocator::instance().provideRuntime     (std::make_shared<DoSomething>());

    appFileLogger().log("=== Program start ===\n");
    appDoSomethingByPointer().do_();

    patterns::engine::Engine             engine;
    patterns::session::SessionManagement session;
    patterns::gui::DummyGui              gui;
    patterns::config::Configurator       configurator;

    gui.clickAddVector({3, 1, 2});  // no access — Configurator not yet connected

    // TEMPLATE METHOD — session establishment via EngineSessionEstablisher
    patterns::session::EngineSessionEstablisher establisher(session, engine);
    establisher.establish();

    configurator.configureGui(gui, session);
    configurator.configureAllowedStrategies(session, {SortStrategyId::Ascending,
                                                      SortStrategyId::Descending});

    // OBSERVER — extra observer allocated on the heap;
    // destroys itself when session closes (SessionClosing -> delete this)
    auto* audit = new patterns::session::SessionAuditObserver();
    session.attach(audit);

    gui.clickAddVector({3, 1, 2});
    gui.clickAddVector({9, 5, 7});
    gui.clickPrintData();

    // Default strategy (AscendingSort) — set in Engine constructor
    gui.clickSortVector(0);
    gui.clickPrintData();

    // STRATEGY — swap to an allowed strategy
    gui.clickSetSortStrategy(SortStrategyId::Descending);
    gui.clickSortVector(1);
    gui.clickPrintData();

    // Attempt to swap to a strategy outside the whitelist — will be rejected
    gui.clickSetSortStrategy(SortStrategyId::Bubble);
    gui.clickAddVector({5, 4, 3, 2, 1});
    gui.clickSortVector(2);  // still Descending — Bubble was not accepted
    gui.clickPrintData();

    // BUILDER — GUI collects commands into a batch, sends all at once
    gui.queueAddVector({8, 6, 4})
       .queueSortVector(3)
       .queuePrintData();
    gui.flushBatch();

    // Close session -> SessionClosing to all observers
    session.closeSession();

    gui.clickAddVector({100, 200});  // session closed

    appFileLogger().log("=== Program end ===\n");

    return 0;
}
