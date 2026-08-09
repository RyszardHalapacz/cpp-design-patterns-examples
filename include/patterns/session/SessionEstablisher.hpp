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
    virtual void configure()     {}
    virtual void connect()       = 0;
    virtual void finalizeSetup() = 0;
};

// Concrete implementation: connects SessionManagement to Engine
class EngineSessionEstablisher : public SessionEstablisher {
public:
    EngineSessionEstablisher(SessionManagement& session,
                             std::shared_ptr<patterns::engine::Engine> engine);

protected:
    std::expected<void, std::string> checkPreconditions() override;
    void connect()                                        override;
    void configure()                                      override;
    void finalizeSetup()                                  override;

private:
    SessionManagement&                        session_;
    std::shared_ptr<patterns::engine::Engine> engine_;
};

} // namespace patterns::session
