#include "patterns/config/Configurator.hpp"
#include "patterns/gui/DummyGui.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::config {

using patterns::services::logApp;

void Configurator::configureGui(patterns::gui::DummyGui& gui,
                                const std::shared_ptr<patterns::session::SessionManagement>& session) {
    logApp("[Configurator] Granting GUI access to selected functions\n");
    gui.connectAddVector      (session, &patterns::session::SessionManagement::addVectorFromGui);
    gui.connectSortVector     (session, &patterns::session::SessionManagement::sortVectorFromGui);
    gui.connectPrintData      (session, &patterns::session::SessionManagement::printDataFromGui);
    gui.connectExecuteBatch   (session, &patterns::session::SessionManagement::executeBatch);
    gui.connectSetSortStrategy(session, &patterns::session::SessionManagement::setSortStrategyFromGui);
}

void Configurator::configureAllowedStrategies(patterns::session::SessionManagement& session,
                                              std::vector<patterns::strategy::SortStrategyId> allowed) {
    logApp("[Configurator] Setting allowed sort strategies\n");
    session.setAllowedStrategies(std::move(allowed));
}

} // namespace patterns::config
