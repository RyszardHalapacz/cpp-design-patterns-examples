#include <gtest/gtest.h>
#include <memory>

#include "patterns/session/SessionManagement.hpp"
#include "patterns/session/SessionEstablisher.hpp"
#include "patterns/session/SessionAuditObserver.hpp"
#include "patterns/engine/Engine.hpp"
#include "patterns/services/ServiceLocator.hpp"
#include "patterns/services/Logger.hpp"
#include "patterns/observer/ISessionObserver.hpp"
#include "patterns/observer/SessionEvent.hpp"
#include "patterns/strategy/SortStrategyId.hpp"
#include "patterns/gui/Command.hpp"

using namespace patterns::session;
using namespace patterns::observer;
using namespace patterns::strategy;
using namespace patterns::services;

class SessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        ServiceLocator::instance().provide<Logger>(std::make_shared<Logger>());
    }
};

// ─── SessionManagement ───────────────────────────────────────────────────────

TEST_F(SessionTest, ConnectToEngineAttachesEngineAsObserver) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Session] Engine connected"), std::string::npos);
}

TEST_F(SessionTest, OpenSessionStartsEngine) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Engine] Start"), std::string::npos);
}

TEST_F(SessionTest, OpenSessionWithoutEngineLogsError) {
    SessionManagement session;
    testing::internal::CaptureStdout();
    session.openSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No Engine"), std::string::npos);
}

TEST_F(SessionTest, CloseSessionNotifiesObserversAndClearsState) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.closeSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("Closing session"), std::string::npos);
    EXPECT_NE(out.find("[Engine] Stop"), std::string::npos);
}

TEST_F(SessionTest, AddVectorFromGuiWhenSessionInactive) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    // session not opened — inactive
    session.addVectorFromGui({1, 2, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("Session inactive"), std::string::npos);
}

TEST_F(SessionTest, AddVectorFromGuiWhenSessionActive) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.addVectorFromGui({5, 4, 3});
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
}

TEST_F(SessionTest, SortVectorFromGuiForwardsEvent) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.addVectorFromGui({3, 1, 2});
    session.sortVectorFromGui(0);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to sort"), std::string::npos);
}

TEST_F(SessionTest, PrintDataFromGuiForwardsEvent) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.printDataFromGui();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}

TEST_F(SessionTest, SetSortStrategyAllowedStrategy) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.setAllowedStrategies({SortStrategyId::Ascending, SortStrategyId::Descending});
    session.setSortStrategyFromGui(SortStrategyId::Descending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("sort strategy"), std::string::npos);
}

TEST_F(SessionTest, SetSortStrategyForbiddenStrategy) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();
    session.setAllowedStrategies({SortStrategyId::Ascending});
    session.setSortStrategyFromGui(SortStrategyId::Bubble);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("not allowed"), std::string::npos);
}

// Observer: attach / detach

struct SpyObserver : ISessionObserver {
    int callCount = 0;
    SessionEventType lastType{};
    void onSessionEvent(const SessionEvent& e) override {
        ++callCount;
        lastType = e.type;
    }
};

TEST_F(SessionTest, AttachAndDetachObserver) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();

    SpyObserver spy;
    session.attach(&spy);
    session.addVectorFromGui({1});
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(spy.callCount, 1);
    EXPECT_EQ(spy.lastType, SessionEventType::VectorAdded);

    session.detach(&spy);
    testing::internal::CaptureStdout();
    session.addVectorFromGui({2});
    testing::internal::GetCapturedStdout();

    EXPECT_EQ(spy.callCount, 1); // no new calls after detach
}

// ─── EngineSessionEstablisher ────────────────────────────────────────────────

TEST_F(SessionTest, EngineSessionEstablisherConnectsAndOpens) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    EngineSessionEstablisher establisher(session, engine);
    establisher.establish();
    std::string out = testing::internal::GetCapturedStdout();

    EXPECT_NE(out.find("[SessionEstablisher] Starting"), std::string::npos);
    EXPECT_NE(out.find("[Session] Engine connected"), std::string::npos);
    EXPECT_NE(out.find("[Engine] Start"), std::string::npos);
    EXPECT_NE(out.find("[SessionEstablisher] Session established"), std::string::npos);
}

// ─── SessionAuditObserver ────────────────────────────────────────────────────

TEST_F(SessionTest, AuditObserverLogsVectorAdded) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type = SessionEventType::VectorAdded;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: vector added"), std::string::npos);

    // manually delete since we won't trigger SessionClosing
    delete audit;
}

TEST_F(SessionTest, AuditObserverLogsSortRequested) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type = SessionEventType::SortRequested;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: sort requested"), std::string::npos);

    delete audit;
}

TEST_F(SessionTest, AuditObserverLogsPrintRequested) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type = SessionEventType::PrintRequested;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: print requested"), std::string::npos);

    delete audit;
}

TEST_F(SessionTest, AuditObserverLogsStrategyChangeRequested) {
    auto* audit = new SessionAuditObserver();

    SessionEvent ev;
    ev.type       = SessionEventType::StrategyChangeRequested;
    ev.strategyId = SortStrategyId::Descending;

    testing::internal::CaptureStdout();
    audit->onSessionEvent(ev);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Recorded: strategy change to"), std::string::npos);

    delete audit;
}

TEST_F(SessionTest, ExecuteBatchDispatchesAllCommandTypes) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();

    using patterns::gui::Command;
    using patterns::gui::CommandType;
    using patterns::gui::CommandBatch;

    CommandBatch batch = {
        Command{CommandType::AddVector,  {3, 1, 2}, 0},
        Command{CommandType::SortVector, {},        0},
        Command{CommandType::PrintData,  {},        0},
    };
    session.executeBatch(batch);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("command batch"), std::string::npos);
    EXPECT_NE(out.find("GUI wants to add vector"), std::string::npos);
    EXPECT_NE(out.find("GUI wants to sort"), std::string::npos);
    EXPECT_NE(out.find("GUI wants to print"), std::string::npos);
}

TEST_F(SessionTest, SetSortStrategyFromGuiWithNoEngineLogsError) {
    SessionManagement session; // no engine connected

    testing::internal::CaptureStdout();
    session.setSortStrategyFromGui(SortStrategyId::Ascending);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("No Engine"), std::string::npos);
}

TEST_F(SessionTest, AuditObserverDeletesItselfOnSessionClosing) {
    patterns::engine::Engine engine;
    SessionManagement session;

    testing::internal::CaptureStdout();
    session.connectToEngine(engine);
    session.openSession();

    // Allocate on heap — will be deleted by delete this inside onSessionEvent
    auto* audit = new SessionAuditObserver();
    session.attach(audit);

    // closeSession fires SessionClosing → audit calls delete this, engine calls stop()
    session.closeSession();
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("[Audit] Session closing"), std::string::npos);
    // If we reach here without crash, delete this worked correctly
}
