#include "patterns/app/Application.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/session/SessionEstablisher.hpp"
#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/manifest/ManifestWriter.hpp"

namespace patterns::app {

using namespace patterns::services;
using patterns::strategy::SortStrategyId;

Application::Application(std::filesystem::path exeDir)
    : exeDir_(std::move(exeDir)) {}

void Application::configure() {
    // SERVICE LOCATOR — register services once at startup
    ServiceLocator::instance().provide<Logger>    (std::make_shared<Logger>());
    ServiceLocator::instance().provide<FileLogger>(std::make_shared<FileLogger>("engine_log.txt"));
    ServiceLocator::instance().provideRuntime     (std::make_shared<DoSomething>());

    logFile("=== Program start ===\n");
    if (auto r = appDoSomethingByPointer()) r->get().do_();

    patterns::manifest::ComponentManifestWriter appWriter;
    appWriter.write(exeDir_ / "application.yml", "patterns", "PatternsApp", APP_VERSION_STR);
    appWriter.printManifest(exeDir_ / "application.yml");

    engine_  = std::make_shared<patterns::engine::Engine>(exeDir_ / "src" / "engine" / "engine.yml");
    session_ = std::make_shared<patterns::session::SessionManagement>();
    gui_     = std::unique_ptr<patterns::gui::DummyGui>(
                   patterns::gui::makeGUI(std::filesystem::path(DUMMY_GUI_MANIFEST_PATH)));

    gui_->clickAddVector({3, 1, 2});  // no access — Configurator not yet connected

    // TEMPLATE METHOD — session establishment via EngineSessionEstablisher
    patterns::session::EngineSessionEstablisher establisher(*session_, engine_);
    if (auto r = establisher.establish(); !r)
        logApp("[App] Session establishment failed: " + r.error() + "\n");

    configurator_.configureGui(*gui_, session_);
    configurator_.configureAllowedStrategies(*session_, {SortStrategyId::Ascending,
                                                         SortStrategyId::Descending});

    // OBSERVER — owned by Application; lifetime no longer tied to session
    audit_ = std::make_shared<patterns::session::SessionAuditObserver>();
    session_->attach(audit_);
}

void Application::run() {
    gui_->clickAddVector({3, 1, 2});
    gui_->clickAddVector({9, 5, 7});
    gui_->clickPrintData();

    // Default strategy (AscendingSort) — set in Engine constructor
    gui_->clickSortVector(0);
    gui_->clickPrintData();

    // STRATEGY — swap to an allowed strategy
    gui_->clickSetSortStrategy(SortStrategyId::Descending);
    gui_->clickSortVector(1);
    gui_->clickPrintData();

    // Attempt to swap to a strategy outside the whitelist — will be rejected
    gui_->clickSetSortStrategy(SortStrategyId::Bubble);
    gui_->clickAddVector({5, 4, 3, 2, 1});
    gui_->clickSortVector(2);  // still Descending — Bubble was not accepted
    gui_->clickPrintData();

    // BUILDER — GUI collects commands into a batch, sends all at once
    gui_->queueAddVector({8, 6, 4})
        .queueSortVector(3)
        .queuePrintData();
    gui_->flushBatch();

    // Close session -> SessionClosing to all observers
    session_->closeSession();

    gui_->clickAddVector({100, 200});  // session closed

    logFile("=== Program end ===\n");
}

} // namespace patterns::app
