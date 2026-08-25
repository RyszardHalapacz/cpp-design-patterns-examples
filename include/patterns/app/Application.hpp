#pragma once
#include <filesystem>
#include <memory>
#include "patterns/engine/Engine.hpp"
#include "patterns/session/SessionManagement.hpp"
#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/gui/IGui.hpp"
#include "patterns/config/Configurator.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/historian/EngineHistorian.hpp"

namespace patterns::app {

// ==================================
// APPLICATION
// Owns all top-level objects; wires them together in configure(),
// then drives the main flow in run().
// ==================================
class Application {
public:
    explicit Application(std::filesystem::path exeDir);

    void configure();
    void run();

private:
    std::filesystem::path                                 exeDir_;
    std::shared_ptr<patterns::engine::Engine>                  engine_;
    std::shared_ptr<patterns::strategy::ISortStrategyFactory>  factory_;
    std::shared_ptr<patterns::historian::EngineHistorian>      historian_;
    std::shared_ptr<patterns::session::SessionManagement>      session_;
    std::shared_ptr<patterns::session::SessionAuditObserver>   audit_;
    std::unique_ptr<patterns::gui::IGui>                       gui_;
    patterns::config::Configurator                             configurator_;
};

} // namespace patterns::app
