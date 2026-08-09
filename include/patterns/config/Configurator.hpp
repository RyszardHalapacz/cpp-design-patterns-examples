#pragma once
#include <memory>
#include <vector>
#include "patterns/strategy/SortStrategyId.hpp"

#include "patterns/gui/DummyGuiAdapter.hpp"
#include "patterns/session/SessionManagement.hpp"

namespace patterns::config {

// ==================================
// CONFIGURATOR
// Single place that decides: which GUI functions can be called
// and which sort strategies may be swapped at runtime.
// ==================================
class Configurator {
public:
    void configureGui(patterns::gui::DummyGuiAdapter& gui,
                      const std::shared_ptr<patterns::session::SessionManagement>& session);

    void configureAllowedStrategies(patterns::session::SessionManagement& session,
                                    std::vector<patterns::strategy::SortStrategyId> allowed);
};

} // namespace patterns::config
