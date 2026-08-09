#include "patterns/config/Configurator.hpp"
#include "patterns/gui/DummyGui.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/services/ServiceLocator.hpp"

namespace patterns::config {

using patterns::services::logApp;
using patterns::gui::CommandBatch;
using patterns::strategy::SortStrategyId;

void Configurator::configureGui(patterns::gui::DummyGui& gui,
                                const std::shared_ptr<patterns::session::SessionManagement>& session) {
    logApp("[Configurator] Granting GUI access to selected functions\n");
    std::weak_ptr<patterns::session::SessionManagement> weak = session;

    gui.connectAddVector([weak](const std::vector<int>& vec) {
        if (auto s = weak.lock()) s->addVectorFromGui(vec);
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.connectSortVector([weak](size_t index) {
        if (auto s = weak.lock()) s->sortVectorFromGui(index);
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.connectPrintData([weak]() {
        if (auto s = weak.lock()) s->printDataFromGui();
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.connectExecuteBatch([weak](const CommandBatch& batch) {
        if (auto s = weak.lock()) s->executeBatch(batch);
        else logApp("[GUI] SessionManagement no longer exists — operation cancelled\n");
    });
    gui.connectSetSortStrategy([weak](SortStrategyId id) {
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
