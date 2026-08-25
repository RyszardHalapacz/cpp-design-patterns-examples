#pragma once
#include <expected>
#include <memory>
#include <string>
#include "patterns/engine/Engine.hpp"
#include "patterns/strategy/ISortStrategyFactory.hpp"
#include "patterns/historian/EngineHistorian.hpp"

namespace patterns::session {

class SessionManagement;

// ==================================
// SESSION COORDINATOR — Template Method
// Fixed algorithm skeleton for session coordination;
// subclasses provide concrete steps.
// C++23: establish() returns std::expected — monadic and_then/or_else chain.
// ==================================
class SessionCoordinator {
public:
    virtual ~SessionCoordinator() = default;

    [[nodiscard]] std::expected<void, std::string> establish();  // non-overridable skeleton

protected:
    [[nodiscard]] virtual std::expected<void, std::string> checkPreconditions() { return {}; }
    [[nodiscard]] virtual std::expected<void, std::string> configure()          { return {}; }
    [[nodiscard]] virtual std::expected<void, std::string> connect()       = 0;
    [[nodiscard]] virtual std::expected<void, std::string> finalizeSetup() = 0;
};

// Concrete implementation: connects SessionManagement to Engine
class EngineSessionCoordinator : public SessionCoordinator {
public:
    EngineSessionCoordinator(SessionManagement& session,
                             std::shared_ptr<patterns::engine::Engine> engine,
                             std::shared_ptr<patterns::strategy::ISortStrategyFactory> factory,
                             std::shared_ptr<patterns::historian::EngineHistorian> historian);

    // Clears Engine's weak_ptr — historian stops recording, but Application still owns it
    void disableHistorian();

    // Re-wires existing historian to Engine (Application must still own the shared_ptr)
    void enableHistorian();

protected:
    [[nodiscard]] std::expected<void, std::string> checkPreconditions() override;
    [[nodiscard]] std::expected<void, std::string> connect()            override;
    [[nodiscard]] std::expected<void, std::string> configure()          override;
    [[nodiscard]] std::expected<void, std::string> finalizeSetup()      override;

private:
    SessionManagement&                                         session_;
    std::shared_ptr<patterns::engine::Engine>                  engine_;
    std::shared_ptr<patterns::strategy::ISortStrategyFactory>  factory_;
    std::weak_ptr<patterns::historian::EngineHistorian>        historian_;
};

} // namespace patterns::session
