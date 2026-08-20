#pragma once
#include <expected>
#include <memory>
#include <string>
#include "patterns/engine/Engine.hpp"

namespace patterns::session {

class SessionManagement;

// ==================================
// SESSION ESTABLISHER — Template Method
// Fixed algorithm skeleton for session establishment;
// subclasses provide concrete steps.
// C++23: establish() returns std::expected — monadic and_then/or_else chain.
// ==================================
class SessionEstablisher {
public:
    virtual ~SessionEstablisher() = default;

    std::expected<void, std::string> establish();  // non-overridable skeleton

protected:
    virtual std::expected<void, std::string> checkPreconditions() { return {}; }
    virtual std::expected<void, std::string> configure()          { return {}; }
    virtual std::expected<void, std::string> connect()       = 0;
    virtual std::expected<void, std::string> finalizeSetup() = 0;
};

// Concrete implementation: connects SessionManagement to Engine
class EngineSessionEstablisher : public SessionEstablisher {
public:
    EngineSessionEstablisher(SessionManagement& session,
                             std::shared_ptr<patterns::engine::Engine> engine);

protected:
    std::expected<void, std::string> checkPreconditions() override;
    std::expected<void, std::string> connect()            override;
    std::expected<void, std::string> configure()          override;
    std::expected<void, std::string> finalizeSetup()      override;

private:
    SessionManagement&                        session_;
    std::shared_ptr<patterns::engine::Engine> engine_;
};

} // namespace patterns::session
