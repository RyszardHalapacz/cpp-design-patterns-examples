#pragma once

namespace patterns::engine  { class Engine; }

namespace patterns::session {

class SessionManagement;

// ==================================
// SESSION ESTABLISHER — Template Method
// Fixed algorithm skeleton for session establishment;
// subclasses provide concrete steps.
// ==================================
class SessionEstablisher {
public:
    virtual ~SessionEstablisher() = default;

    void establish();  // non-overridable skeleton

protected:
    virtual bool checkPreconditions() { return true; }
    virtual void configure()          {}
    virtual void connect()      = 0;
    virtual void finalizeSetup() = 0;
};

// Concrete implementation: connects SessionManagement to Engine
class EngineSessionEstablisher : public SessionEstablisher {
public:
    EngineSessionEstablisher(SessionManagement& session, patterns::engine::Engine& engine);

protected:
    bool checkPreconditions() override;
    void connect()            override;
    void configure()          override;
    void finalizeSetup()      override;

private:
    SessionManagement&       session_;
    patterns::engine::Engine&  engine_;
};

} // namespace patterns::session
