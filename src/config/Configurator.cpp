#include "patterns/config/Configurator.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::config {

using patterns::services::logApp;
using patterns::strategy::SortStrategyId;

void Configurator::configureGui(patterns::gui::DummyGuiAdapter& gui,
                                const std::shared_ptr<patterns::session::SessionManagement>& session) {
    logApp("[Configurator] Granting GUI access to selected functions\n");
    std::weak_ptr<patterns::session::SessionManagement> weak = session;

    gui.registerAddVectorHandler([weak](const std::vector<int>& vec) {
        if (auto s = weak.lock()) s->addVectorFromGui(vec);
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.registerSortVectorHandler([weak](size_t index) {
        if (auto s = weak.lock()) s->sortVectorFromGui(index);
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.registerPrintDataHandler([weak]() {
        if (auto s = weak.lock()) s->printDataFromGui();
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.registerStrategyHandler([weak](SortStrategyId id) {
        if (auto s = weak.lock()) s->setSortStrategyFromGui(id);
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
}

void Configurator::configureAllowedStrategies(patterns::session::SessionManagement& session,
                                              std::vector<patterns::strategy::SortStrategyId> allowed) {
    logApp("[Configurator] Setting allowed sort strategies\n");
    session.setAllowedStrategies(std::move(allowed));
}

} // namespace patterns::config
