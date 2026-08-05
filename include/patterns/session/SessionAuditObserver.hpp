#pragma once
#include "patterns/observer/ISessionObserver.hpp"

namespace patterns::session {

// ==================================
// SESSION AUDIT OBSERVER
// Records session events; destroys itself (delete this)
// when the session closes — lifetime tied to the session.
// ==================================
class SessionAuditObserver : public patterns::observer::ISessionObserver {
public:
    SessionAuditObserver();
    ~SessionAuditObserver() override;

    void onSessionEvent(const patterns::observer::SessionEvent& event) override;
};

} // namespace patterns::session
